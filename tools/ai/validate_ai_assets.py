#!/usr/bin/env python3

"""Validate FluentQt AI docs, schemas, catalog, evals, queries, and Skill."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Iterable
import xml.etree.ElementTree as ET
import zipfile


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from generate_ai_catalog import check_catalog, generate_catalog
from evaluate_ai_catalog import evaluate_catalog
from package_fluentqt_skill import REQUIRED_SKILL_FILES
from query_ai_catalog import component_by_id, pattern_by_id, search_components


SKILL_SOURCE_ROOT = ".agents/skills/build-fluentqt-gui"
REQUIRED_PATHS = (
    "docs/ai/README.md",
    "docs/ai/add-gui-to-project.md",
    "docs/ai/evals/scenarios.json",
    "docs/ai/evals/scenarios.schema.json",
    "docs/ai/guidance.json",
    "docs/ai/fluentqt-ai-catalog.schema.json",
    "docs/ai/project-analysis.schema.json",
    "docs/ai/generated/fluentqt-ai-catalog.json",
    *(f"{SKILL_SOURCE_ROOT}/{relative}" for relative in REQUIRED_SKILL_FILES),
    "tools/ai/package_fluentqt_skill.py",
    "llms.txt",
)


def _resolve_local_ref(root_schema: dict[str, object], reference: str) -> object:
    if not reference.startswith("#/"):
        raise AssertionError(f"Only local JSON Schema references are supported: {reference}")
    value: object = root_schema
    for token in reference[2:].split("/"):
        token = token.replace("~1", "/").replace("~0", "~")
        if not isinstance(value, dict) or token not in value:
            raise AssertionError(f"Unresolvable JSON Schema reference: {reference}")
        value = value[token]
    return value


def _matches_json_type(value: object, expected_type: str) -> bool:
    return {
        "array": isinstance(value, list),
        "boolean": isinstance(value, bool),
        "integer": isinstance(value, int) and not isinstance(value, bool),
        "null": value is None,
        "number": (
            isinstance(value, (int, float)) and not isinstance(value, bool)
        ),
        "object": isinstance(value, dict),
        "string": isinstance(value, str),
    }.get(expected_type, False)


def _validate_json_instance(
    value: object,
    schema: dict[str, object],
    root_schema: dict[str, object],
    path: str = "$",
) -> None:
    """Validate the JSON Schema subset used by the committed AI contracts."""
    reference = schema.get("$ref")
    if reference is not None:
        resolved = _resolve_local_ref(root_schema, str(reference))
        if not isinstance(resolved, dict):
            raise AssertionError(f"JSON Schema reference is not an object: {reference}")
        _validate_json_instance(value, resolved, root_schema, path)

    for nested_schema in schema.get("allOf", []):
        _validate_json_instance(value, nested_schema, root_schema, path)

    if "const" in schema and value != schema["const"]:
        raise AssertionError(f"{path} must equal {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        raise AssertionError(f"{path} is not one of {schema['enum']!r}")

    expected = schema.get("type")
    if expected is not None:
        expected_types = expected if isinstance(expected, list) else [expected]
        if not any(_matches_json_type(value, item) for item in expected_types):
            raise AssertionError(f"{path} must have JSON type {expected!r}")

    if isinstance(value, str) and len(value) < schema.get("minLength", 0):
        raise AssertionError(f"{path} is shorter than minLength")
    if (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and "minimum" in schema
        and value < schema["minimum"]
    ):
        raise AssertionError(f"{path} is smaller than minimum")

    if isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            raise AssertionError(f"{path} has fewer than minItems entries")
        if schema.get("uniqueItems"):
            serialized = [
                json.dumps(item, ensure_ascii=False, sort_keys=True) for item in value
            ]
            if len(serialized) != len(set(serialized)):
                raise AssertionError(f"{path} contains duplicate entries")
        item_schema = schema.get("items")
        if isinstance(item_schema, dict):
            for index, item in enumerate(value):
                _validate_json_instance(
                    item, item_schema, root_schema, f"{path}[{index}]"
                )

    if isinstance(value, dict):
        required = schema.get("required", [])
        for key in required:
            if key not in value:
                raise AssertionError(f"{path} is missing required property {key!r}")
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            unknown = set(value) - set(properties)
            if unknown:
                raise AssertionError(
                    f"{path} has unsupported properties: {', '.join(sorted(unknown))}"
                )
        for key, nested_value in value.items():
            nested_schema = properties.get(key)
            if isinstance(nested_schema, dict):
                _validate_json_instance(
                    nested_value, nested_schema, root_schema, f"{path}.{key}"
                )


def _project_analysis_fixture() -> dict[str, object]:
    return {
        "schema_version": 1,
        "project": {
            "name": "example",
            "languages": ["C++"],
            "build_systems": ["CMake"],
            "entry_points": [],
            "existing_interfaces": ["none"],
        },
        "architecture": {
            "domain_modules": [],
            "long_running_operations": [],
            "persistence": [],
            "external_io": [],
        },
        "gui_goal": {
            "user_outcomes": ["Complete one useful workflow"],
            "work_areas": [],
            "actions": [],
            "data_views": [],
        },
        "integration": {
            "chosen_pattern": "greenfield",
            "evidence": ["No existing application or interaction layer"],
            "rejected_alternatives": [],
        },
        "constraints": {
            "preserve_interfaces": [],
            "supported_platforms": ["desktop"],
            "minimum_qt": "6.2",
            "allow_new_runtime": False,
        },
        "validation": {
            "build_commands": ["cmake --build --preset host --parallel"],
            "test_commands": [],
            "visual_requirements": [],
        },
    }


def _validate_skill(project_root: Path) -> None:
    skill_path = project_root / ".agents/skills/build-fluentqt-gui/SKILL.md"
    contents = skill_path.read_text(encoding="utf-8")
    if "TODO" in contents:
        raise AssertionError("FluentQt GUI Skill still contains TODO placeholders")
    if not contents.startswith("---\nname: build-fluentqt-gui\ndescription:"):
        raise AssertionError("FluentQt GUI Skill has invalid frontmatter")
    for required in (
        "assets/benchmarks/agent-run-workspace.json",
        "assets/composition-recipes.json",
        "assets/fluentqt-ai-catalog.json",
        "assets/project-structure-templates.json",
        "references/art-direction.md",
        "references/design-intelligence.md",
        "references/iconography.md",
        "references/product-copy.md",
        "references/component-selection.md",
        "references/experience-differentiation.md",
        "references/performance-lifecycle.md",
        "references/premium-shell.md",
        "references/product-reference-patterns.md",
        "references/project-architecture.md",
        "references/signature-surface.md",
        "references/theme-system.md",
        "references/visual-evidence-contract.md",
        "references/visual-refinement.md",
        "scripts/init_design_brief.py",
        "scripts/init_project_structure.py",
        "scripts/init_visual_evidence.py",
        "scripts/query_catalog.py",
        "scripts/render_design_board.py",
        "scripts/render_visual_review.py",
        "scripts/validate_design_brief.py",
        "scripts/validate_project_structure.py",
        "scripts/validate_visual_evidence.py",
    ):
        if required not in contents:
            raise AssertionError(f"FluentQt GUI Skill does not route to {required}")
    for forbidden in (".claude/skills", "fluentqt_root.py", "../../../docs"):
        if forbidden in contents:
            raise AssertionError(
                f"Installable FluentQt GUI Skill contains repository coupling: {forbidden}"
            )

    reference_requirements = {
        "references/art-direction.md": (
            "Define the visual world",
            "Use representative product content",
            "Produce three high-fidelity comps",
            "Require a human decision",
            "Art-direction acceptance gate",
        ),
        "references/design-intelligence.md": (
            "Ground the design in the product's world",
            "Spend one controlled aesthetic risk",
            "Critique genericity before human review",
            "Extract an implementation design system",
            "Design-intelligence gate",
        ),
        "references/iconography.md": (
            "Choose one source and record provenance",
            "Decide when to reuse, repair, or generate",
            "Define grid and optical sizes",
            "Optimize the application identity assets",
            "Derive a palette seed from the approved identity",
            "Make icon-only actions accessible",
            "Implement with FluentQt and Qt",
            "Iconography acceptance gate",
        ),
        "references/product-copy.md": (
            "Establish the product register",
            "Rewrite by interface job",
            "Remove assistant narration",
            "Run the copy audit",
            "Product-copy acceptance gate",
        ),
        "references/experience-differentiation.md": (
            "Define the product signature",
            "Generate structurally distinct concepts",
            "Scan semantic component opportunities",
            "Differentiation acceptance gate",
        ),
        "references/product-reference-patterns.md": (
            "Reference-synthesis protocol",
            "Fast selection matrix",
            "Acceptance gate",
        ),
        "references/project-architecture.md": (
            "Select the smallest honest structure",
            "Responsibility contracts",
            "Architecture manifest",
            "Refactor an existing God window safely",
        ),
        "references/performance-lifecycle.md": (
            "Classify the data before choosing a viewport",
            "Preserve item-view virtualization",
            "Choose transient lifetime deliberately",
            "Acceptance gate",
        ),
        "references/premium-shell.md": (
            "Default window material",
            "Reveal the window material",
            "Reject these first-render patterns",
            "Premium-shell acceptance gate",
        ),
        "references/signature-surface.md": (
            "Finish the product object, not the shell",
            "Conversation and run timeline",
            "Composer and command dock",
            "Reject these unfinished surfaces",
            "Signature-surface acceptance gate",
        ),
        "references/theme-system.md": (
            "ThemeRegistry::defaultSnapshot()",
            "apply_user_theme",
            "Audit raw Qt widgets",
            "Install window material with the theme",
            "Theme acceptance gate",
        ),
        "references/component-selection.md": (
            "Produce a decision table",
            "Raw Qt exception rule",
            "Component acceptance gate",
        ),
        "references/visual-refinement.md": (
            "Gallery-equivalent quality bar",
            "Start with window material, then density",
            "normal window, Light",
            "minimum supported window",
            "Record actionable findings",
            "Visual acceptance gate",
        ),
        "references/visual-evidence-contract.md": (
            "Cover mandatory states",
            "Verify dynamic convergence",
            "picture-in-picture crops",
            "contract_version",
            "window_backdrop",
            "signature_finish",
            "Require independent visual review",
            "Visual acceptance requires",
        ),
    }
    for relative_path, anchors in reference_requirements.items():
        reference = (skill_path.parent / relative_path).read_text(encoding="utf-8")
        for anchor in anchors:
            if anchor not in reference:
                raise AssertionError(
                    f"FluentQt GUI Skill reference {relative_path} is missing {anchor!r}"
                )
    metadata = (
        project_root
        / ".agents/skills/build-fluentqt-gui/agents/openai.yaml"
    ).read_text(encoding="utf-8")
    if "Use $build-fluentqt-gui" not in metadata:
        raise AssertionError("Skill UI metadata has a stale default prompt")

    workflow = (project_root / ".github/workflows/ci-cpp.yml").read_text(
        encoding="utf-8"
    )
    for relative_path in REQUIRED_SKILL_FILES:
        source_path = f"{SKILL_SOURCE_ROOT}/{relative_path}"
        if source_path not in workflow:
            raise AssertionError(
                f"C++ source-package gate does not require {source_path}"
            )

    recipes = json.loads(
        (skill_path.parent / "assets/composition-recipes.json").read_text(
            encoding="utf-8"
        )
    )
    if recipes.get("schema_version") != 1 or not isinstance(
        recipes.get("recipes"), list
    ):
        raise AssertionError("Composition recipe catalog has an invalid shape")
    expected_recipe_ids = {
        "agent-run",
        "data-console",
        "document-workbench",
        "focused-utility",
    }
    recipe_ids = {recipe.get("id") for recipe in recipes["recipes"]}
    if recipe_ids != expected_recipe_ids:
        raise AssertionError("Composition recipe catalog has stale recipe ids")
    for recipe in recipes["recipes"]:
        concepts = recipe.get("concepts")
        topologies = {
            concept.get("topology")
            for concept in concepts
            if isinstance(concept, dict)
        } if isinstance(concepts, list) else set()
        if not isinstance(concepts, list) or len(concepts) != 3:
            raise AssertionError(
                f"Composition recipe {recipe.get('id')} must define three concepts"
            )
        if len(topologies) != len(concepts) or None in topologies:
            raise AssertionError(
                f"Composition recipe {recipe.get('id')} repeats a topology"
            )

    benchmark = json.loads(
        (
            skill_path.parent
            / "assets/benchmarks/agent-run-workspace.json"
        ).read_text(encoding="utf-8")
    )
    if (
        benchmark.get("schema_version") != 1
        or benchmark.get("id") != "agent-run-workspace"
        or len(benchmark.get("cross_agent_runs", [])) < 3
    ):
        raise AssertionError("Agent-run visual benchmark has an invalid shape")
    expected_review_dimensions = {
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
    review_dimensions = benchmark.get("blind_review_dimensions", [])
    if (
        not isinstance(review_dimensions, list)
        or len(review_dimensions) != len(expected_review_dimensions)
        or set(review_dimensions) != expected_review_dimensions
    ):
        raise AssertionError(
            "Agent-run visual benchmark must use the nine-dimension scorecard"
        )
    design_checks = benchmark.get("design_intelligence_checks", [])
    if (
        not isinstance(design_checks, list)
        or len(design_checks) < 5
        or not all(isinstance(item, str) and item.strip() for item in design_checks)
    ):
        raise AssertionError(
            "Agent-run visual benchmark must define design-intelligence checks"
        )
    if len(benchmark.get("required_product_evidence", [])) < 12:
        raise AssertionError(
            "Agent-run visual benchmark is missing product-specific design evidence"
        )


def _validate_visual_evidence_validator(project_root: Path) -> None:
    script = (
        project_root
        / ".agents/skills/build-fluentqt-gui/scripts/validate_visual_evidence.py"
    )
    with tempfile.TemporaryDirectory(prefix="fluentqt-visual-evidence-") as temp:
        root = Path(temp)
        (root / "reviewed-app").mkdir()
        (root / "capture.png").write_bytes(b"visual-evidence-fixture")
        manifest_path = root / "visual-evidence.json"
        fixture = {
            "contract_version": 2,
            "application": "validator-fixture",
            "reviewed_build": "reviewed-app",
            "platform": "test platform, scale 1x",
            "profile": "lite",
            "window_backdrop": "mica",
            "surface_fill_policy": "reveal-material",
            "signature_finish": "product",
            "chrome_on_material": "quiet",
            "sparse_canvas_treatment": "composed",
            "primary_input_treatment": "integrated-dock",
            "visible_copy_register": "user-facing",
            "states": [
                {
                    "id": state_id,
                    "status": "pass",
                    "evidence": ["capture.png"],
                }
                for state_id in (
                    "normal-light",
                    "normal-dark",
                    "narrow",
                    "minimum",
                    "long-localized-content",
                    "selected-focus-disabled",
                )
            ],
            "regions": [
                {
                    "id": region_id,
                    "status": "pass",
                    "evidence": ["capture.png"],
                }
                for region_id in (
                    "titlebar",
                    "primary-viewport",
                    "footer-or-primary-input",
                )
            ],
            "dynamic_checks": [],
            "measurements": [
                {
                    "id": "fixture-inset",
                    "expected": "12",
                    "actual": "12",
                    "status": "pass",
                    "evidence": "capture.png",
                }
            ],
            "issues": [],
        }

        def run_validator() -> subprocess.CompletedProcess[str]:
            manifest_path.write_text(
                json.dumps(fixture, ensure_ascii=False), encoding="utf-8"
            )
            return subprocess.run(
                [sys.executable, str(script), str(manifest_path)],
                check=False,
                capture_output=True,
                text=True,
            )

        passing = run_validator()
        if passing.returncode != 0 or "visual evidence: PASS (lite" not in passing.stdout:
            raise AssertionError(
                "Visual evidence validator rejects a complete lite fixture: "
                + passing.stderr.strip()
            )

        fixture["window_backdrop"] = "host-owned"
        fixture["window_backdrop_reason"] = "Embedded in the IDE-owned panel"
        fixture["surface_fill_policy"] = "inherit-host"
        fixture["surface_fill_reason"] = "The host owns the panel surface"
        fixture["primary_input_treatment"] = "none"
        valid_host_owned = run_validator()
        if valid_host_owned.returncode != 0:
            raise AssertionError(
                "Visual evidence validator rejects a valid host-owned surface: "
                + valid_host_owned.stderr.strip()
            )

        fixture["window_backdrop"] = "mica"
        fixture["surface_fill_policy"] = "reveal-material"
        fixture["primary_input_treatment"] = "integrated-dock"
        fixture.pop("window_backdrop_reason", None)
        fixture.pop("surface_fill_reason", None)

        legacy_fixture = json.loads(json.dumps(fixture))
        for field in (
            "contract_version",
            "window_backdrop",
            "surface_fill_policy",
            "signature_finish",
            "chrome_on_material",
            "sparse_canvas_treatment",
            "primary_input_treatment",
            "visible_copy_register",
        ):
            legacy_fixture.pop(field, None)
        manifest_path.write_text(
            json.dumps(legacy_fixture, ensure_ascii=False), encoding="utf-8"
        )
        legacy = subprocess.run(
            [sys.executable, str(script), str(manifest_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if legacy.returncode != 0 or "legacy contract v1" not in legacy.stderr:
            raise AssertionError(
                "Visual evidence validator does not preserve legacy v1 manifests"
            )

        fixture["states"][0]["evidence"] = ["missing.png"]
        missing_file = run_validator()
        if missing_file.returncode == 0 or "does not exist" not in missing_file.stderr:
            raise AssertionError(
                "Visual evidence validator accepts an invented evidence path"
            )

        fixture["states"][0]["evidence"] = ["capture.png"]
        fixture["profile"] = "full"
        incomplete_full = run_validator()
        if (
            incomplete_full.returncode == 0
            or "missing required ids" not in incomplete_full.stderr
        ):
            raise AssertionError(
                "Visual evidence validator accepts lite coverage as full"
            )

        fixture["profile"] = "lite"
        fixture["window_backdrop"] = "solid"
        fixture.pop("window_backdrop_reason", None)
        solid_without_reason = run_validator()
        if (
            solid_without_reason.returncode == 0
            or "window_backdrop_reason" not in solid_without_reason.stderr
        ):
            raise AssertionError(
                "Visual evidence validator accepts Solid backdrop without a reason"
            )

        fixture["window_backdrop"] = "host-owned"
        fixture.pop("window_backdrop_reason", None)
        host_without_reason = run_validator()
        if (
            host_without_reason.returncode == 0
            or "window_backdrop_reason" not in host_without_reason.stderr
        ):
            raise AssertionError(
                "Visual evidence validator accepts host-owned backdrop without a reason"
            )

        fixture["window_backdrop_reason"] = "Embedded in the IDE-owned panel"
        host_with_wrong_fill = run_validator()
        if (
            host_with_wrong_fill.returncode == 0
            or "requires surface_fill_policy" not in host_with_wrong_fill.stderr
        ):
            raise AssertionError(
                "Visual evidence validator accepts inconsistent host-owned material"
            )

        fixture["window_backdrop"] = "mica"
        fixture.pop("window_backdrop_reason", None)
        fixture["signature_finish"] = "wireframe"
        wireframe = run_validator()
        if wireframe.returncode == 0 or "wireframe" not in wireframe.stderr:
            raise AssertionError(
                "Visual evidence validator accepts signature_finish wireframe"
            )

        fixture["signature_finish"] = "product"
        fixture["chrome_on_material"] = "filled-stickers"
        stickers = run_validator()
        if stickers.returncode == 0 or "filled-stickers" not in stickers.stderr:
            raise AssertionError(
                "Visual evidence validator accepts chrome_on_material filled-stickers"
            )

        fixture["chrome_on_material"] = "quiet"
        fixture["primary_input_treatment"] = "independent-card"
        fixture.pop("primary_input_reason", None)
        card_without_reason = run_validator()
        if (
            card_without_reason.returncode == 0
            or "primary_input_reason" not in card_without_reason.stderr
        ):
            raise AssertionError(
                "Visual evidence validator accepts independent-card without a reason"
            )

        fixture["primary_input_reason"] = "The editor is an independent document"
        valid_independent_card = run_validator()
        if valid_independent_card.returncode != 0:
            raise AssertionError(
                "Visual evidence validator rejects a justified independent card: "
                + valid_independent_card.stderr.strip()
            )


def _validate_design_and_review_tooling(project_root: Path) -> None:
    skill_root = project_root / ".agents/skills/build-fluentqt-gui"
    init_brief = skill_root / "scripts/init_design_brief.py"
    render_design = skill_root / "scripts/render_design_board.py"
    validate_brief = skill_root / "scripts/validate_design_brief.py"
    init_evidence = skill_root / "scripts/init_visual_evidence.py"
    render_review = skill_root / "scripts/render_visual_review.py"
    validate_evidence = skill_root / "scripts/validate_visual_evidence.py"

    with tempfile.TemporaryDirectory(prefix="fluentqt-design-review-") as temp:
        root = Path(temp)
        brief_path = root / "design-brief.json"
        initialized = subprocess.run(
            [
                sys.executable,
                str(init_brief),
                "--application",
                "validator-fixture",
                "--recipe",
                "agent-run",
                "--profile",
                "full",
                "--author-id",
                "implementation-agent",
                "--output",
                str(brief_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if initialized.returncode != 0 or not brief_path.is_file():
            raise AssertionError(
                "Design brief initializer failed: "
                + (initialized.stderr or initialized.stdout).strip()
            )

        brief = json.loads(brief_path.read_text(encoding="utf-8"))
        brief["identity"] = {
            "primary_object": "ordered agent run",
            "dominant_time_model": "streaming event timeline",
            "core_outcome": "reviewed repository change",
            "hero_interaction": "inspect and continue the active run",
            "signature_surface": "mixed-height run transcript",
        }
        brief["references"] = {
            "aligned": {
                "name": "Run-first coding agent",
                "fit": "Its ordered transcript matches the primary object.",
                "transferred_traits": ["Mixed-height transcript grammar"],
                "rejected_traits": ["Brand and screenshot geometry"],
            },
            "contrast": {
                "name": "Artifact workbench",
                "fit": "It tests whether a durable result should dominate.",
                "transferred_traits": ["Transient run details"],
                "rejected_traits": ["Permanent editor when no artifact exists"],
            },
        }
        brief["taste_context"] = {
            "user_signals": [
                "Prefer compact professional desktop tools over card dashboards"
            ],
            "project_signals": [
                "Ordered process events and repository outcomes define the product"
            ],
            "non_ui_reference": {
                "name": "Lab run notebook",
                "source": "Documented synthetic subject reference for validation",
                "transferred_traits": [
                    "Ordered trace, precise annotations, and a calm reading rhythm"
                ],
                "rejected_traits": [
                    "Literal paper texture or decorative scientific instruments"
                ],
            },
            "recent_patterns_to_avoid": [
                "Three-column chat shell with a permanent inspector",
                "Uniform rounded cards around every transcript event",
            ],
        }
        brief["subject_vernacular"] = {
            "materials": ["ordered trace", "reviewed patch"],
            "artifacts": ["agent run", "repository change"],
            "instruments": ["composer", "event disclosure"],
            "verbs": ["inspect", "continue"],
            "tempo": "Live operation followed by quiet review",
        }
        brief["art_direction"] = {
            "desired_impression": ["precise", "calm", "capable"],
            "visual_world": (
                "A live technical notebook whose active run leaves a clear trace."
            ),
            "signature_element": (
                "A compact run pulse aligned with grouped event disclosures."
            ),
            "typography_voice": (
                "Compact interface labels support a highly readable transcript rhythm."
            ),
            "palette_strategy": (
                "Quiet neutral material uses semantic cyan, amber, and red status roles."
            ),
            "motion_voice": (
                "Short directional transitions mark state changes while reading stays still."
            ),
            "aesthetic_risk": {
                "move": "A continuous pulse rail turns the ordered run into the visual spine",
                "evidence": "The runtime exposes ordered events with meaningful state changes",
                "quiet_zone": "Transcript text and secondary chrome remain flat and calm",
                "usability_guard": "The rail never replaces text, focus, or semantic status",
                "fallback": "Use a static accent rule if motion or performance evidence fails",
            },
            "anti_goals": [
                "Uniform rounded cards for every event",
                "Permanent inspector without a selected object",
                "Oversized marketing headings inside the work surface",
            ],
            "content_fixture": {
                "id": "agent-run-review-v1",
                "source": "Structured process events in the target protocol",
                "scenario": "Inspect an active repository change and its completed result",
                "required_strings": [
                    "Update the navigation tests",
                    "Running focused test_navigation_view",
                    "3 tests passed",
                ],
                "required_states": ["active tool run", "completed change"],
            },
        }
        brief["iconography"] = {
            "family": "Fluent-aligned project vector set",
            "provenance": "Bundled test fixture with project-owned validation assets",
            "source_strategy": "Extend the licensed project set; generate no replacement mark",
            "style": "Rounded outline geometry with consistent optical weight",
            "base_grid": 20,
            "compact_glyph_size": 16,
            "standard_glyph_size": 20,
            "action_slot_size": 28,
            "stroke_policy": "Normalize routine actions to one outline weight",
            "filled_policy": "Use filled variants only for selected or active state",
            "color_policy": "Inherit semantic foreground; reserve status color for status",
            "icon_only_policy": "Require an accessible name, tooltip, focus cue, and states",
            "application_icon_strategy": "Use an opaque rounded platform tile with a protected safe area",
            "in_app_mark_strategy": "Use the same approved mark without tile or shadow in compact chrome",
            "small_size_validation": "Review 16 px, 32 px, launcher, title bar, Light, and Dark output",
            "palette_seed": "Use the approved cyan anchor; exclude tile neutral and edge antialiasing",
            "prohibited": [
                "emoji controls",
                "mixed icon packs",
                "arbitrary Unicode or raster glyphs",
            ],
        }
        visual_directions = (
            {
                "name": "Technical notebook",
                "composition_character": "A calm vertical reading rhythm with grouped traces.",
                "palette_strategy": "Neutral paper-like layers with cyan active state.",
                "type_strategy": "Readable body-first hierarchy with compact metadata.",
                "icon_strategy": "Compact outline actions align to transcript metadata.",
                "subject_translation": "Notebook trace becomes grouped run chronology.",
                "signature_move": "A pulse rail binds run progress to transcript groups.",
                "aesthetic_risk": "Use the pulse rail as the only expressive device.",
                "risk_guard": "Keep text stationary and expose status without color alone.",
                "restraint_rule": "Only permission and error states gain bounded surfaces.",
            },
            {
                "name": "Operational stage",
                "composition_character": "A decisive stage-and-output split with hard alignment.",
                "palette_strategy": "Cool graphite layers with amber stage emphasis.",
                "type_strategy": "Condensed labels contrast with larger active output.",
                "icon_strategy": "State icons use filled active variants on the stage rail.",
                "subject_translation": "Process stages become a controlled operating sequence.",
                "signature_move": "The active stage opens into a focused output aperture.",
                "aesthetic_risk": "Use a focused stage aperture as the dominant transition.",
                "risk_guard": "Keep all inactive stages readable and keyboard reachable.",
                "restraint_rule": "No ornamental cards or secondary gradients.",
            },
            {
                "name": "Artifact atelier",
                "composition_character": "A spacious result canvas with transient run context.",
                "palette_strategy": "Warm neutral canvas with blue review semantics.",
                "type_strategy": "Document typography leads; run context stays compact.",
                "icon_strategy": "Quiet outline tools defer to document annotations.",
                "subject_translation": "Repository change becomes a durable reviewed artifact.",
                "signature_move": "Change annotations connect the artifact to its generating run.",
                "aesthetic_risk": "Let the generated artifact visually outrank the live run.",
                "risk_guard": "Preserve run recovery and errors in a stable adjacent surface.",
                "restraint_rule": "Run detail never competes with the durable result.",
            },
        )
        comp_root = root / "concepts"
        comp_root.mkdir()
        for index, concept in enumerate(brief["concepts"]):
            concept["scores"] = {
                "workflow_fit": 4,
                "state_visibility": 4,
                "responsiveness": 4,
                "component_semantics": 4,
                "implementation_risk": 3,
                "distinctiveness": 4,
            }
            concept["visual_scores"] = {
                score_id: {
                    "score": 4,
                    "note": (
                        f"{concept['title']} resolves {score_id.replace('_', ' ')} "
                        "in its high-fidelity comp."
                    ),
                }
                for score_id in (
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
            }
            concept["visual_direction"] = visual_directions[index]
            concept["comp"]["content_fixture_id"] = "agent-run-review-v1"
            (comp_root / f"{concept['id']}.png").write_bytes(
                f"high-fidelity-{concept['id']}".encode("utf-8")
            )
        brief["approval"] = {
            "status": "pending",
            "decision_maker_kind": "human",
            "decision_maker_id": "",
            "decided_at": "",
            "selected_concept_id": None,
            "selection_reason": "",
            "rejected_concept_reasons": {},
        }
        brief["genericity_review"] = {
            "default_solution": "A familiar three-column agent chat shell",
            "detected_cliches": [
                "Every event was initially isolated in an identical rounded card"
            ],
            "revisions": [
                {
                    "from": "Uniform event cards",
                    "to": "Grouped transcript sections tied to an ordered pulse rail",
                    "reason": "The revision expresses run chronology and reduces container noise",
                }
            ],
            "recognizable_without_brand": (
                "The pulse rail and grouped trace preserve the ordered-run identity"
            ),
        }
        brief["tuning_axes"] = {
            "density": {"value": 4, "note": "Compact desktop operating density"},
            "contrast": {"value": 3, "note": "Quiet canvas with clear active state"},
            "material_depth": {"value": 2, "note": "Revealed material with few raised hosts"},
            "corner_softness": {"value": 2, "note": "Crisp work surfaces with modest rounding"},
            "motion_energy": {"value": 2, "note": "Short state transitions, still reading surface"},
            "visual_expressiveness": {"value": 3, "note": "One signature rail on restrained chrome"},
        }
        brief["theme"] = {
            "material": "mica",
            "light_strategy": "Quiet translucent canvas with opaque text hosts only.",
            "dark_strategy": "Lower-chroma layers with semantic status contrast.",
            "palette_source": "Approved cyan run-pulse mark and project semantic tokens",
            "palette_derivation": "Expand cyan into accent and focus ramps; keep status roles independent",
            "brand_cues": [
                "Compact wave-shaped progress marker",
                "Grouped reasoning and tool disclosure rhythm",
            ],
        }
        brief["copy_policy"] = {
            "audience": "Developers operating a local agent runtime",
            "voice": "Direct, calm, and technically precise",
            "locale_strategy": "Use stable domain terms and test English and CJK wrapping",
            "allowed_technical_terms": ["repository"],
            "compression_rules": [
                "Name the user object or action instead of the assistant",
                "Remove help that repeats the nearby control",
                "Use one sentence at most for empty, helper, and recovery copy",
            ],
            "state_vocabulary": ["Not started", "Running", "Complete", "Failed"],
            "forbidden_patterns": [
                "raw UUID",
                "protocol method name",
                "assistant self-narration",
                "unsupported marketing adjective",
            ],
        }
        brief["action_inventory"] = [
            {
                "action_id": "start-run",
                "owner_region": "run composer",
                "primary_entry": "accent Start button beside the composer",
                "primary_visible_when": "normal and narrow layouts when no run is active",
                "responsive_fallback": "keyboard submit gesture",
                "fallback_visible_when": "while the composer owns keyboard focus",
                "simultaneously_visible": False,
                "coexistence_reason": "none",
            },
            {
                "action_id": "new-run",
                "owner_region": "run navigation",
                "primary_entry": "navigation add button",
                "primary_visible_when": "wide layout with navigation visible",
                "responsive_fallback": "compact header add button",
                "fallback_visible_when": "narrow layout with navigation hidden",
                "simultaneously_visible": False,
                "coexistence_reason": "none",
            },
        ]
        brief["component_decisions"] = [
            {
                "need": "Growing mixed-height run history",
                "component_id": "list-view",
                "decision": "must-use",
                "reason": "Model/view keeps streaming updates bounded.",
            }
        ]
        brief_path.write_text(
            json.dumps(brief, ensure_ascii=False, indent=2), encoding="utf-8"
        )

        rendered_design = subprocess.run(
            [sys.executable, str(render_design), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if rendered_design.returncode != 0 or not (root / "design-board.svg").is_file():
            raise AssertionError(
                "Design board renderer failed: "
                + (rendered_design.stderr or rendered_design.stdout).strip()
            )

        design_board = (root / "design-board.svg").read_text(encoding="utf-8")
        if design_board.count("data:image/png;base64") != 3:
            raise AssertionError(
                "Design board did not embed all three high-fidelity comps"
            )
        if design_board.count("VISUAL SCORECARD") != 3 or "ICON 4" not in design_board:
            raise AssertionError(
                "Design board does not expose the nine-dimension visual scorecard"
            )
        try:
            ET.fromstring(design_board)
        except ET.ParseError as exc:
            raise AssertionError(f"Design board is not valid SVG XML: {exc}") from exc

        concept_ready = subprocess.run(
            [
                sys.executable,
                str(validate_brief),
                "--stage",
                "concepts",
                str(brief_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            concept_ready.returncode != 0
            or "CONCEPTS READY" not in concept_ready.stdout
        ):
            raise AssertionError(
                "Design brief concept stage rejects complete comps: "
                + concept_ready.stderr.strip()
            )

        pending_decision = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            pending_decision.returncode == 0
            or "approval.status must be 'approved'" not in pending_decision.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts implementation before human approval"
            )

        brief["approval"] = {
            "status": "approved",
            "decision_maker_kind": "human",
            "decision_maker_id": "design-owner",
            "decided_at": "2026-08-19T12:00:00Z",
            "selected_concept_id": "transcript-first",
            "selection_reason": (
                "Transcript first preserves the runtime's ordered-run model."
            ),
            "rejected_concept_reasons": {
                "stage-first": "The protocol does not expose stable stages.",
                "artifact-first": "Not every run creates a durable artifact.",
            },
        }
        brief["implementation_spec"] = {
            "source_concept_id": "transcript-first",
            "container_model": "One reading canvas with grouped event disclosures and an integrated composer",
            "semantic_tokens": [
                "Neutral material layers with cyan reserved for active progress",
                "Four-pixel spacing rhythm with restrained radius and elevation",
            ],
            "typography_roles": [
                "Readable transcript body with compact metadata",
                "Semibold chrome labels and quiet captions",
            ],
            "component_families": [
                "Grouped run event rows with active, complete, and error variants"
            ],
            "state_grammar": [
                "Active state combines pulse, text, and semantic accessible status",
                "Completed state settles motion and preserves the final outcome",
            ],
            "motion_cues": [
                "Short pulse transition with an immediate reduced-motion alternative"
            ],
            "responsive_rules": [
                "Collapse supporting metadata before shrinking the transcript",
                "Keep composer, active run, and outcome readable at minimum width",
            ],
            "locked_decisions": [
                "Transcript remains the hero surface",
                "Pulse rail is the only expressive device",
            ],
            "allowed_adaptations": [
                "Native focus and scrollbar geometry may adapt by platform"
            ],
            "known_risks": [
                "Long localized status text may require extra row height"
            ],
            "comparison_regions": [
                "Full desktop window in the approved Light concept",
                "Active run pulse, transcript group, and composer detail",
            ],
        }
        brief_path.write_text(
            json.dumps(brief, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        selected_board = subprocess.run(
            [sys.executable, str(render_design), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if selected_board.returncode != 0:
            raise AssertionError(
                "Design board renderer rejects an approved brief: "
                + selected_board.stderr.strip()
            )
        selected_board_text = (root / "design-board.svg").read_text(
            encoding="utf-8"
        )
        if "HUMAN PICK" not in selected_board_text or "APPROVED" not in selected_board_text:
            raise AssertionError("Approved design board does not identify the human pick")

        brief_ok = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if brief_ok.returncode != 0 or "design brief: PASS" not in brief_ok.stdout:
            raise AssertionError(
                "Design brief validator rejects a complete brief: "
                + brief_ok.stderr.strip()
            )

        missing_risk_guard = json.loads(json.dumps(brief))
        missing_risk_guard["concepts"][0]["visual_direction"].pop("risk_guard")
        brief_path.write_text(json.dumps(missing_risk_guard), encoding="utf-8")
        risk_guard_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            risk_guard_result.returncode == 0
            or "visual_direction.risk_guard" not in risk_guard_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts an unguarded aesthetic risk"
            )

        generic_without_revision = json.loads(json.dumps(brief))
        generic_without_revision["genericity_review"]["revisions"] = []
        brief_path.write_text(
            json.dumps(generic_without_revision), encoding="utf-8"
        )
        genericity_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            genericity_result.returncode == 0
            or "genericity_review.revisions" not in genericity_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts genericity critique without a revision"
            )

        drifting_spec = json.loads(json.dumps(brief))
        drifting_spec["implementation_spec"]["source_concept_id"] = "stage-first"
        brief_path.write_text(json.dumps(drifting_spec), encoding="utf-8")
        drifting_spec_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            drifting_spec_result.returncode == 0
            or "must match approval.selected_concept_id"
            not in drifting_spec_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts a spec extracted from the wrong concept"
            )

        missing_icon_family = json.loads(json.dumps(brief))
        missing_icon_family["iconography"].pop("family")
        brief_path.write_text(json.dumps(missing_icon_family), encoding="utf-8")
        icon_family_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            icon_family_result.returncode == 0
            or "iconography.family" not in icon_family_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts an incomplete icon system"
            )

        missing_application_icon = json.loads(json.dumps(brief))
        missing_application_icon["iconography"].pop("application_icon_strategy")
        brief_path.write_text(
            json.dumps(missing_application_icon), encoding="utf-8"
        )
        application_icon_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            application_icon_result.returncode == 0
            or "iconography.application_icon_strategy"
            not in application_icon_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts no packaged application-icon strategy"
            )

        missing_palette_derivation = json.loads(json.dumps(brief))
        missing_palette_derivation["theme"].pop("palette_derivation")
        brief_path.write_text(
            json.dumps(missing_palette_derivation), encoding="utf-8"
        )
        palette_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            palette_result.returncode == 0
            or "theme.palette_derivation" not in palette_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts no identity-to-theme derivation"
            )

        weak_copy_policy = json.loads(json.dumps(brief))
        weak_copy_policy["copy_policy"]["compression_rules"] = [
            "Remove repeated help"
        ]
        brief_path.write_text(json.dumps(weak_copy_policy), encoding="utf-8")
        copy_policy_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            copy_policy_result.returncode == 0
            or "copy_policy.compression_rules"
            not in copy_policy_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts an incomplete copy-compression policy"
            )

        missing_action_inventory = json.loads(json.dumps(brief))
        missing_action_inventory.pop("action_inventory")
        brief_path.write_text(
            json.dumps(missing_action_inventory), encoding="utf-8"
        )
        action_inventory_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            action_inventory_result.returncode == 0
            or "action_inventory must be a non-empty array"
            not in action_inventory_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts no action-ownership inventory"
            )

        unjustified_duplicate = json.loads(json.dumps(brief))
        unjustified_duplicate["action_inventory"][1]["simultaneously_visible"] = True
        unjustified_duplicate["action_inventory"][1]["coexistence_reason"] = "none"
        brief_path.write_text(
            json.dumps(unjustified_duplicate), encoding="utf-8"
        )
        duplicate_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            duplicate_result.returncode == 0
            or "coexistence_reason must justify simultaneous visible entries"
            not in duplicate_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts an unjustified duplicate action entry"
            )

        unscored_iconography = json.loads(json.dumps(brief))
        unscored_iconography["concepts"][0]["visual_scores"]["iconography"][
            "score"
        ] = 0
        brief_path.write_text(json.dumps(unscored_iconography), encoding="utf-8")
        visual_score_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            visual_score_result.returncode == 0
            or "visual_scores.iconography.score" not in visual_score_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts an unscored visual dimension"
            )

        self_approved = json.loads(json.dumps(brief))
        self_approved["approval"]["decision_maker_id"] = self_approved["author_id"]
        brief_path.write_text(json.dumps(self_approved), encoding="utf-8")
        self_approval_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            self_approval_result.returncode == 0
            or "must differ from author_id" not in self_approval_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts implementation-author approval"
            )

        lookalike = json.loads(json.dumps(brief))
        preserved_name = lookalike["concepts"][1]["visual_direction"]["name"]
        lookalike["concepts"][1]["visual_direction"] = json.loads(
            json.dumps(lookalike["concepts"][0]["visual_direction"])
        )
        lookalike["concepts"][1]["visual_direction"]["name"] = preserved_name
        brief_path.write_text(json.dumps(lookalike), encoding="utf-8")
        lookalike_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            lookalike_result.returncode == 0
            or "must differ in at least three" not in lookalike_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts visually duplicate concepts"
            )

        incomparable = json.loads(json.dumps(brief))
        incomparable["concepts"][1]["comp"]["theme"] = "dark"
        brief_path.write_text(json.dumps(incomparable), encoding="utf-8")
        incomparable_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            incomparable_result.returncode == 0
            or "same theme and viewport" not in incomparable_result.stderr
        ):
            raise AssertionError(
                "Design brief validator accepts incomparable comp conditions"
            )

        duplicate = json.loads(json.dumps(brief))
        duplicate["concepts"][1]["topology"] = duplicate["concepts"][0]["topology"]
        brief_path.write_text(
            json.dumps(duplicate, ensure_ascii=False), encoding="utf-8"
        )
        duplicate_result = subprocess.run(
            [sys.executable, str(validate_brief), str(brief_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if duplicate_result.returncode == 0 or "distinct topologies" not in duplicate_result.stderr:
            raise AssertionError("Design brief validator accepts duplicate topologies")
        brief_path.write_text(
            json.dumps(brief, ensure_ascii=False, indent=2), encoding="utf-8"
        )

        reviewed_build = root / "reviewed-app"
        reviewed_build.mkdir()
        manifest_path = root / "visual-evidence.json"
        initialized_evidence = subprocess.run(
            [
                sys.executable,
                str(init_evidence),
                "--brief",
                str(brief_path),
                "--reviewed-build",
                str(reviewed_build),
                "--platform",
                "test platform, scale 1x",
                "--output",
                str(manifest_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if initialized_evidence.returncode != 0 or not manifest_path.is_file():
            raise AssertionError(
                "Visual evidence initializer failed: "
                + (initialized_evidence.stderr or initialized_evidence.stdout).strip()
            )

        capture = root / "capture.png"
        capture.write_bytes(b"portable-review-image")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest.update(
            {
                "signature_finish": "product",
                "chrome_on_material": "quiet",
                "sparse_canvas_treatment": "composed",
                "primary_input_treatment": "integrated-dock",
                "visible_copy_register": "user-facing",
            }
        )
        for field in ("states", "regions", "dynamic_checks"):
            for entry in manifest[field]:
                entry["status"] = "pass"
                entry["evidence"] = ["capture.png"]
        manifest["measurements"] = [
            {
                "id": "primary-row-height",
                "expected": "32-40",
                "actual": "36",
                "status": "pass",
                "evidence": "capture.png",
            }
        ]
        review = manifest["review"]
        review.update(
            {
                "reviewer_kind": "independent-agent",
                "reviewer_id": "visual-reviewer",
                "reviewed_at": "2026-08-18T12:00:00Z",
                "verdict": "pass",
                "reference_images": ["capture.png"],
            }
        )
        for score in review["scores"].values():
            score.update(
                {
                    "score": 4,
                    "note": "Final evidence meets the named review dimension.",
                    "evidence": ["capture.png"],
                }
            )
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        rendered_review = subprocess.run(
            [sys.executable, str(render_review), str(manifest_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        review_board = root / "visual-review.html"
        if rendered_review.returncode != 0 or not review_board.is_file():
            raise AssertionError(
                "Visual review renderer failed: "
                + (rendered_review.stderr or rendered_review.stdout).strip()
            )
        review_html = review_board.read_text(encoding="utf-8")
        if capture.resolve().as_uri() not in review_html:
            raise AssertionError("Visual review board did not link local evidence")
        if "Iconography" not in review_html or "Surface composition" not in review_html:
            raise AssertionError(
                "Visual review board does not render all nine judged dimensions"
            )

        portable_board = root / "visual-review-portable.html"
        embedded_review = subprocess.run(
            [
                sys.executable,
                str(render_review),
                str(manifest_path),
                "--output",
                str(portable_board),
                "--embed-images",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if embedded_review.returncode != 0 or not portable_board.is_file():
            raise AssertionError(
                "Portable visual review renderer failed: "
                + (embedded_review.stderr or embedded_review.stdout).strip()
            )
        if "data:image/png;base64" not in portable_board.read_text(encoding="utf-8"):
            raise AssertionError("Portable visual review board did not embed evidence")

        evidence_ok = subprocess.run(
            [
                sys.executable,
                str(validate_evidence),
                "--require-current",
                str(manifest_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if evidence_ok.returncode != 0 or "contract v4" not in evidence_ok.stdout:
            raise AssertionError(
                "Visual evidence validator rejects independent review: "
                + evidence_ok.stderr.strip()
            )

        legacy_brief = json.loads(json.dumps(brief))
        legacy_brief["contract_version"] = 2
        legacy_brief.pop("iconography")
        for concept in legacy_brief["concepts"]:
            concept.pop("visual_scores")
            concept["visual_direction"].pop("icon_strategy")
        legacy_brief_path = root / "design-brief-v2.json"
        legacy_brief_path.write_text(
            json.dumps(legacy_brief, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        legacy_manifest = json.loads(json.dumps(manifest))
        legacy_manifest["contract_version"] = 3
        legacy_manifest["design_brief"] = str(legacy_brief_path)
        legacy_manifest["review"]["scores"].pop("iconography")
        legacy_manifest["review"]["scores"].pop("surface_composition")
        legacy_manifest_path = root / "visual-evidence-v3.json"
        legacy_manifest_path.write_text(
            json.dumps(legacy_manifest, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        legacy_v3 = subprocess.run(
            [sys.executable, str(validate_evidence), str(legacy_manifest_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            legacy_v3.returncode != 0
            or "legacy contract v3" not in legacy_v3.stderr
        ):
            raise AssertionError(
                "Visual evidence validator does not preserve contract v3: "
                + legacy_v3.stderr.strip()
            )
        legacy_v3_current = subprocess.run(
            [
                sys.executable,
                str(validate_evidence),
                "--require-current",
                str(legacy_manifest_path),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            legacy_v3_current.returncode == 0
            or "contract_version 4 is required" not in legacy_v3_current.stderr
        ):
            raise AssertionError(
                "Visual evidence validator treats legacy v3 as current evidence"
            )

        manifest["review"]["reviewer_id"] = manifest["author_id"]
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        self_review = subprocess.run(
            [sys.executable, str(validate_evidence), str(manifest_path)],
            check=False,
            capture_output=True,
            text=True,
        )
        if self_review.returncode == 0 or "must differ" not in self_review.stderr:
            raise AssertionError("Visual evidence validator accepts self-review")


def _validate_installable_skill(project_root: Path) -> None:
    skill_root = project_root / ".agents/skills/build-fluentqt-gui"
    query = skill_root / "scripts/query_catalog.py"
    catalog = project_root / "docs/ai/generated/fluentqt-ai-catalog.json"
    bundled_catalog = skill_root / "assets/fluentqt-ai-catalog.json"
    if bundled_catalog.read_bytes() != catalog.read_bytes():
        raise AssertionError("Installable Skill catalog snapshot is stale")
    if (skill_root / "LICENSE.txt").read_bytes() != (
        project_root / "LICENSE"
    ).read_bytes():
        raise AssertionError("Installable Skill license has drifted from LICENSE")

    environment = dict(os.environ)
    environment.pop("FLUENTQT_ROOT", None)
    with tempfile.TemporaryDirectory(prefix="fluentqt-installable-skill-") as temp:
        temp_root = Path(temp)
        queried = subprocess.run(
            [sys.executable, str(query), "--component", "window"],
            check=False,
            capture_output=True,
            text=True,
            cwd=temp_root,
            env=environment,
        )
        if queried.returncode != 0 or "window" not in queried.stdout.lower():
            raise AssertionError(
                "Skill catalog query failed outside the FluentQt checkout: "
                + (queried.stderr or queried.stdout).strip()
            )

        package = subprocess.run(
            [
                sys.executable,
                str(project_root / "tools/ai/package_fluentqt_skill.py"),
                "--project-root",
                str(project_root),
                "--output-dir",
                str(temp_root / "dist"),
                "--version",
                "validation",
            ],
            check=False,
            capture_output=True,
            text=True,
            cwd=temp_root,
            env=environment,
        )
        if package.returncode != 0:
            raise AssertionError(
                "Could not package installable FluentQt GUI Skill: "
                + package.stderr.strip()
            )
        archive = Path(package.stdout.strip())
        if not archive.is_file():
            raise AssertionError("Skill packager did not produce an archive")

        with zipfile.ZipFile(archive) as packaged_skill:
            entries = set(packaged_skill.namelist())
            prefix = "build-fluentqt-gui/"
            if not entries or any(not entry.startswith(prefix) for entry in entries):
                raise AssertionError("Skill archive has an invalid top-level layout")
            for relative_path in REQUIRED_SKILL_FILES:
                if prefix + relative_path not in entries:
                    raise AssertionError(
                        f"Skill archive is missing {relative_path}"
                    )
            if any(".claude/" in entry for entry in entries):
                raise AssertionError("Skill archive contains an agent-specific copy")
            packaged_skill.extractall(temp_root / "installed")

        installed_query = (
            temp_root
            / "installed"
            / "build-fluentqt-gui"
            / "scripts"
            / "query_catalog.py"
        )
        installed = subprocess.run(
            [sys.executable, str(installed_query), "--component", "field"],
            check=False,
            capture_output=True,
            text=True,
            cwd=temp_root,
            env=environment,
        )
        if installed.returncode != 0 or "field" not in installed.stdout.lower():
            raise AssertionError(
                "Packaged Skill catalog query failed after extraction: "
                + (installed.stderr or installed.stdout).strip()
            )


def _validate_project_structure_tooling(project_root: Path) -> None:
    skill_root = project_root / ".agents/skills/build-fluentqt-gui"
    initializer = skill_root / "scripts/init_project_structure.py"
    validator = skill_root / "scripts/validate_project_structure.py"

    with tempfile.TemporaryDirectory(prefix="fluentqt-project-structure-") as temp:
        root = Path(temp)
        cpp_project = root / "cpp-app"
        cpp_project.mkdir()
        initialized = subprocess.run(
            [
                sys.executable,
                str(initializer),
                "--project-root",
                str(cpp_project),
                "--application",
                "Structure fixture",
                "--language",
                "cpp",
                "--profile",
                "full",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if initialized.returncode != 0:
            raise AssertionError(
                "C++ project structure initializer failed: "
                + (initialized.stderr or initialized.stdout).strip()
            )

        manifest_path = cpp_project / ".fluentqt/architecture.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest.get("template") != "cpp-full":
            raise AssertionError("C++ project structure initializer chose a stale template")
        shell_root = cpp_project / "src/ui/shell"
        (shell_root / "MainWindow.h").write_text(
            "#pragma once\nclass MainWindow { int m_state = 0; };\n",
            encoding="utf-8",
        )
        (shell_root / "MainWindow.cpp").write_text(
            '#include "MainWindow.h"\n', encoding="utf-8"
        )
        manifest["shell_files"] = [
            "ui/shell/MainWindow.h",
            "ui/shell/MainWindow.cpp",
        ]
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        valid_cpp = subprocess.run(
            [
                sys.executable,
                str(validator),
                "--project-root",
                str(cpp_project),
                "--strict",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if valid_cpp.returncode != 0 or "project structure: PASS" not in valid_cpp.stdout:
            raise AssertionError(
                "Project structure validator rejects the generated C++ layout: "
                + (valid_cpp.stderr or valid_cpp.stdout).strip()
            )

        process_owner = cpp_project / "src/ui/pages/ProcessPage.cpp"
        process_owner.write_text(
            "#include <QProcess>\nvoid startProcess() { new QProcess; }\n",
            encoding="utf-8",
        )
        invalid_cpp = subprocess.run(
            [
                sys.executable,
                str(validator),
                "--project-root",
                str(cpp_project),
                "--strict",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if invalid_cpp.returncode == 0 or "UI owns a process" not in invalid_cpp.stdout:
            raise AssertionError(
                "Project structure validator accepts process ownership in the UI layer"
            )

        python_project = root / "python-app"
        python_project.mkdir()
        initialized_python = subprocess.run(
            [
                sys.executable,
                str(initializer),
                "--project-root",
                str(python_project),
                "--application",
                "Python structure fixture",
                "--language",
                "pyside6",
                "--profile",
                "lite",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if initialized_python.returncode != 0:
            raise AssertionError(
                "PySide6 project structure initializer failed: "
                + (initialized_python.stderr or initialized_python.stdout).strip()
            )
        valid_python = subprocess.run(
            [
                sys.executable,
                str(validator),
                "--project-root",
                str(python_project),
                "--strict",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if (
            valid_python.returncode != 0
            or "project structure: PASS" not in valid_python.stdout
        ):
            raise AssertionError(
                "Project structure validator rejects the generated PySide6 layout: "
                + (valid_python.stderr or valid_python.stdout).strip()
            )


def validate(project_root: Path) -> dict[str, int]:
    project_root = project_root.resolve()
    for relative_path in REQUIRED_PATHS:
        if not (project_root / relative_path).is_file():
            raise AssertionError(f"Missing AI-friendly asset: {relative_path}")

    schema = json.loads(
        (project_root / "docs/ai/fluentqt-ai-catalog.schema.json").read_text(
            encoding="utf-8"
        )
    )
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        raise AssertionError("AI catalog schema must use JSON Schema draft 2020-12")
    project_analysis_schema = json.loads(
        (project_root / "docs/ai/project-analysis.schema.json").read_text(
            encoding="utf-8"
        )
    )
    if project_analysis_schema.get("$schema") != schema["$schema"]:
        raise AssertionError("AI JSON schemas must use the same draft")
    _validate_json_instance(
        _project_analysis_fixture(), project_analysis_schema, project_analysis_schema
    )

    generated = generate_catalog(project_root)
    catalog_path = (
        project_root / "docs/ai/generated/fluentqt-ai-catalog.json"
    )
    if not check_catalog(generated, catalog_path):
        raise AssertionError("Committed FluentQt AI catalog is stale")
    skill_catalog_path = (
        project_root
        / ".agents/skills/build-fluentqt-gui/assets/fluentqt-ai-catalog.json"
    )
    if not check_catalog(generated, skill_catalog_path):
        raise AssertionError("Installable Skill catalog snapshot is stale")
    committed = json.loads(catalog_path.read_text(encoding="utf-8"))
    if committed != generated:
        raise AssertionError("Committed FluentQt AI catalog is not canonical")
    _validate_json_instance(committed, schema, schema)

    summary = committed["summary"]
    expected_counts = {
        "route_count": 90,
        "component_count": 69,
        "sample_count": 205,
        "guided_component_count": 69,
    }
    for key, expected in expected_counts.items():
        if summary.get(key) != expected:
            raise AssertionError(
                f"Unexpected AI catalog {key}: {summary.get(key)!r}"
            )

    ids = [component["id"] for component in committed["components"]]
    if len(ids) != len(set(ids)):
        raise AssertionError("AI catalog contains duplicate component ids")
    category_component_ids = [
        component_id
        for category in committed["categories"]
        for component_id in category["components"]
    ]
    if sorted(category_component_ids) != sorted(ids):
        raise AssertionError(
            "AI catalog category membership does not match component inventory"
        )
    derived_counts = {
        "route_count": len(committed["routes"]),
        "component_count": len(committed["components"]),
        "sample_count": sum(
            len(component["samples"]) for component in committed["components"]
        ),
        "integration_pattern_count": len(committed["integration_patterns"]),
        "application_pattern_count": len(committed["application_patterns"]),
        "selection_guide_count": len(committed["selection_guides"]),
        "guided_component_count": sum(
            bool(component["capabilities"])
            for component in committed["components"]
        ),
    }
    if summary != derived_counts:
        raise AssertionError("AI catalog summary does not match catalog contents")
    for component in committed["components"]:
        if not component["capabilities"]:
            raise AssertionError(
                f"Component {component['id']} has no selection guidance"
            )
        for test in component["tests"]:
            if not (project_root / test["source"]).is_file():
                raise AssertionError(
                    f"Component {component['id']} references missing test "
                    f"{test['source']}"
                )
        gallery = component["gallery"]
        if not (project_root / gallery["sample_source"]).is_file():
            raise AssertionError(
                f"Component {component['id']} references missing Gallery source "
                f"{gallery['sample_source']}"
            )
        if gallery["control_image"] and not (
            project_root / gallery["control_image"]
        ).is_file():
            raise AssertionError(
                f"Component {component['id']} references missing control image "
                f"{gallery['control_image']}"
            )

    tree_results = {
        component["id"]
        for component in search_components(committed, "expandable hierarchy")
    }
    if "tree-view" not in tree_results:
        raise AssertionError("Catalog query cannot discover TreeView by intent")
    progress_results = {
        component["id"]
        for component in search_components(committed, "determinate progress")
    }
    if not progress_results.intersection({"progress-bar", "progress-ring"}):
        raise AssertionError("Catalog query cannot discover a determinate progress control")
    if component_by_id(committed, "button") is None:
        raise AssertionError("Catalog query cannot resolve a component id")
    for pattern_id in ("service-client", "file-workbench", "greenfield"):
        if pattern_by_id(committed, pattern_id) is None:
            raise AssertionError(
                f"Catalog query cannot resolve application pattern {pattern_id}"
            )

    scenarios = json.loads(
        (project_root / "docs/ai/evals/scenarios.json").read_text(encoding="utf-8")
    )
    scenarios_schema = json.loads(
        (project_root / "docs/ai/evals/scenarios.schema.json").read_text(
            encoding="utf-8"
        )
    )
    if scenarios_schema.get("$schema") != schema["$schema"]:
        raise AssertionError("AI eval schema must use JSON Schema draft 2020-12")
    _validate_json_instance(scenarios, scenarios_schema, scenarios_schema)
    eval_report = evaluate_catalog(committed, scenarios)
    if not eval_report["passed"]:
        raise AssertionError(
            "AI catalog eval failed: " + "; ".join(eval_report["failures"])
        )

    _validate_skill(project_root)
    _validate_visual_evidence_validator(project_root)
    _validate_design_and_review_tooling(project_root)
    _validate_installable_skill(project_root)
    _validate_project_structure_tooling(project_root)
    return {
        "components": summary["component_count"],
        "samples": summary["sample_count"],
        "application_patterns": summary["application_pattern_count"],
        "integration_patterns": summary["integration_pattern_count"],
        "project_shapes": eval_report["project_shape_count"],
        "retrieval_cases": eval_report["retrieval_case_count"],
        "composition_cases": eval_report["composition_case_count"],
    }


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root", type=Path, default=Path(__file__).resolve().parents[2]
    )
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        counts = validate(args.project_root)
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        "Validated FluentQt AI assets: "
        "{components} components, {samples} samples, "
        "{application_patterns} application patterns, "
        "{integration_patterns} integration patterns, "
        "{project_shapes} project shapes, "
        "{retrieval_cases} retrieval cases, "
        "{composition_cases} composition cases".format(**counts)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
