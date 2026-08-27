#!/usr/bin/env python3
"""Capture Python authoring and compiled C++ Gallery preview evidence."""

from __future__ import annotations

import argparse
from html import escape
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[2]
LIVE_LAUNCHER = Path(__file__).with_name("fluent_qt_live_preview.py")
NATIVE_LAUNCHER = Path(__file__).with_name("fluent_qt_preview.py")


def parse_size(value: str) -> str:
    pieces = value.lower().split("x", 1)
    if len(pieces) != 2:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT")
    try:
        width, height = (int(piece) for piece in pieces)
    except ValueError as error:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT") from error
    if not 520 <= width <= 3840 or not 420 <= height <= 2160:
        raise argparse.ArgumentTypeError(
            "size must be within 520x420 and 3840x2160"
        )
    return "{0}x{1}".format(width, height)


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--route", required=True)
    parser.add_argument("--sample")
    parser.add_argument(
        "--scene",
        type=Path,
        help="Edited Live Scene fork; otherwise export the catalog sample.",
    )
    parser.add_argument("--theme", choices=("light", "dark"), default="light")
    parser.add_argument("--size", type=parse_size, default="920x680")
    parser.add_argument("--rtl", action="store_true")
    parser.add_argument("--settle-ms", type=int, default=300)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--pyside-build-dir", type=Path)
    parser.add_argument("--python", dest="python_executable", type=Path)
    parser.add_argument("--native-build-dir", type=Path)
    parser.add_argument("--preset")
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args(list(argv) if argv is not None else None)
    if not 0 <= args.settle_ms <= 10000:
        parser.error("--settle-ms must be between 0 and 10000")
    if args.output_dir is None:
        identity = "{0}--{1}".format(args.route, args.sample or "first")
        direction = "rtl" if args.rtl else "ltr"
        args.output_dir = (
            PROJECT_ROOT
            / "build"
            / "preview"
            / "compare"
            / identity
            / "{0}-{1}".format(args.theme, direction)
        )
    return args


def live_command(args: argparse.Namespace, output: Path) -> list[str]:
    command = [str(sys.executable), str(LIVE_LAUNCHER)]
    if args.scene is not None:
        command.extend(["--scene", str(args.scene.expanduser().resolve())])
    else:
        command.extend(["--route", args.route])
        if args.sample:
            command.extend(["--sample", args.sample])
    command.extend(
        [
            "--theme",
            args.theme,
            "--size",
            args.size,
            "--settle-ms",
            str(args.settle_ms),
            "--no-watch",
            "--snapshot",
            str(output / "live.png"),
            "--report",
            str(output / "live.json"),
        ]
    )
    if args.rtl:
        command.append("--rtl")
    if args.pyside_build_dir is not None:
        command.extend(
            ["--build-dir", str(args.pyside_build_dir.expanduser().resolve())]
        )
    if args.python_executable is not None:
        command.extend(
            ["--python", str(args.python_executable.expanduser().absolute())]
        )
    return command


def native_command(args: argparse.Namespace, output: Path) -> list[str]:
    command = [str(sys.executable), str(NATIVE_LAUNCHER), "--route", args.route]
    if args.sample:
        command.extend(["--sample", args.sample])
    command.extend(
        [
            "--theme",
            args.theme,
            "--size",
            args.size,
            "--settle-ms",
            str(args.settle_ms),
            "--snapshot",
            str(output / "native.png"),
            "--report",
            str(output / "native.json"),
        ]
    )
    if args.rtl:
        command.append("--rtl")
    if args.native_build_dir is not None:
        command.extend(
            ["--build-dir", str(args.native_build_dir.expanduser().resolve())]
        )
    elif args.preset:
        command.extend(["--preset", args.preset])
    if args.executable is not None:
        command.extend(
            ["--executable", str(args.executable.expanduser().resolve())]
        )
    if args.no_build:
        command.append("--no-build")
    return command


def _read_json(path: Path) -> tuple[dict[str, object] | None, str | None]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return None, str(error)
    if not isinstance(value, dict):
        return None, "JSON root is not an object"
    return value, None


def _nested(value: dict[str, object] | None, *keys: str) -> object:
    current: object = value
    for key in keys:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
    return current


