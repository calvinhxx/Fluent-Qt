#!/usr/bin/env python3
"""Validate a machine-readable FluentQt product and composition brief."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse
from urllib.request import url2pathname


CURRENT_CONTRACT_VERSION = 4
ALLOWED_CONTRACT_VERSIONS = {2, 3, 4}
ALLOWED_PROFILES = {"lite", "full"}
ALLOWED_STAGES = {"concepts", "approved"}
ALLOWED_APPROVAL_STATUSES = {"pending", "approved", "rejected"}
ALLOWED_COMP_SOURCES = {"raster", "svg", "figma-export", "code-native"}
ALLOWED_COMP_SUFFIXES = {".png", ".jpg", ".jpeg", ".webp", ".svg"}
GENERIC_PRIMARY_OBJECTS = {
    "app",
    "application",
    "chat",
    "dashboard",
    "gui",
    "interface",
    "screen",
    "workspace",
}
SCORE_FIELDS = {
    "workflow_fit",
    "state_visibility",
    "responsiveness",
    "component_semantics",
    "implementation_risk",
    "distinctiveness",
}
VISUAL_SCORE_FIELDS = {
    "workflow_fit",
    "product_signature",
    "visual_hierarchy",
    "density_and_typography",
    "theme_and_material",
    "iconography",
    "surface_composition",
    "responsive_quality",
    "state_and_interaction_polish",
}
DENSITY_FIELDS = {
    "chrome_height",
    "edge_inset",
    "related_gap",
    "section_gap",
    "control_height",
    "row_height",
    "icon_size",
}
REQUIRED_IDENTITY_FIELDS = {
    "primary_object",
    "dominant_time_model",
    "core_outcome",
    "hero_interaction",
    "signature_surface",
}
REQUIRED_CONCEPT_FIELDS = {
    "id",
    "title",
    "topology",
    "primary_surface",
    "hero_interaction",
    "narrow_behavior",
    "failure_risk",
}
REQUIRED_VISUAL_DIRECTION_FIELDS = {
    "name",
    "composition_character",
    "palette_strategy",
    "type_strategy",
    "icon_strategy",
    "signature_move",
    "restraint_rule",
}
V4_VISUAL_DIRECTION_FIELDS = {
    "subject_translation",
    "aesthetic_risk",
    "risk_guard",
}
TUNING_AXIS_FIELDS = {
    "density",
    "contrast",
    "material_depth",
    "corner_softness",
    "motion_energy",
    "visual_expressiveness",
}
IMPLEMENTATION_SPEC_LIST_MINIMUMS = {
    "semantic_tokens": 2,
    "typography_roles": 2,
    "component_families": 1,
    "state_grammar": 2,
    "motion_cues": 1,
    "responsive_rules": 2,
    "locked_decisions": 2,
    "allowed_adaptations": 1,
    "known_risks": 1,
    "comparison_regions": 2,
}
ICONOGRAPHY_TEXT_FIELDS = {
    "family",
    "provenance",
    "style",
    "stroke_policy",
    "filled_policy",
    "color_policy",
    "icon_only_policy",
}
V4_ICONOGRAPHY_TEXT_FIELDS = {
    "source_strategy",
    "application_icon_strategy",
    "in_app_mark_strategy",
    "small_size_validation",
    "palette_seed",
}
ICONOGRAPHY_SIZE_FIELDS = {
    "base_grid",
    "compact_glyph_size",
    "standard_glyph_size",
    "action_slot_size",
}
VISUAL_DISTINCTNESS_FIELDS = (
    "composition_character",
    "palette_strategy",
    "type_strategy",
    "signature_move",
)
PLACEHOLDER_PATTERN = re.compile(r"\b(?:REPLACE|TODO|TBD|PLACEHOLDER)\b", re.IGNORECASE)


def load_brief(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read JSON brief: {exc}") from exc
    if not isinstance(data, dict):
        raise ValueError("brief root must be an object")
    return data


def resolve_local_path(raw_path: str, brief_path: Path) -> Path | None:
    direct = Path(raw_path).expanduser()
    if direct.is_absolute():
        return direct.resolve()
    parsed = urlparse(raw_path)
    if parsed.scheme and parsed.scheme != "file":
        return None
    value = (
        url2pathname(unquote(parsed.path))
        if parsed.scheme == "file"
        else raw_path
    )
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = brief_path.parent / path
    return path.resolve()


def _non_empty_string(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _contains_placeholder(value: str) -> bool:
    return PLACEHOLDER_PATTERN.search(value) is not None


def _require_text(
    data: dict[str, Any], key: str, prefix: str, errors: list[str]
) -> str:
    value = data.get(key)
    label = f"{prefix}.{key}" if prefix else key
    if not _non_empty_string(value):
        errors.append(f"{label} must be a non-empty string")
        return ""
    assert isinstance(value, str)
    if _contains_placeholder(value):
        errors.append(f"{label} still contains a placeholder")
    return value.strip()


def _require_string_list(
    data: dict[str, Any], key: str, prefix: str, errors: list[str]
) -> list[str]:
    value = data.get(key)
    label = f"{prefix}.{key}" if prefix else key
    if (
        not isinstance(value, list)
        or not value
        or not all(_non_empty_string(item) for item in value)
    ):
        errors.append(f"{label} must be a non-empty string array")
        return []
    result = [str(item).strip() for item in value]
    if any(_contains_placeholder(item) for item in result):
        errors.append(f"{label} still contains a placeholder")
    return result


def _validate_identity(data: dict[str, Any], errors: list[str]) -> None:
    identity = data.get("identity")
    if not isinstance(identity, dict):
        errors.append("identity must be an object")
        return
    values = {
        field: _require_text(identity, field, "identity", errors)
        for field in REQUIRED_IDENTITY_FIELDS
    }
    primary = values.get("primary_object", "").lower()
    if primary in GENERIC_PRIMARY_OBJECTS:
        errors.append(
            "identity.primary_object must name a domain object, not a generic shell"
        )


def _validate_references(data: dict[str, Any], errors: list[str]) -> None:
    references = data.get("references")
    if not isinstance(references, dict):
        errors.append("references must be an object")
        return
    names: list[str] = []
    for role in ("aligned", "contrast"):
        reference = references.get(role)
        prefix = f"references.{role}"
        if not isinstance(reference, dict):
            errors.append(f"{prefix} must be an object")
            continue
        names.append(_require_text(reference, "name", prefix, errors))
        _require_text(reference, "fit", prefix, errors)
        _require_string_list(reference, "transferred_traits", prefix, errors)
        _require_string_list(reference, "rejected_traits", prefix, errors)
    if len(names) == 2 and names[0].casefold() == names[1].casefold():
        errors.append("aligned and contrast references must be different")


def _validate_taste_context(data: dict[str, Any], errors: list[str]) -> None:
    context = data.get("taste_context")
    if not isinstance(context, dict):
        errors.append("taste_context must be an object")
        return
    _require_string_list(context, "user_signals", "taste_context", errors)
    _require_string_list(context, "project_signals", "taste_context", errors)
    avoided = _require_string_list(
        context, "recent_patterns_to_avoid", "taste_context", errors
    )
    if len(avoided) < 2:
        errors.append("taste_context.recent_patterns_to_avoid needs at least two entries")

    reference = context.get("non_ui_reference")
    if not isinstance(reference, dict):
        errors.append("taste_context.non_ui_reference must be an object")
        return
    _require_text(reference, "name", "taste_context.non_ui_reference", errors)
    _require_text(reference, "source", "taste_context.non_ui_reference", errors)
    _require_string_list(
        reference, "transferred_traits", "taste_context.non_ui_reference", errors
    )
    _require_string_list(
        reference, "rejected_traits", "taste_context.non_ui_reference", errors
    )


def _validate_subject_vernacular(data: dict[str, Any], errors: list[str]) -> None:
    vernacular = data.get("subject_vernacular")
    if not isinstance(vernacular, dict):
        errors.append("subject_vernacular must be an object")
        return
    for field in ("materials", "artifacts", "instruments", "verbs"):
        values = _require_string_list(
            vernacular, field, "subject_vernacular", errors
        )
        if len(values) < 2:
            errors.append(f"subject_vernacular.{field} needs at least two entries")
    _require_text(vernacular, "tempo", "subject_vernacular", errors)


def _validate_art_direction(
    data: dict[str, Any], contract_version: int, errors: list[str]
) -> str:
    direction = data.get("art_direction")
    if not isinstance(direction, dict):
        errors.append("art_direction must be an object")
        return ""
    impressions = _require_string_list(
        direction, "desired_impression", "art_direction", errors
    )
    if len(impressions) != 3:
        errors.append("art_direction.desired_impression must contain exactly three words")
    if len({item.casefold() for item in impressions}) != len(impressions):
        errors.append("art_direction.desired_impression values must be distinct")
    for field in (
        "visual_world",
        "signature_element",
        "typography_voice",
        "palette_strategy",
        "motion_voice",
    ):
        _require_text(direction, field, "art_direction", errors)
    anti_goals = _require_string_list(direction, "anti_goals", "art_direction", errors)
    if len(anti_goals) < 3:
        errors.append("art_direction.anti_goals needs at least three concrete entries")

    if contract_version >= 4:
        risk = direction.get("aesthetic_risk")
        if not isinstance(risk, dict):
            errors.append("art_direction.aesthetic_risk must be an object")
        else:
            for field in (
                "move",
                "evidence",
                "quiet_zone",
                "usability_guard",
                "fallback",
            ):
                _require_text(risk, field, "art_direction.aesthetic_risk", errors)

    fixture = direction.get("content_fixture")
    if not isinstance(fixture, dict):
        errors.append("art_direction.content_fixture must be an object")
        return ""
    fixture_id = _require_text(
        fixture, "id", "art_direction.content_fixture", errors
    )
    _require_text(fixture, "source", "art_direction.content_fixture", errors)
    _require_text(fixture, "scenario", "art_direction.content_fixture", errors)
    strings = _require_string_list(
        fixture, "required_strings", "art_direction.content_fixture", errors
    )
    if len(strings) < 3:
        errors.append(
            "art_direction.content_fixture.required_strings needs at least three entries"
        )
    states = _require_string_list(
        fixture, "required_states", "art_direction.content_fixture", errors
    )
    if len(states) < 2:
        errors.append(
            "art_direction.content_fixture.required_states needs at least two entries"
        )
    return fixture_id


def _validate_regions(
    concept: dict[str, Any], concept_prefix: str, errors: list[str]
) -> None:
    regions = concept.get("layout_regions")
    if not isinstance(regions, list) or len(regions) < 2:
        errors.append(f"{concept_prefix}.layout_regions needs at least two regions")
        return
    seen: set[str] = set()
    primary_count = 0
    for index, region in enumerate(regions):
        prefix = f"{concept_prefix}.layout_regions[{index}]"
        if not isinstance(region, dict):
            errors.append(f"{prefix} must be an object")
            continue
        region_id = _require_text(region, "id", prefix, errors)
        if region_id in seen:
            errors.append(f"duplicate region id in {concept_prefix}: {region_id}")
        seen.add(region_id)
        placement = _require_text(region, "placement", prefix, errors)
        if placement not in {"top", "left", "center", "right", "bottom", "overlay"}:
            errors.append(f"{prefix}.placement has unsupported value {placement!r}")
        lifetime = _require_text(region, "lifetime", prefix, errors)
        if lifetime not in {"persistent", "conditional", "transient"}:
            errors.append(f"{prefix}.lifetime has unsupported value {lifetime!r}")
        priority = _require_text(region, "priority", prefix, errors)
        if priority not in {"hero", "primary", "supporting"}:
            errors.append(f"{prefix}.priority has unsupported value {priority!r}")
        if priority == "hero":
            primary_count += 1
    if primary_count != 1:
        errors.append(f"{concept_prefix} must define exactly one hero region")


def _validate_scores(
    concept: dict[str, Any], concept_prefix: str, errors: list[str]
) -> None:
    scores = concept.get("scores")
    if not isinstance(scores, dict):
        errors.append(f"{concept_prefix}.scores must be an object")
        return
    missing = SCORE_FIELDS - set(scores)
    if missing:
        errors.append(
            f"{concept_prefix}.scores missing: {', '.join(sorted(missing))}"
        )
    for field in SCORE_FIELDS:
        score = scores.get(field)
        if isinstance(score, bool) or not isinstance(score, int) or not 1 <= score <= 5:
            errors.append(f"{concept_prefix}.scores.{field} must be an integer 1..5")


def _validate_visual_scores(
    concept: dict[str, Any], concept_prefix: str, errors: list[str]
) -> None:
    scores = concept.get("visual_scores")
    if not isinstance(scores, dict):
        errors.append(f"{concept_prefix}.visual_scores must be an object")
        return
    missing = VISUAL_SCORE_FIELDS - set(scores)
    if missing:
        errors.append(
            f"{concept_prefix}.visual_scores missing: {', '.join(sorted(missing))}"
        )
    for field in VISUAL_SCORE_FIELDS:
        prefix = f"{concept_prefix}.visual_scores.{field}"
        entry = scores.get(field)
        if not isinstance(entry, dict):
            if field not in missing:
                errors.append(f"{prefix} must be an object")
            continue
        score = entry.get("score")
        if isinstance(score, bool) or not isinstance(score, int) or not 1 <= score <= 5:
            errors.append(f"{prefix}.score must be an integer 1..5")
        _require_text(entry, "note", prefix, errors)


def _validate_visual_direction(
    concept: dict[str, Any],
    concept_prefix: str,
    errors: list[str],
    *,
    contract_version: int,
) -> dict[str, str]:
    direction = concept.get("visual_direction")
    if not isinstance(direction, dict):
        errors.append(f"{concept_prefix}.visual_direction must be an object")
        return {}
    required_fields = set(REQUIRED_VISUAL_DIRECTION_FIELDS)
    if contract_version < 3:
        required_fields = required_fields - {"icon_strategy"}
    if contract_version >= 4:
        required_fields |= V4_VISUAL_DIRECTION_FIELDS
    return {
        field: _require_text(
            direction, field, f"{concept_prefix}.visual_direction", errors
        )
        for field in required_fields
    }


def _validate_comp(
    concept: dict[str, Any],
    concept_prefix: str,
    brief_path: Path,
    fixture_id: str,
    *,
    required: bool,
    errors: list[str],
) -> tuple[str, int, int] | None:
    comp = concept.get("comp")
    if not isinstance(comp, dict):
        if required:
            errors.append(f"{concept_prefix}.comp must be a high-fidelity comp object")
        return None

    raw_path = _require_text(comp, "path", f"{concept_prefix}.comp", errors)
    if raw_path:
        path = resolve_local_path(raw_path, brief_path)
        if path is None or not path.is_file():
            errors.append(f"{concept_prefix}.comp.path file does not exist: {raw_path}")
        elif path.suffix.casefold() not in ALLOWED_COMP_SUFFIXES:
            errors.append(
                f"{concept_prefix}.comp.path must be PNG, JPEG, WebP, or SVG"
            )
        elif path.stat().st_size == 0:
            errors.append(f"{concept_prefix}.comp.path must not be empty")

    fidelity = _require_text(comp, "fidelity", f"{concept_prefix}.comp", errors)
    if fidelity and fidelity != "high":
        errors.append(f"{concept_prefix}.comp.fidelity must be 'high'")
    source_kind = _require_text(
        comp, "source_kind", f"{concept_prefix}.comp", errors
    )
    if source_kind and source_kind not in ALLOWED_COMP_SOURCES:
        errors.append(
            f"{concept_prefix}.comp.source_kind must be one of "
            + ", ".join(sorted(ALLOWED_COMP_SOURCES))
        )
    theme = _require_text(comp, "theme", f"{concept_prefix}.comp", errors)
    comp_fixture_id = _require_text(
        comp, "content_fixture_id", f"{concept_prefix}.comp", errors
    )
    if fixture_id and comp_fixture_id and fixture_id != comp_fixture_id:
        errors.append(
            f"{concept_prefix}.comp.content_fixture_id must match "
            "art_direction.content_fixture.id"
        )

    viewport = comp.get("viewport")
    if not isinstance(viewport, dict):
        errors.append(f"{concept_prefix}.comp.viewport must be an object")
        return None
    width = viewport.get("width")
    height = viewport.get("height")
    if isinstance(width, bool) or not isinstance(width, int) or width < 960:
        errors.append(f"{concept_prefix}.comp.viewport.width must be at least 960")
        width = 0
    if isinstance(height, bool) or not isinstance(height, int) or height < 600:
        errors.append(f"{concept_prefix}.comp.viewport.height must be at least 600")
        height = 0
    return theme, int(width), int(height)


def _validate_concepts(
    data: dict[str, Any],
    profile: str,
    brief_path: Path,
    fixture_id: str,
    contract_version: int,
    errors: list[str],
) -> set[str]:
    concepts = data.get("concepts")
    expected = 1 if profile == "lite" else 3
    if not isinstance(concepts, list) or len(concepts) != expected:
        errors.append(f"concepts needs exactly {expected} entries for {profile}")
        return set()
    ids: set[str] = set()
    topologies: set[str] = set()
    visual_directions: list[tuple[str, dict[str, str]]] = []
    comp_signatures: list[tuple[str, int, int]] = []
    for index, concept in enumerate(concepts):
        prefix = f"concepts[{index}]"
        if not isinstance(concept, dict):
            errors.append(f"{prefix} must be an object")
            continue
        values = {
            field: _require_text(concept, field, prefix, errors)
            for field in REQUIRED_CONCEPT_FIELDS
        }
        concept_id = values.get("id", "")
        topology = values.get("topology", "")
        if concept_id in ids:
            errors.append(f"duplicate concept id: {concept_id}")
        ids.add(concept_id)
        if profile == "full" and topology in topologies:
            errors.append(
                f"full brief concepts must use distinct topologies: {topology}"
            )
        topologies.add(topology)
        _validate_regions(concept, prefix, errors)
        _validate_scores(concept, prefix, errors)
        if contract_version >= 3:
            _validate_visual_scores(concept, prefix, errors)
        _require_string_list(concept, "preferred_components", prefix, errors)
        visual_directions.append(
            (
                concept_id,
                _validate_visual_direction(
                    concept,
                    prefix,
                    errors,
                    contract_version=contract_version,
                ),
            )
        )
        comp_signature = _validate_comp(
            concept,
            prefix,
            brief_path,
            fixture_id,
            required=profile == "full",
            errors=errors,
        )
        if comp_signature is not None:
            comp_signatures.append(comp_signature)

    if profile == "full":
        direction_names = [
            direction.get("name", "").casefold()
            for _, direction in visual_directions
        ]
        if len(set(direction_names)) != len(direction_names):
            errors.append("full brief visual direction names must be distinct")
        for left_index, (left_id, left) in enumerate(visual_directions):
            for right_id, right in visual_directions[left_index + 1 :]:
                differing = sum(
                    left.get(field, "").casefold()
                    != right.get(field, "").casefold()
                    for field in VISUAL_DISTINCTNESS_FIELDS
                )
                if differing < 3:
                    errors.append(
                        f"visual directions {left_id!r} and {right_id!r} must differ "
                        "in at least three of composition, palette, type, and signature move"
                    )
        if len(comp_signatures) == len(concepts) and len(set(comp_signatures)) != 1:
            errors.append(
                "full brief comps must use the same theme and viewport for comparison"
            )
    return ids


def _validate_approval(
    data: dict[str, Any],
    concept_ids: set[str],
    profile: str,
    stage: str,
    errors: list[str],
) -> None:
    approval = data.get("approval")
    if not isinstance(approval, dict):
        errors.append("approval must be an object")
        return
    status = approval.get("status")
    if status not in ALLOWED_APPROVAL_STATUSES:
        errors.append(
            "approval.status must be one of "
            + ", ".join(sorted(ALLOWED_APPROVAL_STATUSES))
        )
        return
    if stage == "concepts" and status != "approved":
        return
    if status != "approved":
        errors.append(
            "approval.status must be 'approved' before implementation; "
            "pending or rejected concepts remain blocked"
        )
        return

    kind = _require_text(
        approval, "decision_maker_kind", "approval", errors
    )
    if kind and kind != "human":
        errors.append("approval.decision_maker_kind must be 'human'")
    maker_id = _require_text(
        approval, "decision_maker_id", "approval", errors
    )
    author_id = str(data.get("author_id", "")).strip()
    if maker_id and author_id and maker_id.casefold() == author_id.casefold():
        errors.append("approval.decision_maker_id must differ from author_id")
    _require_text(approval, "decided_at", "approval", errors)
    selected = _require_text(
        approval, "selected_concept_id", "approval", errors
    )
    if selected and selected not in concept_ids:
        errors.append("approval.selected_concept_id does not name a concept")
    _require_text(approval, "selection_reason", "approval", errors)
    rejected = approval.get("rejected_concept_reasons")
    if profile == "full":
        if not isinstance(rejected, dict):
            errors.append("approval.rejected_concept_reasons must be an object")
            return
        for concept_id in sorted(concept_ids - {selected}):
            reason = rejected.get(concept_id)
            if not _non_empty_string(reason) or _contains_placeholder(str(reason)):
                errors.append(
                    "approval.rejected_concept_reasons."
                    f"{concept_id} needs a concrete reason"
                )


def _validate_genericity_review(data: dict[str, Any], errors: list[str]) -> None:
    review = data.get("genericity_review")
    if not isinstance(review, dict):
        errors.append("genericity_review must be an object")
        return
    _require_text(review, "default_solution", "genericity_review", errors)
    _require_string_list(review, "detected_cliches", "genericity_review", errors)
    _require_text(
        review, "recognizable_without_brand", "genericity_review", errors
    )
    revisions = review.get("revisions")
    if not isinstance(revisions, list) or not revisions:
        errors.append("genericity_review.revisions must be a non-empty array")
        return
    for index, revision in enumerate(revisions):
        prefix = f"genericity_review.revisions[{index}]"
        if not isinstance(revision, dict):
            errors.append(f"{prefix} must be an object")
            continue
        for field in ("from", "to", "reason"):
            _require_text(revision, field, prefix, errors)


def _validate_tuning_axes(data: dict[str, Any], errors: list[str]) -> None:
    axes = data.get("tuning_axes")
    if not isinstance(axes, dict):
        errors.append("tuning_axes must be an object")
        return
    missing = TUNING_AXIS_FIELDS - set(axes)
    if missing:
        errors.append("tuning_axes missing: " + ", ".join(sorted(missing)))
    for field in TUNING_AXIS_FIELDS:
        prefix = f"tuning_axes.{field}"
        entry = axes.get(field)
        if not isinstance(entry, dict):
            if field not in missing:
                errors.append(f"{prefix} must be an object")
            continue
        value = entry.get("value")
        if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= 5:
            errors.append(f"{prefix}.value must be an integer 1..5")
        _require_text(entry, "note", prefix, errors)


def _validate_implementation_spec(
    data: dict[str, Any], selected_concept_id: str, errors: list[str]
) -> None:
    spec = data.get("implementation_spec")
    if not isinstance(spec, dict):
        errors.append("implementation_spec must be an object")
        return
    source_id = _require_text(
        spec, "source_concept_id", "implementation_spec", errors
    )
    if selected_concept_id and source_id and source_id != selected_concept_id:
        errors.append(
            "implementation_spec.source_concept_id must match "
            "approval.selected_concept_id"
        )
    _require_text(spec, "container_model", "implementation_spec", errors)
    for field, minimum in IMPLEMENTATION_SPEC_LIST_MINIMUMS.items():
        values = _require_string_list(spec, field, "implementation_spec", errors)
        if len(values) < minimum:
            errors.append(
                f"implementation_spec.{field} needs at least {minimum} entries"
            )


def _validate_density(data: dict[str, Any], errors: list[str]) -> None:
    density = data.get("density")
    if not isinstance(density, dict):
        errors.append("density must be an object")
        return
    for field in DENSITY_FIELDS:
        value = density.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            errors.append(f"density.{field} must be a positive integer")
        elif value % 4:
            errors.append(f"density.{field} must follow the 4 px rhythm")


def _validate_theme(
    data: dict[str, Any], contract_version: int, errors: list[str]
) -> None:
    theme = data.get("theme")
    if not isinstance(theme, dict):
        errors.append("theme must be an object")
        return
    for field in ("material", "light_strategy", "dark_strategy"):
        _require_text(theme, field, "theme", errors)
    if contract_version >= 4:
        for field in ("palette_source", "palette_derivation"):
            _require_text(theme, field, "theme", errors)
    cues = _require_string_list(theme, "brand_cues", "theme", errors)
    if len(cues) < 2:
        errors.append("theme.brand_cues needs at least two non-color identity cues")


def _validate_iconography(
    data: dict[str, Any], contract_version: int, errors: list[str]
) -> None:
    iconography = data.get("iconography")
    if not isinstance(iconography, dict):
        errors.append("iconography must be an object")
        return
    for field in ICONOGRAPHY_TEXT_FIELDS:
        _require_text(iconography, field, "iconography", errors)
    if contract_version >= 4:
        for field in V4_ICONOGRAPHY_TEXT_FIELDS:
            _require_text(iconography, field, "iconography", errors)
    for field in ICONOGRAPHY_SIZE_FIELDS:
        value = iconography.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            errors.append(f"iconography.{field} must be a positive integer")
        elif value % 4:
            errors.append(f"iconography.{field} must follow the 4 px rhythm")
    standard = iconography.get("standard_glyph_size")
    action_slot = iconography.get("action_slot_size")
    if (
        isinstance(standard, int)
        and not isinstance(standard, bool)
        and isinstance(action_slot, int)
        and not isinstance(action_slot, bool)
        and action_slot < standard
    ):
        errors.append(
            "iconography.action_slot_size must be at least standard_glyph_size"
        )
    prohibited = _require_string_list(
        iconography, "prohibited", "iconography", errors
    )
    if len(prohibited) < 3:
        errors.append("iconography.prohibited needs at least three concrete entries")


def _validate_component_decisions(data: dict[str, Any], errors: list[str]) -> None:
    decisions = data.get("component_decisions")
    if not isinstance(decisions, list) or not decisions:
        errors.append("component_decisions must be a non-empty array")
        return
    for index, decision in enumerate(decisions):
        prefix = f"component_decisions[{index}]"
        if not isinstance(decision, dict):
            errors.append(f"{prefix} must be an object")
            continue
        _require_text(decision, "need", prefix, errors)
        _require_text(decision, "component_id", prefix, errors)
        _require_text(decision, "decision", prefix, errors)
        _require_text(decision, "reason", prefix, errors)


def _validate_action_inventory(
    data: dict[str, Any], contract_version: int, errors: list[str]
) -> None:
    if contract_version < 4:
        return
    inventory = data.get("action_inventory")
    if not isinstance(inventory, list) or not inventory:
        errors.append("action_inventory must be a non-empty array")
        return
    seen: set[str] = set()
    for index, entry in enumerate(inventory):
        prefix = f"action_inventory[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{prefix} must be an object")
            continue
        action_id = _require_text(entry, "action_id", prefix, errors)
        for field in (
            "owner_region",
            "primary_entry",
            "primary_visible_when",
            "responsive_fallback",
            "fallback_visible_when",
        ):
            _require_text(entry, field, prefix, errors)
        folded_id = action_id.casefold()
        if folded_id in seen:
            errors.append(f"{prefix}.action_id duplicates another action id")
        elif folded_id:
            seen.add(folded_id)
        simultaneous = entry.get("simultaneously_visible")
        if not isinstance(simultaneous, bool):
            errors.append(f"{prefix}.simultaneously_visible must be a boolean")
        coexistence = _require_text(entry, "coexistence_reason", prefix, errors)
        if simultaneous and coexistence.casefold() in {"none", "n/a", "no"}:
            errors.append(
                f"{prefix}.coexistence_reason must justify simultaneous visible entries"
            )


def _validate_copy_policy(
    data: dict[str, Any], contract_version: int, errors: list[str]
) -> None:
    policy = data.get("copy_policy")
    if not isinstance(policy, dict):
        errors.append("copy_policy must be an object")
        return
    _require_text(policy, "audience", "copy_policy", errors)
    if contract_version >= 4:
        _require_text(policy, "voice", "copy_policy", errors)
        _require_text(policy, "locale_strategy", "copy_policy", errors)
    _require_string_list(policy, "allowed_technical_terms", "copy_policy", errors)
    forbidden = _require_string_list(
        policy, "forbidden_patterns", "copy_policy", errors
    )
    if contract_version >= 4:
        compression = _require_string_list(
            policy, "compression_rules", "copy_policy", errors
        )
        states = _require_string_list(
            policy, "state_vocabulary", "copy_policy", errors
        )
        if len(compression) < 3:
            errors.append("copy_policy.compression_rules needs at least three rules")
        if len(states) < 4:
            errors.append("copy_policy.state_vocabulary needs at least four states")
        if len(forbidden) < 4:
            errors.append("copy_policy.forbidden_patterns needs at least four entries")


def _validate_design_board(
    data: dict[str, Any], brief_path: Path, errors: list[str]
) -> None:
    raw_path = _require_text(data, "design_board", "", errors)
    if not raw_path:
        return
    path = resolve_local_path(raw_path, brief_path)
    if path is None or not path.is_file():
        errors.append(f"design_board file does not exist: {raw_path}")


def validate_brief(
    data: dict[str, Any],
    brief_path: Path,
    stage: str = "approved",
    *,
    require_current: bool = True,
) -> list[str]:
    errors: list[str] = []
    if stage not in ALLOWED_STAGES:
        return [f"stage must be one of {sorted(ALLOWED_STAGES)}"]
    contract_version = data.get("contract_version")
    if (
        isinstance(contract_version, bool)
        or not isinstance(contract_version, int)
        or contract_version not in ALLOWED_CONTRACT_VERSIONS
    ):
        errors.append(
            "contract_version must be one of "
            + ", ".join(str(item) for item in sorted(ALLOWED_CONTRACT_VERSIONS))
        )
        contract_version = CURRENT_CONTRACT_VERSION
    elif require_current and contract_version != CURRENT_CONTRACT_VERSION:
        errors.append(f"contract_version must be {CURRENT_CONTRACT_VERSION}")
    _require_text(data, "application", "", errors)
    _require_text(data, "author_id", "", errors)
    profile = data.get("profile")
    if profile not in ALLOWED_PROFILES:
        errors.append(f"profile must be one of {sorted(ALLOWED_PROFILES)}")
        profile = "full"
    _validate_identity(data, errors)
    _validate_references(data, errors)
    if contract_version >= 4:
        _validate_taste_context(data, errors)
        _validate_subject_vernacular(data, errors)
    fixture_id = _validate_art_direction(data, int(contract_version), errors)
    concept_ids = _validate_concepts(
        data,
        str(profile),
        brief_path,
        fixture_id,
        int(contract_version),
        errors,
    )
    _validate_approval(data, concept_ids, str(profile), stage, errors)
    if contract_version >= 4:
        _validate_genericity_review(data, errors)
        _validate_tuning_axes(data, errors)
        if stage == "approved":
            approval = data.get("approval")
            selected_concept_id = (
                str(approval.get("selected_concept_id") or "")
                if isinstance(approval, dict)
                else ""
            )
            _validate_implementation_spec(data, selected_concept_id, errors)
    _validate_density(data, errors)
    _validate_theme(data, int(contract_version), errors)
    if contract_version >= 3:
        _validate_iconography(data, int(contract_version), errors)
    _validate_action_inventory(data, int(contract_version), errors)
    _validate_component_decisions(data, errors)
    _validate_copy_policy(data, int(contract_version), errors)
    _validate_design_board(data, brief_path, errors)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("brief", type=Path)
    parser.add_argument("--stage", choices=tuple(sorted(ALLOWED_STAGES)), default="approved")
    args = parser.parse_args()
    try:
        data = load_brief(args.brief)
    except ValueError as exc:
        print(f"design brief: FAIL\n- {exc}", file=sys.stderr)
        return 1
    errors = validate_brief(data, args.brief, args.stage)
    if errors:
        print("design brief: FAIL", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    approval = data.get("approval", {})
    if args.stage == "concepts" and approval.get("status") == "rejected":
        print(
            "design brief: CONCEPTS REJECTED "
            f"({data['profile']}, {len(data['concepts'])} concepts; revision required)"
        )
    elif args.stage == "concepts" and approval.get("status") != "approved":
        print(
            "design brief: CONCEPTS READY "
            f"({data['profile']}, {len(data['concepts'])} concepts; "
            "human decision required)"
        )
    else:
        print(
            "design brief: PASS "
            f"({data['profile']}, {len(data['concepts'])} concepts, "
            f"selected {approval.get('selected_concept_id')})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
