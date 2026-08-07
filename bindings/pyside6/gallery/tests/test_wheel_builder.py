"""Tests for the standalone FluentQt Gallery wheel builder."""

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
import zipfile


WHEEL_BUILDER_PATH = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "build_gallery_wheel.py"
)
WHEEL_BUILDER_SPEC = importlib.util.spec_from_file_location(
    "fluentqt_build_gallery_wheel",
    WHEEL_BUILDER_PATH,
)
WHEEL_BUILDER = importlib.util.module_from_spec(WHEEL_BUILDER_SPEC)
WHEEL_BUILDER_SPEC.loader.exec_module(WHEEL_BUILDER)


class GalleryWheelBuilderTest(unittest.TestCase):
    def test_required_application_files_are_declared(self):
        for name in (
            "__init__.py",
            "__main__.py",
            "application_controller.py",
            "app.py",
            "catalog.py",
            "contract.json",
            "foundation_pages.py",
            "identity.py",
            "metrics.py",
            "native_samples.py",
            "visual.py",
            "window.py",
            "assets/app-icon.png",
            "assets/icon_aliases.json",
            "assets/icon_catalog.json",
            "assets/control_images/Placeholder.png",
            "assets/home_header_tiles/Header-WindowsDesign.png",
        ):
            with self.subTest(name=name):
                self.assertIn(name, WHEEL_BUILDER.REQUIRED_PACKAGE_FILES)

    def test_wheel_is_pure_and_pins_core_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package_dir = root / "fluentqt_gallery"
            for name in WHEEL_BUILDER.REQUIRED_PACKAGE_FILES:
                path = package_dir / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(
                    b"{}" if path.suffix == ".json" else b"# test\n"
                )
            output_dir = root / "wheelhouse"
            WHEEL_BUILDER.build_wheel(
                SimpleNamespace(
                    package_dir=str(package_dir),
                    output_dir=str(output_dir),
                    version="1.5.0",
                    requires_python=">=3.11,<3.14",
                    license_file=[],
                )
            )
            wheels = tuple(output_dir.glob("*.whl"))
            self.assertEqual(len(wheels), 1)
            self.assertTrue(wheels[0].name.endswith("-py3-none-any.whl"))
            with zipfile.ZipFile(wheels[0]) as archive:
                names = set(archive.namelist())
                metadata = archive.read(
                    "fluentqt_gallery-1.5.0.dist-info/METADATA"
                ).decode("utf-8")
                wheel = archive.read(
                    "fluentqt_gallery-1.5.0.dist-info/WHEEL"
                ).decode("utf-8")
            self.assertIn("fluentqt_gallery/__init__.py", names)
            self.assertFalse(any(name.startswith("fluentqt/") for name in names))
            self.assertIn("Requires-Dist: FluentQt (==1.5.0)", metadata)
            self.assertIn("Requires-Python: >=3.11,<3.14", metadata)
            self.assertIn("Root-Is-Purelib: true", wheel)
            self.assertIn("Tag: py3-none-any", wheel)


if __name__ == "__main__":
    unittest.main()
