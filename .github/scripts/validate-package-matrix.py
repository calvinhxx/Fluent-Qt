#!/usr/bin/env python3
"""Validate the shared desktop release package catalog."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Any


DEFAULT_CATALOG = Path(__file__).resolve().parents[1] / "package-matrix.json"
REQUIRED_FIELDS = {
    "id",
    "name",
    "standard",
    "smoke",
    "os",
    "platform",
    "arch",
    "preset",
    "package_preset",
    "qt_version",
    "qt_label",
    "qt_source",
    "qt_arch",
    "host_qt_arch",
    "disable_qt",
    "linux_qt_dependency",
    "asset_suffix",
    "build_parallel",
    "windows_arm64_cross",
    "needs_nsis",
}
QT_ID_SUFFIX = {
    "Qt5.15": "qt515",
    "Qt6.2": "qt62",
    "Qt6.9.3": "qt693",
}

# Default Qt 6 installers intentionally follow different platform tracks.
CURRENT_QT_POLICY = {
    ("windows", "x64"): ("6.2.4", "Qt6.2", "aqt"),
    ("windows", "arm64"): ("6.9.3", "Qt6.9.3", "aqt"),
    ("macos", "x64"): ("6.9.3", "Qt6.9.3", "aqt"),
    ("macos", "arm64"): ("6.9.3", "Qt6.9.3", "aqt"),
    ("linux", "x64"): ("6.2.4", "Qt6.2", "apt6"),
    ("linux", "arm64"): ("6.2.4", "Qt6.2", "apt6"),
}
SMOKE_PLATFORMS = {("windows", "x64"), ("macos", "x64")}


def load_catalog(path: Path) -> dict[str, Any]:
    try:
        with path.open(encoding="utf-8") as stream:
            catalog = json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read valid JSON: {error}") from error
    if not isinstance(catalog, dict):
        raise ValueError("catalog root must be an object")
    return catalog


def validate_catalog(catalog: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if catalog.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    scenarios = catalog.get("scenarios")
    if not isinstance(scenarios, list) or not scenarios:
        return [*errors, "scenarios must be a non-empty array"]

    ids: set[str] = set()
    defaults: dict[tuple[str, str], dict[str, Any]] = {}
    smoke_platforms: set[tuple[str, str]] = set()
    standard_count = 0
    smoke_count = 0

    for index, scenario in enumerate(scenarios):
        if not isinstance(scenario, dict):
            errors.append(f"scenarios[{index}] must be an object")
            continue

        missing = sorted(REQUIRED_FIELDS - scenario.keys())
        if missing:
            errors.append(
                f"scenarios[{index}] is missing fields: {', '.join(missing)}"
            )
            continue

        package_id = scenario["id"]
        context = f"scenario {package_id!r}"
        if not isinstance(package_id, str) or not re.fullmatch(
            r"[a-z0-9]+(?:-[a-z0-9]+)*", package_id
        ):
            errors.append(f"{context} has an invalid id")
        elif package_id in ids:
            errors.append(f"duplicate package id: {package_id}")
        else:
            ids.add(package_id)

        for field in ("standard", "smoke", "windows_arm64_cross", "needs_nsis"):
            if not isinstance(scenario[field], bool):
                errors.append(f"{context} field {field!r} must be boolean")

        qt_label = scenario["qt_label"]
        id_suffix = QT_ID_SUFFIX.get(qt_label) if isinstance(qt_label, str) else None
        if isinstance(qt_label, str) and id_suffix is None:
            errors.append(f"{context} uses unsupported qt_label {qt_label!r}")
        elif id_suffix and isinstance(package_id, str) and not package_id.endswith(
            f"-{id_suffix}"
        ):
            errors.append(f"{context} id must end with '-{id_suffix}' for {qt_label}")

        standard = scenario["standard"] is True
        smoke = scenario["smoke"] is True
        if smoke and not standard:
            errors.append(f"{context} cannot be smoke=true when standard=false")
        if standard:
            standard_count += 1
        if smoke:
            smoke_count += 1
            if scenario["asset_suffix"] != "":
                errors.append(f"{context} smoke packages must be default artifacts")

        platform = scenario["platform"]
        arch = scenario["arch"]
        if standard and scenario["asset_suffix"] == "":
            if not isinstance(platform, str) or not isinstance(arch, str):
                errors.append(f"{context} platform and arch must be strings")
                continue
            key = (platform, arch)
            if key in defaults:
                errors.append(f"multiple default packages target {platform} {arch}")
                continue
            defaults[key] = scenario
            if smoke:
                smoke_platforms.add(key)

    if standard_count != 9:
        errors.append(f"standard release set must contain 9 packages, found {standard_count}")
    if smoke_count != 2:
        errors.append(f"smoke release set must contain 2 packages, found {smoke_count}")

    actual_platforms = set(defaults)
    expected_platforms = set(CURRENT_QT_POLICY)
    for platform, arch in sorted(expected_platforms - actual_platforms):
        errors.append(f"missing default package for {platform} {arch}")
    for platform, arch in sorted(actual_platforms - expected_platforms):
        errors.append(f"unexpected default package for {platform} {arch}")

    for key, expected in CURRENT_QT_POLICY.items():
        scenario = defaults.get(key)
        if scenario is None:
            continue
        actual = (
            scenario["qt_version"],
            scenario["qt_label"],
            scenario["qt_source"],
        )
        if actual != expected:
            errors.append(
                f"scenario {scenario['id']!r} must use version/label/source "
                f"{expected}, found {actual}"
            )

    if smoke_platforms != SMOKE_PLATFORMS:
        errors.append(
            "smoke release set must contain the default Windows x64 and macOS "
            f"x64 packages, found {sorted(smoke_platforms)}"
        )
    return errors


def main() -> int:
    if len(sys.argv) > 2:
        print(f"usage: {Path(sys.argv[0]).name} [catalog.json]", file=sys.stderr)
        return 2

    path = Path(sys.argv[1]) if len(sys.argv) == 2 else DEFAULT_CATALOG
    try:
        catalog = load_catalog(path)
    except ValueError as error:
        print(f"{path}: {error}", file=sys.stderr)
        return 1

    errors = validate_catalog(catalog)
    if errors:
        for error in errors:
            print(f"{path}: {error}", file=sys.stderr)
        return 1

    print(f"Validated {len(catalog['scenarios'])} package scenarios in {path}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
