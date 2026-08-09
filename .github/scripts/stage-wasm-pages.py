#!/usr/bin/env python3

"""Copy the validated WebAssembly Gallery into a Pages-ready artifact."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PROJECT_VERSION_PATTERN = re.compile(
    r"project\(FluentQt\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)"
)
PAYLOAD_FILES = (
    "index.html",
    "fluent_qt_gallery.js",
    "fluent_qt_gallery.wasm",
    "qtloader.js",
    "qtlogo.svg",
    "licenses.html",
    "FluentQt-LICENSE.txt",
    "Qt-LICENSE.txt",
    "Emscripten-LICENSE.txt",
    "NotoSansSC-LICENSE.txt",
    "THIRD_PARTY_NOTICES.md",
)
HELLO_WORLD_PAYLOAD_FILES = (
    "index.html",
    "fluentqt_hello_world.js",
    "fluentqt_hello_world.wasm",
    "qtloader.js",
    "qtlogo.svg",
)


def project_version() -> str:
    contents = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = PROJECT_VERSION_PATTERN.search(contents)
    if not match:
        raise RuntimeError("Could not read the FluentQt project version")
    return match.group(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--qt-version", required=True)
    parser.add_argument("--emscripten-version", required=True)
    parser.add_argument("--validation-mode", choices=("fast", "full"), required=True)
    parser.add_argument("--commit", default="local")
    return parser.parse_args()


def stage(args: argparse.Namespace) -> None:
    source = args.source.resolve()
    destination = args.destination.resolve()
    missing = [name for name in PAYLOAD_FILES if not (source / name).is_file()]
    hello_world_source = source / "hello-world"
    missing.extend(
        f"hello-world/{name}"
        for name in HELLO_WORLD_PAYLOAD_FILES
        if not (hello_world_source / name).is_file()
    )
    if missing:
        raise RuntimeError("Missing Pages payload file(s): " + ", ".join(missing))

    index = (source / "index.html").read_text(encoding="utf-8")
    for reference in ("qtloader.js", "fluent_qt_gallery.js", "licenses.html"):
        if reference not in index:
            raise RuntimeError(f"index.html does not reference {reference}")

    destination.mkdir(parents=True, exist_ok=True)
    for name in PAYLOAD_FILES:
        shutil.copy2(source / name, destination / name)
    hello_world_destination = destination / "hello-world"
    hello_world_destination.mkdir(parents=True, exist_ok=True)
    for name in HELLO_WORLD_PAYLOAD_FILES:
        shutil.copy2(hello_world_source / name, hello_world_destination / name)

    metadata = {
        "project": "Fluent-Qt C++ Web Gallery",
        "project_version": project_version(),
        "qt_version": args.qt_version,
        "qt_target": "wasm_singlethread",
        "emscripten_version": args.emscripten_version,
        "validation_mode": args.validation_mode,
        "commit": args.commit,
    }
    (destination / "build-info.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    total_files = len(PAYLOAD_FILES) + len(HELLO_WORLD_PAYLOAD_FILES)
    print(f"Staged {total_files} WebAssembly files in {destination}")


def main() -> int:
    try:
        stage(parse_args())
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
