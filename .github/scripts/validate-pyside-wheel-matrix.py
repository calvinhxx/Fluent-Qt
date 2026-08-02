#!/usr/bin/env python3
"""Validate the PySide6 compatibility and first-release wheel matrix."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Any


DEFAULT_CATALOG = (
    Path(__file__).resolve().parents[2]
    / "bindings"
    / "pyside6"
    / "wheel-matrix.json"
)
REQUIRED_FIELDS = {
    "id",
    "name",
    "fast",
    "release",
    "compatibility",
    "os",
    "platform",
    "arch",
    "runner_arch",
    "python_version",
    "python_arch",
    "python_tag",
    "qt_version",
    "pyside_version",
    "shiboken_version",
    "qt_host",
    "qt_arch",
    "cmake_arch",
    "build_dir",
    "artifact_suffix",
    "expected_wheel_suffix",
    "legacy_shiboken",
    "check_backdrop_converter",
    "macos_deployment_target",
    "binary_arch",
    "build_parallel",
    "timeout_minutes",
}
PLATFORM_ARCHES = {
    ("linux", "x64"),
    ("linux", "arm64"),
    ("macos", "x64"),
    ("macos", "arm64"),
    ("windows", "x64"),
    ("windows", "arm64"),
}
FAST_IDS = {
    "linux-x64-qt624-cp310",
    "windows-x64-qt624-cp310",
    "macos-arm64-qt693-cp311",
}
COMPATIBILITY_IDS = {
    "linux-x64-qt624-cp310",
    "windows-x64-qt624-cp310",
}
PLATFORM_POLICY = {
    ("linux", "x64", "6.2.4"): (
        "ubuntu-22.04", "X64", "x64", "linux", "gcc_64", "", "x86_64"
    ),
    ("linux", "x64", "6.9.3"): (
        "ubuntu-22.04", "X64", "x64", "linux", "linux_gcc_64", "", "x86_64"
    ),
    ("linux", "arm64", "6.9.3"): (
        "ubuntu-24.04-arm", "ARM64", "arm64", "linux_arm64",
        "linux_gcc_arm64", "", "aarch64",
    ),
    ("windows", "x64", "6.2.4"): (
        "windows-2022", "X64", "x64", "windows",
        "win64_msvc2019_64", "x64", "AMD64",
    ),
    ("windows", "x64", "6.9.3"): (
        "windows-2022", "X64", "x64", "windows",
        "win64_msvc2022_64", "x64", "AMD64",
    ),
    ("windows", "arm64", "6.9.3"): (
        "windows-11-arm", "ARM64", "arm64", "windows_arm64",
        "win64_msvc2022_arm64", "ARM64", "ARM64",
    ),
    ("macos", "x64", "6.9.3"): (
        "macos-15-intel", "X64", "x64", "mac", "clang_64", "", "x86_64"
    ),
    ("macos", "arm64", "6.9.3"): (
        "macos-15", "ARM64", "arm64", "mac", "clang_64", "", "arm64"
    ),
}
WHEEL_PLATFORM_SUFFIX = {
    ("linux", "x64"): "linux_x86_64",
    ("linux", "arm64"): "linux_aarch64",
    ("windows", "x64"): "win_amd64",
    ("windows", "arm64"): "win_arm64",
    ("macos", "x64"): "macosx_12_0_x86_64",
    ("macos", "arm64"): "macosx_12_0_arm64",
}


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
    build_dirs: set[str] = set()
    artifact_suffixes: set[str] = set()
    fast_ids: set[str] = set()
    compatibility_ids: set[str] = set()
    release_platforms: set[tuple[str, str]] = set()

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

        scenario_id = scenario["id"]
        context = f"scenario {scenario_id!r}"
        if not isinstance(scenario_id, str) or not re.fullmatch(
            r"[a-z0-9]+(?:-[a-z0-9]+)*", scenario_id
        ):
            errors.append(f"{context} has an invalid id")
        elif scenario_id in ids:
            errors.append(f"duplicate scenario id: {scenario_id}")
        else:
            ids.add(scenario_id)

        for field in (
            "fast",
            "release",
            "compatibility",
            "legacy_shiboken",
            "check_backdrop_converter",
        ):
            if not isinstance(scenario[field], bool):
                errors.append(f"{context} field {field!r} must be boolean")

        if scenario["release"] is True and scenario["compatibility"] is True:
            errors.append(f"{context} cannot be both release and compatibility")
        if scenario["release"] is not True and scenario["compatibility"] is not True:
            errors.append(f"{context} must be release or compatibility")

        platform = scenario["platform"]
        arch = scenario["arch"]
        platform_arch = (platform, arch)
        if platform_arch not in PLATFORM_ARCHES:
            errors.append(
                f"{context} targets unsupported platform/architecture {platform_arch}"
            )

        versions = (
            scenario["qt_version"],
            scenario["pyside_version"],
            scenario["shiboken_version"],
        )
        if len(set(versions)) != 1:
            errors.append(
                f"{context} must use matching Qt, PySide6, and Shiboken6 versions"
            )

        python_version = scenario["python_version"]
        python_match = re.fullmatch(r"(\d+)\.(\d+)", python_version)
        expected_python_tag = None
        if python_match:
            expected_python_tag = "cp{0}{1}".format(*python_match.groups())
        if scenario["python_tag"] != expected_python_tag:
            errors.append(
                f"{context} python_tag must match python_version "
                f"({expected_python_tag!r})"
            )

        policy_key = (platform, arch, scenario["qt_version"])
        expected_policy = PLATFORM_POLICY.get(policy_key)
        actual_policy = (
            scenario["os"],
            scenario["runner_arch"],
            scenario["python_arch"],
            scenario["qt_host"],
            scenario["qt_arch"],
            scenario["cmake_arch"],
            scenario["binary_arch"],
        )
        if expected_policy is None:
            errors.append(f"{context} has no supported native toolchain policy")
        elif actual_policy != expected_policy:
            errors.append(
                f"{context} native toolchain must be {expected_policy}, "
                f"found {actual_policy}"
            )

        expected_suffix = None
        platform_suffix = WHEEL_PLATFORM_SUFFIX.get(platform_arch)
        if expected_python_tag and platform_suffix:
            expected_suffix = (
                f"{expected_python_tag}-{expected_python_tag}-{platform_suffix}"
            )
        if scenario["expected_wheel_suffix"] != expected_suffix:
            errors.append(
                f"{context} expected_wheel_suffix must be {expected_suffix!r}"
            )

        legacy_expected = scenario["qt_version"] == "6.2.4"
        if scenario["legacy_shiboken"] is not legacy_expected:
            errors.append(
                f"{context} legacy_shiboken must be {legacy_expected}"
            )
        if scenario["check_backdrop_converter"] is legacy_expected:
            errors.append(
                f"{context} check_backdrop_converter must be {not legacy_expected}"
            )

        deployment_target = scenario["macos_deployment_target"]
        expected_target = "12.0" if platform == "macos" else ""
        if deployment_target != expected_target:
            errors.append(
                f"{context} macos_deployment_target must be {expected_target!r}"
            )

        build_dir = scenario["build_dir"]
        if not isinstance(build_dir, str) or not build_dir.startswith("build/pyside6-"):
            errors.append(f"{context} has an invalid build_dir")
        elif build_dir in build_dirs:
            errors.append(f"duplicate build_dir: {build_dir}")
        else:
            build_dirs.add(build_dir)

        artifact_suffix = scenario["artifact_suffix"]
        if not isinstance(artifact_suffix, str) or not re.fullmatch(
            r"[a-z0-9]+(?:-[a-z0-9]+)*", artifact_suffix
        ):
            errors.append(f"{context} has an invalid artifact_suffix")
        elif artifact_suffix in artifact_suffixes:
            errors.append(f"duplicate artifact_suffix: {artifact_suffix}")
        else:
            artifact_suffixes.add(artifact_suffix)

        if not isinstance(scenario["timeout_minutes"], int) or scenario[
            "timeout_minutes"
        ] <= 0:
            errors.append(f"{context} timeout_minutes must be a positive integer")

        if scenario["fast"] is True:
            fast_ids.add(scenario_id)
        if scenario["compatibility"] is True:
            compatibility_ids.add(scenario_id)
            if versions != ("6.2.4", "6.2.4", "6.2.4"):
                errors.append(f"{context} must preserve the Qt 6.2.4 baseline")
            if python_version != "3.10":
                errors.append(f"{context} must preserve the Python 3.10 baseline")
        if scenario["release"] is True:
            if platform_arch in release_platforms:
                errors.append(
                    f"multiple first-release wheels target {platform} {arch}"
                )
            release_platforms.add(platform_arch)
            if versions != ("6.9.3", "6.9.3", "6.9.3"):
                errors.append(f"{context} must use the Qt 6.9.3 release toolchain")
            if python_version != "3.11":
                errors.append(f"{context} must use the CPython 3.11 release ABI")

    if len(scenarios) != 8:
        errors.append(f"matrix must contain 8 scenarios, found {len(scenarios)}")
    if fast_ids != FAST_IDS:
        errors.append(
            f"fast matrix must contain {sorted(FAST_IDS)}, found {sorted(fast_ids)}"
        )
    if compatibility_ids != COMPATIBILITY_IDS:
        errors.append(
            "compatibility matrix must contain the Linux and Windows x64 "
            f"Qt 6.2.4 baselines, found {sorted(compatibility_ids)}"
        )
    if release_platforms != PLATFORM_ARCHES:
        errors.append(
            "first-release matrix must contain x64 and ARM64 for Linux, macOS, "
            f"and Windows, found {sorted(release_platforms)}"
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

    scenarios = catalog["scenarios"]
    release_count = sum(item["release"] is True for item in scenarios)
    compatibility_count = sum(item["compatibility"] is True for item in scenarios)
    print(
        f"Validated {release_count} release wheels and "
        f"{compatibility_count} compatibility lanes in {path}."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
