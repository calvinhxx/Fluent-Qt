#!/usr/bin/env python3

"""Run deterministic project-shape, retrieval, and composition AI catalog evals."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Iterable


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from query_ai_catalog import pattern_by_id, search_components


INTERFACE_TYPES = {
    "library",
    "cli",
    "tui",
    "service",
    "plugin",
    "gui",
    "none",
}


def evaluate_catalog(
    catalog: dict[str, object], scenarios: dict[str, object]
) -> dict[str, object]:
    if scenarios.get("schema_version") != 1:
        raise ValueError("AI eval scenarios schema_version must be 1")
    failures: list[str] = []

    project_shapes = scenarios.get("project_shapes")
    retrieval_cases = scenarios.get("retrieval_cases")
    composition_cases = scenarios.get("composition_cases")
    if not isinstance(project_shapes, list) or not project_shapes:
        raise ValueError("AI eval scenarios must define project_shapes")
    if not isinstance(retrieval_cases, list) or not retrieval_cases:
        raise ValueError("AI eval scenarios must define retrieval_cases")
    if not isinstance(composition_cases, list) or not composition_cases:
        raise ValueError("AI eval scenarios must define composition_cases")

    seen_ids: set[str] = set()
    catalog_component_ids = {
        component["id"] for component in catalog["components"]
    }
    for scenario in project_shapes:
        scenario_id = scenario.get("id")
        if not scenario_id or scenario_id in seen_ids:
            failures.append(f"invalid or duplicate project-shape id: {scenario_id}")
            continue
        seen_ids.add(scenario_id)
        interfaces = set(scenario.get("existing_interfaces", []))
        unknown_interfaces = interfaces - INTERFACE_TYPES
        if not interfaces or unknown_interfaces:
            failures.append(
                f"{scenario_id}: invalid existing interfaces {sorted(interfaces)}"
            )
        if not scenario.get("signals") or not scenario.get("evidence"):
            failures.append(f"{scenario_id}: missing signals or required evidence")

        integration_id = scenario.get("expected_integration")
        integration = pattern_by_id(catalog, integration_id)
        if integration is None or integration["kind"] != "integration_patterns":
            failures.append(
                f"{scenario_id}: unknown integration pattern {integration_id}"
            )
        application_id = scenario.get("expected_application_pattern")
        application = pattern_by_id(catalog, application_id)
        if application is None or application["kind"] != "application_patterns":
            failures.append(
                f"{scenario_id}: unknown application pattern {application_id}"
            )
        elif integration_id not in application["preferred_integrations"]:
            failures.append(
                f"{scenario_id}: {integration_id} is not preferred by {application_id}"
            )

    passed_retrieval = 0
    for case in retrieval_cases:
        case_id = case.get("id")
        if not case_id or case_id in seen_ids:
            failures.append(f"invalid or duplicate retrieval-case id: {case_id}")
            continue
        seen_ids.add(case_id)
        expected = set(case.get("expected_components_any", []))
        if not expected or not expected.issubset(catalog_component_ids):
            failures.append(f"{case_id}: invalid expected component set")
            continue
        hits = {
            component["id"]
            for component in search_components(catalog, case.get("intent", ""))
        }
        if hits.intersection(expected):
            passed_retrieval += 1
        else:
            failures.append(
                f"{case_id}: query {case.get('intent')!r} returned "
                f"{sorted(hits)}, expected one of {sorted(expected)}"
            )

    total_retrieval = len(retrieval_cases)
    passed_composition = 0
    for case in composition_cases:
        case_id = case.get("id")
        if not case_id or case_id in seen_ids:
            failures.append(f"invalid or duplicate composition-case id: {case_id}")
            continue
        seen_ids.add(case_id)
        pattern_ids = case.get("application_patterns", [])
        minimum_difference = case.get("min_pairwise_component_difference", 0)
        required_components = case.get("required_components_by_pattern", {})
        forbidden_component_sets = case.get("forbidden_component_sets", [])
        patterns = [pattern_by_id(catalog, pattern_id) for pattern_id in pattern_ids]
        if (
            len(pattern_ids) < 2
            or not isinstance(minimum_difference, int)
            or minimum_difference < 1
            or any(
                pattern is None or pattern["kind"] != "application_patterns"
                for pattern in patterns
            )
        ):
            failures.append(f"{case_id}: invalid application pattern set")
            continue

        if not isinstance(required_components, dict) or any(
            pattern_id not in pattern_ids
            or not isinstance(component_ids, list)
            or not component_ids
            or any(
                not isinstance(component_id, str) or not component_id
                for component_id in component_ids
            )
            for pattern_id, component_ids in required_components.items()
        ):
            failures.append(f"{case_id}: invalid required component map")
            continue

        if not isinstance(forbidden_component_sets, list) or any(
            not isinstance(component_set, list)
            or len(component_set) < 2
            or len(component_set) != len(set(component_set))
            or any(
                not isinstance(component_id, str)
                or not component_id
                or component_id not in catalog_component_ids
                for component_id in component_set
            )
            for component_set in forbidden_component_sets
        ):
            failures.append(f"{case_id}: invalid forbidden component sets")
            continue

        missing_required = []
        components_by_pattern = {
            pattern["id"]: set(pattern["components"]) for pattern in patterns
        }
        for pattern_id, component_ids in required_components.items():
            missing = set(component_ids) - components_by_pattern[pattern_id]
            if missing:
                missing_required.append(
                    f"{pattern_id} missing {', '.join(sorted(missing))}"
                )
        if missing_required:
            failures.append(
                f"{case_id}: required signature components failed: "
                + "; ".join(missing_required)
            )
            continue

        forbidden_matches = []
        for pattern_id, component_ids in components_by_pattern.items():
            for forbidden in forbidden_component_sets:
                if set(forbidden).issubset(component_ids):
                    forbidden_matches.append(
                        f"{pattern_id} contains {', '.join(forbidden)}"
                    )
        if forbidden_matches:
            failures.append(
                f"{case_id}: forbidden generic component shell: "
                + "; ".join(forbidden_matches)
            )
            continue

        component_sets = [set(pattern["components"]) for pattern in patterns]
        smallest_difference = min(
            len(left.symmetric_difference(right))
            for index, left in enumerate(component_sets)
            for right in component_sets[index + 1 :]
        )
        if smallest_difference < minimum_difference:
            failures.append(
                f"{case_id}: smallest pairwise component difference "
                f"{smallest_difference}, expected at least {minimum_difference}"
            )
            continue
        passed_composition += 1

    return {
        "passed": not failures,
        "project_shape_count": len(project_shapes),
        "retrieval_case_count": total_retrieval,
        "retrieval_passed": passed_retrieval,
        "retrieval_score": passed_retrieval / total_retrieval,
        "composition_case_count": len(composition_cases),
        "composition_passed": passed_composition,
        "failures": failures,
    }


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    default_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=default_root)
    parser.add_argument("--json", action="store_true", dest="as_json")
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    project_root = args.project_root.resolve()
    catalog = json.loads(
        (
            project_root
            / "docs"
            / "ai"
            / "generated"
            / "fluentqt-ai-catalog.json"
        ).read_text(encoding="utf-8")
    )
    scenarios = json.loads(
        (project_root / "docs/ai/evals/scenarios.json").read_text(encoding="utf-8")
    )
    report = evaluate_catalog(catalog, scenarios)
    if args.as_json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(
            "FluentQt AI catalog eval: "
            f"{report['project_shape_count']} project shapes, "
            f"{report['retrieval_passed']}/{report['retrieval_case_count']} "
            "intent queries passed, "
            f"{report['composition_passed']}/{report['composition_case_count']} "
            "composition cases passed"
        )
        for failure in report["failures"]:
            print(f"error: {failure}", file=sys.stderr)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
