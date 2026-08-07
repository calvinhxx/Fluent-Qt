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
            if not (
                item["platform"] == "linux"
                and item["arch"] == "arm64"
                and item["release"] is True
            )
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

    def test_linux_arm64_rejects_python_311(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "linux-arm64-qt693-cp312"
        )
        scenario["python_version"] = "3.11"
        scenario["python_tag"] = "cp311"
        scenario["expected_wheel_suffix"] = "cp311-cp311-linux_aarch64"
        scenario["publish_wheel_suffix"] = (
            "cp311-cp311-manylinux_2_39_aarch64"
        )

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("does not support the CPython 3.11" in error for error in errors),
            errors,
        )

    def test_release_matrix_requires_python_313(self):
        catalog = copy.deepcopy(self.catalog)
        catalog["scenarios"] = [
            item
            for item in catalog["scenarios"]
            if item["id"] != "windows-x64-qt693-cp313"
        ]

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("CPython 3.11 through 3.13" in error for error in errors),
            errors,
        )

    def test_release_wheel_requires_python_range_is_enforced(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "macos-arm64-qt693-cp311"
        )
        scenario["requires_python"] = ">=3.10"

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("release wheel must use Requires-Python" in error for error in errors),
            errors,
        )

    def test_compatibility_artifact_is_not_public_python_metadata(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "linux-x64-qt624-cp310"
        )
        scenario["requires_python"] = ">=3.11,<3.14"

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any(
                "compatibility artifact must use Requires-Python" in error
                for error in errors
            ),
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

    def test_linux_release_must_match_pyside_manylinux_floor(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "linux-x64-qt693-cp311"
        )
        scenario["manylinux_policy"] = "manylinux_2_17"
        scenario["publish_wheel_suffix"] = (
            "cp311-cp311-manylinux_2_17_x86_64"
        )

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(any("manylinux_policy" in error for error in errors), errors)
        self.assertTrue(
            any("publish_wheel_suffix" in error for error in errors),
            errors,
        )

    def test_compatibility_lane_cannot_be_published(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "linux-x64-qt624-cp310"
        )
        scenario["publish_wheel_suffix"] = (
            "cp310-cp310-manylinux_2_17_x86_64"
        )

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("publish_wheel_suffix" in error for error in errors),
            errors,
        )

    def test_manylinux_lane_reserves_time_for_second_build(self):
        catalog = copy.deepcopy(self.catalog)
        scenario = next(
            item
            for item in catalog["scenarios"]
            if item["id"] == "linux-x64-qt693-cp311"
        )
        scenario["timeout_minutes"] = 35

        errors = VALIDATOR.validate_catalog(catalog)

        self.assertTrue(
            any("both native and manylinux builds" in error for error in errors),
            errors,
        )


if __name__ == "__main__":
    unittest.main()
