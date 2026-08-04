#!/usr/bin/env python3
"""Repair and audit a FluentQt wheel against its PySide6 manylinux floor."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
from email.parser import Parser
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any
import zipfile


POLICY_PATTERN = re.compile(r"^manylinux_(\d+)_(\d+)$")
ARCHITECTURES = {"x86_64", "aarch64"}
RUNTIME_LIBRARY_PATTERNS = (
    "libQt6*.so.6*",
    "libpyside6*.so.*",
    "libshiboken6*.so.*",
)
REQUIRED_RUNTIME_LIBRARY_PATTERNS = (
    "libQt6Core.so.6",
    "libQt6Gui.so.6",
    "libQt6Widgets.so.6",
    "libpyside6*.so.*",
    "libshiboken6*.so.*",
)


def platform_tag(policy: str, architecture: str) -> str:
    if POLICY_PATTERN.fullmatch(policy) is None:
        raise ValueError("policy must use the manylinux_MAJOR_MINOR form")
    if architecture not in ARCHITECTURES:
        raise ValueError(
            "architecture must be one of: {0}".format(
                ", ".join(sorted(ARCHITECTURES))
            )
        )
    return f"{policy}_{architecture}"


def find_native_wheel(wheel_dir: Path, architecture: str) -> Path:
    expected_suffix = f"-linux_{architecture}.whl"
    candidates = sorted(
        path
        for path in wheel_dir.glob("*.whl")
        if path.name.endswith(expected_suffix)
        and "-manylinux_" not in path.name
    )
    if len(candidates) != 1:
        raise RuntimeError(
            "Expected one native {0} wheel in {1}, found {2}".format(
                architecture,
                wheel_dir,
                len(candidates),
            )
        )
    return candidates[0]


def installed_auditwheel_version() -> str:
    output = subprocess.check_output(
        [sys.executable, "-m", "auditwheel", "--version"],
        stderr=subprocess.STDOUT,
        text=True,
    )
    match = re.search(r"\bauditwheel\s+(\d+\.\d+\.\d+)\b", output)
    if match is None:
        raise RuntimeError(f"Could not parse auditwheel version: {output.strip()}")
    return match.group(1)


def repair_command(
    wheel: Path,
    output_dir: Path,
    target_platform: str,
    excludes: list[str],
) -> list[str]:
    command = [
        sys.executable,
        "-m",
        "auditwheel",
        "repair",
        "--plat",
        target_platform,
        "--only-plat",
        "--wheel-dir",
        str(output_dir),
    ]
    for pattern in excludes:
        command.extend(("--exclude", pattern))
    command.append(str(wheel))
    return command


def read_wheel_metadata(wheel: Path) -> tuple[Any, list[str], list[str]]:
    with zipfile.ZipFile(wheel) as archive:
        names = archive.namelist()
        metadata_names = [
            name for name in names if name.endswith(".dist-info/METADATA")
        ]
        wheel_names = [name for name in names if name.endswith(".dist-info/WHEEL")]
        if len(metadata_names) != 1 or len(wheel_names) != 1:
            raise RuntimeError("Wheel must contain one METADATA and one WHEEL file")
        metadata = Parser().parsestr(
            archive.read(metadata_names[0]).decode("utf-8")
        )
        wheel_metadata = Parser().parsestr(
            archive.read(wheel_names[0]).decode("utf-8")
        )
        tags = wheel_metadata.get_all("Tag", [])
        return metadata, tags, names


def validate_archive(
    wheel: Path,
    target_platform: str,
    pyside_version: str,
    shiboken_version: str,
) -> dict[str, Any]:
    if not wheel.name.endswith(f"-{target_platform}.whl"):
        raise RuntimeError(
            f"Repaired wheel does not use the exact {target_platform} tag: "
            f"{wheel.name}"
        )

    metadata, tags, names = read_wheel_metadata(wheel)
    if not tags or any(not tag.endswith(f"-{target_platform}") for tag in tags):
        raise RuntimeError(
            f"WHEEL metadata does not exclusively target {target_platform}: {tags}"
        )

    expected_requirements = {
        f"PySide6-Essentials (=={pyside_version})",
        f"shiboken6 (=={shiboken_version})",
    }
    requirements = set(metadata.get_all("Requires-Dist", []))
    missing_requirements = sorted(expected_requirements - requirements)
    if missing_requirements:
        raise RuntimeError(
            "Wheel metadata does not pin the excluded runtime libraries: "
            + ", ".join(missing_requirements)
        )

    bundled_runtime_libraries = sorted(
        name
        for name in names
        if any(
            fnmatch.fnmatch(Path(name).name, pattern)
            for pattern in RUNTIME_LIBRARY_PATTERNS
        )
    )
    if bundled_runtime_libraries:
        raise RuntimeError(
            "Wheel bundled a second PySide6/Qt runtime: "
            + ", ".join(bundled_runtime_libraries)
        )

    extension_names = [
        name
        for name in names
        if name.startswith("fluentqt/_fluentqt") and name.endswith(".so")
    ]
    if len(extension_names) != 1:
        raise RuntimeError(
            f"Expected one FluentQt extension, found {len(extension_names)}"
        )
    return {
        "metadata_version": metadata["Version"],
        "requirements": sorted(requirements),
        "tags": tags,
        "extension": extension_names[0],
    }


def inspect_extension(
    wheel: Path,
    extension_name: str,
    required_rpaths: list[str],
) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="fluentqt-manylinux-") as temp_dir:
        extension_path = Path(temp_dir) / Path(extension_name).name
        with zipfile.ZipFile(wheel) as archive:
            extension_path.write_bytes(archive.read(extension_name))
        dynamic = subprocess.check_output(
            ["readelf", "-d", str(extension_path)],
            stderr=subprocess.STDOUT,
            text=True,
        )

    needed = re.findall(
        r"\(NEEDED\).*?Shared library: \[([^]]+)\]",
        dynamic,
    )
    path_fields = re.findall(
        r"\((?:RPATH|RUNPATH)\).*?Library (?:rpath|runpath): \[([^]]+)\]",
        dynamic,
    )
    runtime_paths = {
        entry
        for field in path_fields
        for entry in field.split(":")
        if entry
    }
    missing_rpaths = sorted(set(required_rpaths) - runtime_paths)
    if missing_rpaths:
        raise RuntimeError(
            "Extension is missing relocatable runtime paths: "
            + ", ".join(missing_rpaths)
        )

    for pattern in REQUIRED_RUNTIME_LIBRARY_PATTERNS:
        if not any(fnmatch.fnmatch(name, pattern) for name in needed):
            raise RuntimeError(
                f"Extension does not declare required runtime library {pattern}"
            )
    return {
        "needed": sorted(needed),
        "runtime_paths": sorted(runtime_paths),
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wheel-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--policy", required=True)
    parser.add_argument("--architecture", choices=sorted(ARCHITECTURES), required=True)
    parser.add_argument("--auditwheel-version", required=True)
    parser.add_argument("--pyside-version", required=True)
    parser.add_argument("--shiboken-version", required=True)
    parser.add_argument("--required-rpath", action="append", default=[])
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--report", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        target_platform = platform_tag(args.policy, args.architecture)
        actual_auditwheel = installed_auditwheel_version()
        if actual_auditwheel != args.auditwheel_version:
            raise RuntimeError(
                "auditwheel version mismatch: expected {0}, found {1}".format(
                    args.auditwheel_version,
                    actual_auditwheel,
                )
            )
        input_wheel = find_native_wheel(args.wheel_dir, args.architecture)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        existing = list(args.output_dir.glob("*.whl"))
        if existing:
            raise RuntimeError(
                f"Output directory already contains {len(existing)} wheel(s)"
            )

        command = repair_command(
            input_wheel,
            args.output_dir,
            target_platform,
            args.exclude,
        )
        subprocess.check_call(command)
        repaired = sorted(args.output_dir.glob("*.whl"))
        if len(repaired) != 1:
            raise RuntimeError(
                f"auditwheel produced {len(repaired)} repaired wheels"
            )
        repaired_wheel = repaired[0]
        archive_report = validate_archive(
            repaired_wheel,
            target_platform,
            args.pyside_version,
            args.shiboken_version,
        )
        extension_report = inspect_extension(
            repaired_wheel,
            archive_report["extension"],
            args.required_rpath,
        )
        auditwheel_show = subprocess.check_output(
            [sys.executable, "-m", "auditwheel", "show", str(repaired_wheel)],
            stderr=subprocess.STDOUT,
            text=True,
        )

        report = {
            "schema_version": 1,
            "auditwheel_version": actual_auditwheel,
            "input_wheel": input_wheel.name,
            "output_wheel": repaired_wheel.name,
            "output_sha256": sha256_file(repaired_wheel),
            "policy": args.policy,
            "architecture": args.architecture,
            "target_platform": target_platform,
            "excluded_libraries": args.exclude,
            "archive": archive_report,
            "extension": extension_report,
            "auditwheel_show": auditwheel_show,
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(str(repaired_wheel))
        return 0
    except Exception as error:
        print(f"FluentQt manylinux repair failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
