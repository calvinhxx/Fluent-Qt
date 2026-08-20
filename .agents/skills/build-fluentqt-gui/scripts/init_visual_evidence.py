#!/usr/bin/env python3
"""Initialize visual evidence v4 from a human-approved design brief."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from validate_design_brief import load_brief, validate_brief


FULL_STATES = (
    "normal-light",
    "normal-dark",
    "narrow",
    "minimum",
    "empty",
    "collection-density",
    "long-localized-content",
    "input-single-line",
    "input-max-lines",
    "scroll-end",
    "async-settled",
    "selected-focus-disabled",
    "transient-surface",
    "ime-preedit",
)
LITE_STATES = (
    "normal-light",
    "normal-dark",
    "narrow",
    "minimum",
    "long-localized-content",
    "selected-focus-disabled",
)
FULL_REGIONS = (
    "titlebar",
    "navigation-or-list",
    "primary-viewport",
    "pane-header-or-status",
    "footer-or-primary-input",
    "scroll-viewport",
    "transient-surface",
)
LITE_REGIONS = (
    "titlebar",
    "primary-viewport",
    "footer-or-primary-input",
)
FULL_DYNAMIC = ("wrapped-text-height", "multiline-input", "async-scroll-end")
SCORE_FIELDS = (
    "workflow_fit",
    "product_signature",
    "visual_hierarchy",
    "density_and_typography",
    "theme_and_material",
    "iconography",
    "surface_composition",
    "responsive_quality",
    "state_and_interaction_polish",
)


def build_manifest(
    brief: dict[str, object],
    brief_path: Path,
    reviewed_build: str,
    platform: str,
) -> dict[str, object]:
    profile = str(brief["profile"])
    states = LITE_STATES if profile == "lite" else FULL_STATES
    regions = LITE_REGIONS if profile == "lite" else FULL_REGIONS
    dynamic = () if profile == "lite" else FULL_DYNAMIC
    return {
        "contract_version": 4,
        "application": brief["application"],
        "author_id": brief["author_id"],
        "design_brief": str(brief_path.resolve()),
        "reviewed_build": reviewed_build,
        "platform": platform,
        "profile": profile,
        "window_backdrop": "mica",
        "surface_fill_policy": "reveal-material",
        "signature_finish": "wireframe",
        "chrome_on_material": "filled-stickers",
        "sparse_canvas_treatment": "dead-space",
        "primary_input_treatment": "none",
        "visible_copy_register": "developer-labeled",
        "states": [
            {"id": state_id, "status": "unverified", "evidence": []}
            for state_id in states
        ],
        "regions": [
            {"id": region_id, "status": "unverified", "evidence": []}
            for region_id in regions
        ],
        "measurements": [
            {
                "id": "REPLACE with one repeated painted metric",
                "expected": "REPLACE with a logical-pixel range",
                "actual": "unmeasured",
                "status": "unverified",
                "evidence": "REPLACE.png"
            }
        ],
        "dynamic_checks": [
            {"id": check_id, "status": "unverified", "evidence": []}
            for check_id in dynamic
        ],
        "issues": [],
        "review": {
            "reviewer_kind": "independent-agent",
            "reviewer_id": "REPLACE with a reviewer different from author_id",
            "reviewed_at": "REPLACE after the final build is reviewed",
            "verdict": "unverified",
            "review_board": "visual-review.html",
            "reference_images": [],
            "scores": {
                score_id: {
                    "score": 0,
                    "note": "REPLACE with an evidence-grounded judgment",
                    "evidence": []
                }
                for score_id in SCORE_FIELDS
            },
            "findings": []
        }
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--brief", type=Path, required=True)
    parser.add_argument("--reviewed-build", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    try:
        brief = load_brief(args.brief)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    brief_errors = validate_brief(brief, args.brief)
    if brief_errors:
        print(
            "error: design brief must pass the approved stage before evidence is initialized",
            file=sys.stderr,
        )
        for error in brief_errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    output = args.output.resolve()
    if output.exists() and not args.force:
        print(f"error: output exists (use --force): {output}", file=sys.stderr)
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            build_manifest(
                brief,
                args.brief,
                args.reviewed_build,
                args.platform,
            ),
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
