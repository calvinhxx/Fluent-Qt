#!/usr/bin/env python3

from __future__ import annotations

import argparse
import copy
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zlib


SCRIPT = Path(__file__).with_name("fluent_qt_gui_verify.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_gui_verify", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def solid_png(
    width: int, height: int, rgba: tuple[int, int, int, int] = (0, 0, 0, 255)
) -> bytes:

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    pixel = bytes(rgba)
    row = b"\x00" + (pixel * width)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(row * height))
        + png_chunk(b"IEND", b"")
    )


PNG_FIXTURE = solid_png(1280, 960)
CHANGED_PNG_FIXTURE = solid_png(1280, 960, (1, 0, 0, 255))


def geometry_report(x: int = 16) -> dict[str, object]:
    rect = {"x": x, "y": 20, "width": 120, "height": 32}
    return {
        "schema_version": 1,
        "tool": "FluentQt Named Widget Geometry",
        "root_size": {"width": 640, "height": 480},
        "widget_count": 1,
        "widgets": [
            {
                "path": "QWidget#probe",
                "class": "QWidget",
                "object_name": "probe",
                "stable": True,
                "rect": rect,
                "visible_rect": copy.deepcopy(rect),
                "minimum_size": {"width": 0, "height": 0},
                "maximum_size": {"width": 16777215, "height": 16777215},
                "size_hint": {"width": 120, "height": 32},
                "enabled": True,
                "has_focus": False,
                "clipped": False,
                "layout_direction": "ltr",
                "accessible_name": "",
            }
        ],
    }


def capture_report(snapshot: Path, x: int = 16) -> dict[str, object]:
    return {
        "schema_version": 2,
        "tool": "FluentQt Gallery Preview",
        "status": "ok",
        "selection": {"route": "button", "sample": "button-styles"},
        "scene": {
            "requested_theme": "light",
            "theme": "light",
            "layout_direction": "ltr",
            "settle_ms": 250,
            "requested_width": 640,
            "requested_height": 480,
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
            "font": {
                "family": "SF Pro",
                "style_name": "Regular",
                "point_size": 13,
                "pixel_size": -1,
                "weight": 400,
                "italic": False,
            },
            "screen": {
                "name": "Private Display Name",
                "manufacturer": "Private Manufacturer",
                "model": "Private Model",
                "serial_number": "PRIVATE-SERIAL-123",
                "depth": 30,
                "geometry": {"x": 0, "y": 0, "width": 1440, "height": 900},
                "available_geometry": {
                    "x": 0,
                    "y": 25,
                    "width": 1440,
                    "height": 875,
                },
                "physical_dpi_x": 220,
                "physical_dpi_y": 220,
            },
            "system": {
                "product_type": "macos",
                "product_version": "15.0",
                "kernel_type": "darwin",
                "kernel_version": "24.0",
                "cpu_architecture": "arm64",
            },
            "scale_environment": {
                "QT_SCALE_FACTOR": "1",
                "QT_SCREEN_SCALE_FACTORS": "",
                "QT_FONT_DPI": "",
                "QT_AUTO_SCREEN_SCALE_FACTOR": "",
                "QT_ENABLE_HIGHDPI_SCALING": "",
            },
        },
        "artifacts": {
            "snapshot": {
                "requested": True,
                "written": True,
                "path": str(snapshot),
                "sha256": MODULE.sha256_file(snapshot) if snapshot.is_file() else None,
                "error": "",
            }
        },
        "interaction_report": {
            "schema_version": 1,
            "requested": False,
            "status": "not-requested",
            "summary": {"total": 0, "executed": 0, "passed": 0, "failed": 0},
            "steps": [],
        },
        "geometry_report": geometry_report(x),
        "quality_report": {
            "schema_version": 1,
            "tool": "FluentQt Inspector",
            "root": {
                "class": "QWidget",
                "object_name": "previewRoot",
                "width": 640,
                "height": 480,
            },
            "summary": {
                "findings": 0,
                "by_severity": {"info": 0, "warning": 0, "error": 0},
                "by_category": {},
            },
            "findings": [],
        },
    }


def comparison_report(
    baseline: Path,
    actual: Path,
    policy: dict[str, object],
    region: list[int] | None = None,
) -> dict[str, object]:
    dimensions = MODULE.png_dimensions(baseline)
    assert dimensions is not None
    compared_size = (
        {"width": region[2], "height": region[3]}
        if region is not None
        else {"width": dimensions[0], "height": dimensions[1]}
    )
    total_pixels = compared_size["width"] * compared_size["height"]
    return {
        "schema_version": 1,
        "tool": "FluentQt Visual Compare",
        "status": "pass",
        "error": None,
        "baseline_size": compared_size,
        "actual_size": compared_size,
        "checks": {
            "size_matches": True,
            "pixel_limits_pass": True,
            "translation_limit_pass": True,
        },
        "policy": MODULE.expected_comparator_policy(policy),
        "metrics": {
            "total_pixels": total_pixels,
            "different_pixels": 0,
            "different_ratio": 0,
            "max_channel_delta": 0,
            "mean_max_channel_delta": 0,
            "difference_bounds": None,
            "estimated_translation": {
                "dx": 0,
                "dy": 0,
                "confident": False,
                "baseline_edge_pixels": 0,
                "actual_edge_pixels": 0,
                "zero_offset_score": 0,
                "best_score": 0,
                "improvement": 0,
            },
        },
        "inputs": {
            "baseline": str(baseline.resolve()),
            "baseline_sha256": MODULE.sha256_file(baseline),
            "actual": str(actual.resolve()),
            "actual_sha256": MODULE.sha256_file(actual),
            "region": MODULE.expected_region(region),
        },
        "artifacts": {"diff": None},
    }


def evidence_recipe_record(value: dict[str, object], path: Path) -> dict[str, object]:
    return {
        "id": value["id"],
        "path": str(path),
        "sha256": MODULE.sha256_file(path),
        "author": value["author"],
        "coverage": value["coverage"],
        "path_base": value["path_base"],
        "resolved_path_base": str(MODULE.recipe_path_base(value, path)),
    }


def write_approved_baseline_metadata(
    baseline_dir: Path,
    value: dict[str, object],
    contract_sha256: str,
    *,
    approved_by: str = "independent-baseline-reviewer",
) -> dict[str, object]:
    image = baseline_dir / "baseline.png"
    report = baseline_dir / "baseline-report.json"
    scenario_id = value["scenarios"][0]["id"]
    source_evidence = {
        "schema_version": 1,
        "tool": "FluentQt GUI Baseline Provenance",
        "source_evidence_sha256": "e" * 64,
        "recipe": {
            "id": value["id"],
            "sha256": "f" * 64,
            "author": value["author"],
        },
        "scenario": {
            "id": scenario_id,
            "pre_baseline_status": "pass",
            "contract_sha256": contract_sha256,
            "artifacts": {
                "actual_sha256": MODULE.sha256_file(image),
                "capture_report_sha256": MODULE.sha256_file(report),
                "baseline_report_sha256": MODULE.sha256_file(report),
            },
        },
        "binaries": {
            "gallery_sha256": "1" * 64,
            "comparator_sha256": "2" * 64,
        },
        "git": {"revision": "a" * 40, "dirty": False},
    }
    source_path = baseline_dir / "source-evidence.json"
    source_path.write_text(json.dumps(source_evidence), encoding="utf-8")
    metadata = {
        "schema_version": 1,
        "status": "approved",
        "recipe_id": value["id"],
        "scenario_id": scenario_id,
        "scenario_contract_sha256": contract_sha256,
        "approved_by": approved_by,
        "approver_kind": "human",
        "approved_at": "2026-08-29T00:00:00+00:00",
        "approval_note": "Reviewed the native-resolution capture.",
        "source_evidence": "source-evidence.json",
        "source_evidence_sha256": MODULE.sha256_file(source_path),
        "image_sha256": MODULE.sha256_file(image),
        "capture_report_sha256": MODULE.sha256_file(report),
    }
    (baseline_dir / "baseline.json").write_text(
        json.dumps(metadata), encoding="utf-8"
    )
    return metadata


