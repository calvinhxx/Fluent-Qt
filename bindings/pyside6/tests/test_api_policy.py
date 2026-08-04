"""Regression tests for the PySide6 API version and deprecation policy."""

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import unittest


VERIFIER_PATH = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "verify_api_policy.py"
)
SPEC = importlib.util.spec_from_file_location(
    "fluentqt_verify_api_policy",
    VERIFIER_PATH,
)
VERIFIER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFIER)


class ApiPolicyTest(unittest.TestCase):
    def setUp(self):
        self.manifest = VERIFIER.load_manifest(VERIFIER.DEFAULT_MANIFEST)
        _, self.project_version = VERIFIER.read_project_version(
            VERIFIER.DEFAULT_PROJECT_FILE
        )

    def test_repository_policy_is_valid(self):
        self.assertEqual(
            VERIFIER.validate_manifest(self.manifest, self.project_version),
            [],
        )

    def test_api_version_must_follow_project_major_minor(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["api_version"] = "9.9"

        errors = VERIFIER.validate_manifest(manifest, self.project_version)

        self.assertTrue(any("api_version" in error for error in errors), errors)

    def test_deprecation_requires_a_later_major_removal(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["deprecations"] = [
            {
                "symbol": "fluentqt.binding_build_info",
                "deprecated_in": "1.5.0",
                "remove_in": "1.6.0",
                "replacement": None,
                "reason": "Test entry.",
            }
        ]

        errors = VERIFIER.validate_manifest(manifest, self.project_version)

        self.assertTrue(
            any("later major release" in error for error in errors),
            errors,
        )

    def test_deprecation_symbol_must_stay_in_manifest(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["deprecations"] = [
            {
                "symbol": "fluentqt.missing_symbol",
                "deprecated_in": "1.5.0",
                "remove_in": "2.0.0",
                "replacement": None,
                "reason": "Test entry.",
            }
        ]

        errors = VERIFIER.validate_manifest(manifest, self.project_version)

        self.assertTrue(
            any("manifest API symbol" in error for error in errors),
            errors,
        )

    def test_replacement_must_be_an_existing_different_symbol(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["deprecations"] = [
            {
                "symbol": "fluentqt.binding_build_info",
                "deprecated_in": "1.5.0",
                "remove_in": "2.0.0",
                "replacement": "fluentqt.binding_build_info",
                "reason": "Test entry.",
            }
        ]

        errors = VERIFIER.validate_manifest(manifest, self.project_version)

        self.assertTrue(any("replacement" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
