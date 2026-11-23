#!/usr/bin/env python3
"""Build the pre-bundled browser Vercel AI SDK module for Fluent-Qt Studio."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SDK_DIR = ROOT / "tools/site/studio-ai-sdk"
OUTPUT = ROOT / "site/studio/studio-ai-sdk.mjs"
INPUTS = (
    SDK_DIR / "package.json",
    SDK_DIR / "package-lock.json",
    SDK_DIR / "entry.mjs",
    SDK_DIR / "build.mjs",
)


def is_current() -> bool:
    if not OUTPUT.is_file():
        return False
    out_mtime = OUTPUT.stat().st_mtime
    for path in INPUTS:
        if path.is_file() and path.stat().st_mtime > out_mtime:
            return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true", help="Rebuild even if output appears up-to-date")
    args = parser.parse_args()

    if not args.force and is_current():
        return 0

    npm = shutil.which("npm")
    if not npm:
        raise RuntimeError("npm is required to build site/studio/studio-ai-sdk.mjs")

    node_modules = SDK_DIR / "node_modules"
    if not node_modules.is_dir():
        install_cmd = [npm, "ci"] if (SDK_DIR / "package-lock.json").is_file() else [npm, "install"]
        subprocess.run(install_cmd, cwd=SDK_DIR, check=True)

    subprocess.run([npm, "run", "build"], cwd=SDK_DIR, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
