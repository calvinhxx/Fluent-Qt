#!/usr/bin/env python3

from __future__ import annotations

import argparse
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("fluent_qt_gui_verify.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_gui_verify", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def geometry_report(x: int = 16) -> dict[str, object]:
    return {
        "schema_version": 1,
        "widgets": [
            {
                "object_name": "probe",
                "stable": True,
                "rect": {"x": x, "y": 20, "width": 120, "height": 32},
                "clipped": False,
            }
        ],
    }


def capture_report(snapshot: Path, x: int = 16) -> dict[str, object]:
    return {
        "schema_version": 2,
        "status": "ok",
        "selection": {"route": "button", "sample": "button-styles"},
        "scene": {
            "theme": "light",
            "layout_direction": "ltr",
            "actual_width": 640,
            "actual_height": 480,
        },
        "environment": {
            "fingerprint_schema_version": 1,
            "qt_version": "6.9.3",
            "platform_plugin": "cocoa",
            "style": "macos",
            "device_pixel_ratio": 2,
            "logical_dpi_x": 72,
            "logical_dpi_y": 72,
            "locale": "en_US",
            "font": {"family": "SF Pro"},
            "system": {"product_type": "macos", "cpu_architecture": "arm64"},
            "scale_environment": {"QT_SCALE_FACTOR": "1"},
        },
        "artifacts": {"snapshot": {"written": True, "path": str(snapshot)}},
        "interaction_report": {
            "schema_version": 1,
            "requested": False,
            "status": "not-requested",
            "steps": [],
        },
        "geometry_report": geometry_report(x),
        "quality_report": {
            "schema_version": 1,
            "summary": {
                "findings": 0,
                "by_severity": {"info": 0, "warning": 0, "error": 0},
            },
            "findings": [],
        },
    }


def recipe(baseline: str) -> dict[str, object]:
    return {
        "schema_version": 1,
        "id": "gui-probe",
        "path_base": "repository",
        "author": {"id": "implementation-agent", "kind": "ai"},
        "selection": {"route": "button", "sample": "button-styles"},
        "coverage": {
            "required_tags": ["theme:light", "width:narrow", "state:default"]
        },
        "defaults": {
            "settle_ms": 0,
            "require_native_desktop": True,
            "inspector": {
                "max_findings": 0,
                "max_by_severity": {"info": 0, "warning": 0, "error": 0},
                "allowed_codes": [],
            },
            "geometry": {"required": ["probe"], "tolerance": 0},
            "pixel": {
                "channel_threshold": 0,
                "max_different_pixels": 0,
                "search_radius": 4,
                "max_translation": 0,
                "regions": [{"id": "detail", "rect": [1, 2, 3, 4]}],
            },
        },
        "scenarios": [
            {
                "id": "light-narrow-default",
                "theme": "light",
                "direction": "ltr",
                "size": "640x480",
                "tags": ["theme:light", "width:narrow", "state:default"],
                "baseline": baseline,
                "review": ["The probe remains aligned."],
            }
        ],
    }


class FluentQtGuiVerifyTest(unittest.TestCase):
    def test_recipe_requires_explicit_coverage_and_visual_contracts(self):
        value = recipe("baselines/light")
        self.assertEqual(MODULE.validate_recipe(value), [])

        value["coverage"] = {"required_tags": ["theme:dark"]}
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("missing required tags" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["geometry"] = {"required": []}
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("geometry.required" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["pixel"]["regions"][0]["rect"] = [1, 2, 0, 4]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("positive width" in error for error in errors))

        value = recipe("baselines/light")
        value.pop("path_base")
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("path_base" in error for error in errors))

    def test_recipe_path_base_is_explicit_and_survives_recipe_relocation(self):
        repository_recipe = recipe("tests/visual-baselines/gui/probe/light")
        expected = MODULE.PROJECT_ROOT / "tests/visual-baselines/gui/probe/light"
        original_path = MODULE.PROJECT_ROOT / "tools/dev/recipe.json"
        copied_path = MODULE.PROJECT_ROOT / "build/gui-verification/recipes/recipe.json"

        self.assertEqual(
            MODULE.select_baseline(
                repository_recipe["scenarios"][0]["baseline"],
                MODULE.recipe_path_base(repository_recipe, original_path),
            ),
            expected,
        )
        self.assertEqual(
            MODULE.select_baseline(
                repository_recipe["scenarios"][0]["baseline"],
                MODULE.recipe_path_base(repository_recipe, copied_path),
            ),
            expected,
        )

        portable_recipe = recipe("baselines/light")
        portable_recipe["path_base"] = "recipe"
        self.assertEqual(
            MODULE.select_baseline(
                portable_recipe["scenarios"][0]["baseline"],
                MODULE.recipe_path_base(portable_recipe, copied_path),
            ),
            copied_path.parent / "baselines/light",
        )

    def test_checked_in_example_keeps_baselines_when_copied_under_build(self):
        example_path = SCRIPT.with_name("gui-verification.example.json")
        value = MODULE.read_json(example_path)
        copied_path = MODULE.PROJECT_ROOT / "build/gui-verification/recipes/copied.json"

        self.assertEqual(MODULE.validate_recipe(value), [])
        self.assertEqual(value["path_base"], "repository")
        path_base = MODULE.recipe_path_base(value, copied_path)
        for scenario in value["scenarios"]:
            baseline = MODULE.select_baseline(scenario["baseline"], path_base)
            self.assertEqual(
                baseline,
                (MODULE.PROJECT_ROOT / scenario["baseline"]).resolve(),
            )

    def test_inspector_and_geometry_are_hard_deterministic_gates(self):
        report = capture_report(Path("actual.png"), x=17)
        report["quality_report"]["findings"] = [
            {"code": "text.clipped", "severity": "error"}
        ]
        inspector = MODULE.inspector_check(
            report, {"max_findings": 0, "max_by_severity": {"error": 0}}
        )
        geometry = MODULE.geometry_contract_check(
            report,
            {
                "required": [
                    {
                        "object_name": "probe",
                        "rect": {"x": 16, "y": 20, "width": 120, "height": 32},
                        "tolerance": 0,
                    }
                ]
            },
        )
        self.assertEqual(inspector["status"], "fail")
        self.assertEqual(geometry["status"], "fail")

    def test_missing_baseline_never_becomes_a_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            baseline = Path(temporary) / "missing"
            metadata, report, checks = MODULE.baseline_bundle(
                baseline,
                "gui-probe",
                "light-narrow-default",
                "a" * 64,
                "implementation-agent",
            )
            self.assertIsNone(metadata)
            self.assertIsNone(report)
            self.assertEqual(checks[0]["status"], "human-required")
            self.assertEqual(MODULE.combined_status(checks), "human-required")

    def test_full_scenario_combines_capture_geometry_fingerprint_and_pixels(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recipe_path = root / "recipe.json"
            baseline_dir = root / "baselines" / "light"
            baseline_dir.mkdir(parents=True)
            value = recipe("baselines/light")
            value["path_base"] = "recipe"
            recipe_path.write_text(json.dumps(value), encoding="utf-8")

            baseline_image = baseline_dir / "baseline.png"
            baseline_image.write_bytes(b"png-payload")
            baseline_report = baseline_dir / "baseline-report.json"
            baseline_report.write_text(
                json.dumps(capture_report(baseline_image)), encoding="utf-8"
            )
            contract = MODULE.scenario_contract(
                value, value["scenarios"][0], None
            )
            contract_sha256 = MODULE.sha256_bytes(
                MODULE.canonical_json(contract)
            )
            metadata = {
                "schema_version": 1,
                "status": "approved",
                "recipe_id": "gui-probe",
                "scenario_id": "light-narrow-default",
                "scenario_contract_sha256": contract_sha256,
                "approved_by": "independent-baseline-reviewer",
                "approver_kind": "human",
                "approved_at": "2026-08-29T00:00:00+00:00",
                "approval_note": "Accepted after native-resolution review.",
                "image_sha256": MODULE.sha256_file(baseline_image),
                "capture_report_sha256": MODULE.sha256_file(baseline_report),
            }
            (baseline_dir / "baseline.json").write_text(
                json.dumps(metadata), encoding="utf-8"
            )

            commands = []

            def fake_run(command, **_kwargs):
                command = [str(item) for item in command]
                commands.append(command)
                if "--preview" in command:
                    snapshot = Path(command[command.index("--snapshot") + 1])
                    report_path = Path(command[command.index("--report") + 1])
                    snapshot.write_bytes(b"png-payload")
                    report_path.write_text(
                        json.dumps(capture_report(snapshot)), encoding="utf-8"
                    )
                    return subprocess.CompletedProcess(command, 0, "", "")
                report_path = Path(command[command.index("--report") + 1])
                report_path.write_text(
                    json.dumps({"schema_version": 1, "status": "pass"}),
                    encoding="utf-8",
                )
                return subprocess.CompletedProcess(command, 0, "", "")

            output = root / "output"
            with mock.patch.object(MODULE.subprocess, "run", side_effect=fake_run):
                result = MODULE.run_scenario(
                    value,
                    recipe_path,
                    value["scenarios"][0],
                    output,
                    root / "gallery",
                    root / "comparator",
                )

            self.assertEqual(result["pre_baseline_status"], "pass")
            self.assertEqual(result["status"], "pass")
            self.assertIn("pixels.full", [item["id"] for item in result["checks"]])
            region_commands = [command for command in commands if "--region" in command]
            self.assertEqual(
                region_commands[0][region_commands[0].index("--region") + 1],
                "2,4,6,8",
            )

    def test_review_digest_identity_and_attestation_are_enforced(self):
        with tempfile.TemporaryDirectory() as temporary:
            evidence_path = Path(temporary) / "evidence.json"
            evidence = {
                "schema_version": 1,
                "deterministic_status": "pass",
                "recipe": {
                    "author": {"id": "implementation-agent", "kind": "ai"}
                },
                "scenarios": [
                    {"id": "light", "conditions": {"actions": "actions.json"}}
                ],
            }
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            review = {
                "schema_version": 1,
                "reviewer": {"id": "visual-reviewer", "kind": "ai"},
                "evidence_sha256": MODULE.sha256_file(evidence_path),
                "verdict": "pass",
                "summary": "All declared states were inspected at native resolution.",
                "reviewed_scenarios": ["light"],
                "attestation": {
                    "independent": True,
                    "visual_artifacts_opened": True,
                    "interaction_evidence_reviewed": True,
                },
                "findings": [],
            }
            self.assertEqual(
                MODULE.validate_review(evidence, evidence_path, review), []
            )

            review["reviewer"]["id"] = "implementation-agent"
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("differ" in error for error in errors))

            review["reviewer"]["id"] = "visual-reviewer"
            review["evidence_sha256"] = "0" * 64
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("sha256" in error for error in errors))

    def test_baseline_approval_refuses_self_approval(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            actual = root / "actual.png"
            report = root / "capture.json"
            actual.write_bytes(b"png")
            report.write_text("{}", encoding="utf-8")
            evidence_path = root / "evidence.json"
            evidence_path.write_text(
                json.dumps(
                    {
                        "recipe": {
                            "id": "gui-probe",
                            "author": {"id": "implementation-agent", "kind": "ai"}
                        },
                        "scenarios": [
                            {
                                "id": "light",
                                "pre_baseline_status": "pass",
                                "contract_sha256": "a" * 64,
                                "baseline_dir": str(root / "baseline"),
                                "artifacts": {
                                    "actual": str(actual),
                                    "report": str(report),
                                },
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            args = argparse.Namespace(
                evidence=evidence_path,
                scenario="light",
                baseline_dir=None,
                approved_by="implementation-agent",
                approver_kind="ai",
                approval_note="self review",
                replace=False,
            )
            with self.assertRaises(MODULE.VerificationError):
                MODULE.approve_baseline(args)

            args.approved_by = "independent-reviewer"
            args.approver_kind = "human"
            args.approval_note = "Reviewed the native-resolution capture."
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(MODULE.approve_baseline(args), 0)
            metadata = json.loads(
                (root / "baseline" / "baseline.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["recipe_id"], "gui-probe")
            self.assertEqual(metadata["scenario_contract_sha256"], "a" * 64)

    def test_finalize_is_the_only_successful_final_decision(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence_path = root / "evidence.json"
            evidence = {
                "schema_version": 1,
                "deterministic_status": "pass",
                "recipe": {
                    "author": {"id": "implementation-agent", "kind": "ai"}
                },
                "scenarios": [
                    {"id": "light", "conditions": {"actions": None}}
                ],
            }
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            review_path = root / "review.json"
            review_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "reviewer": {"id": "fresh-reviewer", "kind": "human"},
                        "evidence_sha256": MODULE.sha256_file(evidence_path),
                        "verdict": "pass",
                        "summary": "Reviewed the complete native-resolution scene.",
                        "reviewed_scenarios": ["light"],
                        "attestation": {
                            "independent": True,
                            "visual_artifacts_opened": True,
                            "interaction_evidence_reviewed": False,
                        },
                        "findings": [],
                    }
                ),
                encoding="utf-8",
            )
            output = root / "verification.json"
            args = argparse.Namespace(
                evidence=evidence_path, review=review_path, output=output
            )

            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(MODULE.finalize_review(args), 0)
            self.assertEqual(
                json.loads(output.read_text(encoding="utf-8"))["status"],
                "pass",
            )


if __name__ == "__main__":
    unittest.main()
