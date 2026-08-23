#!/usr/bin/env python3

"""Validate the catalog that drives the reusable C++ CI workflow."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / ".github" / "ci-cpp-matrix.json"

EXPECTED_IDS = {
    "fast": {
        "linux-x64-qt62-fast",
        "win-x64-qt62-fast",
        "mac-arm64-qt62-build",
    },
    "full": {
        "linux-x64-qt62-full",
        "linux-x64-qt62-contract-sanitizers",
        "linux-arm64-qt62-full",
        "linux-x64-qt515-full",
        "mac-arm64-qt62-full",
        "mac-x64-qt62-gallery",
        "win-x64-qt62-platform",
        "win-x64-qt515-api",
        "win-arm64-qt693-native",
        "win-arm64-qt693-cross",
    },
}

EXPECTED_FIRST_WINDOW_TRIAL_IDS = {
    "fast": {
        "linux-x64-qt62-fast",
        "win-x64-qt62-fast",
        "mac-arm64-qt62-build",
    },
    "full": {
        "linux-x64-qt62-full",
        "linux-arm64-qt62-full",
        "linux-x64-qt515-full",
        "mac-arm64-qt62-full",
        "win-x64-qt62-platform",
    },
}

REQUIRED_FIELDS = {
    "mode",
    "id",
    "name",
    "os",
    "preset",
    "qt_version",
    "qt_source",
    "qt_arch",
    "qt_archives",
    "host_qt_arch",
    "vcpkg_triplet",
    "build",
    "test",
    "build_targets",
    "build_parallel",
    "test_labels",
    "exclude_labels",
    "ctest_timeout",
    "timeout_minutes",
    "configure_args",
    "first_window_trial",
    "windows_arm64_cross",
}


def validate_catalog(data: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(data, dict):
        return ["catalog root must be an object"]
    if data.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    scenarios = data.get("scenarios")
    if not isinstance(scenarios, list):
        errors.append("scenarios must be an array")
        return errors

    ids_by_mode = {mode: set() for mode in EXPECTED_IDS}
    trial_ids_by_mode = {mode: set() for mode in EXPECTED_IDS}
    all_ids: set[str] = set()
    for index, scenario in enumerate(scenarios):
        context = f"scenarios[{index}]"
        if not isinstance(scenario, dict):
            errors.append(f"{context} must be an object")
            continue

        missing = sorted(REQUIRED_FIELDS - scenario.keys())
        if missing:
            errors.append(f"{context} is missing fields: {', '.join(missing)}")

        scenario_id = scenario.get("id")
        mode = scenario.get("mode")
        if not isinstance(scenario_id, str) or not scenario_id:
            errors.append(f"{context}.id must be a non-empty string")
        elif scenario_id in all_ids:
            errors.append(f"duplicate scenario id: {scenario_id}")
        else:
            all_ids.add(scenario_id)
            if mode in ids_by_mode:
                ids_by_mode[mode].add(scenario_id)

        if mode not in EXPECTED_IDS:
            errors.append(f"{context}.mode must be fast or full")
        if scenario.get("qt_source") not in {"apt", "aqt"}:
            errors.append(f"{context}.qt_source must be apt or aqt")
        for field in (
            "build",
            "test",
            "first_window_trial",
            "windows_arm64_cross",
        ):
            if not isinstance(scenario.get(field), bool):
                errors.append(f"{context}.{field} must be a boolean")
        if (
            mode in trial_ids_by_mode
            and isinstance(scenario_id, str)
            and scenario.get("first_window_trial") is True
        ):
            trial_ids_by_mode[mode].add(scenario_id)
        for field in ("ctest_timeout", "timeout_minutes"):
            value = scenario.get(field)
            if not isinstance(value, int) or value <= 0:
                errors.append(f"{context}.{field} must be a positive integer")

    for mode, expected_ids in EXPECTED_IDS.items():
        actual_ids = ids_by_mode[mode]
        missing = sorted(expected_ids - actual_ids)
        unexpected = sorted(actual_ids - expected_ids)
        if missing:
            errors.append(f"{mode} matrix is missing: {', '.join(missing)}")
        if unexpected:
            errors.append(f"{mode} matrix has unexpected ids: {', '.join(unexpected)}")

        actual_trial_ids = trial_ids_by_mode[mode]
        expected_trial_ids = EXPECTED_FIRST_WINDOW_TRIAL_IDS[mode]
        missing_trials = sorted(expected_trial_ids - actual_trial_ids)
        unexpected_trials = sorted(actual_trial_ids - expected_trial_ids)
        if missing_trials:
            errors.append(
                f"{mode} first-window trials are missing: {', '.join(missing_trials)}"
            )
        if unexpected_trials:
            errors.append(
                f"{mode} first-window trials have unexpected ids: "
                f"{', '.join(unexpected_trials)}"
            )

    return errors


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CATALOG
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"error: unable to read {path}: {error}", file=sys.stderr)
        return 1

    errors = validate_catalog(data)
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print(
        f"Validated {len(EXPECTED_IDS['fast'])} fast and "
        f"{len(EXPECTED_IDS['full'])} full C++ CI scenarios in {path}."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
