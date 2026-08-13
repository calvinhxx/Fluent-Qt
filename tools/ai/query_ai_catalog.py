#!/usr/bin/env python3

"""Query the generated FluentQt AI catalog without loading it all into context."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
from typing import Iterable


TERM_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9+.-]*")

STOP_TERMS = {
    "a",
    "an",
    "and",
    "for",
    "from",
    "in",
    "into",
    "need",
    "needs",
    "of",
    "on",
    "or",
    "show",
    "the",
    "to",
    "user",
    "users",
    "when",
    "with",
}

TERM_ALIASES = {
    "activity": ("progress", "status", "running", "log"),
    "cancel": ("cancellation", "stop", "abort"),
    "cancellation": ("cancel", "stop", "abort"),
    "confirmation": ("confirm", "decision", "dialog"),
    "conversation": ("message", "chat", "text", "thread", "content"),
    "desktop": ("application", "window"),
    "error": ("failure", "critical", "validation"),
    "loading": ("progress", "shimmer", "busy"),
    "multi-line": ("multiline", "multiple lines", "text edit"),
    "multiline": ("multi-line", "multiple lines", "text edit"),
    "panel": ("pane", "drawer", "sidebar", "surface"),
    "permission": ("approval", "confirm", "decision", "dialog"),
    "prompt": ("input", "text", "entry", "edit"),
    "running": ("progress", "activity", "operation"),
    "settings": ("preference", "option", "toggle"),
    "sidebar": ("navigation", "pane", "panel"),
    "streamed": ("stream", "incremental", "live", "append", "scrolling"),
    "task": ("workflow", "operation", "action", "command"),
}


def load_catalog(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def component_by_id(
    catalog: dict[str, object], component_id: str
) -> dict[str, object] | None:
    return next(
        (
            component
            for component in catalog["components"]
            if component["id"] == component_id
        ),
        None,
    )


def search_components(
    catalog: dict[str, object], query: str, limit: int = 8
) -> list[dict[str, object]]:
    raw_terms = [term.lower() for term in TERM_PATTERN.findall(query)]
    terms = list(dict.fromkeys(term for term in raw_terms if term not in STOP_TERMS))
    if not terms:
        terms = list(dict.fromkeys(raw_terms))
    if not terms:
        return []
    guide_text: dict[str, list[str]] = {}
    for guide in catalog["selection_guides"]:
        for candidate in guide["candidates"]:
            guide_text.setdefault(candidate["component_id"], []).extend(
                (
                    guide["question"],
                    candidate["choose_when"],
                    candidate["avoid_when"],
                )
            )

    pattern_text: dict[str, list[str]] = {}
    for pattern in catalog["application_patterns"]:
        context = " ".join(
            (
                pattern["id"],
                pattern["title"],
                " ".join(pattern["signals"]),
            )
        )
        for component_id in pattern["components"]:
            pattern_text.setdefault(component_id, []).append(context)

    scored = []
    for component in catalog["components"]:
        sample_text = " ".join(
            f"{sample['id']} {sample['title']} {sample['description']}"
            for sample in component["samples"]
        )
        identity = f"{component['id']} {component['title']}".lower()
        contract = " ".join(
            (
                component["description"],
                " ".join(component["capabilities"]),
                " ".join(component["search_terms"]),
            )
        ).lower()
        guide = " ".join(guide_text.get(component["id"], [])).lower()
        patterns = " ".join(pattern_text.get(component["id"], [])).lower()
        samples = sample_text.lower()
        weighted_fields = (
            (40, identity),
            (16, guide),
            (12, contract),
            (8, patterns),
            (4, samples),
        )

        score = 0
        matched_terms = 0
        for term in terms:
            variants = (term, *TERM_ALIASES.get(term, ()))
            best_term_score = 0
            for variant in variants:
                for weight, field in weighted_fields:
                    occurrences = field.count(variant)
                    if occurrences:
                        exact_bonus = 5 if variant == term else 0
                        best_term_score = max(
                            best_term_score,
                            weight + min(occurrences, 5) + exact_bonus,
                        )
            if best_term_score:
                matched_terms += 1
                score += best_term_score

        required_matches = (
            math.ceil(len(terms) * 2 / 3)
            if len(terms) <= 3
            else max(2, math.ceil(len(terms) / 4))
        )
        if matched_terms < required_matches:
            continue
        score += round(1000 * matched_terms / len(terms))
        combined = " ".join((identity, contract, guide, patterns, samples))
        if query.lower().strip() in combined:
            score += 100
        if component["id"] == query.lower() or component["title"].lower() == query.lower():
            score += 1000
        scored.append((score, component["id"], component))
    scored.sort(key=lambda item: (-item[0], item[1]))
    return [component for _, _, component in scored[:limit]]


def pattern_by_id(
    catalog: dict[str, object], pattern_id: str
) -> dict[str, object] | None:
    for key in ("application_patterns", "integration_patterns"):
        for pattern in catalog[key]:
            if pattern["id"] == pattern_id:
                return {"kind": key, **pattern}
    return None


def guide_by_id(
    catalog: dict[str, object], guide_id: str
) -> dict[str, object] | None:
    return next(
        (guide for guide in catalog["selection_guides"] if guide["id"] == guide_id),
        None,
    )


def format_component(component: dict[str, object]) -> str:
    lines = [
        f"## {component['title']} (`{component['id']}`)",
        "",
        component["description"],
        "",
        f"- C++: `{component['cpp']['public_header']}` / "
        f"`{component['cpp']['qualified_type']}` / "
        f"`{component['cpp']['cmake_target']}`",
        f"- Python: `{component['python']['import_statement']}`",
        f"- Capabilities: {', '.join(component['capabilities'])}",
        f"- Focused test: `{component['tests'][0]['target']}` "
        f"(`{component['tests'][0]['source']}`)",
        f"- Gallery source: `{component['gallery']['sample_source']}`",
        "- Samples: "
        + "; ".join(
            f"{sample['id']} - {sample['title']}" for sample in component["samples"]
        ),
    ]
    return "\n".join(lines)


def format_pattern(pattern: dict[str, object]) -> str:
    lines = [f"## {pattern['title']} (`{pattern['id']}`)", ""]
    if pattern["kind"] == "application_patterns":
        lines.extend(
            (
                "Signals:",
                *(f"- {signal}" for signal in pattern["signals"]),
                "",
                "Preferred integrations: "
                + ", ".join(pattern["preferred_integrations"]),
                "Candidate components (apply integration window ownership first): "
                + ", ".join(pattern["components"]),
            )
        )
    else:
        lines.extend(
            (
                f"Window ownership: {pattern['window_ownership']}",
                f"Choose when: {pattern['choose_when']}",
                f"Avoid when: {pattern['avoid_when']}",
                "Required evidence:",
                *(f"- {item}" for item in pattern["required_evidence"]),
            )
        )
    return "\n".join(lines)


def format_guide(guide: dict[str, object]) -> str:
    lines = [f"## {guide['question']} (`{guide['id']}`)", ""]
    for candidate in guide["candidates"]:
        lines.extend(
            (
                f"### `{candidate['component_id']}`",
                f"Choose when: {candidate['choose_when']}",
                f"Avoid when: {candidate['avoid_when']}",
                "",
            )
        )
    return "\n".join(lines).rstrip()


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    default_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=default_root)
    parser.add_argument("--catalog", type=Path)
    parser.add_argument("--json", action="store_true", dest="as_json")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--component")
    group.add_argument("--search")
    group.add_argument("--pattern")
    group.add_argument("--guide")
    group.add_argument("--list-components", action="store_true")
    parser.add_argument("--limit", type=int, default=8)
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    project_root = args.project_root.resolve()
    catalog_path = (
        args.catalog.resolve()
        if args.catalog is not None
        else project_root
        / "docs"
        / "ai"
        / "generated"
        / "fluentqt-ai-catalog.json"
    )
    catalog = load_catalog(catalog_path)
    result: object
    formatter = None
    if args.component:
        result = component_by_id(catalog, args.component)
        formatter = format_component
    elif args.search:
        result = search_components(catalog, args.search, args.limit)
    elif args.pattern:
        result = pattern_by_id(catalog, args.pattern)
        formatter = format_pattern
    elif args.guide:
        result = guide_by_id(catalog, args.guide)
        formatter = format_guide
    else:
        result = [
            {"id": component["id"], "title": component["title"]}
            for component in catalog["components"]
        ]

    if result is None or result == []:
        print("No matching FluentQt catalog entry found.")
        return 1
    if args.as_json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    elif isinstance(result, list):
        if args.list_components:
            for item in result:
                print(f"{item['id']}\t{item['title']}")
        else:
            print("\n\n".join(format_component(item) for item in result))
    else:
        assert formatter is not None
        print(formatter(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
