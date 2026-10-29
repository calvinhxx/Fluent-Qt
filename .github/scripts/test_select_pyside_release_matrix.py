#!/usr/bin/env python3
"""Regression tests for PySide6 full-release queue ordering."""

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import unittest


SELECTOR_PATH = Path(__file__).with_name("select-pyside-release-matrix.py")
SPEC = importlib.util.spec_from_file_location(
    "select_pyside_release_matrix", SELECTOR_PATH
)
SELECTOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SELECTOR)


class PySideReleaseMatrixSelectorTest(unittest.TestCase):
    def setUp(self):
        self.catalog = SELECTOR.load_catalog(SELECTOR.DEFAULT_CATALOG)

    def test_critical_representative_lane_is_queued_first(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)

        self.assertEqual(
            matrix["include"][0]["id"], SELECTOR.CRITICAL_RELEASE_ID
        )

    def test_every_dynamic_lane_runs_extended_acceptance(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)

        self.assertTrue(
            all(
                scenario["extended_acceptance"] is True
                for scenario in matrix["include"]
            )
        )

    def test_remaining_platforms_follow_queue_priority(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)

        self.assertEqual(
            [scenario["id"] for scenario in matrix["include"]],
            [
                "windows-arm64-qt693-cp311-abi3",
                "macos-x64-qt693-cp311-abi3",
                "linux-x64-qt693-cp311-abi3",
                "linux-arm64-qt693-cp312-abi3",
                "windows-x64-qt693-cp311-abi3",
            ],
        )

    def test_every_lane_carries_its_interpreter_test_set(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)

        for scenario in matrix["include"]:
            self.assertEqual(
                scenario["test_python_versions"][0], scenario["python_version"]
            )
            self.assertTrue(scenario["stable_abi"])

    def test_fixed_fast_and_compatibility_lanes_are_excluded(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)
        selected_ids = {scenario["id"] for scenario in matrix["include"]}
        excluded_ids = {
            scenario["id"]
            for scenario in self.catalog["scenarios"]
            if scenario["fast"] is True or scenario["compatibility"] is True
        }

        self.assertTrue(selected_ids.isdisjoint(excluded_ids))
        self.assertEqual(len(matrix["include"]), 5)

    def test_auditwheel_version_is_injected_into_every_lane(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)
        expected = self.catalog["manylinux"]["auditwheel_version"]

        self.assertTrue(
            all(
                scenario["auditwheel_version"] == expected
                for scenario in matrix["include"]
            )
        )

    def test_missing_critical_lane_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"] = [
            scenario
            for scenario in catalog["scenarios"]
            if scenario["id"] != SELECTOR.CRITICAL_RELEASE_ID
        ]

        with self.assertRaisesRegex(ValueError, "exactly once"):
            SELECTOR.select_release_matrix(catalog)


if __name__ == "__main__":
    unittest.main()
