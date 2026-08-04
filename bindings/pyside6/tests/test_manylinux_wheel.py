"""Tests for the FluentQt manylinux repair and audit helper."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile


TOOL_PATH = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "repair_manylinux_wheel.py"
)
SPEC = importlib.util.spec_from_file_location(
    "fluentqt_repair_manylinux_wheel",
    TOOL_PATH,
)
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ManylinuxWheelTest(unittest.TestCase):
    def create_repaired_wheel(self, root: Path, bundle_qt: bool = False) -> Path:
        wheel = root / (
            "fluentqt-1.5.2-cp311-cp311-"
            "manylinux_2_28_x86_64.whl"
        )
        with zipfile.ZipFile(wheel, "w") as archive:
            archive.writestr(
                "fluentqt-1.5.2.dist-info/METADATA",
                "Metadata-Version: 2.1\n"
                "Name: FluentQt\n"
                "Version: 1.5.2\n"
                "Requires-Dist: PySide6-Essentials (==6.9.3)\n"
                "Requires-Dist: shiboken6 (==6.9.3)\n\n",
            )
            archive.writestr(
                "fluentqt-1.5.2.dist-info/WHEEL",
                "Wheel-Version: 1.0\n"
                "Root-Is-Purelib: false\n"
                "Tag: cp311-cp311-manylinux_2_28_x86_64\n\n",
            )
            archive.writestr(
                "fluentqt/_fluentqt.cpython-311-x86_64-linux-gnu.so",
                b"synthetic ELF",
            )
            if bundle_qt:
                archive.writestr(
                    "fluentqt.libs/libQt6Core-deadbeef.so.6.9.3",
                    b"duplicate runtime",
                )
        return wheel

    def test_platform_tag_rejects_unscoped_linux_tag(self):
        with self.assertRaisesRegex(ValueError, "manylinux_MAJOR_MINOR"):
            TOOL.platform_tag("linux", "x86_64")

    def test_repair_command_pins_platform_and_excludes_runtime(self):
        command = TOOL.repair_command(
            Path("input.whl"),
            Path("wheelhouse"),
            "manylinux_2_28_x86_64",
            ["libQt6*.so.6", "libpyside6*.so.*"],
        )

        self.assertIn("--only-plat", command)
        self.assertEqual(command.count("--exclude"), 2)
        self.assertIn("manylinux_2_28_x86_64", command)

    def test_repaired_archive_pins_external_runtime(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            wheel = self.create_repaired_wheel(Path(temp_dir))

            report = TOOL.validate_archive(
                wheel,
                "manylinux_2_28_x86_64",
                "6.9.3",
                "6.9.3",
            )

        self.assertEqual(report["metadata_version"], "1.5.2")
        self.assertEqual(
            report["tags"],
            ["cp311-cp311-manylinux_2_28_x86_64"],
        )

    def test_repaired_archive_rejects_bundled_qt(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            wheel = self.create_repaired_wheel(Path(temp_dir), bundle_qt=True)

            with self.assertRaisesRegex(RuntimeError, "second PySide6/Qt runtime"):
                TOOL.validate_archive(
                    wheel,
                    "manylinux_2_28_x86_64",
                    "6.9.3",
                    "6.9.3",
                )

    def test_extension_audit_reads_runtime_dependencies_and_rpaths(self):
        dynamic_section = """
 0x0000000000000001 (NEEDED) Shared library: [libQt6Core.so.6]
 0x0000000000000001 (NEEDED) Shared library: [libQt6Gui.so.6]
 0x0000000000000001 (NEEDED) Shared library: [libQt6Widgets.so.6]
 0x0000000000000001 (NEEDED) Shared library: [libpyside6.abi3.so.6.9]
 0x0000000000000001 (NEEDED) Shared library: [libshiboken6.abi3.so.6.9]
 0x000000000000001d (RUNPATH) Library runpath: [$ORIGIN/../PySide6:$ORIGIN/../shiboken6:$ORIGIN/../PySide6/Qt/lib]
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            wheel = self.create_repaired_wheel(Path(temp_dir))
            with mock.patch.object(
                TOOL.subprocess,
                "check_output",
                return_value=dynamic_section,
            ):
                report = TOOL.inspect_extension(
                    wheel,
                    "fluentqt/_fluentqt.cpython-311-x86_64-linux-gnu.so",
                    [
                        "$ORIGIN/../PySide6",
                        "$ORIGIN/../shiboken6",
                        "$ORIGIN/../PySide6/Qt/lib",
                    ],
                )

        self.assertIn("libQt6Widgets.so.6", report["needed"])
        self.assertEqual(len(report["runtime_paths"]), 3)

    def test_native_wheel_discovery_is_architecture_specific(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expected = root / "fluentqt-1.5.2-cp311-cp311-linux_x86_64.whl"
            expected.touch()
            (root / "fluentqt-1.5.2-cp311-cp311-linux_aarch64.whl").touch()

            self.assertEqual(
                TOOL.find_native_wheel(root, "x86_64"),
                expected,
            )


if __name__ == "__main__":
    unittest.main()
