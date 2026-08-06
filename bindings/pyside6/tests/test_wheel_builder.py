"""Tests for FluentQt wheel metadata helpers."""

import importlib.util
from pathlib import Path
import tempfile
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
    def test_native_and_facade_stubs_are_required_in_wheel(self):
        for name in (
            "__init__.pyi",
            "_fluentqt.pyi",
            "basicinput.pyi",
            "collections.pyi",
            "design.pyi",
            "windowing.pyi",
        ):
            with self.subTest(name=name):
                self.assertIn(name, WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)

    def test_scrolling_facade_is_required_in_wheel(self):
        self.assertIn(
            "scrolling.py",
            WHEEL_BUILDER.REQUIRED_PACKAGE_FILES,
        )

    def test_design_facade_and_semantic_alias_data_are_required_in_wheel(self):
        for name in ("design.py", "design.pyi", "_icon_aliases.json"):
            with self.subTest(name=name):
                self.assertIn(name, WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)

    def test_python_gallery_is_excluded_from_core_wheel(self):
        self.assertFalse(
            any(
                name.startswith("gallery/")
                for name in WHEEL_BUILDER.REQUIRED_PACKAGE_FILES
            )
        )

    def test_core_wheel_builder_rejects_staged_gallery_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            package_dir = Path(temporary)
            gallery_file = package_dir / "gallery" / "__init__.py"
            gallery_file.parent.mkdir(parents=True)
            gallery_file.write_text("# must not ship\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "must not contain Gallery"):
                WHEEL_BUILDER.package_files(package_dir)

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
