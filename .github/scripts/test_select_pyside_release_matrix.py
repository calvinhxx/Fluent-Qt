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

    def test_longest_representative_lane_is_queued_first(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)

        self.assertEqual(
            matrix["include"][0]["id"], SELECTOR.CRITICAL_RELEASE_ID
        )

    def test_all_dynamic_extended_acceptance_lanes_form_first_wave(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)
        expected_ids = {
            scenario["id"]
            for scenario in self.catalog["scenarios"]
            if scenario["release"] is True
            and scenario["fast"] is False
            and scenario["extended_acceptance"] is True
        }
        first_wave = matrix["include"][: len(expected_ids)]

        self.assertEqual(
            {scenario["id"] for scenario in first_wave}, expected_ids
        )
        self.assertTrue(
            all(scenario["extended_acceptance"] is True for scenario in first_wave)
        )

    def test_fixed_fast_and_compatibility_lanes_are_excluded(self):
        matrix = SELECTOR.select_release_matrix(self.catalog)
        selected_ids = {scenario["id"] for scenario in matrix["include"]}
        excluded_ids = {
            scenario["id"]
            for scenario in self.catalog["scenarios"]
            if scenario["fast"] is True or scenario["compatibility"] is True
        }

        self.assertTrue(selected_ids.isdisjoint(excluded_ids))
        self.assertEqual(len(matrix["include"]), 16)

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
