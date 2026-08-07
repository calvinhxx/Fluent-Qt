#!/usr/bin/env python3
"""Select the ordered PySide6 full-release matrix for GitHub Actions."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


DEFAULT_CATALOG = (
    Path(__file__).resolve().parents[2]
    / "bindings"
    / "pyside6"
    / "wheel-matrix.json"
)
CRITICAL_RELEASE_ID = "windows-arm64-qt693-cp311"


def load_catalog(path: Path) -> dict[str, Any]:
    try:
        with path.open(encoding="utf-8") as stream:
            catalog = json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read valid JSON: {error}") from error
    if not isinstance(catalog, dict):
        raise ValueError("catalog root must be an object")
    return catalog


def select_release_matrix(catalog: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    """Return release lanes in critical-path-first queue order."""

    scenarios = catalog.get("scenarios")
    manylinux = catalog.get("manylinux")
    if not isinstance(scenarios, list):
        raise ValueError("scenarios must be an array")
    if not isinstance(manylinux, dict):
        raise ValueError("manylinux must be an object")
    auditwheel_version = manylinux.get("auditwheel_version")
    if not isinstance(auditwheel_version, str) or not auditwheel_version:
        raise ValueError("manylinux.auditwheel_version must be a non-empty string")

    selected: list[dict[str, Any]] = []
    for scenario in scenarios:
        if not isinstance(scenario, dict):
            raise ValueError("every scenario must be an object")
        if scenario.get("release") is True and scenario.get("fast") is False:
            selected.append(
                {**scenario, "auditwheel_version": auditwheel_version}
            )

    critical = [
        scenario
        for scenario in selected
        if scenario.get("id") == CRITICAL_RELEASE_ID
    ]
    if len(critical) != 1:
        raise ValueError(
            f"dynamic release matrix must contain {CRITICAL_RELEASE_ID!r} exactly once"
        )
    if critical[0].get("extended_acceptance") is not True:
        raise ValueError(
            f"critical release lane {CRITICAL_RELEASE_ID!r} must run extended acceptance"
        )

    def queue_priority(scenario: dict[str, Any]) -> int:
        if scenario.get("id") == CRITICAL_RELEASE_ID:
            return 0
        if scenario.get("extended_acceptance") is True:
            return 1
        return 2

    selected.sort(key=queue_priority)
    return {"include": selected}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--catalog",
        type=Path,
        default=DEFAULT_CATALOG,
        help="Path to the reviewed PySide6 wheel catalog.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        matrix = select_release_matrix(load_catalog(args.catalog))
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(json.dumps(matrix, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
