#!/usr/bin/env python3

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("validate-ci-cpp-matrix.py")
SPEC = importlib.util.spec_from_file_location("validate_ci_cpp_matrix", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ValidateCiCppMatrixTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads(MODULE.DEFAULT_CATALOG.read_text(encoding="utf-8"))

    def test_repository_catalog_is_valid(self):
        self.assertEqual(MODULE.validate_catalog(self.catalog), [])

    def test_missing_scenario_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"].pop()
        self.assertTrue(any("is missing" in error for error in MODULE.validate_catalog(catalog)))

    def test_duplicate_id_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][1]["id"] = catalog["scenarios"][0]["id"]
        self.assertTrue(any("duplicate scenario id" in error for error in MODULE.validate_catalog(catalog)))

    def test_unknown_mode_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["mode"] = "nightly"
        self.assertTrue(any("mode must be fast or full" in error for error in MODULE.validate_catalog(catalog)))

    def test_invalid_qt_source_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["qt_source"] = "system"
        self.assertTrue(any("qt_source" in error for error in MODULE.validate_catalog(catalog)))

    def test_non_positive_timeout_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["timeout_minutes"] = 0
        self.assertTrue(any("positive integer" in error for error in MODULE.validate_catalog(catalog)))

    def test_non_boolean_first_window_trial_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["first_window_trial"] = "yes"
        self.assertTrue(
            any(
                "first_window_trial must be a boolean" in error
                for error in MODULE.validate_catalog(catalog)
            )
        )

    def test_first_window_trial_lane_set_is_locked(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"][0]["first_window_trial"] = False
        self.assertTrue(
            any(
                "fast first-window trials are missing" in error
                for error in MODULE.validate_catalog(catalog)
            )
        )

    def test_required_execution_lanes_cannot_disable_build_or_test(self):
        for contract in MODULE.REQUIRED_EXECUTION_LANES.values():
            for field in ("build", "test"):
                with self.subTest(lane=contract["id"], field=field):
                    catalog = copy.deepcopy(self.catalog)
                    scenario = next(
                        item
                        for item in catalog["scenarios"]
                        if item["id"] == contract["id"]
                    )
                    scenario[field] = False
                    errors = MODULE.validate_catalog(catalog)
                    self.assertTrue(
                        any("execution lane" in error for error in errors),
                        errors,
                    )

    def test_required_execution_lane_targets_and_labels_are_locked(self):
        for contract in MODULE.REQUIRED_EXECUTION_LANES.values():
            mutations = {
                "build_targets": "",
                "test_labels": "^nothing$",
                "exclude_labels": "",
            }
            for field, value in mutations.items():
                with self.subTest(lane=contract["id"], field=field):
                    catalog = copy.deepcopy(self.catalog)
                    scenario = next(
                        item
                        for item in catalog["scenarios"]
                        if item["id"] == contract["id"]
                    )
                    scenario[field] = value
                    errors = MODULE.validate_catalog(catalog)
                    self.assertTrue(
                        any("execution lane" in error for error in errors),
                        errors,
                    )


if __name__ == "__main__":
    unittest.main()
