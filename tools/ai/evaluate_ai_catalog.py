#!/usr/bin/env python3

"""Run deterministic project-shape and intent-retrieval AI catalog evals."""

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
    if not isinstance(project_shapes, list) or not project_shapes:
        raise ValueError("AI eval scenarios must define project_shapes")
    if not isinstance(retrieval_cases, list) or not retrieval_cases:
        raise ValueError("AI eval scenarios must define retrieval_cases")

    seen_ids: set[str] = set()
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
        component_ids = {component["id"] for component in catalog["components"]}
        if not expected or not expected.issubset(component_ids):
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
    return {
        "passed": not failures,
        "project_shape_count": len(project_shapes),
        "retrieval_case_count": total_retrieval,
        "retrieval_passed": passed_retrieval,
        "retrieval_score": passed_retrieval / total_retrieval,
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
            "intent queries passed"
        )
        for failure in report["failures"]:
            print(f"error: {failure}", file=sys.stderr)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
