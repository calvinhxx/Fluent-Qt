#!/usr/bin/env python3

"""Regression tests for PySide6 platform artifact summaries."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


SCRIPT_PATH = Path(__file__).with_name("verify-pyside-platform-artifacts.py")
SPEC = importlib.util.spec_from_file_location(
    "verify_pyside_platform_artifacts", SCRIPT_PATH
)
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


class PySidePlatformArtifactTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = VERIFY.load_json(VERIFY.DEFAULT_CATALOG)

    def test_all_six_platform_architecture_summaries_are_populated(self):
        expected_counts = {
            ("linux", "x64"): 8,
            ("linux", "arm64"): 4,
            ("windows", "x64"): 8,
            ("windows", "arm64"): 5,
            ("macos", "x64"): 5,
            ("macos", "arm64"): 5,
        }

        actual_counts = {
            target: len(VERIFY.expected_artifacts(self.catalog, *target))
            for target in expected_counts
        }

        self.assertEqual(actual_counts, expected_counts)
        self.assertEqual(sum(actual_counts.values()), 35)

    def test_expected_names_follow_scenario_artifact_suffixes(self):
        names = VERIFY.expected_artifacts(self.catalog, "linux", "arm64")

        self.assertIn(
            "fluentqt-pyside6-qt693-cp312-linux-arm64",
            names,
        )
        self.assertIn(
            "fluentqt-pyside6-showcase-qt693-cp312-linux-arm64",
            names,
        )
        self.assertNotIn(
            "fluentqt-pyside6-showcase-qt693-cp313-linux-arm64",
            names,
        )

    def test_job_names_follow_the_reviewed_matrix(self):
        names = VERIFY.expected_job_names(self.catalog, "macos", "x64")

        self.assertEqual(
            names,
            [
                "PySide6 release / macOS x64 / CPython 3.11 / Qt 6.9.3",
                "PySide6 release / macOS x64 / CPython 3.12 / Qt 6.9.3",
                "PySide6 release / macOS x64 / CPython 3.13 / Qt 6.9.3",
            ],
        )

    def test_reusable_workflow_job_prefix_is_accepted(self):
        expected = "PySide6 release / Linux ARM64 / CPython 3.12 / Qt 6.9.3"
        results = [(f"PySide6 validation / {expected}", "success")]

        self.assertEqual(
            VERIFY.matching_job_conclusions(expected, results),
            ["success"],
        )

    def test_unsupported_target_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            VERIFY.expected_artifacts(self.catalog, "linux", "x86")


if __name__ == "__main__":
    unittest.main()
