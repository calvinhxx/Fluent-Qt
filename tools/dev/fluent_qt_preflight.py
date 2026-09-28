#!/usr/bin/env python3
"""Run local integration gates and the changed surface on explicit Qt builds."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
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


@dataclass(frozen=True)
class FocusedCppSelection:
    targets: tuple[str, ...]
    label_regex: str
    scope: str = "selected"


def registered_sources(root: Path, directory: str, command: str) -> dict[str, set[str]]:
    """Read literal source ownership from existing CMake registrations, not a second catalog."""
    owners: dict[str, set[str]] = {}
    for cmake in (root / directory).rglob("CMakeLists.txt"):
        text = re.sub(r"(?m)^\s*#.*$", "", cmake.read_text(encoding="utf-8"))
        for match in re.finditer(rf"\b{command}\s*\(([^)]*)\)", text, re.DOTALL):
            body = match[1]
            target = re.match(r'\s*(?:NAME\s+)?"?(test_[a-z0-9_]+)\b', body)
            if not target:
                continue
            body = body.replace("${CMAKE_CURRENT_SOURCE_DIR}", str(cmake.parent))
            body = body.replace("${CMAKE_SOURCE_DIR}", str(root))
            for token in re.findall(r'[^\s";]+\.(?:cpp|h|py)\b', body):
                if "$" in token:
                    continue
                source = (cmake.parent / token).resolve()
                if source.is_relative_to(root):
                    owners.setdefault(source.relative_to(root).as_posix(), set()).add(target[1])
    return owners


def quick_selection(paths: list[str], root: Path = ROOT):
    """Owner feedback only; headers and unknown inputs retain the broad CI selection."""
    cpp_owners = registered_sources(root, "tests", "add_qt_test_module")
    targets: set[str] = set()
    broad_paths = []
    for path in paths:
        own_test = ("tests/" + Path(path).with_name("Test" + Path(path).stem + ".cpp").as_posix()
                    .removeprefix("src/"))
        if path in cpp_owners:
            targets.update(cpp_owners[path])
        elif (path.startswith("src/components/") and Path(path).suffix == ".cpp"
              and own_test in cpp_owners and (root / path).with_suffix(".h").is_file()):
            targets.update(cpp_owners[own_test])
        else:
            broad_paths.append(path)
    broad, _ = selection_for(broad_paths)
    if broad.scope == "all":
        cpp = broad
    else:
        labels = sorted(targets | set(broad.groups))
        cpp = (FocusedCppSelection(tuple(sorted(targets | set(broad.targets))),
                                   "^(" + "|".join(labels) + ")$")
               if labels else CLASSIFIER.CppTestSelection("none"))

    py_owners = registered_sources(root, "bindings/pyside6", "add_test")
    python_tests: set[str] = set()
    full_python = False
    spatial_modules = {
        "particle_compositor.py": {"gallery_particle_compositor", "gallery_spatial"},
        "glyph_paint_device.py": {"gallery_glyph_paint_device", "gallery_particle_compositor", "gallery_spatial"},
        "spatial_backdrop.py": {"gallery_spatial_backdrop", "gallery_spatial"},
        "spatial_controller.py": {"gallery_spatial", "gallery_particle_compositor", "gallery_spatial_backdrop"},
    }
    for path in paths:
        if path in py_owners:
            python_tests.update(name for name in py_owners[path] if not name.endswith("_native"))
        elif path.startswith("bindings/pyside6/gallery/src/fluentqt_gallery/") and Path(path).name in spatial_modules:
            python_tests.update("test_pyside6_" + name for name in spatial_modules[Path(path).name])
            python_tests.add("test_pyside6_gallery_python_snippet_catalog")
        elif (not CLASSIFIER.is_documentation_path(path) and CLASSIFIER.affects_pyside(path)
              and not path.startswith("src/components/")):
            full_python = True
        elif path.startswith("src/components/") and Path(path).suffix == ".h":
            full_python = True
    return cpp, "^(" + "|".join(sorted(python_tests)) + ")$" if python_tests and not full_python else "", full_python or bool(python_tests)


def quick_checks(paths: list[str], output_dir: Path = ROOT / "build/local-preflight") -> list[tuple[str, list[str]]]:
    checks: dict[str, list[str]] = {}
    for path in paths:
        file = ROOT / path
        if path.startswith(("tools/", "scripts/", ".github/")):
            test = file if file.name.startswith("test_") else file.with_name("test_" + file.name.replace("-", "_"))
            if file.suffix == ".py" and test.is_file():
                checks[str(test)] = [sys.executable, str(test)]
            else:
                # An unrecognized tool must not turn into an empty successful gate.
                checks.update(RELEASE.integration_checks(output_dir))
                checks["CI boundaries"] = [sys.executable, ".github/scripts/validate-ci-workflow-boundaries.py"]
        elif CLASSIFIER.is_documentation_path(path):
            checks["documentation"] = [sys.executable, "tools/docs/validate_documentation.py", "--project-root", "."]
        elif path == "docs/development/visual-evidence-inventory.json":
            checks["visual evidence"] = [sys.executable, "tools/quality/validate_visual_evidence_inventory.py", "--project-root", "."]
        if file.suffix in {".cpp", ".h"}:
            checks["C++ format"] = [sys.executable, "tools/quality/check_cpp_format.py", "--working-tree"]
    return list(checks.items())


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


def lane_commands(lane: dict, cpp, config: str, python_regex: str = "") -> list[list[str]]:
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
    if lane["kind"] == "pyside" and python_regex:
        test += ["-R", python_regex]
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
    parser.add_argument("--base-ref", help="Diff baseline: HEAD for --quick, origin/main otherwise.")
    parser.add_argument("--quick", action="store_true", help="Focused current-host feedback, without the release SDK matrix.")
    parser.add_argument("--paths", nargs="+", help="Explicit repository-relative paths for --quick; other edits remain unverified.")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--checks-only", action="store_true", help="Run source/packaging gates without Qt or release/tag checks.")
    mode.add_argument("--plan", action="store_true", help="Show selection and configured SDKs without building or testing.")
    parser.add_argument("--build-dir", action="append", default=[], type=Path, help="Configured native Qt test build; repeat for other SDK versions.")
    parser.add_argument("--pyside-build-dir", action="append", default=[], type=Path, help="Configured bindings + Gallery test build; repeat for compatibility/release Qt.")
    parser.add_argument("--config", default="Release")
    parser.add_argument("--report", type=Path, default=ROOT / "build/local-preflight/report.json")
    args = parser.parse_args(argv)
    if args.paths and not args.quick:
        parser.error("--paths requires --quick")
    if args.quick and args.checks_only:
        parser.error("--quick and --checks-only have different validation scopes")
    if args.paths and any(CLASSIFIER._has_unsafe_path_syntax(path) for path in args.paths):
        parser.error("--paths must contain normalized repository-relative paths")
    args.base_ref = args.base_ref or ("HEAD" if args.quick else "origin/main")
    report = {"status": "not_run", "host": platform.platform(), "checks": [], "lanes": []}
    report_path = args.report.resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        paths = [] if args.checks_only else (sorted(set(args.paths)) if args.paths else changed_paths(ROOT, args.base_ref))
        python_regex = ""
        if args.quick:
            cpp, python_regex, pyside = quick_selection(paths)
        else:
            cpp, pyside = selection_for(paths)
        gallery_needed = any(path.startswith(("app/", "tests/gallery/"))
                             and not CLASSIFIER.is_documentation_path(path) for path in paths)
        required = {} if args.quick else required_versions(ROOT, cpp.scope != "none", pyside)
        report.update({"base_ref": args.base_ref, "changed_paths": paths,
                       "cpp_scope": cpp.scope, "required_versions": required,
                       "mode": "quick" if args.quick else "integration",
                       "coverage": "owning tests on supplied SDKs; integration and platform checks deferred" if args.quick else "CI-selected integration",
                       "explicit_paths": bool(args.paths), "cpp_targets": cpp.targets,
                       "cpp_label_regex": cpp.label_regex,
                       "python_test_regex": python_regex})
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
                    lane["commands"] = lane_commands(lane, cpp, args.config, python_regex)
                    report["lanes"].append(lane)
        print(f"Selected C++: {' '.join(cpp.targets) or 'none'}; "
              f"PySide6: {(python_regex or 'all') if pyside else 'none'}. Baselines: {required}")
        for lane in report["lanes"]:
            print(f"Configured {lane['kind']}: Qt {lane['qt']} at {lane['build_dir']}")
        if args.plan:
            report["status"] = "planned"
            report["missing_coverage"] = missing_coverage(required, [])
            print("Plan only: no build or runtime validation was performed.")
            return 0

        if args.quick and ((cpp.scope != "none" and not args.build_dir) or (pyside and not args.pyside_build_dir)):
            raise ValueError("Quick runtime checks need the selected --build-dir and/or --pyside-build-dir; use --plan to inspect selection")
        with tempfile.TemporaryDirectory(prefix="fluentqt-local-preflight-") as temporary:
            checks = quick_checks(paths, Path(temporary)) if args.quick else RELEASE.integration_checks(Path(temporary))
            for label, command in checks:
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
        report["status"] = "quick_passed" if args.quick else "passed"
        print("Selected current-host checks passed. Platform, installed-wheel and release gates remain separate.")
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