def fake_tool_runner(value: dict[str, object], commands=None):
    recorded = commands if commands is not None else []

    def run(command, **_kwargs):
        command = [str(item) for item in command]
        recorded.append(command)
        if "--preview" in command:
            snapshot = Path(command[command.index("--snapshot") + 1])
            report_path = Path(command[command.index("--report") + 1])
            snapshot.write_bytes(PNG_FIXTURE)
            report_path.write_text(
                json.dumps(capture_report(snapshot)), encoding="utf-8"
            )
            return subprocess.CompletedProcess(command, 0, "", "")
        report_path = Path(command[command.index("--report") + 1])
        baseline = Path(command[command.index("--baseline") + 1])
        actual = Path(command[command.index("--actual") + 1])
        region = None
        if "--region" in command:
            region = [
                int(item)
                for item in command[command.index("--region") + 1].split(",")
            ]
        report_path.write_text(
            json.dumps(
                comparison_report(
                    baseline,
                    actual,
                    value["defaults"]["pixel"],
                    region,
                )
            ),
            encoding="utf-8",
        )
        return subprocess.CompletedProcess(command, 0, "", "")

    return run


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
            "settle_ms": 250,
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
    def test_read_json_wraps_numeric_and_nesting_parser_limits(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "hostile.json"
            payloads = (
                '{"value":' + ("9" * 5000) + "}",
                '{"value":' + ("[" * 2000) + "0" + ("]" * 2000) + "}",
            )
            for payload in payloads:
                with self.subTest(length=len(payload)):
                    path.write_text(payload, encoding="utf-8")
                    with self.assertRaises(MODULE.VerificationError):
                        MODULE.read_json(path)

    def test_recipe_schema_uses_the_executable_safety_limits(self):
        schema = json.loads(
            SCRIPT.with_name("gui-verification-recipe.schema.json").read_text(
                encoding="utf-8"
            )
        )
        definitions = schema["$defs"]
        self.assertEqual(
            definitions["pixel"]["properties"]["channel_threshold"]["maximum"],
            MODULE.MAX_CHANNEL_THRESHOLD,
        )
        self.assertEqual(
            definitions["pixel"]["properties"]["max_different_ratio"]["maximum"],
            MODULE.MAX_DIFFERENT_RATIO,
        )
        self.assertEqual(
            definitions["geometry"]["properties"]["tolerance"]["maximum"],
            MODULE.MAX_GEOMETRY_TOLERANCE,
        )
        self.assertEqual(
            definitions["inspector"]["properties"]["max_findings"]["maximum"],
            MODULE.MAX_INSPECTOR_BUDGET,
        )
        self.assertEqual(
            definitions["action_script"]["properties"]["steps"]["minItems"],
            1,
        )
        self.assertEqual(
            definitions["action_step"]["properties"]["text"]["minLength"],
            1,
        )
        self.assertEqual(
            definitions["action_step"]["properties"]["target"]["minLength"],
            1,
        )
        self.assertFalse(
            definitions["geometry_probe"]["oneOf"][1]["properties"]["rect"][
                "additionalProperties"
            ]
        )
        action_schema = json.loads(
            SCRIPT.with_name("gallery-preview-actions.schema.json").read_text(
                encoding="utf-8"
            )
        )
        action_step = action_schema["$defs"]["step"]["properties"]
        self.assertEqual(action_step["target"]["minLength"], 1)
        self.assertEqual(action_step["expect"]["minProperties"], 1)
        review_schema = json.loads(
            SCRIPT.with_name("gui-verification-review.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertFalse(
            review_schema["properties"]["reviewer"]["additionalProperties"]
        )
        self.assertIn(
            "pattern", review_schema["properties"]["reviewer"]["properties"]["id"]
        )

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
        value["defaults"]["geometry"]["required"] = ["probe", "probe"]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("duplicate object_name" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["pixel"]["regions"][0]["rect"] = [1, 2, 0, 4]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("positive width" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["pixel"]["regions"][0]["rect"] = [
            0,
            0,
            10**400,
            1,
        ]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("must not exceed" in error for error in errors))
        self.assertEqual(
            len(
                MODULE.pixel_comparisons(
                    value["defaults"]["pixel"], 2.0
                )
            ),
            1,
        )

        value = recipe("baselines/light")
        value["defaults"]["pixel"]["regions"][0]["rect"] = [639, 0, 2, 1]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("logical viewport" in error for error in errors))

        value = recipe("baselines/light")
        value.pop("path_base")
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("path_base" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["review"] = ["   "]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("visual review prompts" in error for error in errors))

    def test_recipe_rejects_explicit_null_for_present_optional_fields(self):
        example = json.loads(
            SCRIPT.with_name("gui-verification.example.json").read_text(
                encoding="utf-8"
            )
        )
        paths = (
            ("selection", "sample"),
            ("environment",),
            ("defaults", "settle_ms"),
            ("defaults", "timeout_seconds"),
            ("defaults", "require_native_desktop"),
            ("defaults", "inspector", "max_findings"),
            ("defaults", "inspector", "max_by_severity"),
            ("defaults", "inspector", "allowed_codes"),
            ("defaults", "geometry", "tolerance"),
            ("defaults", "pixel", "max_different_ratio"),
            ("defaults", "pixel", "regions"),
            ("scenarios", 2, "actions", "steps", 0, "id"),
            ("scenarios", 2, "actions", "steps", 0, "target"),
            ("scenarios", 2, "actions", "steps", 0, "observe"),
            ("scenarios", 2, "actions", "steps", 1, "key"),
            ("scenarios", 2, "actions", "steps", 3, "expect"),
            ("scenarios", 2, "actions", "steps", 5, "milliseconds"),
        )
        for path in paths:
            with self.subTest(path=path):
                changed = copy.deepcopy(example)
                parent = changed
                for part in path[:-1]:
                    parent = parent[part]
                parent[path[-1]] = None
                self.assertTrue(MODULE.validate_recipe(changed))

        for invalid_required in (None, []):
            for index in range(len(example["scenarios"])):
                with self.subTest(
                    path=("scenarios", index, "geometry", "required"),
                    value=invalid_required,
                ):
                    changed = copy.deepcopy(example)
                    changed["scenarios"][index]["geometry"] = {
                        "required": invalid_required
                    }
                    errors = MODULE.validate_recipe(changed)
                    self.assertTrue(
                        any(
                            f"scenarios[{index}].geometry.required" in error
                            for error in errors
                        )
                    )

    def test_recipe_rejects_semantic_and_effectively_unbounded_claims(self):
        value = recipe("baselines/light")
        value["scenarios"][0]["theme"] = "dark"
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("exactly theme:dark" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][1] = "width:normal"
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("does not match viewport width" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["direciton"] = "rtl"
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("unsupported fields: direciton" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"].append("input:keyboard")
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "wait",
                    "milliseconds": 0,
                    "expect": {"visible": True},
                }
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("requires a key or type_text" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][2] = "state:hover"
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("non-default state" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][2] = "state:banana"
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "wait",
                    "milliseconds": 0,
                    "expect": {"visible": True},
                }
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("unsupported state tags" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"].extend(
            ["state:hover", "input:mouse"]
        )
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "mouse_move",
                    "target": "probe",
                    "expect": {"visible": True},
                }
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("exactly one state tag" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][2] = "state:pressed"
        value["scenarios"][0]["tags"].append("input:mouse")
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "mouse_press",
                    "target": "probe",
                    "expect": {"enabled": True},
                },
                {
                    "action": "mouse_release",
                    "target": "probe",
                    "expect": {"enabled": True},
                },
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("pressed state must end" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][2] = "state:hover"
        value["scenarios"][0]["tags"].append("input:mouse")
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "mouse_move",
                    "target": "probe",
                    "expect": {"enabled": True},
                },
                {
                    "action": "mouse_leave",
                    "target": "probe",
                    "expect": {"enabled": True},
                },
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("hover state must end" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][2] = "state:focus"
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "focus",
                    "target": "probe",
                    "expect": {"has_focus": True},
                },
                {
                    "action": "focus",
                    "target": "other",
                    "expect": {"enabled": True},
                },
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("final has_focus=true" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"].append("input:telepathy")
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "key",
                    "target": "probe",
                    "key": "space",
                    "expect": {"checked": True},
                }
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("unsupported input tags" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"][2] = "state:native-input"
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "set_property",
                    "target": "probe",
                    "property": "enabled",
                    "value": True,
                    "expect": {"enabled": True},
                }
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("cannot claim native state" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["actions"] = {"schema_version": 1, "steps": []}
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("non-empty array" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"].append("input:keyboard")
        value["scenarios"][0]["actions"] = {
            "schema_version": 1,
            "steps": [
                {
                    "action": "type_text",
                    "target": "probe",
                    "text": "",
                    "expect": {"visible": True},
                }
            ],
        }
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("requires non-empty text" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["pixel"]["channel_threshold"] = 255
        value["defaults"]["pixel"]["max_different_ratio"] = 1.0
        value["defaults"]["geometry"]["tolerance"] = 999
        value["defaults"]["inspector"]["max_findings"] = 999
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("channel_threshold" in error for error in errors))
        self.assertTrue(any("max_different_ratio" in error for error in errors))
        self.assertTrue(any("tolerance" in error for error in errors))
        self.assertTrue(any("max_findings" in error for error in errors))

        value = recipe("baselines/light")
        probe = {"object_name": "probe"}
        value["defaults"]["geometry"]["required"][0] = probe
        probe["min_width"] = 0
        probe["max_width"] = False
        probe["not_clipped"] = "yes"
        probe["rect"] = {"x": False, "y": 0, "width": 1, "height": 1}
        value["defaults"]["pixel"]["regions"][0]["rect"] = [False, 0, 1, 1]
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("min_width" in error for error in errors))
        self.assertTrue(any("max_width" in error for error in errors))
        self.assertTrue(any("not_clipped" in error for error in errors))
        self.assertTrue(any("rect must contain integer" in error for error in errors))
        self.assertTrue(any("non-negative x,y" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["timeout_seconds"] = 301
        value["defaults"]["settle_ms"] = True
        value["defaults"]["require_native_desktop"] = "yes"
        value["environment"] = {"QT_SCALE_FACTOR": {"nested": True}}
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("timeout_seconds" in error for error in errors))
        self.assertTrue(any("settle_ms" in error for error in errors))
        self.assertTrue(any("require_native_desktop" in error for error in errors))
        self.assertTrue(any("QT_SCALE_FACTOR" in error for error in errors))

        for location in ("defaults", "scenario"):
            with self.subTest(native_null_location=location):
                value = recipe("baselines/light")
                target = (
                    value["defaults"]
                    if location == "defaults"
                    else value["scenarios"][0]
                )
                target["require_native_desktop"] = None
                errors = MODULE.validate_recipe(value)
                self.assertTrue(
                    any("require_native_desktop" in error for error in errors)
                )
                self.assertTrue(
                    MODULE.effective_require_native_desktop(
                        value["defaults"], value["scenarios"][0]
                    )
                )

        value = recipe("baselines/light")
        value["author"]["id"] = "implementation-agent "
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("author.id" in error for error in errors))

        value = recipe("baselines/light")
        value["defaults"]["require_native_desktop"] = False
        value["scenarios"][0]["tags"].append("platform:desktop-qpa")
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("cannot disable desktop QPA" in error for error in errors))

        value = recipe("baselines/light")
        value["scenarios"][0]["tags"].extend(["platform:windows", "qt:5"])
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("unsupported platform tags" in error for error in errors))
        self.assertTrue(any("unsupported coverage categories" in error for error in errors))

        value = recipe("baselines/light")
        width, height = (int(piece) for piece in value["scenarios"][0]["size"].split("x"))
        value["defaults"]["pixel"]["max_different_pixels"] = width * height - 1
        errors = MODULE.validate_recipe(value)
        self.assertTrue(any("10%" in error for error in errors))

    def test_scenario_contract_binds_every_acceptance_policy(self):
        with tempfile.TemporaryDirectory() as temporary:
            action_path = Path(temporary) / "actions.json"
            action_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "steps": [
                            {
                                "action": "key",
                                "target": "probe",
                                "key": "space",
                                "expect": {"checked": True},
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            original = recipe("baselines/light")
            baseline = MODULE.sha256_bytes(
                MODULE.canonical_json(
                    MODULE.scenario_contract(
                        original, original["scenarios"][0], action_path
                    )
                )
            )
            mutations = [
                lambda value: value["defaults"]["pixel"].update(
                    {"channel_threshold": 1}
                ),
                lambda value: value["defaults"]["geometry"].update(
                    {"tolerance": 1}
                ),
                lambda value: value["defaults"]["inspector"].update(
                    {"max_findings": 1}
                ),
                lambda value: value["coverage"]["required_tags"].append(
                    "direction:ltr"
                ),
                lambda value: value["scenarios"][0]["tags"].append(
                    "direction:ltr"
                ),
                lambda value: value["scenarios"][0]["review"].append(
                    "Check a second region."
                ),
                lambda value: value["scenarios"][0].update(
                    {"baseline": "baselines/other"}
                ),
                lambda value: value.setdefault("environment", {}).update(
                    {"QT_SCALE_FACTOR": "2"}
                ),
            ]
            for mutate in mutations:
                changed = copy.deepcopy(original)
                mutate(changed)
                digest = MODULE.sha256_bytes(
                    MODULE.canonical_json(
                        MODULE.scenario_contract(
                            changed, changed["scenarios"][0], action_path
                        )
                    )
                )
                self.assertNotEqual(digest, baseline)

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

    def test_platform_baseline_keys_use_repository_names(self):
        path_base = Path("/tmp/baselines")
        cases = (
            ("Darwin", "arm64", "macos-arm64"),
            ("Windows", "AMD64", "windows-x64"),
            ("Linux", "x86_64", "linux-x64"),
        )
        for system, machine, expected_key in cases:
            with self.subTest(system=system, machine=machine):
                with (
                    mock.patch.object(MODULE.platform, "system", return_value=system),
                    mock.patch.object(MODULE.platform, "machine", return_value=machine),
                ):
                    self.assertEqual(MODULE.host_key()[1], expected_key)
                    self.assertEqual(
                        MODULE.select_baseline(
                            {expected_key: "approved"}, path_base
                        ),
                        path_base.resolve() / "approved",
                    )

    def test_approved_baseline_destination_requires_two_safe_segments(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline_root = root / "tests/visual-baselines/gui"
            with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                self.assertTrue(
                    MODULE.is_approved_baseline_bundle_path(
                        baseline_root / "combobox" / "light-normal"
                    )
                )
                for invalid in (
                    baseline_root,
                    baseline_root / "combobox",
                    baseline_root / "combobox/light/extra",
                    baseline_root / "ComboBox/light",
                ):
                    with self.subTest(invalid=invalid):
                        self.assertFalse(
                            MODULE.is_approved_baseline_bundle_path(invalid)
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
            {
                "code": "text.clipped",
                "category": "text",
                "severity": "error",
                "path": "QWidget#probe",
                "rect": {"x": 17, "y": 20, "width": 120, "height": 32},
                "message": "Text is clipped.",
                "details": {},
            }
        ]
        report["quality_report"]["summary"] = {
            "findings": 1,
            "by_severity": {"info": 0, "warning": 0, "error": 1},
            "by_category": {"text": 1},
        }
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

    def test_malformed_nested_reports_cannot_pass(self):
        report = capture_report(Path("actual.png"))
        report["quality_report"]["findings"] = ["malformed"]
        report["geometry_report"] = {
            "schema_version": 1,
            "widgets": [{"object_name": "probe"}],
        }
        inspector = MODULE.inspector_check(report, {"max_findings": 0})
        geometry = MODULE.geometry_contract_check(
            report, {"required": ["probe"]}
        )
        self.assertEqual(inspector["status"], "incomplete")
        self.assertEqual(geometry["status"], "incomplete")

        report = capture_report(Path("actual.png"))
        report["quality_report"]["root"] = {}
        report["geometry_report"]["widgets"][0]["visible_rect"] = {
            "x": 0,
            "y": 0,
            "width": 0,
            "height": 0,
        }
        self.assertEqual(
            MODULE.inspector_check(report, {"max_findings": 0})["status"],
            "incomplete",
        )
        self.assertEqual(
            MODULE.geometry_contract_check(
                report, {"required": ["probe"]}
            )["status"],
            "incomplete",
        )

    def test_required_zero_size_geometry_probe_cannot_pass(self):
        report = capture_report(Path("actual.png"))
        widget = report["geometry_report"]["widgets"][0]
        widget["rect"] = {"x": 10, "y": 10, "width": 0, "height": 0}
        widget["visible_rect"] = copy.deepcopy(widget["rect"])
        widget["clipped"] = False

        self.assertEqual(MODULE.geometry_report_errors(report["geometry_report"]), [])
        result = MODULE.geometry_contract_check(
            report, {"required": ["probe"]}
        )
        self.assertEqual(result["status"], "fail")
        self.assertIn("zero-size rect", result["message"])
        self.assertIn("no visible area", result["message"])

    def test_capture_identity_binds_tool_scene_snapshot_and_action_requests(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            actual = root / "actual.png"
            actual.write_bytes(PNG_FIXTURE)
            action_path = root / "actions.json"
            action_script = {
                "schema_version": 1,
                "steps": [
                    {
                        "id": "toggle",
                        "action": "key",
                        "target": "probe",
                        "key": "space",
                        "expect": {"checked": True},
                    }
                ],
            }
            action_path.write_text(json.dumps(action_script), encoding="utf-8")
            value = recipe("baseline")
            report = capture_report(actual)
            report["environment"]["locale"] = "SUPER_SECRET_LOCALE"
            report["environment"]["font"]["family"] = (
                "/Users/private/fonts/Secret Font"
            )
            report["interaction_report"] = {
                "schema_version": 1,
                "requested": True,
                "source": str(action_path),
                "status": "pass",
                "summary": {"total": 1, "executed": 1, "passed": 1, "failed": 0},
                "steps": [
                    {
                        "index": 0,
                        "request": action_script["steps"][0],
                        "status": "pass",
                    }
                ],
            }
            report["geometry_report"]["widgets"][0][
                "accessible_name"
            ] = "/Users/private/.ssh/id_ed25519 SUPER_SECRET"
            report["quality_report"]["findings"] = [
                {
                    "code": "private-path",
                    "category": "text",
                    "severity": "info",
                    "path": "/Users/private/.ssh/id_ed25519",
                    "rect": {"x": 0, "y": 0, "width": 1, "height": 1},
                    "message": "SUPER_SECRET",
                    "details": {"path": "/Users/private/.ssh/id_ed25519"},
                }
            ]
            report["quality_report"]["summary"] = {
                "findings": 1,
                "by_severity": {"info": 1, "warning": 0, "error": 0},
                "by_category": {"text": 1},
            }
            report["environment"]["scale_environment"][
                "QT_SCREEN_SCALE_FACTORS"
            ] = "/Users/private/.ssh/id_ed25519 SUPER_SECRET"
            checks = MODULE.identity_checks(
                value, value["scenarios"][0], report, action_path, actual
            )
            self.assertTrue(all(item["status"] == "pass" for item in checks))

            changed = copy.deepcopy(report)
            changed["tool"] = "Not FluentQt"
            changed["scene"]["settle_ms"] = 0
            changed["artifacts"]["snapshot"]["path"] = str(root / "other.png")
            changed["interaction_report"]["steps"][0]["request"] = {
                "action": "wait",
                "milliseconds": 0,
            }
            statuses = {
                item["id"]: item["status"]
                for item in MODULE.identity_checks(
                    value,
                    value["scenarios"][0],
                    changed,
                    action_path,
                    actual,
                )
            }
            self.assertEqual(statuses["capture.schema"], "incomplete")
            self.assertEqual(statuses["capture.scene"], "fail")
            self.assertEqual(statuses["capture.snapshot"], "incomplete")
            self.assertEqual(statuses["capture.interactions"], "fail")

    def test_capture_environment_requires_complete_fingerprint(self):
        complete = capture_report(Path("actual.png"))
        complete["artifacts"]["snapshot"].update(
            {
                "requested": False,
                "written": False,
                "path": "",
                "sha256": "",
                "error": "",
            }
        )
        sanitized = MODULE.sanitized_baseline_report(
            complete, {"required": ["probe"], "tolerance": 0}
        )
        self.assertEqual(
            MODULE.capture_environment_errors(complete["environment"]),
            [],
        )
        self.assertEqual(len(sanitized["environment_sha256"]), 64)
        self.assertTrue(MODULE.is_sha256(sanitized["environment_sha256"]))

        mutations = (
            lambda report: report["environment"].pop("qt_version"),
            lambda report: report["environment"].pop("device_pixel_ratio"),
            lambda report: report["environment"].update(
                {"device_pixel_ratio": float("inf")}
            ),
            lambda report: report["environment"].update(
                {"device_pixel_ratio": float("nan")}
            ),
            lambda report: report["environment"].update(
                {"device_pixel_ratio": 1e308}
            ),
            lambda report: report["environment"].update(
                {"device_pixel_ratio": 10**400}
            ),
            lambda report: report["environment"]["system"].pop(
                "cpu_architecture"
            ),
        )
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                changed = copy.deepcopy(complete)
                mutate(changed)
                self.assertTrue(
                    MODULE.capture_environment_errors(changed["environment"])
                )
                self.assertEqual(
                    MODULE.capture_environment_check(changed)["status"],
                    "incomplete",
                )
                self.assertEqual(
                    MODULE.native_desktop_check(changed, True)["status"],
                    "incomplete",
                )
                self.assertEqual(
                    MODULE.fingerprint_check(changed, sanitized)["status"],
                    "human-required",
                )

    def test_capture_snapshot_must_match_scene_native_resolution(self):
        with tempfile.TemporaryDirectory() as temporary:
            actual = Path(temporary) / "actual.png"
            actual.write_bytes(solid_png(640, 480))
            value = recipe("baselines/light")
            report = capture_report(actual)
            statuses = {
                item["id"]: item["status"]
                for item in MODULE.identity_checks(
                    value,
                    value["scenarios"][0],
                    report,
                    None,
                    actual,
                )
            }
            self.assertEqual(statuses["capture.snapshot"], "incomplete")

    def test_capture_report_is_closed_schema_and_sanitized_projection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            actual = root / "actual.png"
            actual.write_bytes(PNG_FIXTURE)
            report = capture_report(actual)
            report["environment"]["locale"] = "SUPER_SECRET_LOCALE"
            report["environment"]["font"]["family"] = (
                "/Users/private/fonts/Secret Font"
            )
            report["environment"]["scale_environment"][
                "QT_SCREEN_SCALE_FACTORS"
            ] = "/Users/private/.ssh/id_ed25519 SUPER_SECRET"
            report["geometry_report"]["widgets"][0][
                "accessible_name"
            ] = "/Users/private/.ssh/id_ed25519 SUPER_SECRET"
            secret_widget = copy.deepcopy(
                report["geometry_report"]["widgets"][0]
            )
            secret_widget["path"] = "QWidget#SUPER_SECRET_WIDGET"
            secret_widget["object_name"] = "SUPER_SECRET_WIDGET"
            report["geometry_report"]["widgets"].append(secret_widget)
            report["geometry_report"]["widget_count"] = 2
            report["quality_report"]["findings"] = [
                {
                    "code": "private-path",
                    "category": "text",
                    "severity": "info",
                    "path": "/Users/private/.ssh/id_ed25519",
                    "rect": {"x": 0, "y": 0, "width": 1, "height": 1},
                    "message": "SUPER_SECRET",
                    "details": {"path": "/Users/private/.ssh/id_ed25519"},
                }
            ]
            report["quality_report"]["summary"] = {
                "findings": 1,
                "by_severity": {"info": 1, "warning": 0, "error": 0},
                "by_category": {"text": 1},
            }
            report["interaction_report"] = {
                "schema_version": 1,
                "requested": True,
                "source": str(root / "actions.json"),
                "status": "pass",
                "summary": {"total": 1, "executed": 1, "passed": 1, "failed": 0},
                "steps": [
                    {
                        "index": 0,
                        "id": "type-secret",
                        "action": "type_text",
                        "target": "probe",
                        "mechanism": "event",
                        "request": {
                            "action": "type_text",
                            "target": "probe",
                            "text": "/Users/private/.ssh/id_ed25519 SUPER_SECRET",
                        },
                        "expect": {"text": "SUPER_SECRET"},
                        "observation": {
                            "properties": {
                                "text": "/Users/private/.ssh/id_ed25519"
                            }
                        },
                        "message": "/Users/private/.ssh/id_ed25519 SUPER_SECRET",
                        "status": "pass",
                    }
                ],
            }
            self.assertEqual(
                MODULE.capture_report_errors(report), []
            )
            sanitized = MODULE.sanitized_baseline_report(
                report, {"required": ["probe"], "tolerance": 0}
            )
            self.assertEqual(MODULE.baseline_report_errors(sanitized), [])
            self.assertEqual(
                set(sanitized),
                {
                    "schema_version",
                    "tool",
                    "environment_sha256",
                    "geometry_report",
                    "interaction_report",
                    "quality_report",
                },
            )
            sanitized_text = json.dumps(sanitized)
            self.assertNotIn("SUPER_SECRET", sanitized_text)
            self.assertNotIn("/Users/private", sanitized_text)
            self.assertEqual(
                set(sanitized["interaction_report"]["steps"][0]),
                {"index", "action", "status"},
            )
            self.assertNotIn("environment", sanitized)
            self.assertTrue(MODULE.is_sha256(sanitized["environment_sha256"]))

            baseline_mutations = (
                lambda value: value.update({"environment_sha256": "not-a-digest"}),
                lambda value: value["geometry_report"].update(
                    {"widget_count": True}
                ),
                lambda value: value["interaction_report"]["steps"][0].update(
                    {"index": 1}
                ),
                lambda value: value["interaction_report"]["summary"].update(
                    {"total": 2}
                ),
                lambda value: value["quality_report"]["summary"][
                    "by_severity"
                ].update({"info": 2}),
            )
            for mutate_baseline in baseline_mutations:
                with self.subTest(mutate_baseline=mutate_baseline):
                    changed_baseline = copy.deepcopy(sanitized)
                    mutate_baseline(changed_baseline)
                    self.assertTrue(
                        MODULE.baseline_report_errors(changed_baseline)
                    )

            extra_widget_baseline = copy.deepcopy(sanitized)
            extra_widget_baseline["geometry_report"]["widgets"].append(
                {
                    "object_name": "/Users/private/.ssh/id_ed25519 SUPER_SECRET",
                    "rect": {"x": 0, "y": 0, "width": 1, "height": 1},
                }
            )
            extra_widget_baseline["geometry_report"]["widget_count"] = 2
            self.assertEqual(
                MODULE.baseline_report_errors(extra_widget_baseline), []
            )
            extra_check = MODULE.geometry_baseline_check(
                report,
                extra_widget_baseline,
                {"required": ["probe"], "tolerance": 0},
            )
            self.assertEqual(extra_check["status"], "fail")
            self.assertIn("exactly", extra_check["message"])

            mutations = (
                lambda value: value.update({"private_path": "/private/token"}),
                lambda value: value["selection"].update({"SECRET": "token"}),
                lambda value: value["artifacts"].update({"SECRET": "token"}),
                lambda value: value["interaction_report"].update(
                    {"SECRET": "token"}
                ),
            )
            for mutate in mutations:
                with self.subTest(mutate=mutate):
                    changed = copy.deepcopy(report)
                    mutate(changed)
                    self.assertTrue(
                        MODULE.capture_report_errors(changed)
                    )
                    self.assertEqual(
                        MODULE.capture_report_check(changed)["status"],
                        "incomplete",
                    )
                    with self.assertRaises(MODULE.VerificationError):
                        MODULE.sanitized_baseline_report(
                            changed, {"required": ["probe"], "tolerance": 0}
                        )

    def test_native_desktop_requires_os_consistent_qpa_kernel(self):
        cases = (
            ("xcb", "ubuntu", "linux", "Linux", "pass"),
            ("wayland", "debian", "linux", "Linux", "pass"),
            ("xcb", "freebsd", "freebsd", "FreeBSD", "incomplete"),
            ("xcb", "android", "linux", "Linux", "incomplete"),
            ("xcb", "haiku", "haiku", "Haiku", "incomplete"),
            ("cocoa", "macos", "linux", "Linux", "incomplete"),
            ("windows", "windows", "winnt", "Windows", "pass"),
        )
        for plugin, product, kernel, host_system, expected in cases:
            with self.subTest(plugin=plugin, product=product, kernel=kernel):
                report = capture_report(Path("actual.png"))
                report["environment"]["platform_plugin"] = plugin
                report["environment"]["system"]["product_type"] = product
                report["environment"]["system"]["kernel_type"] = kernel
                with (
                    mock.patch.object(
                        MODULE.platform, "system", return_value=host_system
                    ),
                    mock.patch.object(
                        MODULE.platform, "machine", return_value="arm64"
                    ),
                ):
                    self.assertEqual(
                        MODULE.native_desktop_check(report, True)["status"],
                        expected,
                    )

    def test_capture_architecture_must_match_baseline_routing_host(self):
        report = capture_report(Path("actual.png"))
        with (
            mock.patch.object(MODULE.platform, "system", return_value="Darwin"),
            mock.patch.object(MODULE.platform, "machine", return_value="arm64"),
        ):
            self.assertEqual(MODULE.host_key()[1], "macos-arm64")
            self.assertEqual(
                MODULE.native_desktop_check(report, True)["status"], "pass"
            )
            report["environment"]["system"]["cpu_architecture"] = "x86_64"
            result = MODULE.native_desktop_check(report, True)
            self.assertEqual(result["status"], "incomplete")
            self.assertIn("baseline routing", result["message"])

    def test_capture_snapshot_uses_qt_half_up_fractional_scaling(self):
        with tempfile.TemporaryDirectory() as temporary:
            actual = Path(temporary) / "actual.png"
            actual.write_bytes(solid_png(965, 722))
            value = recipe("baselines/light")
            value["scenarios"][0]["size"] = "643x481"
            report = capture_report(actual)
            report["environment"]["device_pixel_ratio"] = 1.5
            for field, dimension in (
                ("requested_width", 643),
                ("actual_width", 643),
                ("requested_height", 481),
                ("actual_height", 481),
            ):
                report["scene"][field] = dimension
            statuses = {
                item["id"]: item["status"]
                for item in MODULE.identity_checks(
                    value,
                    value["scenarios"][0],
                    report,
                    None,
                    actual,
                )
            }
            self.assertEqual(statuses["capture.scene"], "pass")
            self.assertEqual(statuses["capture.snapshot"], "pass")

    def test_pixel_regions_use_qt_half_up_fractional_scaling(self):
        policy = recipe("baselines/light")["defaults"]["pixel"]
        comparisons = MODULE.pixel_comparisons(policy, 1.5)
        self.assertEqual(comparisons[1][2], [2, 3, 5, 6])

    def test_extreme_dpr_fails_closed_without_rounding_overflow(self):
        for device_pixel_ratio in (1e308, 10**400):
            with self.subTest(device_pixel_ratio=type(device_pixel_ratio).__name__):
                with tempfile.TemporaryDirectory() as temporary:
                    actual = Path(temporary) / "actual.png"
                    actual.write_bytes(PNG_FIXTURE)
                    value = recipe("baselines/light")
                    report = capture_report(actual)
                    report["environment"][
                        "device_pixel_ratio"
                    ] = device_pixel_ratio
                    statuses = {
                        item["id"]: item["status"]
                        for item in MODULE.identity_checks(
                            value,
                            value["scenarios"][0],
                            report,
                            None,
                            actual,
                        )
                    }
                    self.assertEqual(
                        statuses["capture.snapshot"], "incomplete"
                    )
                    self.assertEqual(
                        len(
                            MODULE.pixel_comparisons(
                                value["defaults"]["pixel"],
                                device_pixel_ratio,
                            )
                        ),
                        1,
                    )
        with tempfile.TemporaryDirectory() as temporary:
            actual = Path(temporary) / "actual.png"
            actual.write_bytes(PNG_FIXTURE)
            value = recipe("baselines/light")
            report = capture_report(actual)
            report["scene"]["actual_width"] = 10**400
            statuses = {
                item["id"]: item["status"]
                for item in MODULE.identity_checks(
                    value,
                    value["scenarios"][0],
                    report,
                    None,
                    actual,
                )
            }
            self.assertEqual(statuses["capture.snapshot"], "incomplete")
            self.assertTrue(
                MODULE.capture_report_errors(report)
            )

    def test_png_integrity_rejects_invalid_chunk_types(self):
        valid = solid_png(1, 1)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "invalid.png"
            for chunk_type in (b"abcd", b"a1Aa"):
                with self.subTest(chunk_type=chunk_type):
                    path.write_bytes(
                        valid[:33]
                        + png_chunk(chunk_type, b"")
                        + valid[33:]
                    )
                    self.assertIsNone(MODULE.png_dimensions(path))

    def test_comparator_report_binds_inputs_policy_and_translation_confidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline = root / "baseline.png"
            actual = root / "actual.png"
            baseline.write_bytes(PNG_FIXTURE)
            actual.write_bytes(PNG_FIXTURE)
            policy = recipe("baseline")["defaults"]["pixel"]
            report = comparison_report(baseline, actual, policy)
            self.assertEqual(
                MODULE.validate_comparator_report(
                    report, 0, baseline, actual, None, policy
                ),
                [],
            )

            valid_report = copy.deepcopy(report)
            baseline.write_bytes(PNG_FIXTURE[:33])
            actual.write_bytes(PNG_FIXTURE[:33])
            valid_report["inputs"]["baseline_sha256"] = MODULE.sha256_file(
                baseline
            )
            valid_report["inputs"]["actual_sha256"] = MODULE.sha256_file(actual)
            errors = MODULE.validate_comparator_report(
                valid_report, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("valid PNG" in error for error in errors))
            baseline.write_bytes(PNG_FIXTURE)
            actual.write_bytes(PNG_FIXTURE)

            changed = copy.deepcopy(report)
            changed["inputs"]["actual_sha256"] = "0" * 64
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("actual digest" in error for error in errors))

            changed = copy.deepcopy(report)
            changed["policy"]["channel_threshold"] = 64
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("requested policy" in error for error in errors))

            changed = copy.deepcopy(report)
            changed["baseline_size"]["width"] = 2
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("baseline size" in error for error in errors))

            changed = copy.deepcopy(report)
            changed["metrics"]["total_pixels"] = 2
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("compared image size" in error for error in errors))

            changed = copy.deepcopy(report)
            pixel_count = 1280 * 960
            changed["metrics"]["total_pixels"] = float(pixel_count)
            changed["metrics"]["different_pixels"] = 0.5
            changed["metrics"]["different_ratio"] = 0.5 / pixel_count
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("positive integer" in error for error in errors))
            self.assertTrue(any("different_pixels" in error for error in errors))

            changed = copy.deepcopy(report)
            changed["metrics"]["estimated_translation"]["dx"] = 0.5
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(
                any("estimated_translation.dx" in error for error in errors)
            )

            changed = copy.deepcopy(report)
            changed["metrics"]["different_pixels"] = 1
            changed["metrics"]["different_ratio"] = 1 / pixel_count
            errors = MODULE.validate_comparator_report(
                changed, 0, baseline, actual, None, policy
            )
            self.assertTrue(any("pixel limit check" in error for error in errors))

            tolerant_policy = copy.deepcopy(policy)
            tolerant_policy["max_different_pixels"] = 1
            tolerant_report = comparison_report(
                baseline, actual, tolerant_policy
            )
            tolerant_report["metrics"]["different_pixels"] = 1
            tolerant_report["metrics"]["different_ratio"] = 1 / pixel_count
            self.assertEqual(
                MODULE.validate_comparator_report(
                    tolerant_report,
                    0,
                    baseline,
                    actual,
                    None,
                    tolerant_policy,
                ),
                [],
            )

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
            baseline_image.write_bytes(PNG_FIXTURE)
            baseline_report = baseline_dir / "baseline-report.json"
            baseline_report.write_text(
                json.dumps(
                    MODULE.sanitized_baseline_report(
                        capture_report(baseline_image),
                        value["defaults"]["geometry"],
                    )
                ),
                encoding="utf-8",
            )
            contract = MODULE.scenario_contract(
                value, value["scenarios"][0], None
            )
            contract_sha256 = MODULE.sha256_bytes(
                MODULE.canonical_json(contract)
            )
            write_approved_baseline_metadata(
                baseline_dir, value, contract_sha256
            )

            commands = []

            def fake_run(command, **_kwargs):
                command = [str(item) for item in command]
                commands.append(command)
                if "--preview" in command:
                    snapshot = Path(command[command.index("--snapshot") + 1])
                    report_path = Path(command[command.index("--report") + 1])
                    snapshot.write_bytes(PNG_FIXTURE)
                    report_path.write_text(
                        json.dumps(capture_report(snapshot)), encoding="utf-8"
                    )
                    return subprocess.CompletedProcess(command, 0, "", "")
                report_path = Path(command[command.index("--report") + 1])
                baseline = Path(command[command.index("--baseline") + 1])
                actual = Path(command[command.index("--actual") + 1])
                region = None
                if "--region" in command:
                    region = [
                        int(item)
                        for item in command[command.index("--region") + 1].split(",")
                    ]
                report_path.write_text(
                    json.dumps(
                        comparison_report(
                            baseline,
                            actual,
                            value["defaults"]["pixel"],
                            region,
                        )
                    ),
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

            def capture_without_comparator_output(command, **_kwargs):
                command = [str(item) for item in command]
                if "--preview" in command:
                    snapshot = Path(command[command.index("--snapshot") + 1])
                    report_path = Path(command[command.index("--report") + 1])
                    snapshot.write_bytes(PNG_FIXTURE)
                    report_path.write_text(
                        json.dumps(capture_report(snapshot)), encoding="utf-8"
                    )
                return subprocess.CompletedProcess(command, 0, "", "")

            with mock.patch.object(
                MODULE.subprocess,
                "run",
                side_effect=capture_without_comparator_output,
            ):
                stale_pixel_result = MODULE.run_scenario(
                    value,
                    recipe_path,
                    value["scenarios"][0],
                    output,
                    root / "gallery",
                    root / "comparator",
                )
            pixel_checks = {
                item["id"]: item["status"]
                for item in stale_pixel_result["checks"]
                if str(item["id"]).startswith("pixels.")
            }
            self.assertTrue(pixel_checks)
            self.assertTrue(
                all(status == "incomplete" for status in pixel_checks.values())
            )

            source_path = baseline_dir / "source-evidence.json"
            source = json.loads(source_path.read_text(encoding="utf-8"))
            source["scenario"]["contract_sha256"] = "0" * 64
            source_path.write_text(json.dumps(source), encoding="utf-8")
            _metadata, _report, checks = MODULE.baseline_bundle(
                baseline_dir,
                value["id"],
                value["scenarios"][0]["id"],
                contract_sha256,
                value["author"]["id"],
            )
            self.assertEqual(checks[0]["status"], "fail")

    def test_reused_output_directory_cannot_reuse_stale_capture(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recipe_path = root / "recipe.json"
            value = recipe("baselines/missing")
            value["path_base"] = "recipe"
            recipe_path.write_text(json.dumps(value), encoding="utf-8")
            output = root / "output"

            def write_capture(command, **_kwargs):
                command = [str(item) for item in command]
                snapshot = Path(command[command.index("--snapshot") + 1])
                report_path = Path(command[command.index("--report") + 1])
                snapshot.write_bytes(PNG_FIXTURE)
                report_path.write_text(
                    json.dumps(capture_report(snapshot)), encoding="utf-8"
                )
                return subprocess.CompletedProcess(command, 0, "", "")

            with mock.patch.object(MODULE.subprocess, "run", side_effect=write_capture):
                first = MODULE.run_scenario(
                    value,
                    recipe_path,
                    value["scenarios"][0],
                    output,
                    root / "gallery",
                    root / "comparator",
                )
            self.assertEqual(first["pre_baseline_status"], "pass")

            with mock.patch.object(
                MODULE.subprocess,
                "run",
                return_value=subprocess.CompletedProcess([], 0, "", ""),
            ):
                second = MODULE.run_scenario(
                    value,
                    recipe_path,
                    value["scenarios"][0],
                    output,
                    root / "gallery",
                    root / "comparator",
                )
            self.assertEqual(second["status"], "incomplete")
            self.assertFalse(
                (output / "scenarios" / "light-narrow-default" / "actual.png").exists()
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
            valid_review = copy.deepcopy(review)

            review["reviewer"]["id"] = "implementation-agent"
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("differ" in error for error in errors))

            review["reviewer"]["id"] = "implementation-agent "
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("reviewer.id" in error for error in errors))

            review["reviewer"]["id"] = "visual-reviewer"
            review["evidence_sha256"] = "0" * 64
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("sha256" in error for error in errors))

            review = copy.deepcopy(valid_review)
            review["reviewed_scenarios"] = ["light", "light"]
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("duplicates" in error for error in errors))

            review = copy.deepcopy(valid_review)
            review["reviewed_scenarios"] = ["light", "unknown"]
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("unknown scenarios" in error for error in errors))

            review = copy.deepcopy(valid_review)
            review["attestation"].pop("interaction_evidence_reviewed")
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(
                any("interaction_evidence_reviewed" in error for error in errors)
            )

            review = copy.deepcopy(valid_review)
            review["summary"] = "   "
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("summary" in error for error in errors))

            review = copy.deepcopy(valid_review)
            review["verdict"] = "fail"
            review["findings"] = [
                {
                    "severity": "minor",
                    "scenario_id": "light",
                    "region": "   ",
                    "artifact": "actual.png",
                    "message": "Visible spacing issue.",
                }
            ]
            errors = MODULE.validate_review(evidence, evidence_path, review)
            self.assertTrue(any("findings[0].region" in error for error in errors))

    def test_build_failure_writes_an_empty_scenario_summary(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recipe_path = root / "recipe.json"
            recipe_path.write_text(
                json.dumps(recipe("tests/visual-baselines/gui/probe/light")),
                encoding="utf-8",
            )
            output = root / "output"
            args = argparse.Namespace(
                recipe=recipe_path,
                output_dir=output,
                replace_output=False,
                no_build=False,
                preset="test",
                build_dir=None,
                gallery=None,
                comparator=None,
            )
            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                mock.patch.object(
                    MODULE,
                    "build_dependencies",
                    return_value={"requested": True, "status": "fail"},
                ),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(MODULE.run_recipe(args), 1)
            evidence = json.loads(
                (output / "evidence.json").read_text(encoding="utf-8")
            )
            self.assertEqual(evidence["scenarios"], [])
            self.assertEqual(evidence["summary"], {"total": 0, "by_status": {}})

    def test_run_output_cannot_overlap_approved_baseline_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            value = recipe("baselines/light")
            value["path_base"] = "recipe"
            recipe_path = root / "recipe.json"
            recipe_path.write_text(json.dumps(value), encoding="utf-8")
            args = argparse.Namespace(
                recipe=recipe_path,
                output_dir=root / "baselines/light",
                replace_output=True,
            )
            with self.assertRaisesRegex(
                MODULE.VerificationError, "must not overlap"
            ):
                MODULE.run_recipe(args)

    def test_baseline_approval_refuses_self_approval(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            value = recipe("tests/visual-baselines/gui/probe/light")
            recipe_path = root / "recipe.json"
            recipe_path.write_text(json.dumps(value), encoding="utf-8")
            output = root / "output"
            scenario_id = value["scenarios"][0]["id"]
            build_dir = root / "build/test"
            gallery = build_dir / "app/fluent_qt_gallery"
            comparator = build_dir / "tools/dev/fluent_qt_visual_compare"
            gallery.parent.mkdir(parents=True)
            comparator.parent.mkdir(parents=True)
            gallery.write_bytes(b"gallery")
            comparator.write_bytes(b"comparator")
            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                mock.patch.object(
                    MODULE.subprocess,
                    "run",
                    side_effect=fake_tool_runner(value),
                ),
            ):
                scenario = MODULE.run_scenario(
                    value,
                    recipe_path,
                    value["scenarios"][0],
                    output,
                    gallery,
                    comparator,
                )
            self.assertEqual(scenario["pre_baseline_status"], "pass")
            self.assertEqual(scenario["status"], "human-required")
            evidence = {
                "schema_version": 1,
                "tool": "FluentQt GUI Verify",
                "status": "human-required",
                "deterministic_status": "human-required",
                "recipe": evidence_recipe_record(value, recipe_path),
                "binaries": {
                    "build_dir": str(build_dir),
                    "gallery": {
                        "path": str(gallery),
                        "sha256": MODULE.sha256_file(gallery),
                    },
                    "comparator": {
                        "path": str(comparator),
                        "sha256": MODULE.sha256_file(comparator),
                    },
                },
                "scenarios": [scenario],
                "summary": {"total": 1, "by_status": {"human-required": 1}},
            }
            evidence_path = root / "evidence.json"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            args = argparse.Namespace(
                evidence=evidence_path,
                scenario=scenario_id,
                baseline_dir=None,
                approved_by="implementation-agent",
                approver_kind="ai",
                approval_note="self review",
                replace=False,
            )
            with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                with self.assertRaises(MODULE.VerificationError):
                    MODULE.approve_baseline(args)

            args.approved_by = "implementation-agent "
            with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                with self.assertRaises(MODULE.VerificationError):
                    MODULE.approve_baseline(args)

            args.approved_by = "independent-reviewer"
            args.approver_kind = "human"
            args.approval_note = "Reviewed the native-resolution capture."
            overlapping_baseline = (
                root / "tests/visual-baselines/gui/probe/light"
            )
            overlapping_baseline.mkdir(parents=True)
            overlapping_evidence = overlapping_baseline / "evidence.json"
            overlapping_evidence.write_text(
                json.dumps(evidence), encoding="utf-8"
            )
            args.evidence = overlapping_evidence
            with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                with self.assertRaisesRegex(
                    MODULE.VerificationError, "overlaps immutable"
                ):
                    MODULE.approve_baseline(args)
            args.evidence = evidence_path
            overlapping_evidence.unlink()
            overlapping_baseline.rmdir()
            actual_path = Path(scenario["artifacts"]["actual"])
            actual_path.write_bytes(CHANGED_PNG_FIXTURE)
            with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                with self.assertRaisesRegex(MODULE.VerificationError, "digest"):
                    MODULE.approve_baseline(args)
            actual_path.write_bytes(PNG_FIXTURE)
            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(MODULE.approve_baseline(args), 0)
            baseline_dir = root / "tests/visual-baselines/gui/probe/light"
            metadata = json.loads(
                (baseline_dir / "baseline.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["recipe_id"], "gui-probe")
            self.assertEqual(
                metadata["scenario_contract_sha256"], scenario["contract_sha256"]
            )
            metadata_path = baseline_dir / "baseline.json"
            for mutate in (
                lambda value: value.update(
                    {"external_signature_verified": True}
                ),
                lambda value: value.update({"approval_note": 1}),
            ):
                with self.subTest(metadata_mutation=mutate):
                    changed_metadata = copy.deepcopy(metadata)
                    mutate(changed_metadata)
                    metadata_path.write_text(
                        json.dumps(changed_metadata), encoding="utf-8"
                    )
                    _metadata, _report, checks = MODULE.baseline_bundle(
                        baseline_dir,
                        "gui-probe",
                        scenario_id,
                        scenario["contract_sha256"],
                        "implementation-agent",
                    )
                    self.assertNotEqual(MODULE.combined_status(checks), "pass")
            metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
            provenance_text = (
                baseline_dir / "source-evidence.json"
            ).read_text(encoding="utf-8")
            provenance = json.loads(provenance_text)
            self.assertEqual(
                provenance["tool"], "FluentQt GUI Baseline Provenance"
            )
            baseline_report_text = (
                baseline_dir / "baseline-report.json"
            ).read_text(encoding="utf-8")
            for checked_text in (provenance_text, baseline_report_text):
                self.assertNotIn(str(root), checked_text)
                self.assertNotIn("PRIVATE-SERIAL-123", checked_text)
                self.assertNotIn("Private Display Name", checked_text)
                self.assertNotIn("Private Manufacturer", checked_text)
                self.assertNotIn("Private Model", checked_text)
                self.assertNotIn("stdout", checked_text)
                self.assertNotIn("stderr", checked_text)
            self.assertNotIn("path", provenance_text)

            bundle_files = [
                baseline_dir / "baseline.png",
                baseline_dir / "baseline-report.json",
                baseline_dir / "source-evidence.json",
                baseline_dir / "baseline.json",
            ]
            original_hashes = {
                path.name: MODULE.sha256_file(path) for path in bundle_files
            }
            comparator.write_bytes(b"stale-comparator")
            args.replace = True
            with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                with self.assertRaisesRegex(
                    MODULE.VerificationError, "stale comparator binary provenance"
                ):
                    MODULE.approve_baseline(args)
            self.assertEqual(
                {path.name: MODULE.sha256_file(path) for path in bundle_files},
                original_hashes,
            )

    def test_finalize_is_the_only_successful_final_decision(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            value = recipe("tests/visual-baselines/gui/probe/light")
            recipe_path = root / "recipe.json"
            recipe_path.write_text(json.dumps(value), encoding="utf-8")
            baseline_dir = root / "tests/visual-baselines/gui/probe/light"
            baseline_dir.mkdir(parents=True)
            baseline_image = baseline_dir / "baseline.png"
            baseline_image.write_bytes(PNG_FIXTURE)
            baseline_report = baseline_dir / "baseline-report.json"
            baseline_report.write_text(
                json.dumps(
                    MODULE.sanitized_baseline_report(
                        capture_report(baseline_image),
                        value["defaults"]["geometry"],
                    )
                ),
                encoding="utf-8",
            )
            contract = MODULE.scenario_contract(
                value, value["scenarios"][0], None
            )
            contract_sha256 = MODULE.sha256_bytes(MODULE.canonical_json(contract))
            write_approved_baseline_metadata(
                baseline_dir,
                value,
                contract_sha256,
                approved_by="baseline-reviewer",
            )
            build_dir = root / "build/test"
            gallery = build_dir / "app/fluent_qt_gallery"
            comparator = build_dir / "tools/dev/fluent_qt_visual_compare"
            gallery.parent.mkdir(parents=True)
            comparator.parent.mkdir(parents=True)
            gallery.write_bytes(b"gallery")
            comparator.write_bytes(b"comparator")
            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                mock.patch.object(
                    MODULE.subprocess,
                    "run",
                    side_effect=fake_tool_runner(value),
                ),
            ):
                scenario = MODULE.run_scenario(
                    value,
                    recipe_path,
                    value["scenarios"][0],
                    root / "output",
                    gallery,
                    comparator,
                )
            self.assertEqual(scenario["status"], "pass")
            evidence_path = root / "evidence.json"
            evidence = {
                "schema_version": 1,
                "tool": "FluentQt GUI Verify",
                "status": "review-required",
                "deterministic_status": "pass",
                "recipe": evidence_recipe_record(value, recipe_path),
                "binaries": {
                    "build_dir": str(build_dir),
                    "gallery": {
                        "path": str(gallery),
                        "sha256": MODULE.sha256_file(gallery),
                    },
                    "comparator": {
                        "path": str(comparator),
                        "sha256": MODULE.sha256_file(comparator),
                    },
                },
                "scenarios": [scenario],
                "summary": {"total": 1, "by_status": {"pass": 1}},
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
                        "reviewed_scenarios": [value["scenarios"][0]["id"]],
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

            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(MODULE.finalize_review(args), 0)
            self.assertEqual(
                json.loads(output.read_text(encoding="utf-8"))["status"],
                "pass",
            )

            Path(scenario["artifacts"]["actual"]).write_bytes(
                CHANGED_PNG_FIXTURE
            )
            changed_output = root / "verification-changed.json"
            args.output = changed_output
            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(MODULE.finalize_review(args), 1)
            self.assertEqual(
                json.loads(changed_output.read_text(encoding="utf-8"))["status"],
                "incomplete",
            )

    def test_finalize_rejects_handcrafted_pass_without_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence_path = root / "evidence.json"
            evidence_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "tool": "FluentQt GUI Verify",
                        "status": "review-required",
                        "deterministic_status": "pass",
                        "recipe": {
                            "author": {"id": "implementation-agent", "kind": "ai"}
                        },
                        "scenarios": [{"id": "light", "status": "pass"}],
                        "summary": {"total": 1, "by_status": {"pass": 1}},
                    }
                ),
                encoding="utf-8",
            )
            review_path = root / "review.json"
            review_path.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "reviewer": {"id": "fresh-reviewer", "kind": "human"},
                        "evidence_sha256": MODULE.sha256_file(evidence_path),
                        "verdict": "pass",
                        "summary": "Claimed review.",
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
                self.assertEqual(MODULE.finalize_review(args), 1)
            result = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(result["status"], "incomplete")
            self.assertTrue(result["validation_errors"])

    def test_finalize_rejects_output_aliasing_immutable_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence_path = root / "evidence.json"
            review_path = root / "review.json"
            recipe_path = root / "recipe.json"
            actual_path = root / "actual.png"
            baseline_path = (
                root / "tests/visual-baselines/gui/probe/light/baseline.png"
            )
            recipe_path.write_text("recipe", encoding="utf-8")
            actual_path.write_bytes(PNG_FIXTURE)
            baseline_path.parent.mkdir(parents=True)
            baseline_path.write_bytes(PNG_FIXTURE)
            evidence_contents = json.dumps(
                {
                    "schema_version": 1,
                    "recipe": {"path": str(recipe_path)},
                    "scenarios": [
                        {
                            "baseline_dir": str(baseline_path.parent),
                            "artifacts": {"actual": str(actual_path)},
                        }
                    ],
                }
            )
            review_contents = json.dumps({"schema_version": 1})
            evidence_path.write_text(evidence_contents, encoding="utf-8")
            review_path.write_text(review_contents, encoding="utf-8")

            original_inputs = {
                path: path.read_bytes()
                for path in (
                    evidence_path,
                    review_path,
                    recipe_path,
                    actual_path,
                    baseline_path,
                )
            }
            for output in original_inputs:
                args = argparse.Namespace(
                    evidence=evidence_path,
                    review=review_path,
                    output=output,
                )
                with mock.patch.object(MODULE, "PROJECT_ROOT", root):
                    with self.assertRaises(MODULE.VerificationError):
                        MODULE.finalize_review(args)

            self.assertEqual(
                {path: path.read_bytes() for path in original_inputs},
                original_inputs,
            )

            temporary_alias = root / "decision.tmp"
            temporary_alias.write_text(evidence_contents, encoding="utf-8")
            temporary_alias_contents = temporary_alias.read_bytes()
            args = argparse.Namespace(
                evidence=temporary_alias,
                review=review_path,
                output=root / "decision",
            )
            with (
                mock.patch.object(MODULE, "PROJECT_ROOT", root),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(MODULE.finalize_review(args), 1)
            self.assertEqual(temporary_alias.read_bytes(), temporary_alias_contents)


if __name__ == "__main__":
    unittest.main()
