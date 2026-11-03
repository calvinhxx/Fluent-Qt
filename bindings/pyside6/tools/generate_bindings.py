#!/usr/bin/env python3
"""Run Shiboken transactionally and publish only changed generated files."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def stable_includes(content: bytes) -> bytes:
    """Canonicalize Shiboken's unordered extra-include block, not source includes."""
    pattern = rb"(?m)(^// Extra includes\r?\n)((?:#include [^\r\n]+\r?\n)+)"
    return re.sub(pattern, lambda match: match[1] + b"".join(sorted(match[2].splitlines(keepends=True))), content)


def stable_header(content: bytes) -> bytes:
    lines = stable_includes(content).splitlines(keepends=True)
    # Shiboken 6.2 also swaps a protected-access trampoline with the adjacent
    # matching override. Put that nonvirtual inline helper first. Virtual-method
    # order, data members, signatures and bodies remain untouched.
    for index in range(len(lines) - 1):
        method = re.match(rb"^\s+[^()\r\n{}]+?\b([A-Za-z_]\w*)\([^{}\r\n]*\)[^{};\r\n]*\boverride;\s*$", lines[index])
        helper = lines[index + 1]
        if (method and helper.lstrip().startswith(b"inline ") and not re.search(rb"\bvirtual\b", helper)
                and re.search(rb"\b" + re.escape(method[1]) + rb"_protected\s*\(", helper)):
            lines[index], lines[index + 1] = helper, lines[index]
    return b"".join(lines)


def publish(staging: Path, output: Path) -> tuple[int, int]:
    manifest = output / "generated-files.json"
    previous = json.loads(manifest.read_text(encoding="utf-8")) if manifest.exists() else []
    files = sorted(path.relative_to(staging).as_posix() for path in staging.rglob("*") if path.is_file())
    if not files:
        raise RuntimeError("Shiboken succeeded without generating files")
    # Shiboken 6.2's extra includes can vary even with QT_HASH_SEED fixed.
    # Stabilize only this generated block; preserve everything else.
    for name in files:
        if Path(name).suffix == ".h":
            path = staging / name
            content = path.read_bytes()
            normalized = stable_header(content)
            if normalized != content:
                path.write_bytes(normalized)
    # A successful generator may drop types. Only remove files previously owned
    # by this publisher; stamps and other build products are not its outputs.
    for name in set(previous) | set(files):
        candidate = output / name
        if not candidate.resolve().is_relative_to(output.resolve()) or candidate.is_symlink():
            raise ValueError(f"Unsafe generated path: {name}")
    changed = 0
    for name in files:
        original, destination = staging / name, output / name
        if destination.exists() and destination.read_bytes() == original.read_bytes():
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_name(destination.name + ".new")
        shutil.copyfile(original, temporary)
        temporary.replace(destination)
        changed += 1
    removed = set(previous) - set(files)
    for name in removed:
        (output / name).unlink(missing_ok=True)
    content = json.dumps(files, indent=2) + "\n"
    if not manifest.exists() or manifest.read_text(encoding="utf-8") != content:
        manifest.write_text(content, encoding="utf-8")
    # Ninja must see a declared output change even if only generated headers
    # changed. Their dynamic names are not known at CMake configure time.
    headers = [(name, hashlib.sha256((staging / name).read_bytes()).hexdigest())
               for name in files if Path(name).suffix in (".h", ".hpp", ".hxx")]
    header_digest = hashlib.sha256(json.dumps(headers).encode()).hexdigest() + "\n"
    header_stamp = output / "headers.sha256"
    if not header_stamp.exists() or header_stamp.read_text() != header_digest:
        header_stamp.write_text(header_digest, encoding="ascii")
    return changed, len(removed)


def generate(output: Path, command: list[str]) -> int:
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if any(arg.startswith("--output-directory") for arg in command):
        raise ValueError("The wrapper owns Shiboken's output directory")
    with tempfile.TemporaryDirectory(prefix="shiboken-", dir=output.parent) as temporary:
        staging = Path(temporary)
        # Options precede Shiboken's positional binding header and typesystem.
        # Older Shiboken uses QHash iteration for extra includes. Determinism
        # here prevents random header rewrites; this is not application policy.
        result = subprocess.run([command[0], "--output-directory=" + str(staging), *command[1:]],
                                env=dict(os.environ, QT_HASH_SEED="0"))
        if result.returncode:
            return result.returncode
        output.mkdir(parents=True, exist_ok=True)
        changed, removed = publish(staging, output)
        print(f"Binding outputs: {changed} changed, {removed} removed; unchanged files retain timestamps.", flush=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("a Shiboken command is required after --")
    return generate(args.output_directory, command)


if __name__ == "__main__":
    raise SystemExit(main())