def comparison_document(
    args: argparse.Namespace,
    output: Path,
    live_returncode: int,
    native_returncode: int,
) -> dict[str, object]:
    live, live_error = _read_json(output / "live.json")
    native, native_error = _read_json(output / "native.json")
    width, height = (int(part) for part in args.size.split("x", 1))
    direction = "rtl" if args.rtl else "ltr"
    native_sample = _nested(native, "selection", "sample")
    checks = {
        "live_process_ok": live_returncode == 0,
        "native_process_ok": native_returncode == 0,
        "live_report_readable": live_error is None,
        "native_report_readable": native_error is None,
        "theme_matches": (
            _nested(live, "window", "theme")
            == _nested(native, "scene", "theme")
            == args.theme
        ),
        "direction_matches": (
            _nested(live, "window", "layout_direction")
            == _nested(native, "scene", "layout_direction")
            == direction
        ),
        "window_size_matches": (
            _nested(live, "window", "width"),
            _nested(live, "window", "height"),
            _nested(native, "scene", "actual_width"),
            _nested(native, "scene", "actual_height"),
        )
        == (width, height, width, height),
        "snapshots_written": (
            _nested(live, "snapshot", "written") is True
            and _nested(native, "artifacts", "snapshot", "written") is True
        ),
        "live_reload_clean": not _nested(live, "reload", "last_error"),
        "native_selection_matches": (
            _nested(native, "selection", "route") == args.route
            and (args.sample is None or native_sample == args.sample)
        ),
        "native_status_ok": _nested(native, "status") == "ok",
    }
    scene = _nested(live, "scene")
    if scene is None and args.scene is not None:
        scene = str(args.scene.expanduser().resolve())
    status = "ready-for-review" if all(checks.values()) else "review-required"
    return {
        "schema_version": 1,
        "tool": "FluentQt Live/Native Compare",
        "status": status,
        "selection": {
            "route": args.route,
            "sample": native_sample or args.sample,
        },
        "conditions": {
            "theme": args.theme,
            "layout_direction": direction,
            "size": args.size,
            "settle_ms": args.settle_ms,
        },
        "live": {
            "returncode": live_returncode,
            "scene": scene,
            "snapshot": str(output / "live.png"),
            "report": str(output / "live.json"),
            "report_error": live_error,
        },
        "native": {
            "returncode": native_returncode,
            "snapshot": str(output / "native.png"),
            "report": str(output / "native.json"),
            "report_error": native_error,
        },
        "checks": checks,
        "artifacts": {
            "html": str(output / "comparison.html"),
            "json": str(output / "comparison.json"),
        },
    }


def comparison_html(document: dict[str, object]) -> str:
    selection = document["selection"]
    conditions = document["conditions"]
    return """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FluentQt Live/Native Compare</title>
<style>
body {{ font: 15px system-ui; margin: 24px; background: #15171b; color: #f5f5f5; }}
.grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }}
section {{ border: 1px solid #3b3f47; border-radius: 8px; overflow: hidden; }}
h2 {{ font-size: 15px; margin: 0; padding: 12px; }}
img {{ display: block; width: 100%; background: white; }}
@media (max-width: 900px) {{ .grid {{ grid-template-columns: 1fr; }} }}
</style>
</head>
<body>
<h1>Live / Native</h1>
<p>{route}/{sample} · {theme} · {direction} · {size} · {status}</p>
<p>The framing differs by design: review structure and properties, not pixel equality.</p>
<div class="grid">
  <section><h2>Python authoring window</h2><img src="live.png" alt="Python authoring window"></section>
  <section><h2>Compiled C++ SampleCard</h2><img src="native.png" alt="Compiled C++ SampleCard"></section>
</div>
</body></html>
""".format(
        route=escape(str(selection["route"])),
        sample=escape(str(selection.get("sample") or "first")),
        theme=escape(str(conditions["theme"])),
        direction=escape(str(conditions["layout_direction"])),
        size=escape(str(conditions["size"])),
        status=escape(str(document["status"])),
    )


def _write_text(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(".{0}.{1}.tmp".format(path.name, os.getpid()))
    try:
        temporary.write_text(value, encoding="utf-8")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def run(args: argparse.Namespace) -> int:
    output = args.output_dir.expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment.setdefault("QT_QPA_PLATFORM", "offscreen")

    live = subprocess.run(
        live_command(args, output), cwd=PROJECT_ROOT, env=environment, check=False
    )
    native = subprocess.run(
        native_command(args, output), cwd=PROJECT_ROOT, env=environment, check=False
    )
    document = comparison_document(args, output, live.returncode, native.returncode)
    _write_text(
        output / "comparison.json",
        json.dumps(document, indent=2, ensure_ascii=False) + "\n",
    )
    _write_text(output / "comparison.html", comparison_html(document))
    print("[compare] {0}".format(output / "comparison.html"), flush=True)
    return 0 if document["status"] == "ready-for-review" else 2


def main(argv: Iterable[str] | None = None) -> int:
    try:
        return run(parse_args(argv))
    except OSError as error:
        print("fluent_qt_compare: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
