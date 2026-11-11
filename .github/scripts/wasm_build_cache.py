#!/usr/bin/env python3
"""Reuse exact-input WASM builds; every consumer and browser check still runs."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import stat
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = 1
MANIFEST = "build-cache.json"
COMMIT = re.compile(r"[0-9a-f]{40}")
# The installed SDK contains docs/ai, llms.txt and the .agents Skill: those
# remain inputs. Other documentation and website files do not build the SDK.
WEB_ONLY_PREFIXES = ("site/", "tools/site/")
WEB_ONLY_FILES = {"AGENTS.md", "README.md", "README.zh-CN.md"}
REQUIRED_SDK = ("include/FluentQt/FluentQt.h", "lib/libFluentQt.a",
                "lib/cmake/FluentQt/FluentQtConfig.cmake")


def encoded(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_input(path: str) -> bool:
    return not (path in WEB_ONLY_FILES or path.startswith(WEB_ONLY_PREFIXES)
                or (path.startswith("docs/") and not path.startswith("docs/ai/")))


def fingerprint(root: Path, toolchain: dict[str, str]) -> dict:
    entries = subprocess.check_output(["git", "ls-files", "--stage", "-z"], cwd=root)
    files = {}
    for entry in entries.split(b"\0"):
        if not entry:
            continue
        attributes, raw_path = entry.split(b"\t", 1)
        mode, _object, stage = attributes.decode().split()
        path = os.fsdecode(raw_path)
        if not build_input(path):
            continue
        source = root / path
        if stage != "0" or mode not in ("100644", "100755") or source.is_symlink():
            raise ValueError(f"Non-regular or unmerged build input: {path}")
        files[path] = {"mode": mode, "sha256": digest(source)}
    if "CMakeLists.txt" not in files or not toolchain or any(not v for v in toolchain.values()):
        raise ValueError("Build fingerprint needs the project and complete toolchain identity")
    inputs = {"schema_version": SCHEMA, "files": files, "toolchain": toolchain}
    return {"digest": hashlib.sha256(encoded(inputs)).hexdigest(), "inputs": inputs}


def load_inputs(path: Path) -> dict:
    data = json.loads(path.read_text())
    if (data["inputs"]["schema_version"] != SCHEMA
            or hashlib.sha256(encoded(data["inputs"])).hexdigest() != data["digest"]):
        raise ValueError("Invalid build-input fingerprint")
    return data


def inventory(root: Path) -> dict:
    if root.is_symlink() or not root.is_dir():
        raise ValueError("Cache must be a real directory")
    files = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Symlink in build cache: {path.relative_to(root)}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError("Non-regular cache entry")
        relative = path.relative_to(root).as_posix()
        if relative == MANIFEST:
            continue
        if not relative.startswith(("payload/", "sdk/")):
            raise ValueError(f"Unexpected cache entry: {relative}")
        files[relative] = {"sha256": digest(path), "size": path.stat().st_size,
                           "mode": stat.S_IMODE(path.stat().st_mode)}
    return files


def staging_module():
    spec = importlib.util.spec_from_file_location("wasm_cache_stage", Path(__file__).with_name("stage-wasm-pages.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def required_files() -> set[str]:
    stage = staging_module()
    files = {f"payload/{name}" for name in stage.PAYLOAD_FILES}
    for directory, names in (("hello-world", stage.HELLO_WORLD_PAYLOAD_FILES),
                             ("hello-world-spatial", stage.HELLO_WORLD_SPATIAL_PAYLOAD_FILES),
                             ("plugin-extension", stage.PLUGIN_EXTENSION_PAYLOAD_FILES)):
        files.update(f"payload/{directory}/{name}" for name in names)
    return files | {f"sdk/{name}" for name in REQUIRED_SDK} | {"payload/build-info.json"}


def verify(cache: Path, inputs: dict) -> dict:
    if cache.is_symlink() or (cache / MANIFEST).is_symlink():
        raise ValueError("Build cache and manifest must not be symlinks")
    manifest = json.loads((cache / MANIFEST).read_text())
    if (manifest.get("schema_version") != SCHEMA or manifest.get("input_digest") != inputs["digest"]
            or not COMMIT.fullmatch(manifest.get("build_commit", ""))):
        raise ValueError("Cache source or input identity does not match this build")
    files = inventory(cache)
    if files != manifest.get("files") or not required_files().issubset(files):
        raise ValueError("Cache file inventory or checksums do not match")
    if any(files[path]["size"] == 0 for path in required_files()):
        raise ValueError("Required cache files must not be empty")
    metadata = json.loads((cache / "payload/build-info.json").read_text())
    if (metadata["commit"] != manifest["build_commit"]
            or metadata.get("build_commit") != manifest["build_commit"]
            or metadata.get("build_inputs_sha256") != inputs["digest"]
            or metadata.get("qt_version") != inputs["inputs"]["toolchain"]["QT_VERSION"]
            or metadata.get("emscripten_version") != inputs["inputs"]["toolchain"]["EMSCRIPTEN_VERSION"]):
        raise ValueError("Cached payload lost its build provenance")
    return manifest


def pack(cache: Path, source: Path, sdk: Path, inputs: dict, commit: str, qt: str, emscripten: str, mode: str) -> None:
    if not COMMIT.fullmatch(commit):
        raise ValueError("A real source commit is required")
    # Cache paths are workflow-owned build output, never an arbitrary source tree.
    if cache.is_symlink() or cache.name != "wasm-reusable":
        raise ValueError("Use the dedicated wasm-reusable cache directory")
    cache.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=cache.parent, prefix="wasm-reusable-") as temporary:
        prepared = Path(temporary) / "entry"
        prepared.mkdir()
        staging_module().stage(argparse.Namespace(
            source=source, destination=prepared / "payload", qt_version=qt,
            emscripten_version=emscripten, validation_mode=mode, commit=commit,
            build_commit=commit, build_inputs_sha256=inputs["digest"],
        ))
        shutil.copytree(sdk, prepared / "sdk", symlinks=True)
        manifest = {"schema_version": SCHEMA, "input_digest": inputs["digest"],
                    "build_commit": commit, "files": inventory(prepared)}
        (prepared / MANIFEST).write_bytes(encoded(manifest))
        verify(prepared, inputs)
        if cache.exists():
            shutil.rmtree(cache)
        prepared.rename(cache)


def toolchain_identity() -> dict[str, str]:
    identity = {name: os.environ[name] for name in (
        "QT_VERSION", "EMSCRIPTEN_VERSION", "EMSDK_REPOSITORY_REVISION", "RUNNER_OS", "RUNNER_ARCH",
        "ImageOS", "ImageVersion")}
    identity["python"] = platform.python_version()
    for tool in ("cmake", "ninja"):
        identity[tool] = subprocess.check_output([tool, "--version"], text=True).splitlines()[0]
    return identity


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("fingerprint", "verify", "pack", "stage"))
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--inputs", type=Path, default=Path("build/wasm-cache-inputs.json"))
    parser.add_argument("--cache", type=Path, default=Path("build/wasm-reusable"))
    parser.add_argument("--source", type=Path, default=Path("build/wasm/app"))
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--commit")
    parser.add_argument("--mode", choices=("fast", "full"), default="full")
    args = parser.parse_args()
    if args.command == "fingerprint":
        inputs = fingerprint(args.root, toolchain_identity())
        args.inputs.parent.mkdir(parents=True, exist_ok=True)
        args.inputs.write_bytes(encoded(inputs))
        print(inputs["digest"])
        return
    inputs = load_inputs(args.inputs)
    if args.command == "pack":
        if args.sdk is None:
            parser.error("pack requires --sdk")
        pack(args.cache, args.source, args.sdk, inputs, args.commit or "",
             os.environ["QT_VERSION"], os.environ["EMSCRIPTEN_VERSION"], args.mode)
    manifest = verify(args.cache, inputs)
    if args.command == "stage":
        if not COMMIT.fullmatch(args.commit or ""):
            parser.error("stage requires the current validation commit")
        staging_module().stage(argparse.Namespace(
            source=args.cache / "payload", destination=Path("build/wasm-pages"),
            qt_version=os.environ["QT_VERSION"], emscripten_version=os.environ["EMSCRIPTEN_VERSION"],
            validation_mode=args.mode, commit=args.commit, build_commit=manifest["build_commit"],
            build_inputs_sha256=inputs["digest"],
        ))
    print(f"Verified WASM build from {manifest['build_commit']} for inputs {inputs['digest']}")


if __name__ == "__main__":
    main()
