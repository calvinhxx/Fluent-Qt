#!/usr/bin/env python3
"""Measure paired builds in a disposable GitHub runner, never in a local checkout."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time


LANES = {
    "linux-sanitizers": {"os": "ubuntu-22.04", "qt": "6.2.4", "arch": "", "triplet": "x64-linux"},
    "windows-targeted": {"os": "windows-2022", "qt": "6.2.4", "arch": "win64_msvc2019_64", "triplet": "x64-windows"},
    "intel-cache": {"os": "macos-15-intel", "qt": "6.2.4", "arch": "clang_64", "triplet": "x64-osx"},
    "python-pch": {"os": "macos-15", "qt": "6.9.3", "arch": "clang_64", "triplet": ""},
    "wasm": {"os": "ubuntu-24.04", "qt": "6.9.3", "arch": "", "triplet": ""},
}
SCENARIOS = {
    "linux-sanitizers": "linux-x64-qt62-contract-sanitizers",
    "windows-targeted": "win-x64-qt62-platform",
    "intel-cache": "mac-x64-qt62-gallery",
}
# These fixture-only fixes keep the comparison from being interrupted by known
# failures. They do not change test selection, grouping, or library sources.
BASELINE_FIXTURES = (
    "tests/components/spatial/TestSpatialPlugin.cpp",
    "tests/gallery/GallerySingleInstanceProbe.cpp",
    "tests/gallery/TestGallerySingleInstance.cpp",
)


def variants(lane: str) -> list[dict]:
    if lane in {"linux-sanitizers", "windows-targeted"}:
        return [{"version": x, "cache": False} for x in ("baseline", "candidate", "candidate", "baseline")]
    if lane == "intel-cache":
        return [{"version": "candidate", "cache": x} for x in (False, "cold", "warm", False)]
    if lane == "python-pch":
        return [{"version": "candidate", "cache": False, "pch": x} for x in (False, True, True, False)]
    return [{"version": "candidate", "cache": False} for _ in range(2)]


def checked_output(command, cwd=None, env=None):
    return subprocess.check_output(command, cwd=cwd, env=env, text=True, encoding="utf-8", errors="replace").strip()


def save_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def object_snapshot(build: Path) -> dict:
    return {str(p.relative_to(build)): (p.stat().st_size, p.stat().st_mtime_ns)
            for suffix in ("*.o", "*.obj") for p in build.rglob(suffix) if p.is_file()}


def existing_link_outputs(build: Path) -> list[Path]:
    reply = build / ".cmake/api/v1/reply"
    models = sorted(reply.glob("codemodel-v2-*.json"))
    if not models:
        raise RuntimeError("CMake did not publish its codemodel")
    model = json.loads(models[-1].read_text(encoding="utf-8"))
    configuration = next((c for c in model["configurations"] if c["name"] == "Debug"), model["configurations"][0])
    outputs = set()
    for target in configuration["targets"]:
        data = json.loads((reply / target["jsonFile"]).read_text(encoding="utf-8"))
        if data["type"] not in {"EXECUTABLE", "MODULE_LIBRARY", "SHARED_LIBRARY"}:
            continue
        for artifact in data.get("artifacts", []):
            path = (build / artifact["path"]).resolve()
            if not path.is_relative_to(build.resolve()):
                raise ValueError(f"Refusing an artifact outside the benchmark build: {path}")
            if path.is_file():
                outputs.add(path)
    if not outputs:
        raise RuntimeError("No linked outputs were produced")
    return sorted(outputs)


def ninja_timings(path: Path) -> list[dict]:
    if not path.exists():
        return []
    entries = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise ValueError(f"Invalid Ninja timing row: {line}")
        start, end = int(fields[0]), int(fields[1])
        entries.append({"output": fields[3], "start_ms": start, "end_ms": end,
                        "duration_ms": end - start})
    return entries


class Measurement:
    def __init__(self, root: Path, output: Path, lane: str):
        self.root, self.output, self.lane = root, output, lane
        self.report = {"lane": lane, "host": platform.platform(), "machine": platform.machine(),
                       "runner_name": os.environ.get("RUNNER_NAME"), "cpu_count": os.cpu_count(),
                       "measurements": [], "variants": []}
        self.failed = False

    def persist(self):
        save_json(self.output / "report.json", self.report)

    def run(self, label: str, command: list[str], env: dict, *, required=True):
        print(f"BEGIN {label}: {' '.join(command)}", flush=True)
        log = self.output / (label + ".log")
        started = time.time()
        launch = command
        if Path("/usr/bin/time").is_file() and os.name != "nt":
            launch = ["/usr/bin/time", "-l" if sys.platform == "darwin" else "-v", *command]
        with log.open("w", encoding="utf-8") as stream:
            process = subprocess.Popen(launch, cwd=self.root, env=env, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
            last_progress = time.monotonic()
            for line in process.stdout:
                stream.write(line)
                if time.monotonic() - last_progress >= 45:
                    print(f"{label}: {line.rstrip()}", flush=True)
                    last_progress = time.monotonic()
            code = process.wait()
        result = {"phase": label, "command": command, "started_at": started,
                  "wall_seconds": round(time.time() - started, 3), "exit_code": code, "log": log.name}
        self.report["measurements"].append(result)
        self.persist()
        print("END " + json.dumps(result), flush=True)
        if code:
            self.failed = True
            print(log.read_text(encoding="utf-8")[-6000:], flush=True)
            if required:
                raise RuntimeError(f"{label} failed ({code})")
        return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", choices=["all", *LANES])
    parser.add_argument("--lane", choices=LANES)
    parser.add_argument("--baseline")
    parser.add_argument("--candidate")
    parser.add_argument("--root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--plan", action="store_true")
    args = parser.parse_args(argv)
    if args.matrix:
        rows = [{"lane": lane, **data} for lane, data in LANES.items() if args.matrix in {"all", lane}]
        print("matrix=" + json.dumps({"include": rows}))
        return 0
    if not args.lane:
        parser.error("--lane is required")
    if args.plan:
        print(json.dumps(variants(args.lane), indent=2))
        return 0
    if os.environ.get("GITHUB_ACTIONS") != "true":
        parser.error("Measurements may only reset a disposable GitHub Actions checkout")
    for name in ("baseline", "candidate"):
        if not re.fullmatch(r"[0-9a-f]{40}", getattr(args, name) or ""):
            parser.error(f"--{name} must be an immutable full commit SHA")
    root = args.root.resolve()
    if root != Path(os.environ["GITHUB_WORKSPACE"]).resolve():
        parser.error("--root must be this runner's GITHUB_WORKSPACE")
    dirty = checked_output(["git", "status", "--porcelain"], root)
    if dirty:
        parser.error("The disposable checkout must start clean:\n" + dirty)
    output = args.output.resolve()
    if output.is_relative_to(root):
        parser.error("Reports must be outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    build = root / "build/ci-performance"
    original_head = checked_output(["git", "rev-parse", "HEAD"], root)
    for ref in (args.baseline, args.candidate):
        if checked_output(["git", "rev-parse", ref + "^{commit}"], root) != ref:
            parser.error("Commit resolution changed")
    scenario = next((s for s in json.loads((root / ".github/ci-cpp-matrix.json").read_text())["scenarios"]
                     if s["id"] == SCENARIOS.get(args.lane)), None)
    measurement = Measurement(root, output, args.lane)
    measurement.report.update({"baseline": args.baseline, "candidate": args.candidate,
                               "workflow_sha": original_head, "baseline_fixture_overlays": list(BASELINE_FIXTURES),
                               "order": variants(args.lane), "ccache_published": False,
                               "scope": "Build/relink/CTest benchmark; does not replace CI Gate or publish packages."})
    measurement.report["unchanged_runtime_inputs"] = checked_output(
        ["git", "diff", "--name-only", args.baseline, args.candidate, "--", "src", "app", "cmake",
         "CMakeLists.txt", "CMakePresets.json", "vcpkg.json"], root).splitlines()
    if args.lane in {"wasm", "intel-cache"} and measurement.report["unchanged_runtime_inputs"]:
        raise RuntimeError("WASM/cache experiments require unchanged runtime build inputs")
    base_env = dict(os.environ, QT_QPA_PLATFORM="offscreen", SKIP_VISUAL_TEST="1")
    base_env.pop("CMAKE_C_COMPILER_LAUNCHER", None)
    base_env.pop("CMAKE_CXX_COMPILER_LAUNCHER", None)
    for tool in ("cmake", "ninja", "ccache"):
        if shutil.which(tool):
            measurement.report[tool] = checked_output([tool, "--version"]).splitlines()[0]
    measurement.persist()
    try:
        for index, variant in enumerate(variants(args.lane), 1):
            prefix = f"{index:02}"
            ref = args.baseline if variant["version"] == "baseline" else args.candidate
            subprocess.run(["git", "checkout", "--force", "--detach", ref], cwd=root, check=True)
            if variant["version"] == "baseline":
                for relative in BASELINE_FIXTURES:
                    (root / relative).write_bytes(subprocess.check_output(["git", "show", args.candidate + ":" + relative], cwd=root))
            if build.exists():
                shutil.rmtree(build)
            query = build / ".cmake/api/v1/query"
            query.mkdir(parents=True)
            (query / "codemodel-v2").touch()
            env = dict(base_env)
            env["CCACHE_DISABLE"] = "1"
            if variant["cache"]:
                env.pop("CCACHE_DISABLE")
                env.update(CCACHE_DIR=str(output / "ccache"), CCACHE_MAXSIZE="192M",
                           CCACHE_NOHASHDIR="1", CCACHE_COMPRESSLEVEL="5")
                if variant["cache"] == "cold" and (output / "ccache").exists():
                    shutil.rmtree(output / "ccache")
                subprocess.run(["ccache", "--zero-stats"], env=env, check=True)
            launcher = "ccache" if variant["cache"] else ""
            configure = ["cmake"]
            targets = []
            jobs = 4
            if scenario:
                configure += ["--preset", scenario["preset"], "-B", str(build),
                              "-DVCPKG_INSTALLED_DIR=" + str(output / "vcpkg-installed")]
                configure += scenario["configure_args"].split()
                if env.get("QT_ROOT_DIR"):
                    configure += ["-DCMAKE_PREFIX_PATH=" + env["QT_ROOT_DIR"].replace("\\", "/")]
                targets = scenario["build_targets"].split()
                if args.lane == "linux-sanitizers":
                    env.update(ASAN_OPTIONS=scenario["asan_options"], UBSAN_OPTIONS=scenario["ubsan_options"])
            elif args.lane == "wasm":
                configure += ["--preset", "wasm", "-B", str(build)]
            else:
                jobs = 3
                configure += ["-S", str(root), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
                              "-DCMAKE_PREFIX_PATH=" + env["QT_ROOT_DIR"], "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
                              "-DPython_EXECUTABLE=" + sys.executable, "-DFLUENT_QT_PYSIDE6_REQUIRES_PYTHON=>=3.11,<3.14",
                              "-DFLUENT_QT_PYSIDE6_PRECOMPILE_WRAPPERS=" + ("ON" if variant["pch"] else "OFF"),
                              "-DFLUENT_QT_BUILD_SPATIAL=ON", "-DFLUENT_QT_BUILD_PYSIDE6_BINDINGS=ON",
                              "-DFLUENT_QT_REQUIRE_INTERFACE_ZOOM=ON", "-DFLUENT_QT_BUILD_PYSIDE6_GALLERY=ON",
                              "-DFLUENT_QT_BUILD_EXAMPLES=OFF", "-DFLUENT_QT_BUILD_GALLERY=OFF",
                              "-DFLUENT_QT_BUILD_TESTS=OFF", "-DFLUENT_QT_INSTALL=OFF", "-DBUILD_TESTING=ON"]
                targets = ["fluentqt_pyside6_stubs"]
            configure += ["-DCMAKE_C_COMPILER_LAUNCHER=" + launcher,
                          "-DCMAKE_CXX_COMPILER_LAUNCHER=" + launcher, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
            record = {"index": index, **variant, "sha": ref, "parallel": jobs}
            measurement.report["variants"].append(record)
            measurement.run(prefix + "-configure", configure, env)
            command = [sys.executable, str(root / "tools/dev/fluent_qt_build.py"), "--jobs", str(jobs),
                       str(build), "--config", "Debug" if scenario else "Release"]
            if targets:
                command += ["--target", *targets]
            if os.name == "nt":
                command += ["--", "/clp:PerformanceSummary"]
            measurement.run(prefix + "-build", command, env)
            before_objects = object_snapshot(build)
            outputs = existing_link_outputs(build)
            record.update(objects=len(before_objects), linked_outputs=[str(p.relative_to(build)) for p in outputs])
            save_json(output / (prefix + "-ninja-build.json"), ninja_timings(build / ".ninja_log"))
            if variant["cache"]:
                measurement.run(prefix + "-cache-stats", ["ccache", "--show-stats", "--verbose"], env)
            # Only generated linker outputs in this disposable build are removed.
            # Keep all objects; reject the measurement if the rebuild recompiles any.
            for path in outputs:
                path.unlink(missing_ok=True)
                if os.name == "nt":
                    for suffix in (".pdb", ".ilk"):
                        path.with_suffix(suffix).unlink(missing_ok=True)
            measurement.run(prefix + "-relink", command, env)
            record["relink_changed_objects"] = sorted(k for k, v in object_snapshot(build).items() if before_objects.get(k) != v)
            if record["relink_changed_objects"]:
                raise RuntimeError("Relink measurement recompiled objects")
            save_json(output / (prefix + "-ninja-after-relink.json"), ninja_timings(build / ".ninja_log"))
            if scenario and scenario["test"]:
                test = ["ctest", "--test-dir", str(build), "-C", "Debug", "-L", scenario["test_labels"],
                        "-LE", scenario["exclude_labels"], "--parallel", str(scenario["ctest_parallel"]),
                        "--timeout", str(scenario["ctest_timeout"]), "--output-on-failure", "--no-tests=error"]
                measurement.run(prefix + "-test", test, env, required=False)
            elif args.lane == "python-pch":
                # Match ci-python.yml: use PySide's runtime instead of the SDK
                # plugin paths exported by install-qt-action for compilation.
                test_env = dict(env)
                for name in ("PYTHONPATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH",
                             "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH"):
                    test_env.pop(name, None)
                measurement.run(prefix + "-test", ["ctest", "--test-dir", str(build), "-L", "^pyside$",
                                                  "--output-on-failure", "--no-tests=error"], test_env, required=False)
            measurement.persist()
    finally:
        subprocess.run(["git", "checkout", "--force", "--detach", original_head], cwd=root, check=True)
        measurement.persist()
    summary = Path(os.environ["GITHUB_STEP_SUMMARY"])
    with summary.open("a", encoding="utf-8") as stream:
        stream.write(f"## {args.lane}\n\nSame runner; fixed source/build paths; dependencies prepared outside the timed builds.\n\n")
        stream.write("| Variant | Phase | Seconds | Result |\n|---|---|---:|---|\n")
        for row in measurement.report["measurements"]:
            stream.write(f"| {row['phase'].split('-')[0]} | {row['phase']} | {row['wall_seconds']:.2f} | {row['exit_code']} |\n")
    return int(measurement.failed)


if __name__ == "__main__":
    raise SystemExit(main())
