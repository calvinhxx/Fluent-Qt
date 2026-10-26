#!/usr/bin/env python3
"""Prepare pinned Qt Wasm source overlays without changing an installed SDK."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
import urllib.request

QT_VERSION = "6.9.3"
QTBASE_COMMIT = "be09b211db70e1b0155d05c18668ad76e7e3df51"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def child_path(root: Path, relative: str) -> Path:
    name = PurePosixPath(relative)
    if name.is_absolute() or ".." in name.parts or "\\" in relative or ":" in relative:
        raise RuntimeError(f"Unsafe manifest path: {relative}")
    path = root / relative
    if not path.resolve().is_relative_to(root.resolve()):
        raise RuntimeError(f"Manifest path escapes its root: {relative}")
    return path


def load_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("qt_version") != QT_VERSION or manifest.get("qtbase_commit") != QTBASE_COMMIT:
        raise RuntimeError("Unsupported Qt source revision; this overlay is reviewed for Qt 6.9.3 / " + QTBASE_COMMIT)
    if manifest.get("emscripten_version") != "3.1.70":
        raise RuntimeError("The reviewed overlay requires Emscripten 3.1.70")
    return manifest


def verify_file(path: Path, expected: str, label: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"Missing {label}: {path}")
    actual = sha256(path)
    if actual != expected:
        raise RuntimeError(f"Hash mismatch for {label}: {path}; expected {expected}, found {actual}")


def verify_sources(manifest: dict, cache: Path) -> None:
    for relative, spec in manifest["files"].items():
        verify_file(child_path(cache, relative), spec["sha256"], "offline Qt source")


def fetch_sources(manifest: dict, cache: Path, *, offline: bool = False) -> None:
    for relative, spec in manifest["files"].items():
        path = child_path(cache, relative)
        if path.exists():
            verify_file(path, spec["sha256"], "cached Qt source")
            continue
        if offline:
            raise RuntimeError(f"Missing offline Qt source: {path}; run the fetch command with network access first")
        url = f"https://raw.githubusercontent.com/qt/qtbase/{manifest['qtbase_commit']}/{relative}"
        request = urllib.request.Request(url, headers={"User-Agent": "FluentQt-Qt-overlay"})
        with urllib.request.urlopen(request, timeout=60) as response:
            content = response.read()
        actual = hashlib.sha256(content).hexdigest()
        if actual != spec["sha256"]:
            raise RuntimeError(f"Downloaded Qt source has an unexpected hash: {url}")
        write_if_changed(path, content)
    verify_sources(manifest, cache)


def verify_sdk(manifest: dict, sdk: Path) -> dict[str, str]:
    observed = {}
    for group in ("sdk_archives", "sdk_headers"):
        for relative, spec in manifest[group].items():
            path = child_path(sdk, relative)
            verify_file(path, spec["sha256"], "supported Qt 6.9.3 SDK file")
            observed[relative] = sha256(path)
    return observed


def require_local_output(output: Path, sdk: Path) -> None:
    if output.resolve().is_relative_to(sdk.resolve()):
        raise RuntimeError("Overlay output must be outside the Qt SDK; the installed SDK is read-only")


def write_if_changed(path: Path, content: bytes) -> None:
    if path.exists() and path.read_bytes() == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(dir=path.parent, prefix=path.name + ".")
    temporary_path = Path(temporary)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
        temporary_path.replace(path)
    finally:
        temporary_path.unlink(missing_ok=True)


def apply_patch_exact(source: str, patch: str) -> str:
    """Apply verified unified hunks without fuzz, external git, or shell execution."""
    original = source.splitlines(keepends=True)
    lines = patch.splitlines(keepends=True)
    output = []
    cursor = 0
    position = 0
    hunks = 0
    while position < len(lines):
        line = lines[position]
        if line.startswith(("--- ", "+++ ")):
            position += 1
            continue
        match = re.match(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", line)
        if not match:
            raise RuntimeError("Unsupported or malformed Qt overlay patch")
        old_count = int(match[2]) if match[2] is not None else 1
        old_start = int(match[1]) - (1 if old_count else 0)
        new_count = int(match[4]) if match[4] is not None else 1
        if old_start < cursor or old_start > len(original):
            raise RuntimeError("Overlapping or out-of-range Qt patch hunk")
        output.extend(original[cursor:old_start])
        cursor = old_start
        consumed = produced = 0
        position += 1
        while position < len(lines) and not lines[position].startswith("@@ "):
            operation = lines[position][:1]
            text = lines[position][1:]
            if operation not in (" ", "+", "-"):
                raise RuntimeError("Unsupported Qt patch line")
            if operation in (" ", "-"):
                if cursor >= len(original) or original[cursor] != text:
                    raise RuntimeError("Qt patch context does not match the verified source")
                cursor += 1
                consumed += 1
            if operation in (" ", "+"):
                output.append(text)
                produced += 1
            position += 1
        if (consumed, produced) != (old_count, new_count):
            raise RuntimeError("Qt patch hunk lengths do not match its header")
        hunks += 1
    if not hunks:
        raise RuntimeError("Qt overlay patch has no hunks")
    output.extend(original[cursor:])
    return "".join(output)


def prepare_sources(manifest_path: Path, cache: Path, sdk: Path, output: Path) -> dict:
    require_local_output(output, sdk)
    manifest = load_manifest(manifest_path)
    verify_sources(manifest, cache)
    before = verify_sdk(manifest, sdk)
    patched = {}
    for relative, spec in manifest["patches"].items():
        patch = child_path(manifest_path.parent, spec["file"])
        verify_file(patch, spec["sha256"], "Qt overlay patch")
        content = apply_patch_exact(child_path(cache, relative).read_text(encoding="utf-8"), patch.read_text(encoding="utf-8"))
        destination = output / Path(relative).name
        write_if_changed(destination, content.encode("utf-8"))
        patched[destination.name] = sha256(destination)
    for relative in manifest["sdk_headers"]:
        destination = output / Path(relative).name
        write_if_changed(destination, child_path(sdk, relative).read_bytes())
        patched[destination.name] = sha256(destination)
    if verify_sdk(manifest, sdk) != before:
        raise RuntimeError("Qt SDK changed while preparing the overlay")
    result = {"qt_version": QT_VERSION, "qtbase_commit": QTBASE_COMMIT,
              "manifest_sha256": sha256(manifest_path), "sdk_sha256": before,
              "patched_sha256": patched, "sdk_unchanged": True}
    write_if_changed(output / "prepared.json", (json.dumps(result, indent=2) + "\n").encode())
    return result


def archive_members(path: Path) -> list[tuple[str, str]]:
    data = path.read_bytes()
    if not data.startswith(b"!<arch>\n"):
        raise RuntimeError(f"Expected a regular LLVM archive: {path}")
    cursor = 8
    names = b""
    result = []
    while cursor < len(data):
        header = data[cursor:cursor + 60]
        if len(header) != 60 or header[58:60] != b"`\n":
            raise RuntimeError(f"Malformed archive member: {path}")
        name = header[:16].decode("ascii").strip()
        size = int(header[48:58])
        content = data[cursor + 60:cursor + 60 + size]
        if len(content) != size:
            raise RuntimeError(f"Truncated archive member: {path}")
        cursor += 60 + size + size % 2
        if name == "//":
            names = content
            continue
        if name in ("/", "/SYM64/"):
            continue
        if name.startswith("#1/"):
            length = int(name[3:])
            name = content[:length].decode("utf-8").rstrip("\0")
            content = content[length:]
        elif name.startswith("/") and name[1:].isdigit():
            start = int(name[1:])
            name = names[start:names.index(b"/\n", start)].decode("utf-8")
        else:
            name = name.rstrip("/")
        result.append((name, hashlib.sha256(content).hexdigest()))
    return result


def verify_replacement(original: Path, candidate: Path, member: str, object_file: Path) -> None:
    before, after = archive_members(original), archive_members(candidate)
    if [name for name, _ in before] != [name for name, _ in after]:
        raise RuntimeError("Overlay changed the Qt archive member inventory/order")
    if sum(name == member for name, _ in before) != 1:
        raise RuntimeError("Expected exactly one Qt archive member to replace")
    changed = [left[0] for left, right in zip(before, after) if left[1] != right[1]]
    if changed != [member] or dict(after)[member] != sha256(object_file):
        raise RuntimeError("Overlay must replace only the reviewed Qt object with the compiled replacement")


def publish_archives(manifest_path: Path, sdk: Path, cache: Path, object_dir: Path, output: Path, ar: Path) -> dict:
    require_local_output(output, sdk)
    manifest = load_manifest(manifest_path)
    verify_sources(manifest, cache)
    before = verify_sdk(manifest, sdk)
    output.mkdir(parents=True, exist_ok=True)
    staged = []
    entries = {}
    try:
        for relative, spec in manifest["sdk_archives"].items():
            original = child_path(sdk, relative)
            objects = list(object_dir.rglob(spec["member"]))
            if len(objects) != 1:
                raise RuntimeError(f"Expected one compiled {spec['member']}, found {len(objects)}")
            object_file = objects[0]
            descriptor, temporary = tempfile.mkstemp(dir=output, prefix=original.name + ".", suffix=".a")
            os.close(descriptor)
            candidate = Path(temporary)
            staged.append((candidate, output / original.name))
            shutil.copyfile(original, candidate)
            subprocess.run([str(ar), "rcsD", str(candidate), str(object_file)], check=True)
            verify_replacement(original, candidate, spec["member"], object_file)
            entries[relative] = {"original_sha256": before[relative], "copy_sha256": sha256(candidate),
                                 "object_sha256": sha256(object_file), "member": spec["member"]}
        if verify_sdk(manifest, sdk) != before:
            raise RuntimeError("Installed Qt SDK changed while copying archives")
        verify_sources(manifest, cache)
        # Publish the pair only after both replacements and the original SDK verify.
        for candidate, destination in staged:
            if destination.is_file() and sha256(destination) == sha256(candidate):
                candidate.unlink()
            else:
                candidate.replace(destination)
        result = {"manifest_sha256": sha256(manifest_path), "archives": entries, "sdk_unchanged": True}
        write_if_changed(output / "overlay.json", (json.dumps(result, indent=2) + "\n").encode())
        return result
    finally:
        for candidate, _ in staged:
            candidate.unlink(missing_ok=True)


def default_manifest() -> Path:
    installed = Path(__file__).resolve().parent / "data/manifest.json"
    if installed.is_file():
        return installed
    return Path(__file__).resolve().parents[2] / "src/compatibility/private/wasm/qt-6.9.3/manifest.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=default_manifest())
    commands = parser.add_subparsers(dest="operation", required=True)
    fetch = commands.add_parser("fetch", help="Explicitly fetch pinned source files, reusing a verified cache")
    fetch.add_argument("--cache", type=Path, required=True)
    fetch.add_argument("--offline", action="store_true")
    prepare = commands.add_parser("prepare", help="Offline verification and exact source patching")
    prepare.add_argument("--cache", type=Path, required=True)
    prepare.add_argument("--sdk", type=Path, required=True)
    prepare.add_argument("--output", type=Path, required=True)
    archives = commands.add_parser("archives", help="Publish project-local archives after member verification")
    archives.add_argument("--sdk", type=Path, required=True)
    archives.add_argument("--cache", type=Path, required=True)
    archives.add_argument("--object-dir", type=Path, required=True)
    archives.add_argument("--output", type=Path, required=True)
    archives.add_argument("--ar", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.operation == "fetch":
            fetch_sources(load_manifest(args.manifest), args.cache, offline=args.offline)
        elif args.operation == "prepare":
            prepare_sources(args.manifest, args.cache, args.sdk, args.output)
        else:
            publish_archives(args.manifest, args.sdk, args.cache, args.object_dir, args.output, args.ar)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Qt Wasm accessibility overlay: {error}\n")
    print(f"Qt Wasm accessibility overlay {args.operation}: verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
