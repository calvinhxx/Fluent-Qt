"""Configure-only contracts for Gallery requirements and ordinary consumers."""

from pathlib import Path
import os
import subprocess
from tempfile import TemporaryDirectory
import unittest


ROOT = Path(__file__).resolve().parents[2]


class InterfaceZoomConfigurationTest(unittest.TestCase):
    def configure_missing_headers(self, arguments, require=False):
        with TemporaryDirectory(prefix="fluentqt-zoom-config-") as directory:
            source = Path(directory)
            (source / "probe.cpp").write_text("int probe() { return 0; }\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.16)\n"
                "project(ZoomConfigurationContract LANGUAGES CXX)\n"
                "set(QT_VERSION_MAJOR 5)\n"
                "function(fluent_qt_spatial_private_headers_available output)\n"
                "  set(${output} FALSE PARENT_SCOPE)\n"
                "endfunction()\n"
                f'include("{ROOT.as_posix()}/cmake/FluentQtInterfaceZoom.cmake")\n'
                "add_library(probe STATIC probe.cpp)\n"
                f"fluent_qt_add_interface_zoom_support(probe {arguments})\n"
            )
            return subprocess.run(
                [os.environ.get("CMAKE_COMMAND", "cmake"), "-S", str(source),
                 "-B", str(source / "build"),
                 f"-DFLUENT_QT_REQUIRE_INTERFACE_ZOOM={'ON' if require else 'OFF'}"],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                timeout=60,
            )

    def test_gallery_missing_headers_fails_with_installation_guidance(self):
        result = self.configure_missing_headers("REQUIRED")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("desktop Gallery requires live zoom", result.stdout)
        self.assertIn("qtbase5-private-dev", result.stdout)
        self.assertIn("FLUENT_QT_BUILD_PYSIDE6_GALLERY", result.stdout)

    def test_ordinary_binding_does_not_require_private_headers(self):
        result = self.configure_missing_headers("")
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_explicit_binding_requirement_is_respected(self):
        result = self.configure_missing_headers("", require=True)
        self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_bootstrap_fixture_can_still_test_startup_only_scaling(self):
        result = self.configure_missing_headers("RESTART_ONLY", require=True)
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
