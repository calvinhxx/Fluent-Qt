"""Tests for FluentQt wheel metadata helpers."""

import importlib.util
from pathlib import Path
import unittest


WHEEL_BUILDER_PATH = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "build_wheel.py"
)
WHEEL_BUILDER_SPEC = importlib.util.spec_from_file_location(
    "fluentqt_build_wheel",
    WHEEL_BUILDER_PATH,
)
WHEEL_BUILDER = importlib.util.module_from_spec(WHEEL_BUILDER_SPEC)
WHEEL_BUILDER_SPEC.loader.exec_module(WHEEL_BUILDER)


class WheelBuilderTest(unittest.TestCase):
    def test_scrolling_facade_is_required_in_wheel(self):
        self.assertIn(
            "scrolling.py",
            WHEEL_BUILDER.REQUIRED_PACKAGE_FILES,
        )

    def test_qt_62_uses_monolithic_pyside6_distribution(self):
        requirement = WHEEL_BUILDER.pyside_runtime_requirement("6.2.4")
        self.assertEqual(requirement, "PySide6 (==6.2.4)")

    def test_qt_63_and_newer_use_essentials_distribution(self):
        for version in ("6.3.0", "6.9.3", "7.0.0"):
            with self.subTest(version=version):
                requirement = WHEEL_BUILDER.pyside_runtime_requirement(version)
                self.assertEqual(
                    requirement,
                    "PySide6-Essentials (=={0})".format(version),
                )

    def test_invalid_version_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "Invalid PySide6 version"):
            WHEEL_BUILDER.pyside_runtime_requirement("invalid")


if __name__ == "__main__":
    unittest.main()
