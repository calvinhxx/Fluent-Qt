#!/usr/bin/env python3
"""Reuse a Linux-local source snapshot and configured builds from a Windows checkout."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
import time
import uuid

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(stream.read()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def linux_environment(inherited: dict, config: dict, profile: dict) -> dict:
    env = dict(inherited)
    # WSL appends Windows PATH entries by default. CMake probes every one of
    # those mounted directories when an optional package/tool is absent, even
    # when all source and build files already live on ext4. Keep this local to
    # the worker's children; explicit profile overrides still take precedence.
    if "PATH" in env:
        env["PATH"] = ":".join(part for part in env["PATH"].split(":")
                               if not re.match(r"^/mnt/[a-z](?:/|$)", part, re.IGNORECASE))
    env.update(config.get("environment", {}))
    env.update(profile.get("environment", {}))
    return env


def relative_parts(relative: str) -> tuple[str, ...]:
    parts = PurePosixPath(relative).parts
    if not parts or PurePosixPath(relative).is_absolute() or any(p in ("..", ".git") for p in parts) or "\\" in relative:
        raise ValueError(f"Unsafe snapshot path: {relative!r}")
    return parts


def owned_path(root: Path, relative: str) -> Path:
    path = root.joinpath(*relative_parts(relative))
    if not path.resolve().is_relative_to(root.resolve()) or path.is_symlink():
        raise ValueError(f"Snapshot path escapes its root or is a symlink: {relative}")
    return path


def inventory(source: Path) -> dict[str, dict]:
    result = subprocess.check_output(
        ["git", "-C", str(source), "ls-files", "--cached", "--others", "--exclude-standard", "-z"]
    )
    staged = subprocess.check_output(["git", "-C", str(source), "ls-files", "--stage", "-z"])
    modes = {}
    for item in staged.split(b"\0"):
        if item:
            info, name = item.split(b"\t", 1)
            modes[os.fsdecode(name)] = info.split()[0] == b"100755"
    files = {}
    for name in sorted(set(os.fsdecode(item) for item in result.split(b"\0") if item)):
        path = owned_path(source, name)
        if not path.exists():  # A tracked deletion in the working tree.
            continue
        if not path.is_file():
            raise ValueError(f"Snapshot supports regular files only: {name}")
        # The Git mode is portable across Windows and WSL mounts. Newly added,
        # untracked sources are data until Git records an executable bit.
        files[name] = {"sha256": digest(path), "executable": modes.get(name, False)}
    if "CMakeLists.txt" not in files or "tools/dev/fluent_qt_build.py" not in files:
        raise ValueError("Source is not a complete FluentQt working tree")
    return files


def sync_source(source: Path, target: Path, state: Path, *, adopt: bool = False, files: dict | None = None) -> dict:
    source, target = source.resolve(), target.resolve()
    if source == target or source in target.parents or target in source.parents:
        raise ValueError("Source checkout and snapshot must not overlap")
    if (target / ".git").exists():
        raise ValueError("Refusing to synchronize over a Git checkout")
    current = inventory(source) if files is None else files
    if not isinstance(current, dict) or not {"CMakeLists.txt", "tools/dev/fluent_qt_build.py"} <= current.keys():
        raise ValueError("Incomplete source inventory")
    for name, entry in current.items():
        relative_parts(name)
        if not isinstance(entry, dict) or not re.fullmatch(r"[0-9a-f]{64}", entry.get("sha256", "")) or not isinstance(entry.get("executable"), bool):
            raise ValueError(f"Invalid source inventory entry: {name}")
    if state.exists():
        previous = json.loads(state.read_text(encoding="utf-8"))
        if previous["source"] != str(source) or previous["target"] != str(target):
            raise ValueError("Snapshot belongs to another source or destination")
        old = previous["files"]
    else:
        old = {}
        if target.exists():
            existing = [p for p in target.rglob("*") if p.is_file() or p.is_symlink()]
            if existing and not adopt:
                raise ValueError("Existing snapshot needs --adopt-source once; only identical files can be adopted")
            for path in existing:
                name = path.relative_to(target).as_posix()
                checked = owned_path(target, name)
                if name not in current or not checked.is_file() or digest(checked) != current[name]["sha256"]:
                    raise ValueError(f"Cannot adopt a changed or unknown snapshot file: {name}")
                old[name] = current[name]

    # A snapshot must not acquire inputs missing from its manifest. CMake globs
    # and resource loaders can consume arbitrary files, so keep notes/artifacts
    # outside source rather than guessing which unknown extensions are harmless.
    for path in target.rglob("*"):
        if not path.is_file() and not path.is_symlink():
            continue
        name = path.relative_to(target).as_posix()
        if name in old:
            continue
        owned_path(target, name)
        if path.parent.name == "__pycache__" and path.suffix == ".pyc":
            continue  # Python may create bytecode while running snapshot tools.
        raise ValueError(f"Unmanaged snapshot file: {path}; keep notes and artifacts outside the source snapshot")

    # Check all old owned files before changing anything. Keep cache-side edits
    # visible instead of silently overwriting an agent's work in the snapshot.
    for name, entry in old.items():
        path = owned_path(target, name)
        if path.exists() and digest(path) != entry["sha256"]:
            raise ValueError(f"Snapshot was edited directly: {path}; reconcile it before syncing")
    copied, removed = [], []
    pending, modes = [], []
    for name, entry in current.items():
        path = owned_path(target, name)
        if path.exists() and name not in old:
            raise ValueError(f"Unmanaged file would be overwritten: {path}")
        if not path.exists() or old.get(name, {}).get("sha256") != entry["sha256"]:
            pending.append(name)
        elif old[name].get("executable") != entry["executable"]:
            modes.append(name)
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="source-sync-", dir=target.parent) as temporary:
        staging = Path(temporary)
        for name in pending:
            payload = owned_path(source, name).read_bytes()
            if hashlib.sha256(payload).hexdigest() != current[name]["sha256"]:
                raise ValueError(f"Source changed during sync: {name}; retry after edits settle")
            staged = staging / name
            staged.parent.mkdir(parents=True, exist_ok=True)
            staged.write_bytes(payload)
            staged.chmod(0o755 if current[name]["executable"] else 0o644)
        for name in pending:
            destination = owned_path(target, name)
            destination.parent.mkdir(parents=True, exist_ok=True)
            (staging / name).replace(destination)
            copied.append(name)
    for name in modes:
        owned_path(target, name).chmod(0o755 if current[name]["executable"] else 0o644)
    for name in old.keys() - current.keys():
        path = owned_path(target, name)
        if path.exists():
            path.unlink()  # Only an unchanged, previously managed file.
        removed.append(name)
    write_json(state, {"source": str(source), "target": str(target), "files": current})
    return {"copied": copied, "removed": sorted(removed), "mode_changes": modes, "files": current}


@contextmanager
def workspace_lock(root: Path):
    import fcntl

    root.mkdir(parents=True, exist_ok=True)
    with (root / "validation.lock").open("a") as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another validation owns this workspace; wait for it to finish") from error
        yield


def cache_source(build: Path) -> str | None:
    cache = build / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
                return line.partition("=")[2]
    return None


def cache_matches(build: Path, definitions: list[str]) -> bool:
    cache = build / "CMakeCache.txt"
    if not cache.exists():
        return False
    values = {}
    for line in cache.read_text(encoding="utf-8").splitlines():
        if ":" in line and "=" in line and not line.startswith(("//", "#")):
            key, _, value = line.partition("=")
            values[key.split(":", 1)[0]] = value
    expected = {"FLUENT_QT_DEBUG_RELEASE_RUNTIME": "ON", "CMAKE_GENERATOR": "Ninja"}
    for item in definitions:
        key, separator, value = item[2:].partition("=")
        if not separator:
            raise ValueError("CMake definitions require =value")
        expected[key.split(":", 1)[0]] = value
    kit_keys = {"QT_DIR", "Qt5_DIR", "Qt6_DIR", "CMAKE_PREFIX_PATH", "CMAKE_TOOLCHAIN_FILE",
                "CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "VCPKG_TARGET_TRIPLET", "Python_EXECUTABLE"}
    for key in kit_keys & expected.keys() & values.keys():
        if values[key] != expected[key]:
            raise ValueError(f"{key} changed; choose a fresh profile/build_dir instead of mixing toolchains")
    return all(values.get(key) == value for key, value in expected.items())


def run_linux(args, config: dict) -> int:
    if sys.platform != "linux":
        raise RuntimeError("This worker must run inside Linux/WSL")
    workspace = Path(config["workspace"]).expanduser().resolve()
    if str(workspace).startswith("/mnt/"):
        raise ValueError("Use a Linux-local workspace, not a Windows/shared mount")
    profile = config["profiles"][args.profile]
    source = workspace / "source"
    build = workspace / "build" / args.profile if not profile.get("build_dir") else Path(profile["build_dir"]).expanduser().resolve()
    if str(build).startswith("/mnt/") or build == source or source in build.parents:
        raise ValueError("Build directory must be Linux-local and outside the source snapshot")
    env = linux_environment(os.environ, config, profile)
    cmake = config.get("cmake", "cmake")
    ctest = config.get("ctest", "ctest")
    if Path(cmake).is_absolute():
        env["PATH"] = str(Path(cmake).parent) + os.pathsep + env.get("PATH", "")
    python = profile.get("python", sys.executable)
    configure_args = profile.get("cmake_args", [])
    if any(not item.startswith("-D") for item in configure_args):
        raise ValueError("Profile cmake_args must be -D definitions; source/build/generator are owned by the runner")
    command = [cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
               "-DFLUENT_QT_DEBUG_RELEASE_RUNTIME=ON", *configure_args]
    signature = {"command": command, "environment": {**config.get("environment", {}),
                 **profile.get("environment", {}), "PATH": env.get("PATH", "")}}
    stamp = workspace / "profiles" / (args.profile + ".json")
    run_id = args.run_id or (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + "-" + args.profile + "-" + uuid.uuid4().hex[:8])
    report = {"profile": args.profile, "run_id": run_id, "started_utc": datetime.now(timezone.utc).isoformat(), "steps": []}
    started = time.monotonic()
    with workspace_lock(workspace):
        directory = workspace / "logs" / run_id
        directory.mkdir(parents=True, exist_ok=False)
        env.setdefault("FLUENTQT_QMLBRIDGE_NATIVE_EVIDENCE_DIR", str(directory / "qmlbridge-native"))

        def execute(name, argv):
            print(f"[{name}] {' '.join(argv)}", flush=True)
            before = time.monotonic()
            with (directory / (name + ".log")).open("w", encoding="utf-8") as log:
                result = subprocess.run(argv, cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT)
            report["steps"].append({"name": name, "command": argv, "seconds": round(time.monotonic() - before, 3), "exit_code": result.returncode})
            print(f"[{name}] {report['steps'][-1]['seconds']}s, exit {result.returncode}; {directory / (name + '.log')}", flush=True)
            if result.returncode:
                raise subprocess.CalledProcessError(result.returncode, argv)

        try:
            before = time.monotonic()
            supplied = json.loads(args.inventory.read_text(encoding="utf-8")) if args.inventory else None
            synced = sync_source(args.project_root, source, workspace / "source-index.json", adopt=args.adopt_source, files=supplied)
            report["sync"] = {"seconds": round(time.monotonic() - before, 3), "copied": synced["copied"], "removed": synced["removed"],
                              "files": len(synced["files"]), "digest": hashlib.sha256(json.dumps(synced["files"], sort_keys=True).encode()).hexdigest()}
            print(f"[sync] {report['sync']['seconds']}s, {len(synced['copied'])} copied, {len(synced['removed'])} removed", flush=True)
            if not args.sync_only:
                existing = cache_source(build)
                if existing and Path(existing).resolve() != source:
                    raise ValueError(f"Build cache belongs to another source: {existing}")
                previous = json.loads(stamp.read_text()) if stamp.exists() else None
                # Validate the kit even when another condition forces configure.
                matches = cache_matches(build, configure_args)
                if args.configure or not existing or previous != signature or not matches:
                    execute("configure", command)
                    write_json(stamp, signature)
                else:
                    print("[configure] Reusing the existing configuration; Ninja handles changed CMake inputs", flush=True)
                if not args.configure_only:
                    build_command = [python, str(source / "tools/dev/fluent_qt_build.py"), str(build)]
                    if args.jobs:
                        build_command += ["--jobs", str(args.jobs)]
                    if args.target:
                        build_command += ["--target", *args.target]
                    execute("build", build_command)
                    if args.test_label or args.test_regex:
                        test_command = [ctest, "--test-dir", str(build), "--no-tests=error", "--output-on-failure",
                                        "--output-junit", str(directory / "test.xml"), "-j", "1"]
                        if args.test_label:
                            test_command += ["-L", args.test_label]
                        if args.test_regex:
                            test_command += ["-R", args.test_regex]
                        if not args.desktop:
                            test_command += ["-LE", "^(manual_visual|local_desktop|native_window|performance)$"]
                        execute("test", test_command)
            report["source_unchanged"] = None if args.inventory else inventory(args.project_root) == synced["files"]
            if report["source_unchanged"] is False:
                raise RuntimeError("Source checkout changed during validation; results describe the snapshot only")
            report["status"] = "passed-snapshot" if args.inventory else "passed"
            report["source_verification"] = "Windows launcher report" if args.inventory else "worker"
            return 0
        except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
            report.update(status="failed", error=str(error))
            print(str(error), file=sys.stderr)
            return 1
        finally:
            report["seconds"] = round(time.monotonic() - started, 3)
            write_json(directory / "run.json", report)
            write_json(workspace / "last-run.json", report)
            print(f"Report: {directory / 'run.json'}", flush=True)


def run_windows(args, config, original):
    started = time.monotonic()
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + "-" + args.profile + "-" + uuid.uuid4().hex[:8]
    directory = args.project_root / "build/wsl-validation" / run_id
    directory.mkdir(parents=True)
    before = inventory(args.project_root)
    manifest = directory / "input.json"
    write_json(manifest, before)
    print(f"[inventory] {round(time.monotonic() - started, 3)}s, {len(before)} source files", flush=True)
    # Translate paths in the worker: only one Windows->WSL launch, without a shell.
    bootstrap = (
        "import runpy,subprocess,sys; "
        "paths=[subprocess.check_output(['wslpath','-a',p],text=True).strip() for p in sys.argv[1:5]]; "
        "sys.argv=[paths[0],*sys.argv[6:],'--config',paths[1],'--project-root',paths[2],'--inventory',paths[3],'--run-id',sys.argv[5]]; "
        "runpy.run_path(paths[0],run_name='__main__')"
    )
    result = subprocess.call(["wsl", "--distribution", config["distribution"], "--exec", "python3", "-c", bootstrap,
                              str(Path(__file__).resolve()), str(args.config.resolve()), str(args.project_root.resolve()), str(manifest.resolve()), run_id, *original])
    try:
        unchanged = inventory(args.project_root) == before
        source_error = None
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        unchanged, source_error = False, str(error)
    report = {"profile": args.profile, "run_id": run_id, "source_digest": hashlib.sha256(json.dumps(before, sort_keys=True).encode()).hexdigest(),
              "source_unchanged": unchanged, "worker_exit_code": result, "worker_report": config["workspace"] + "/logs/" + run_id + "/run.json",
              "seconds": round(time.monotonic() - started, 3), "status": "passed" if result == 0 and unchanged else "failed"}
    if source_error:
        report["source_error"] = source_error
    write_json(directory / "run.json", report)
    write_json(args.project_root / "build/wsl-validation/last-run.json", report)
    print(f"Launcher report: {directory / 'run.json'}", flush=True)
    if not unchanged:
        print("Source checkout changed during validation; results describe the snapshot only", file=sys.stderr)
    return result or (0 if unchanged else 1)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=PROJECT_ROOT / ".fluentqt-wsl.json")
    parser.add_argument("--project-root", type=Path, default=PROJECT_ROOT)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--target", nargs="+")
    parser.add_argument("--jobs", type=int)
    parser.add_argument("--test-label")
    parser.add_argument("--test-regex")
    parser.add_argument("--desktop", action="store_true", help="Allow explicitly selected desktop tests; no visual approval is implied")
    parser.add_argument("--configure", action="store_true")
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--sync-only", action="store_true")
    parser.add_argument("--adopt-source", action="store_true", help="Adopt an existing identical snapshot once")
    parser.add_argument("--inventory", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--run-id", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        config = json.loads(args.config.read_text(encoding="utf-8"))
        if config.get("version") != 1:
            raise ValueError("Unsupported workspace configuration version")
        if not re.fullmatch(r"[A-Za-z0-9_-]+", args.profile):
            raise ValueError("Profile names use letters, digits, underscores and hyphens")
        if args.run_id and not re.fullmatch(r"[A-Za-z0-9_-]+", args.run_id):
            raise ValueError("Invalid run identifier")
        if args.jobs is not None and args.jobs < 1:
            raise ValueError("--jobs must be positive")
        if not (args.target or args.sync_only or args.configure_only):
            raise ValueError("Select --target explicitly (or --sync-only / --configure-only)")
        if os.name == "nt":
            original = list(sys.argv[1:] if argv is None else argv)
            return run_windows(args, config, original)
        return run_linux(args, config)
    except (KeyError, OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"WSL validation: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
