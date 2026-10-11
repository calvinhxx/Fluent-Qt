#!/usr/bin/env python3
"""Open a copy of the settings scene with local selection and edit review."""

import argparse
from pathlib import Path

from fluent_qt_live_preview import main as launch


ROOT = Path(__file__).resolve().parents[2]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/preview/settings-editor")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--python", type=Path)
    parser.add_argument("--theme", choices=("light", "dark"), default="light")
    parser.add_argument("--size", default="1240x760")
    args = parser.parse_args(argv)
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    scene = directory / "settings.preview.py"
    # Keep accepted changes and pending comparisons when reopening the demo.
    if not scene.exists():
        scene.write_bytes((ROOT / "tools/dev/scenes/settings_page.preview.py").read_bytes())
    command = ["--scene", str(scene), "--session-dir", str(directory / "live"),
               "--content-only", "--edit", "--size", args.size, "--theme", args.theme]
    for name in ("build_dir", "python"):
        if getattr(args, name):
            command.extend(["--" + name.replace("_", "-"), str(getattr(args, name).absolute())])
    return launch(command)


if __name__ == "__main__":
    raise SystemExit(main())
