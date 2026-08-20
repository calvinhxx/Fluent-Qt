#!/usr/bin/env python3
"""Initialize a FluentQt design brief from a bundled composition recipe."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any


SKILL_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECIPES = SKILL_ROOT / "assets" / "composition-recipes.json"
SCORE_FIELDS = (
    "workflow_fit",
    "state_visibility",
    "responsiveness",
    "component_semantics",
    "implementation_risk",
    "distinctiveness",
)
VISUAL_SCORE_FIELDS = (
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


def load_recipes(path: Path) -> dict[str, dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not isinstance(data.get("recipes"), list):
        raise ValueError("composition recipe catalog has an unsupported shape")
    return {recipe["id"]: recipe for recipe in data["recipes"]}


def build_brief(
    recipe: dict[str, Any], application: str, profile: str, author_id: str
) -> dict[str, Any]:
    concepts = []
    source_concepts = recipe["concepts"] if profile == "full" else recipe["concepts"][:1]
    for source in source_concepts:
        concept = json.loads(json.dumps(source))
        concept["scores"] = {field: 0 for field in SCORE_FIELDS}
        concept["visual_direction"] = {
            "name": "REPLACE with a memorable direction name",
            "composition_character": "REPLACE with the intended visual rhythm and hierarchy",
            "palette_strategy": "REPLACE with semantic color roles, not copied brand colors",
            "type_strategy": "REPLACE with the type contrast and reading rhythm",
            "icon_strategy": "REPLACE with how the shared icon family supports this direction",
            "subject_translation": "REPLACE with how domain vocabulary becomes structure",
            "signature_move": "REPLACE with one product-specific visual or interaction gesture",
            "aesthetic_risk": "REPLACE with the one deliberate visual risk in this concept",
            "risk_guard": "REPLACE with the readability, focus, motion, and feasibility guard",
            "restraint_rule": "REPLACE with what this direction deliberately refuses to decorate",
        }
        concept["visual_scores"] = {
            field: {
                "score": 0,
                "note": "REPLACE with a comp-specific visual judgment",
            }
            for field in VISUAL_SCORE_FIELDS
        }
        if profile == "full":
            concept["comp"] = {
                "path": f"concepts/{concept['id']}.png",
                "fidelity": "high",
                "source_kind": "raster",
                "theme": "light",
                "viewport": {"width": 1440, "height": 900},
                "content_fixture_id": "REPLACE-content-fixture-id",
            }
        concepts.append(concept)
    return {
        "contract_version": 4,
        "application": application,
        "profile": profile,
        "author_id": author_id,
        "recipe_id": recipe["id"],
        "identity": {
            "primary_object": "REPLACE with the domain object users inspect or change",
            "dominant_time_model": "REPLACE with stream, ordered run, revision, queue, or snapshot",
            "core_outcome": "REPLACE with the durable successful result",
            "hero_interaction": "REPLACE with the interaction that deserves visual priority",
            "signature_surface": "REPLACE with the surface that expresses the object and time model"
        },
        "references": {
            "aligned": {
                "name": "REPLACE with one evidence-aligned product or Gallery surface",
                "fit": "REPLACE with why its structural grammar fits",
                "transferred_traits": ["REPLACE with a transferable relationship"],
                "rejected_traits": ["REPLACE with a trait that conflicts with this product"]
            },
            "contrast": {
                "name": "REPLACE with one structurally contrastive reference",
                "fit": "REPLACE with the alternative composition it exposes",
                "transferred_traits": ["REPLACE with one useful contrast rule"],
                "rejected_traits": ["REPLACE with a trait that should not transfer"]
            }
        },
        "taste_context": {
            "user_signals": [
                "REPLACE with an explicit preference, dislike, or recorded absence"
            ],
            "project_signals": [
                "REPLACE with a brand, shipped UI, asset, token, or copy signal"
            ],
            "non_ui_reference": {
                "name": "REPLACE with a subject-world reference",
                "source": "REPLACE with a local path or authoritative source",
                "transferred_traits": [
                    "REPLACE with one material, rhythm, or interaction trait"
                ],
                "rejected_traits": [
                    "REPLACE with one literal or decorative trait that must not transfer"
                ],
            },
            "recent_patterns_to_avoid": [
                "REPLACE with a repeated shell or aesthetic from a recent output",
                "REPLACE with a second generic pattern that must not recur",
            ],
        },
        "subject_vernacular": {
            "materials": [
                "REPLACE with a domain material or surface",
                "REPLACE with a second domain material or surface",
            ],
            "artifacts": [
                "REPLACE with a user-recognized object or result",
                "REPLACE with a second meaningful artifact",
            ],
            "instruments": [
                "REPLACE with a tool users repeatedly operate",
                "REPLACE with a second domain instrument",
            ],
            "verbs": [
                "REPLACE with a core user action",
                "REPLACE with a second workflow verb",
            ],
            "tempo": "REPLACE with the dominant workflow rhythm",
        },
        "art_direction": {
            "desired_impression": [
                "REPLACE with a precise impression word",
                "REPLACE with a second impression word",
                "REPLACE with a third impression word",
            ],
            "visual_world": "REPLACE with a domain-specific visual world, not a UI style label",
            "signature_element": "REPLACE with the element recognizable without logo or accent",
            "typography_voice": "REPLACE with the intended hierarchy, weight, and reading cadence",
            "palette_strategy": "REPLACE with semantic Light and Dark color relationships",
            "motion_voice": "REPLACE with the motion character and where motion must stay quiet",
            "aesthetic_risk": {
                "move": "REPLACE with one memorable, subject-grounded visual risk",
                "evidence": "REPLACE with why the product justifies this risk",
                "quiet_zone": "REPLACE with what stays visually restrained around it",
                "usability_guard": "REPLACE with readability, focus, motion, and localization limits",
                "fallback": "REPLACE with the safe alternative if evidence rejects the risk",
            },
            "anti_goals": [
                "REPLACE with a generic visual pattern this product must avoid",
                "REPLACE with a second concrete anti-goal",
                "REPLACE with a third concrete anti-goal",
            ],
            "content_fixture": {
                "id": "REPLACE-content-fixture-id",
                "source": "REPLACE with repository evidence or a documented synthetic fixture",
                "scenario": "REPLACE with one representative end-to-end product moment",
                "required_strings": [
                    "REPLACE with real primary copy or data",
                    "REPLACE with real secondary copy or data",
                    "REPLACE with a realistic status, error, or outcome",
                ],
                "required_states": [
                    "REPLACE with the normal state visible in every concept",
                    "REPLACE with one meaningful active, error, or completion state",
                ],
            },
        },
        "genericity_review": {
            "default_solution": "REPLACE with the untuned shell or aesthetic an agent would likely produce",
            "detected_cliches": [
                "REPLACE with a generic choice found during concept critique"
            ],
            "revisions": [
                {
                    "from": "REPLACE with the generic choice",
                    "to": "REPLACE with the subject-specific revision",
                    "reason": "REPLACE with why the revision better fits the product",
                }
            ],
            "recognizable_without_brand": "REPLACE with why the signature survives without logo or accent",
        },
        "tuning_axes": {
            axis: {
                "value": 3,
                "note": "REPLACE with what this value means for the selected direction",
            }
            for axis in (
                "density",
                "contrast",
                "material_depth",
                "corner_softness",
                "motion_energy",
                "visual_expressiveness",
            )
        },
        "concepts": concepts,
        "approval": {
            "status": "pending",
            "decision_maker_kind": "human",
            "decision_maker_id": "",
            "decided_at": "",
            "selected_concept_id": None,
            "selection_reason": "",
            "rejected_concept_reasons": {},
        },
        "density": recipe["default_density"],
        "theme": {
            "material": "mica",
            "light_strategy": "REPLACE with semantic Light surface and contrast decisions",
            "dark_strategy": "REPLACE with semantic Dark surface and contrast decisions",
            "palette_source": "REPLACE with the authoritative icon, brand asset, token set, or neutral baseline",
            "palette_derivation": "REPLACE with how identity anchors become restrained Light/Dark semantic roles",
            "brand_cues": [
                "REPLACE with a non-color shape, rhythm, or motion cue",
                "REPLACE with a product-specific information or interaction cue"
            ]
        },
        "iconography": {
            "family": "REPLACE with one coherent icon family or project-owned set",
            "provenance": "REPLACE with the source path, package, and license",
            "source_strategy": "REPLACE with reuse, repair, extend, or generate and why",
            "style": "REPLACE with the shared outline, corner, and optical character",
            "base_grid": 20,
            "compact_glyph_size": 16,
            "standard_glyph_size": 20,
            "action_slot_size": 28,
            "stroke_policy": "REPLACE with one normalized stroke policy",
            "filled_policy": "REPLACE with when filled icons are allowed",
            "color_policy": "REPLACE with semantic foreground and status-color rules",
            "icon_only_policy": "REPLACE with accessible name, tooltip, target, and state rules",
            "application_icon_strategy": "REPLACE with platform tile, safe area, and multi-resolution packaging",
            "in_app_mark_strategy": "REPLACE with the separate transparent title and compact mark",
            "small_size_validation": "REPLACE with 16 px, 32 px, launcher, and Light/Dark evidence",
            "palette_seed": "REPLACE with the approved chromatic anchor, neutral temperature, and excluded pixels",
            "prohibited": [
                "emoji used as an action icon",
                "mixed icon packs in one hierarchy",
                "arbitrary Unicode glyphs or scaled raster icons",
            ],
        },
        "copy_policy": {
            "audience": "REPLACE with the intended user and expertise level",
            "voice": "REPLACE with a short direct product voice",
            "locale_strategy": "REPLACE with terminology, wrapping, and localization rules",
            "allowed_technical_terms": ["REPLACE with a term users already understand"],
            "compression_rules": [
                "REPLACE with how labels name user objects or actions",
                "REPLACE with what explanatory copy should be removed",
                "REPLACE with the maximum useful empty/error/helper structure"
            ],
            "state_vocabulary": [
                "REPLACE with the not-started state",
                "REPLACE with the active state",
                "REPLACE with the successful state",
                "REPLACE with the failed state"
            ],
            "forbidden_patterns": [
                "raw protocol method names",
                "unexplained identifiers and filesystem paths",
                "assistant self-narration such as I will or I can",
                "unsupported marketing adjectives and duplicated help"
            ]
        },
        "action_inventory": [
            {
                "action_id": "REPLACE with a stable semantic action id",
                "owner_region": "REPLACE with the region that owns the decision",
                "primary_entry": "REPLACE with the normal-layout control or gesture",
                "primary_visible_when": "REPLACE with its exact visibility condition",
                "responsive_fallback": "REPLACE with the narrow-layout entry or none",
                "fallback_visible_when": "REPLACE with a mutually exclusive condition or none",
                "simultaneously_visible": False,
                "coexistence_reason": "none"
            }
        ],
        "component_decisions": [
            {
                "need": "REPLACE with one required behavior",
                "component_id": concepts[0]["preferred_components"][0],
                "decision": "must-use",
                "reason": "REPLACE with the behavioral and density reason"
            }
        ],
        "implementation_spec": {
            "source_concept_id": "REPLACE after human approval",
            "container_model": "REPLACE with the accepted surface and region hierarchy",
            "semantic_tokens": [
                "REPLACE with the principal Light/Dark palette relationship",
                "REPLACE with the spacing/radius/material relationship",
            ],
            "typography_roles": [
                "REPLACE with content typography",
                "REPLACE with chrome or data typography",
            ],
            "component_families": [
                "REPLACE with a shared component family and variants"
            ],
            "state_grammar": [
                "REPLACE with how one important state is expressed",
                "REPLACE with a second state relationship",
            ],
            "motion_cues": [
                "REPLACE with one purposeful motion and reduced-motion behavior"
            ],
            "responsive_rules": [
                "REPLACE with the normal-to-narrow transformation",
                "REPLACE with the minimum-width preservation rule",
            ],
            "locked_decisions": [
                "REPLACE with a hierarchy or container decision that may not drift",
                "REPLACE with a typography, palette, density, or signature decision",
            ],
            "allowed_adaptations": [
                "REPLACE with an adaptation allowed for native behavior or localization"
            ],
            "known_risks": [
                "REPLACE with an implementation or platform risk to recheck"
            ],
            "comparison_regions": [
                "REPLACE with the full-window comparison",
                "REPLACE with one high-risk detail or state comparison",
            ],
        },
        "planned_states": recipe["required_states"],
        "design_board": "design-board.svg"
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--application")
    parser.add_argument("--recipe", default="agent-run")
    parser.add_argument("--profile", choices=("lite", "full"), default="full")
    parser.add_argument("--author-id")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--recipes", type=Path, default=DEFAULT_RECIPES)
    parser.add_argument("--list-recipes", action="store_true")
    parser.add_argument("--force", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        recipes = load_recipes(args.recipes)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: cannot load composition recipes: {exc}", file=sys.stderr)
        return 1
    if args.list_recipes:
        for recipe in recipes.values():
            print(f"{recipe['id']}: {recipe['title']}")
        return 0
    missing = [
        option
        for option, value in (
            ("--application", args.application),
            ("--author-id", args.author_id),
            ("--output", args.output),
        )
        if value is None
    ]
    if missing:
        print(
            "error: the following arguments are required unless --list-recipes is used: "
            + ", ".join(missing),
            file=sys.stderr,
        )
        return 1
    recipe = recipes.get(args.recipe)
    if recipe is None:
        print(
            f"error: unknown recipe {args.recipe!r}; choose from "
            + ", ".join(sorted(recipes)),
            file=sys.stderr,
        )
        return 1
    output = args.output.resolve()
    if output.exists() and not args.force:
        print(f"error: output exists (use --force): {output}", file=sys.stderr)
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            build_brief(recipe, args.application, args.profile, args.author_id),
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
