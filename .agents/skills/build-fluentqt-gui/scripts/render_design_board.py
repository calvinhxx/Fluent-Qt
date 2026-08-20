#!/usr/bin/env python3
"""Render a FluentQt brief into a portable high-fidelity selection board."""

from __future__ import annotations

import argparse
import base64
import json
import mimetypes
from pathlib import Path
import sys
import textwrap
from typing import Any
from xml.sax.saxutils import escape, quoteattr

from validate_design_brief import resolve_local_path


BOARD_WIDTH = 1920
CARD_WIDTH = 608
CARD_GAP = 24
CARD_HEIGHT = 820
CARD_TOP = 170
COMP_X_INSET = 24
COMP_Y = 314
COMP_WIDTH = 560
COMP_HEIGHT = 350
VISUAL_SCORE_LABELS = (
    ("workflow_fit", "FLOW"),
    ("product_signature", "SIGN"),
    ("visual_hierarchy", "HIER"),
    ("density_and_typography", "TYPE"),
    ("theme_and_material", "MAT"),
    ("iconography", "ICON"),
    ("surface_composition", "SURF"),
    ("responsive_quality", "RESP"),
    ("state_and_interaction_polish", "STATE"),
)


def _text(x: int, y: int, value: str, css_class: str) -> str:
    return f'<text x="{x}" y="{y}" class="{css_class}">{escape(value)}</text>'


