#!/usr/bin/env python3

"""Build and render one compiled FluentQt Gallery sample."""

from __future__ import annotations

import argparse
from pathlib import Path
import platform
import subprocess
import sys
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def default_preset(system: str | None = None, machine: str | None = None) -> str:
    system_name = (system or platform.system()).lower()
    machine_name = (machine or platform.machine()).lower()
    if system_name == "darwin":
        return "vcpkg-osx-x64" if machine_name in {"x86_64", "amd64"} else "vcpkg-osx"
    if system_name == "windows":
        return (
            "vcpkg-windows-arm64"
            if machine_name in {"arm64", "aarch64"}
            else "vcpkg-windows"
        )
    if system_name == "linux":
        return (
            "vcpkg-linux-arm64"
            if machine_name in {"arm64", "aarch64"}
            else "vcpkg-linux"
        )
    host = system or platform.system()
    raise ValueError(f"Unsupported preview host platform: {host}")


def parse_size(value: str) -> str:
    normalized = value.lower()
    pieces = normalized.split("x", 1)
    if len(pieces) != 2:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT")
    try:
        width, height = (int(piece) for piece in pieces)
    except ValueError as error:
        raise argparse.ArgumentTypeError("size must be WIDTHxHEIGHT") from error
    if not 320 <= width <= 3840 or not 240 <= height <= 2160:
        raise argparse.ArgumentTypeError(
            "size must be within 320x240 and 3840x2160"
        )
    return f"{width}x{height}"


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--route", required=True, help="Gallery component route id.")
    parser.add_argument(
        "--sample", help="Gallery sample id; defaults to the first sample."
    )
    parser.add_argument(
        "--theme", choices=("light", "dark", "system"), default="light"
    )
    parser.add_argument("--rtl", action="store_true", help="Use right-to-left layout.")
    parser.add_argument("--size", type=parse_size, default="800x640")
    parser.add_argument("--snapshot", type=Path, help="Write a settled PNG snapshot.")
    parser.add_argument(
        "--report", help="Write the Inspector JSON report; use '-' for stdout."
    )
    parser.add_argument("--settle-ms", type=int, default=250)
    parser.add_argument(
        "--keep-open", action="store_true", help="Keep a one-shot preview open."
    )

    parser.add_argument("--preset", help="CMake build preset.")
    parser.add_argument(
        "--build-dir", type=Path, help="Explicit configured build directory."
    )
    parser.add_argument(
        "--executable", type=Path, help="Explicit fluent_qt_gallery executable."
    )
    parser.add_argument(
        "--no-build", action="store_true", help="Launch the existing executable."
    )
    args = parser.parse_args(list(argv) if argv is not None else None)

    if not 0 <= args.settle_ms <= 10000:
        parser.error("--settle-ms must be between 0 and 10000")
    if args.preset is None and args.build_dir is None:
        args.preset = default_preset()
    return args


def inferred_build_directory(args: argparse.Namespace) -> Path:
    if args.build_dir is not None:
        return args.build_dir.expanduser().resolve()
    assert args.preset
    return PROJECT_ROOT / "build" / args.preset


def build_command(args: argparse.Namespace) -> list[str]:
    if args.build_dir is not None:
        command = [
            "cmake",
            "--build",
            str(args.build_dir.expanduser().resolve()),
        ]
    else:
        command = ["cmake", "--build", "--preset", args.preset]
    command.extend(["--target", "fluent_qt_gallery", "--parallel"])
    return command


def build_gallery(args: argparse.Namespace) -> bool:
    if args.no_build:
        return True
    print("[preview] building fluent_qt_gallery", flush=True)
    completed = subprocess.run(build_command(args), cwd=PROJECT_ROOT, check=False)
    if completed.returncode != 0:
        print(
            "[preview] build failed",
            file=sys.stderr,
            flush=True,
        )
        return False
    return True


def resolve_gallery_executable(build_dir: Path) -> Path:
    app_dir = build_dir / "app"
    direct_candidates = (
        app_dir / "fluent_qt_gallery",
        app_dir
        / "Fluent-Qt Gallery.app"
        / "Contents"
        / "MacOS"
        / "Fluent-Qt Gallery",
        app_dir / "fluent_qt_gallery.exe",
        app_dir / "Debug" / "fluent_qt_gallery.exe",
        app_dir / "Release" / "fluent_qt_gallery.exe",
        app_dir / "RelWithDebInfo" / "fluent_qt_gallery.exe",
    )
    for candidate in direct_candidates:
        if candidate.is_file():
            return candidate.resolve()

    discovered = sorted(
        {
            candidate.resolve()
            for name in ("fluent_qt_gallery", "fluent_qt_gallery.exe")
            for candidate in app_dir.rglob(name)
            if candidate.is_file()
        }
    )
    if len(discovered) == 1:
        return discovered[0]
    if discovered:
        choices = "\n  ".join(str(path) for path in discovered)
        raise RuntimeError(
            "Multiple Gallery executables found; pass --executable:\n  " + choices
        )
    raise FileNotFoundError(
        f"Could not find fluent_qt_gallery under {app_dir}; build it first"
    )


def preview_arguments(args: argparse.Namespace) -> list[str]:
    command = ["--preview"]
    command.extend(["--route", args.route])
    if args.sample:
        command.extend(["--sample", args.sample])
    command.extend(["--theme", args.theme, "--size", args.size])
    command.extend(["--settle-ms", str(args.settle_ms)])
    if args.rtl:
        command.append("--rtl")
    if args.snapshot:
        command.extend(["--snapshot", str(args.snapshot.expanduser().resolve())])
    if args.report:
        report = (
            args.report
            if args.report == "-"
            else str(Path(args.report).expanduser().resolve())
        )
        command.extend(["--report", report])
    if args.keep_open:
        command.append("--keep-open")
    return command


def selected_executable(args: argparse.Namespace) -> Path:
    if args.executable:
        executable = args.executable.expanduser().resolve()
        if not executable.is_file():
            raise FileNotFoundError(f"Preview executable does not exist: {executable}")
        return executable
    return resolve_gallery_executable(inferred_build_directory(args))


def run_once(args: argparse.Namespace) -> int:
    if not build_gallery(args):
        return 1
    executable = selected_executable(args)
    completed = subprocess.run(
        [str(executable), *preview_arguments(args)],
        cwd=PROJECT_ROOT,
        check=False,
    )
    return completed.returncode


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        return run_once(args)
    except (FileNotFoundError, RuntimeError, ValueError) as error:
        print(f"fluent_qt_preview: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
