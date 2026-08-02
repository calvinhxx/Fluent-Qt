#!/usr/bin/env python3
"""Regression tests for the PySide6 wheel-matrix policy."""

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import unittest


VALIDATOR_PATH = Path(__file__).with_name("validate-pyside-wheel-matrix.py")
SPEC = importlib.util.spec_from_file_location(
    "validate_pyside_wheel_matrix", VALIDATOR_PATH
)
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)


class PySideWheelMatrixValidatorTest(unittest.TestCase):
    def setUp(self):
        self.catalog = VALIDATOR.load_catalog(VALIDATOR.DEFAULT_CATALOG)

    def test_repository_catalog_is_valid(self):
        self.assertEqual(VALIDATOR.validate_catalog(self.catalog), [])

    def test_missing_release_architecture_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"] = [
            item
            for item in catalog["scenarios"]
            if item["id"] != "linux-arm64-qt693-cp311"
        ]

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("first-release matrix" in error for error in errors), errors
        )

    def test_qt_62_arm64_release_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "windows-arm64-qt693-cp311"
        )
        scenario["qt_version"] = "6.2.4"
        scenario["pyside_version"] = "6.2.4"
        scenario["shiboken_version"] = "6.2.4"

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("no supported native toolchain policy" in error for error in errors),
            errors,
        )

    def test_32_bit_x86_is_rejected(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = catalog["scenarios"][0]
        scenario["arch"] = "x86"

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("unsupported platform/architecture" in error for error in errors),
            errors,
        )


if __name__ == "__main__":
    unittest.main()