def _wrapped_text(
    x: int,
    y: int,
    value: str,
    css_class: str,
    *,
    width: int,
    line_height: int,
    max_lines: int = 2,
) -> list[str]:
    lines = textwrap.wrap(value, width=max(16, width // 8)) or [""]
    return [
        _text(x, y + index * line_height, line, css_class)
        for index, line in enumerate(lines[:max_lines])
    ]


def _region_rect(placement: str) -> tuple[int, int, int, int]:
    if placement == "top":
        return 16, 16, 528, 44
    if placement == "bottom":
        return 16, 284, 528, 48
    if placement == "left":
        return 16, 72, 120, 200
    if placement == "right":
        return 424, 72, 120, 200
    if placement == "overlay":
        return 348, 104, 172, 132
    return 148, 72, 264, 200


def _image_data_uri(path: Path) -> str:
    mime_type, _ = mimetypes.guess_type(path.name)
    if mime_type is None:
        mime_type = "image/svg+xml" if path.suffix.casefold() == ".svg" else "image/png"
    encoded = base64.b64encode(path.read_bytes()).decode("ascii")
    return f"data:{mime_type};base64,{encoded}"


def _render_topology(
    concept: dict[str, Any], frame_x: int, frame_y: int
) -> list[str]:
    result = [
        f'<rect x="{frame_x}" y="{frame_y}" width="{COMP_WIDTH}" '
        f'height="{COMP_HEIGHT}" rx="12" class="window-frame"/>'
    ]
    regions = concept.get("layout_regions", [])
    for region in regions:
        if not isinstance(region, dict):
            continue
        x, y, width, height = _region_rect(str(region.get("placement", "center")))
        priority = str(region.get("priority", "supporting"))
        lifetime = str(region.get("lifetime", "persistent"))
        classes = ["region", priority]
        if lifetime != "persistent":
            classes.append("conditional")
        result.append(
            f'<rect x="{frame_x + x}" y="{frame_y + y}" width="{width}" '
            f'height="{height}" rx="8" class="{" ".join(classes)}"/>'
        )
        result.append(
            _text(
                frame_x + x + 10,
                frame_y + y + min(25, height // 2 + 5),
                str(region.get("id", "region")).replace("-", " "),
                "region-label",
            )
        )
    result.append(
        _text(
            frame_x + 18,
            frame_y + COMP_HEIGHT - 16,
            "HIGH-FIDELITY COMP REQUIRED — schematic does not count",
            "missing-comp",
        )
    )
    return result


def _render_comp(
    concept: dict[str, Any], frame_x: int, frame_y: int, brief_path: Path
) -> list[str]:
    comp = concept.get("comp")
    if not isinstance(comp, dict):
        return _render_topology(concept, frame_x, frame_y)
    raw_path = comp.get("path")
    path = (
        resolve_local_path(str(raw_path), brief_path)
        if isinstance(raw_path, str) and raw_path.strip()
        else None
    )
    if path is None or not path.is_file():
        return _render_topology(concept, frame_x, frame_y)
    data_uri = _image_data_uri(path)
    viewport = comp.get("viewport") if isinstance(comp.get("viewport"), dict) else {}
    viewport_label = f"{viewport.get('width', '?')}×{viewport.get('height', '?')}"
    return [
        f'<rect x="{frame_x}" y="{frame_y}" width="{COMP_WIDTH}" '
        f'height="{COMP_HEIGHT}" rx="12" class="comp-frame"/>',
        f'<image x="{frame_x}" y="{frame_y}" width="{COMP_WIDTH}" '
        f'height="{COMP_HEIGHT}" preserveAspectRatio="xMidYMid meet" '
        f'href={quoteattr(data_uri)}/>',
        f'<rect x="{frame_x + 14}" y="{frame_y + 14}" width="176" '
        f'height="28" rx="14" class="comp-badge"/>',
        _text(frame_x + 28, frame_y + 33, "HIGH-FIDELITY COMP", "comp-badge-text"),
        _text(
            frame_x + COMP_WIDTH - 14,
            frame_y + COMP_HEIGHT - 14,
            f"{comp.get('theme', '?')} · {viewport_label}",
            "comp-meta end",
        ),
    ]


def _render_concept(
    concept: dict[str, Any],
    index: int,
    brief_path: Path,
    selected_concept_id: str,
) -> list[str]:
    card_x = 24 + index * (CARD_WIDTH + CARD_GAP)
    frame_x = card_x + COMP_X_INSET
    concept_id = str(concept.get("id", ""))
    selected = concept_id == selected_concept_id
    visual = (
        concept.get("visual_direction")
        if isinstance(concept.get("visual_direction"), dict)
        else {}
    )
    card_classes = "concept-card selected" if selected else "concept-card"
    result = [
        f'<rect x="{card_x}" y="{CARD_TOP}" width="{CARD_WIDTH}" '
        f'height="{CARD_HEIGHT}" rx="20" class="{card_classes}"/>',
        _text(card_x + 24, CARD_TOP + 38, f"CONCEPT 0{index + 1}", "index"),
        _text(
            card_x + 24,
            CARD_TOP + 78,
            str(concept.get("title", "Untitled")),
            "concept-title",
        ),
        _text(
            card_x + 24,
            CARD_TOP + 108,
            str(visual.get("name", "visual direction not defined")),
            "direction-name",
        ),
        _text(
            card_x + 24,
            CARD_TOP + 136,
            str(concept.get("topology", "")),
            "topology",
        ),
    ]
    if selected:
        result.extend(
            [
                f'<rect x="{card_x + CARD_WIDTH - 136}" y="{CARD_TOP + 22}" '
                f'width="112" height="30" rx="15" class="selected-badge"/>',
                _text(
                    card_x + CARD_WIDTH - 80,
                    CARD_TOP + 42,
                    "HUMAN PICK",
                    "selected-text middle",
                ),
            ]
        )
    result.extend(_render_comp(concept, frame_x, COMP_Y, brief_path))

    notes_y = COMP_Y + COMP_HEIGHT + 38
    note_width = CARD_WIDTH - 48
    notes = (
        ("Character · ", str(visual.get("composition_character", "")), "body"),
        ("Subject · ", str(visual.get("subject_translation", "")), "body"),
        ("Signature · ", str(visual.get("signature_move", "")), "body"),
        ("Risk · ", str(visual.get("aesthetic_risk", "")), "risk"),
    )
    for prefix, value, css_class in notes:
        result.extend(
            _wrapped_text(
                card_x + 24,
                notes_y,
                prefix + value,
                css_class,
                width=note_width,
                line_height=19,
                max_lines=2,
            )
        )
        notes_y += 58
    scores = (
        concept.get("visual_scores")
        if isinstance(concept.get("visual_scores"), dict)
        else {}
    )
    result.append(
        _text(card_x + 24, CARD_TOP + CARD_HEIGHT - 62, "VISUAL SCORECARD", "score-label")
    )
    chip_width = 56
    chip_gap = 4
    chip_y = CARD_TOP + CARD_HEIGHT - 49
    chip_x = card_x + (CARD_WIDTH - (chip_width * 9 + chip_gap * 8)) // 2
    for score_id, label in VISUAL_SCORE_LABELS:
        entry = scores.get(score_id) if isinstance(scores.get(score_id), dict) else {}
        score = entry.get("score", 0)
        score_class = "good" if isinstance(score, int) and score >= 4 else "weak"
        if isinstance(score, bool) or not isinstance(score, int) or score <= 0:
            score_class = "pending"
        score_display = score if isinstance(score, int) and not isinstance(score, bool) else "—"
        result.extend(
            [
                f'<rect x="{chip_x}" y="{chip_y}" width="{chip_width}" '
                f'height="28" rx="8" class="score-chip {score_class}"/>',
                _text(
                    chip_x + chip_width // 2,
                    chip_y + 19,
                    f"{label} {score_display}",
                    "score-text middle",
                ),
            ]
        )
        chip_x += chip_width + chip_gap
    return result


def render_board(data: dict[str, Any], brief_path: Path) -> str:
    concepts = data.get("concepts")
    if not isinstance(concepts, list) or not concepts:
        raise ValueError("brief must contain at least one concept")
    if len(concepts) > 3:
        raise ValueError("selection board supports at most three concepts")
    visible = concepts[:3]
    height = CARD_TOP + CARD_HEIGHT + 28
    identity = data.get("identity") if isinstance(data.get("identity"), dict) else {}
    art = (
        data.get("art_direction")
        if isinstance(data.get("art_direction"), dict)
        else {}
    )
    fixture = (
        art.get("content_fixture")
        if isinstance(art.get("content_fixture"), dict)
        else {}
    )
    approval = data.get("approval") if isinstance(data.get("approval"), dict) else {}
    vernacular = (
        data.get("subject_vernacular")
        if isinstance(data.get("subject_vernacular"), dict)
        else {}
    )
    selected = str(approval.get("selected_concept_id") or "")
    impressions = art.get("desired_impression")
    impression_text = (
        " · ".join(str(item) for item in impressions)
        if isinstance(impressions, list)
        else "not defined"
    )
    elements = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{BOARD_WIDTH}" '
        f'height="{height}" viewBox="0 0 {BOARD_WIDTH} {height}">',
        """<style>
        text { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
        .background { fill: #edf2f6; }
        .eyebrow { fill: #59697b; font-size: 13px; font-weight: 700; letter-spacing: 1.4px; }
        .board-title { fill: #101820; font-size: 34px; font-weight: 740; }
        .board-subtitle { fill: #4f6073; font-size: 15px; }
        .approval { fill: #304457; font-size: 14px; font-weight: 650; text-anchor: end; }
        .concept-card { fill: #ffffff; stroke: #c9d5e0; stroke-width: 1.2; }
        .concept-card.selected { stroke: #0788ae; stroke-width: 3; }
        .index { fill: #0788ae; font-size: 13px; font-weight: 760; letter-spacing: .8px; }
        .concept-title { fill: #101820; font-size: 25px; font-weight: 720; }
        .direction-name { fill: #17677e; font-size: 15px; font-weight: 680; }
        .topology { fill: #6a7888; font-size: 12px; }
        .comp-frame { fill: #111820; stroke: #aab8c6; stroke-width: 1; }
        .window-frame { fill: #f5f8fa; stroke: #b6c3cf; stroke-width: 1.2; }
        .region { fill: #dce5ed; stroke: #b5c2cf; stroke-width: 1; }
        .region.primary { fill: #c7e6ee; stroke: #5aa9bb; }
        .region.hero { fill: #0f9fbe; stroke: #087f99; }
        .region.conditional { stroke-dasharray: 5 4; }
        .region-label { fill: #263544; font-size: 11px; font-weight: 650; }
        .missing-comp { fill: #a13e36; font-size: 12px; font-weight: 720; }
        .comp-badge { fill: #10232b; fill-opacity: .86; }
        .comp-badge-text { fill: #ffffff; font-size: 11px; font-weight: 720; letter-spacing: .6px; }
        .comp-meta { fill: #ffffff; font-size: 12px; font-weight: 650; paint-order: stroke; stroke: #101820; stroke-width: 3px; }
        .end { text-anchor: end; }
        .body { fill: #263544; font-size: 14px; font-weight: 620; }
        .caption { fill: #58697a; font-size: 13px; }
        .risk { fill: #963b34; font-size: 13px; }
        .score-label { fill: #708091; font-size: 10px; font-weight: 760; letter-spacing: .8px; }
        .score-chip { stroke-width: 1; }
        .score-chip.good { fill: #d9f0e5; stroke: #87bea1; }
        .score-chip.weak { fill: #f8e4d8; stroke: #d9a383; }
        .score-chip.pending { fill: #e8edf2; stroke: #bac5cf; }
        .score-text { fill: #263544; font-size: 10px; font-weight: 760; }
        .selected-badge { fill: #d8f2f7; }
        .selected-text { fill: #087f99; font-size: 11px; font-weight: 760; }
        .middle { text-anchor: middle; }
        </style>""",
        f'<rect width="{BOARD_WIDTH}" height="{height}" class="background"/>',
        _text(24, 34, "FLUENTQT HUMAN ART-DIRECTION REVIEW", "eyebrow"),
        _text(24, 76, str(data.get("application", "Application")), "board-title"),
        _text(
            24,
            108,
            "Primary object: " + str(identity.get("primary_object", "not defined")),
            "board-subtitle",
        ),
        _text(
            24,
            134,
            "Impression: "
            + impression_text
            + " · Tempo: "
            + str(vernacular.get("tempo", "not defined")),
            "board-subtitle",
        ),
        _text(
            BOARD_WIDTH - 24,
            58,
            "Decision: " + str(approval.get("status", "pending")).upper(),
            "approval",
        ),
        _text(
            BOARD_WIDTH - 24,
            84,
            "Shared content: " + str(fixture.get("id", "not defined")),
            "approval",
        ),
    ]
    for index, concept in enumerate(visible):
        if isinstance(concept, dict):
            elements.extend(_render_concept(concept, index, brief_path, selected))
    elements.append("</svg>")
    return "\n".join(elements) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("brief", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        data = json.loads(args.brief.read_text(encoding="utf-8"))
        output = (args.output or args.brief.parent / "design-board.svg").resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(render_board(data, args.brief), encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: could not render design board: {exc}", file=sys.stderr)
        return 1
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
