#!/usr/bin/env python3

"""Run isolated typography checks before the existing Qt-free CI preflight."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]


def run_font_contracts(environment_dir: Path) -> None:
    python = environment_dir / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    environment = dict(os.environ)
    for name in ("PYTHONHOME", "PYTHONPATH"):
        environment.pop(name, None)
    commands = (
        [sys.executable, "-m", "venv", str(environment_dir)],
        [str(python), "-m", "pip", "install", "--disable-pip-version-check",
         "-r", "tools/fonts/requirements.txt"],
        [str(python), "tools/fonts/test_fetch_font_sources.py"],
        [str(python), "tools/fonts/fetch_font_sources.py", "--download"],
        [str(python), "tools/fonts/test_generate_typography_assets.py"],
        [str(python), "tools/fonts/generate_typography_assets.py", "--check"],
        [str(python), "tools/fonts/generate_typography_assets.py", "--check-web-fallback"],
    )
    for command in commands:
        subprocess.run(command, cwd=ROOT, env=environment, check=True)


def main() -> int:
    try:
        run_font_contracts(ROOT / "build/ci-fonts-venv")
        subprocess.run(
            [sys.executable, "tools/dev/fluent_qt_preflight.py", "--checks-only"],
            cwd=ROOT,
            check=True,
        )
    except subprocess.CalledProcessError as error:
        return error.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
