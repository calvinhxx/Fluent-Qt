#!/usr/bin/env python3
"""Run local integration gates and the changed surface on explicit Qt builds."""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
EXCLUDED_LABELS = "^(manual_visual|local_desktop|known_contract_gap)$"


def load_helper(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


CLASSIFIER = load_helper("local_ci_classifier", ROOT / ".github/scripts/classify_ci_changes.py")
RELEASE = load_helper("local_release_preflight", ROOT / "scripts/release/preflight.py")


def changed_paths(root: Path, base_ref: str) -> list[str]:
    """Include commits, staged/unstaged edits, untracked files, and rename sources."""
    commands = (
        ["diff", "--name-only", "--no-renames", "-z", f"{base_ref}...HEAD", "--"],
        ["diff", "--name-only", "--no-renames", "-z", "HEAD", "--"],
        ["ls-files", "--others", "--exclude-standard", "-z"],
    )
    paths: set[str] = set()
    for command in commands:
        result = subprocess.run(["git", *command], cwd=root, capture_output=True, check=True)
        paths.update(os.fsdecode(path) for path in result.stdout.split(b"\0") if path)
    return sorted(paths)


def selection_for(paths: list[str]):
    # Tooling checks run separately; changing a checker alone needs no Qt rebuild.
    runtime_paths = [
        path for path in paths
        if not path.startswith((".github/", ".githooks/", "tools/", "scripts/"))
    ]
    if not runtime_paths:
        return CLASSIFIER.CppTestSelection("none"), False
    cpp = CLASSIFIER.select_cpp_tests(runtime_paths)
    return cpp, CLASSIFIER.classify_changes(runtime_paths).should_build_pyside


def version_line(version: str) -> str:
    match = re.match(r"^(\d+)\.(\d+)(?:\.|$)", version)
    if not match:
        raise ValueError(f"Cannot identify Qt version: {version!r}")
    return ".".join(match.groups())


def required_versions(root: Path, cpp_needed: bool, pyside_needed: bool) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    if cpp_needed:
        scenarios = json.loads((root / ".github/ci-cpp-matrix.json").read_text())["scenarios"]
        versions = {version_line(row["qt_version"]) for row in scenarios if row["test"]}
        # The minimum supported line of each Qt major, taken from CI's own matrix.
        minimum: dict[int, tuple[int, int]] = {}
        for value in versions:
            parts = tuple(map(int, value.split(".")))
            minimum[parts[0]] = min(minimum.get(parts[0], parts), parts)
        result["cpp"] = [".".join(map(str, value)) for _, value in sorted(minimum.items())]
    if pyside_needed:
        scenarios = json.loads((root / "bindings/pyside6/wheel-matrix.json").read_text())["scenarios"]
        result["pyside"] = sorted({
            version_line(row["qt_version"]) for row in scenarios
            if row["compatibility"] or row["release"]
        }, key=lambda value: tuple(map(int, value.split("."))))
    return result


def read_build(build_dir: Path, kind: str, root: Path) -> dict:
    cache = {}
    for line in (build_dir / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        match = re.match(r"([^/#][^:]*):[^=]+=(.*)$", line)
        if match:
            cache[match[1]] = match[2]
    source_root = cache.get("CMAKE_HOME_DIRECTORY")
    if not source_root or Path(source_root).resolve() != root.resolve():
        raise ValueError(f"{build_dir}: build belongs to a different checkout")
    required_options = ["BUILD_TESTING"]
    required_options += (["FLUENT_QT_BUILD_TESTS"] if kind == "cpp" else [
        "FLUENT_QT_BUILD_PYSIDE6_BINDINGS", "FLUENT_QT_BUILD_PYSIDE6_GALLERY",
    ])
    for option in required_options:
        if cache.get(option, "").upper() not in {"ON", "1", "YES", "TRUE"}:
            raise ValueError(f"{build_dir}: configure with {option}=ON first")
    qt_dir = next((cache[key] for key in ("Qt6Core_DIR", "Qt5Core_DIR")
                   if cache.get(key) and not cache[key].endswith("-NOTFOUND")), None)
    qt_version = None
    if qt_dir:
        for file in sorted(Path(qt_dir).glob("*ConfigVersion*.cmake")):
            match = re.search(r'set\(PACKAGE_VERSION\s+"?([\d.]+)', file.read_text(encoding="utf-8"))
            if match:
                qt_version = match[1]
                break
    if not qt_version:
        raise ValueError(f"{build_dir}: could not read the configured Qt SDK version")
    result = {"kind": kind, "build_dir": str(build_dir), "qt": qt_version,
              "build_type": cache.get("CMAKE_BUILD_TYPE", ""), "status": "not_run",
              "gallery_enabled": cache.get("FLUENT_QT_BUILD_GALLERY", "").upper() in {"ON", "1", "YES", "TRUE"}}
    if kind == "pyside":
        interpreter = cache.get("Python_EXECUTABLE") or cache.get("_Python_EXECUTABLE")
        if not interpreter:
            raise ValueError(f"{build_dir}: no configured Python interpreter")
        result["python"] = interpreter
    return result


def lane_commands(lane: dict, cpp, config: str) -> list[list[str]]:
    build_dir = lane["build_dir"]
    # Makefile generators cannot discover a newly added target until configure.
    configure = ["cmake", "-S", str(ROOT), "-B", build_dir]
    # The default bindings build owns generated stubs and the staged Gallery too.
    build = [sys.executable, str(ROOT / "tools/dev/fluent_qt_build.py"), build_dir, "--config", config]
    if lane["kind"] == "cpp":
        build += ["--target", *cpp.targets]
    labels = cpp.label_regex if lane["kind"] == "cpp" else "^pyside$"
    test = [
        "ctest", "--test-dir", build_dir, "--build-config", config,
        "-L", labels, "-LE", EXCLUDED_LABELS, "--no-tests=error",
        "--output-on-failure", "--timeout", "240",
    ]
    return [configure, build, test]


def check_python_runtime(lane: dict) -> dict:
    code = (
        "import json,sys,PySide6,shiboken6; from PySide6.QtCore import qVersion; "
        "print(json.dumps({'python_version':sys.version.split()[0],"
        "'pyside':PySide6.__version__,'shiboken':shiboken6.__version__,'qt_runtime':qVersion()}))"
    )
    process = subprocess.run([lane["python"], "-c", code], capture_output=True, text=True, check=True)
    versions = json.loads(process.stdout)
    if any(versions[key] != lane["qt"] for key in ("pyside", "shiboken", "qt_runtime")):
        raise ValueError(f"{lane['build_dir']}: Qt SDK/PySide/Shiboken versions differ: {versions}")
    return versions


def missing_coverage(required: dict, lanes: list[dict]) -> list[str]:
    passed = {(lane["kind"], version_line(lane["qt"])) for lane in lanes if lane["status"] == "passed"}
    return [f"{kind}/Qt {version}" for kind, versions in required.items()
            for version in versions if (kind, version) not in passed]


def run_logged(label: str, command: list[str], log_dir: Path, index: int) -> dict:
    log_dir.mkdir(parents=True, exist_ok=True)
    log = log_dir / f"{index:02d}.log"
    print(f"[preflight] {label}", flush=True)
    environment = os.environ.copy()
    if command[0] == "ctest":
        environment.setdefault("QT_QPA_PLATFORM", "offscreen")
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, env=environment, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding="utf-8", errors="replace")[-6000:], file=sys.stderr)
        print(f"Failed: {label}; full log: {log}", file=sys.stderr)
    return {"label": label, "command": command, "log": str(log), "returncode": result.returncode}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-ref", default="origin/main")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--checks-only", action="store_true", help="Run source/packaging gates without Qt or release/tag checks.")
    mode.add_argument("--plan", action="store_true", help="Show selection and configured SDKs without building or testing.")
    parser.add_argument("--build-dir", action="append", default=[], type=Path, help="Configured native Qt test build; repeat for other SDK versions.")
    parser.add_argument("--pyside-build-dir", action="append", default=[], type=Path, help="Configured bindings + Gallery test build; repeat for compatibility/release Qt.")
    parser.add_argument("--config", default="Release")
    parser.add_argument("--report", type=Path, default=ROOT / "build/local-preflight/report.json")
    args = parser.parse_args(argv)
    report = {"status": "not_run", "host": platform.platform(), "checks": [], "lanes": []}
    report_path = args.report.resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        paths = [] if args.checks_only else changed_paths(ROOT, args.base_ref)
        cpp, pyside = selection_for(paths)
        gallery_needed = any(path.startswith(("app/", "tests/gallery/"))
                             and not CLASSIFIER.is_documentation_path(path) for path in paths)
        required = required_versions(ROOT, cpp.scope != "none", pyside)
        report.update({"base_ref": args.base_ref, "changed_paths": paths,
                       "cpp_scope": cpp.scope, "required_versions": required})
        if not args.checks_only:
            report["head_sha"] = subprocess.run(
                ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True, capture_output=True, check=True
            ).stdout.strip()
        for kind, directories, needed in (("cpp", args.build_dir, cpp.scope != "none"),
                                          ("pyside", args.pyside_build_dir, pyside)):
            if needed:
                for directory in directories:
                    lane = read_build(directory.resolve(), kind, ROOT)
                    if kind == "cpp" and gallery_needed and not lane["gallery_enabled"]:
                        raise ValueError(f"{directory}: configure with FLUENT_QT_BUILD_GALLERY=ON first")
                    lane["commands"] = lane_commands(lane, cpp, args.config)
                    report["lanes"].append(lane)
        print(f"Selected C++: {cpp.scope}; PySide6: {pyside}. Baselines: {required}")
        for lane in report["lanes"]:
            print(f"Configured {lane['kind']}: Qt {lane['qt']} at {lane['build_dir']}")
        if args.plan:
            report["status"] = "planned"
            report["missing_coverage"] = missing_coverage(required, [])
            print("Plan only: no build or runtime validation was performed.")
            return 0

        with tempfile.TemporaryDirectory(prefix="fluentqt-local-preflight-") as temporary:
            for label, command in RELEASE.integration_checks(Path(temporary)):
                result = run_logged(label, command, report_path.parent / "logs", len(report["checks"]))
                report["checks"].append(result)
                if result["returncode"]:
                    report["status"] = "failed"
                    return 1
        if args.checks_only:
            report["status"] = "checks_only_passed"
            print("Source/packaging gates passed. Qt runtime compatibility was not tested.")
            return 0

        for lane in report["lanes"]:
            if lane["kind"] == "pyside":
                lane.update(check_python_runtime(lane))
            for index, command in enumerate(lane["commands"]):
                result = run_logged(f"{lane['kind']} Qt {lane['qt']}: {command[0]}", command,
                                    report_path.parent / "logs", len(report["checks"]))
                report["checks"].append(result)
                if result["returncode"]:
                    lane["status"] = "failed"
                    report["status"] = "failed"
                    return 1
                if index in (0, 1):
                    actual = read_build(Path(lane["build_dir"]), lane["kind"], ROOT)
                    if (actual["qt"], actual.get("python")) != (lane["qt"], lane.get("python")):
                        raise ValueError("The configured toolchain changed during the build; regenerate the plan")
            lane["status"] = "passed"
        report["missing_coverage"] = missing_coverage(required, report["lanes"])
        if report["missing_coverage"]:
            report["status"] = "incomplete"
            print("Not verified: " + ", ".join(report["missing_coverage"]))
            print("Configure the missing SDK builds, or validate those lines on another host/CI.")
            return 2
        report["status"] = "passed"
        print("Selected local checks passed. Platform, installed-wheel and release gates remain separate.")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        report["status"] = "failed"
        report["error"] = str(error)
        print(f"error: {error}", file=sys.stderr)
        return 1
    finally:
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {report_path}")


if __name__ == "__main__":
    raise SystemExit(main())
