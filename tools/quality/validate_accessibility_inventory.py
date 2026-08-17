#!/usr/bin/env python3

"""Validate the accessibility inventory against the canonical Gallery catalog."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path


CLASSIFICATIONS = {"native", "augmented", "adapter", "gap", "not-applicable"}
RISKS = {"low", "medium", "high"}
STATUSES = {"covered", "open", "not-applicable"}
REQUIRED_FIELDS = {
    "id",
    "classification",
    "risk",
    "status",
    "evidence",
    "gap",
}
OPTIONAL_FIELDS = {"next_gate"}


def load_json(path: Path) -> dict[str, object]:
    with path.open("r", encoding="utf-8") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise AssertionError(f"{path} must contain a JSON object")
    return value


def validate(project_root: Path) -> Counter[str]:
    inventory_path = (
        project_root / "docs/development/accessibility-inventory.json"
    )
    inventory = load_json(inventory_path)
    if inventory.get("schema_version") != 1:
        raise AssertionError("accessibility inventory schema_version must be 1")

    source_value = inventory.get("source_catalog")
    if not isinstance(source_value, str) or not source_value:
        raise AssertionError("accessibility inventory source_catalog is required")
    source_path = project_root / source_value
    catalog = load_json(source_path)

    catalog_components = catalog.get("components")
    if not isinstance(catalog_components, list):
        raise AssertionError(f"{source_path} is missing components")
    catalog_ids = [
        component.get("id")
        for component in catalog_components
        if isinstance(component, dict)
    ]
    if len(catalog_ids) != len(catalog_components) or not all(
        isinstance(component_id, str) and component_id for component_id in catalog_ids
    ):
        raise AssertionError(f"{source_path} contains an invalid component id")

    entries = inventory.get("components")
    if not isinstance(entries, list):
        raise AssertionError("accessibility inventory components must be an array")

    inventory_ids: list[str] = []
    classifications: Counter[str] = Counter()
    for index, entry in enumerate(entries):
        prefix = f"components[{index}]"
        if not isinstance(entry, dict):
            raise AssertionError(f"{prefix} must be an object")
        unknown = set(entry) - REQUIRED_FIELDS - OPTIONAL_FIELDS
        missing = REQUIRED_FIELDS - set(entry)
        if unknown:
            raise AssertionError(
                f"{prefix} has unsupported fields: {', '.join(sorted(unknown))}"
            )
        if missing:
            raise AssertionError(
                f"{prefix} is missing fields: {', '.join(sorted(missing))}"
            )

        component_id = entry["id"]
        if not isinstance(component_id, str) or not component_id:
            raise AssertionError(f"{prefix}.id must be a non-empty string")
        inventory_ids.append(component_id)

        classification = entry["classification"]
        risk = entry["risk"]
        status = entry["status"]
        if classification not in CLASSIFICATIONS:
            raise AssertionError(f"{component_id} has invalid classification")
        if risk not in RISKS:
            raise AssertionError(f"{component_id} has invalid risk")
        if status not in STATUSES:
            raise AssertionError(f"{component_id} has invalid status")
        classifications[str(classification)] += 1

        evidence = entry["evidence"]
        if not isinstance(evidence, list) or not evidence or not all(
            isinstance(path, str) and path for path in evidence
        ):
            raise AssertionError(f"{component_id} must list evidence paths")
        for relative_path in evidence:
            if not (project_root / relative_path).exists():
                raise AssertionError(
                    f"{component_id} evidence does not exist: {relative_path}"
                )

        gap = entry["gap"]
        if not isinstance(gap, str):
            raise AssertionError(f"{component_id}.gap must be a string")
        next_gate = entry.get("next_gate", "")
        if not isinstance(next_gate, str):
            raise AssertionError(f"{component_id}.next_gate must be a string")

        if classification == "gap":
            if status != "open" or not gap or not next_gate:
                raise AssertionError(
                    f"{component_id} gaps require open status, gap, and next_gate"
                )
        elif classification == "not-applicable":
            if status != "not-applicable" or not gap or next_gate:
                raise AssertionError(
                    f"{component_id} not-applicable entries require a rationale only"
                )
        elif status != "covered" or gap or next_gate:
            raise AssertionError(
                f"{component_id} covered entries cannot retain gap or next_gate"
            )

    duplicate_ids = sorted(
        component_id
        for component_id, count in Counter(inventory_ids).items()
        if count > 1
    )
    if duplicate_ids:
        raise AssertionError(
            "duplicate accessibility inventory ids: " + ", ".join(duplicate_ids)
        )

    missing_ids = sorted(set(catalog_ids) - set(inventory_ids))
    extra_ids = sorted(set(inventory_ids) - set(catalog_ids))
    if missing_ids or extra_ids:
        details = []
        if missing_ids:
            details.append("missing: " + ", ".join(missing_ids))
        if extra_ids:
            details.append("unknown: " + ", ".join(extra_ids))
        raise AssertionError("accessibility inventory drift: " + "; ".join(details))

    return classifications


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    args = parser.parse_args()

    try:
        counts = validate(args.project_root.resolve())
    except (AssertionError, OSError, json.JSONDecodeError) as error:
        print(f"Accessibility inventory validation failed: {error}")
        return 1

    total = sum(counts.values())
    summary = ", ".join(
        f"{name}={counts[name]}" for name in sorted(CLASSIFICATIONS)
    )
    print(f"Accessibility inventory passed: {total} components ({summary})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
