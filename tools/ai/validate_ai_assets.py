#!/usr/bin/env python3

"""Validate FluentQt AI docs, schemas, catalog, evals, queries, and Skill."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Iterable


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from generate_ai_catalog import check_catalog, generate_catalog
from evaluate_ai_catalog import evaluate_catalog
from query_ai_catalog import component_by_id, pattern_by_id, search_components


REQUIRED_PATHS = (
    "docs/ai/README.md",
    "docs/ai/add-gui-to-project.md",
    "docs/ai/evals/scenarios.json",
    "docs/ai/evals/scenarios.schema.json",
    "docs/ai/guidance.json",
    "docs/ai/fluentqt-ai-catalog.schema.json",
    "docs/ai/project-analysis.schema.json",
    "docs/ai/generated/fluentqt-ai-catalog.json",
    ".agents/skills/build-fluentqt-gui/SKILL.md",
    ".agents/skills/build-fluentqt-gui/agents/openai.yaml",
    ".agents/skills/build-fluentqt-gui/references/component-selection.md",
    ".agents/skills/build-fluentqt-gui/references/experience-differentiation.md",
    ".agents/skills/build-fluentqt-gui/references/performance-lifecycle.md",
    ".agents/skills/build-fluentqt-gui/references/premium-shell.md",
    ".agents/skills/build-fluentqt-gui/references/product-reference-patterns.md",
    ".agents/skills/build-fluentqt-gui/references/signature-surface.md",
    ".agents/skills/build-fluentqt-gui/references/theme-system.md",
    ".agents/skills/build-fluentqt-gui/references/visual-evidence-contract.md",
    ".agents/skills/build-fluentqt-gui/references/visual-refinement.md",
    ".agents/skills/build-fluentqt-gui/scripts/validate_visual_evidence.py",
    ".claude/skills/build-fluentqt-gui/SKILL.md",
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
        "docs/ai/README.md",
        "docs/ai/add-gui-to-project.md",
        "references/component-selection.md",
        "references/experience-differentiation.md",
        "references/performance-lifecycle.md",
        "references/premium-shell.md",
        "references/product-reference-patterns.md",
        "references/signature-surface.md",
        "references/theme-system.md",
        "references/visual-evidence-contract.md",
        "references/visual-refinement.md",
        "scripts/validate_visual_evidence.py",
        "tools/ai/evaluate_ai_catalog.py",
        "tools/ai/query_ai_catalog.py",
        "tools/ai/validate_ai_assets.py",
    ):
        if required not in contents:
            raise AssertionError(f"FluentQt GUI Skill does not route to {required}")

    reference_requirements = {
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
            "apply_style_theme",
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

    claude_loader_path = (
        project_root / ".claude/skills/build-fluentqt-gui/SKILL.md"
    )
    claude_loader = claude_loader_path.read_text(encoding="utf-8")
    if not claude_loader.startswith(
        "---\nname: build-fluentqt-gui\ndescription:"
    ):
        raise AssertionError("Claude Skill loader has invalid frontmatter")
    if "../../../.agents/skills/build-fluentqt-gui/SKILL.md" not in claude_loader:
        raise AssertionError("Claude Skill loader does not route to the canonical Skill")
    claude_target = (
        claude_loader_path.parent
        / "../../../.agents/skills/build-fluentqt-gui/SKILL.md"
    ).resolve()
    if claude_target != skill_path.resolve():
        raise AssertionError("Claude Skill loader resolves outside the canonical Skill")
    canonical_description = contents.split("\n", 3)[2]
    claude_description = claude_loader.split("\n", 3)[2]
    if claude_description != canonical_description:
        raise AssertionError("Cross-agent Skill descriptions have drifted")


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
    committed = json.loads(catalog_path.read_text(encoding="utf-8"))
    if committed != generated:
        raise AssertionError("Committed FluentQt AI catalog is not canonical")
    _validate_json_instance(committed, schema, schema)

    summary = committed["summary"]
    expected_counts = {
        "route_count": 88,
        "component_count": 67,
        "sample_count": 199,
        "guided_component_count": 67,
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
