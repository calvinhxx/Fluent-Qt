"""Qt-free CMake checks for Spatial's optional private-header dependencies."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--cmake", default=shutil.which("cmake") or "cmake")
ARGS, UNITTEST_ARGS = parser.parse_known_args()
RHI_HEADERS = ("rhi/qrhi.h", "rhi/qrhi_platform.h", "rhi/qshader.h")


def qt_private_targets(root, *, major=6, missing_target=None, missing_directory=None,
                       widget_header=None, conditional=False, disabled_headers=None,
                       rhi_headers=()):
    source = []
    dependencies = {"Core": (), "Gui": ("Core",), "Widgets": ("Core", "Gui")}
    for module in ("Core", "Gui", "Widgets"):
        source.extend([
            f"add_library(Qt{major}::{module} INTERFACE IMPORTED)",
            f"set_target_properties(Qt{major}::{module} PROPERTIES "
            f"_qt_module_has_private_headers {'FALSE' if module == disabled_headers else 'TRUE'})",
        ])
        if module == missing_target:
            continue
        directory = root / module
        if module != missing_directory:
            directory.mkdir()
            headers = ((widget_header,) if module == "Widgets" and widget_header else
                       rhi_headers if module == "Gui" else ())
            for relative in headers:
                header = directory / relative
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text("// Path fixture, not a usable Qt header.\n", encoding="utf-8")
        includes = directory.as_posix()
        if conditional:
            condition = f"$<BOOL:$<TARGET_PROPERTY:Qt{major}::{module},_qt_module_has_private_headers>>"
            # Qt 6.9's installed exports include empty conditional entries.
            includes = f"$<{condition}:>;$<{condition}:{includes}>"
        links = ";".join([f"Qt{major}::{parent}Private" for parent in dependencies[module]]
                         + [f"Qt{major}::{module}"])
        source.extend([
            f"add_library(Qt{major}::{module}Private INTERFACE IMPORTED)",
            f"set_target_properties(Qt{major}::{module}Private PROPERTIES "
            f'INTERFACE_INCLUDE_DIRECTORIES "{includes}" INTERFACE_LINK_LIBRARIES "{links}")',
        ])
    return source


class SpatialPrivateHeadersContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._workspace = tempfile.TemporaryDirectory(prefix="fluentqt-spatial-config-")
        cls.addClassCleanup(cls._workspace.cleanup)
        cls._projects = Path(cls._workspace.name).resolve()

    def configure(self, *, major=6, missing_target=None, missing_directory=None,
                  widget_header=None, conditional=False, disabled_headers=None):
        root = self._projects / "headers"
        root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="case-", dir=root) as temporary:
            headers = Path(temporary).resolve()
            source = ["cmake_minimum_required(VERSION 3.16)", "project(PrivateHeaders NONE)",
                      f"set(QT_VERSION_MAJOR {major})"]
            source += qt_private_targets(headers, major=major, missing_target=missing_target,
                                         missing_directory=missing_directory, widget_header=widget_header,
                                         conditional=conditional, disabled_headers=disabled_headers)
            source.extend([
                f'include("{(ROOT / "cmake/FluentQtSpatial.cmake").as_posix()}")',
                "fluent_qt_spatial_widgets_private_available(available)",
                'file(WRITE "${CMAKE_BINARY_DIR}/availability.txt" "${available}")',
                # This is the same optional linkage as the renderer. CMake must
                # generate successfully even when an exported directory is absent.
                "add_library(consumer INTERFACE)",
                "if(available)",
                f"  target_link_libraries(consumer INTERFACE Qt{major}::WidgetsPrivate)",
                "endif()",
            ])
            (root / "CMakeLists.txt").write_text("\n".join(source) + "\n", encoding="utf-8")
            result = subprocess.run([ARGS.cmake, "-S", str(root), "-B", str(root / "build")],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return (root / "build/availability.txt").read_text() == "TRUE"

    def configure_renderer(self, *, major=6, version="6.9.3", platform="windows",
                           rhi_headers=RHI_HEADERS, **private_options):
        # Exercise the production target creation, not a copied linkage condition.
        # A real compiled target makes CMake validate imported include directories.
        # Reuse compiler discovery; fresh header fixtures keep missing-path cases isolated.
        root = self._projects / "renderer"
        root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="case-", dir=root) as temporary:
            headers = Path(temporary).resolve()
            for relative in ("dummy.cpp", "support/spatial/SpatialTextureHost.cpp",
                             "support/spatial/SpatialTextureHost.h",
                             "support/spatial/shaders/spatial_rhi.qrc"):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("// Configuration fixture only.\n", encoding="utf-8")
            source = ["cmake_minimum_required(VERSION 3.16)", "project(PrivateRenderer CXX)",
                      f"set(QT_VERSION_MAJOR {major})", f"set(Qt6_VERSION {version})",
                      f"set(WIN32 {'TRUE' if platform == 'windows' else 'FALSE'})",
                      f"set(APPLE {'TRUE' if platform == 'macos' else 'FALSE'})"]
            source += qt_private_targets(headers, major=major, rhi_headers=rhi_headers,
                                         widget_header="private/qwidget_p.h", **private_options)
            source += [
                f"add_library(Qt{major}::OpenGLWidgets INTERFACE IMPORTED)",
                "add_library(FluentQt::Plugins INTERFACE IMPORTED)",
                "add_library(FluentQt::Particles INTERFACE IMPORTED)",
                "function(fluent_qt_configure_cpp_target target)",
                "  set_target_properties(${target} PROPERTIES CXX_STANDARD 17)",
                "endfunction()",
                "function(fluent_qt_enable_project_warnings target)", "endfunction()",
                "function(fluent_qt_enable_sanitizers target)", "endfunction()",
                "set(FLUENT_QT_BUILD_GALLERY TRUE)",
                'set(FLUENT_QT_SPATIAL_SOURCES "${PROJECT_SOURCE_DIR}/dummy.cpp")',
                'set(FLUENT_QT_SPATIAL_RENDERER_SOURCES "${PROJECT_SOURCE_DIR}/dummy.cpp")',
                f'include("{(ROOT / "cmake/FluentQtSpatial.cmake").as_posix()}")',
                "fluent_qt_add_spatial_library()",
                "get_target_property(definitions FluentQtSpatialRenderer COMPILE_DEFINITIONS)",
                "get_target_property(links FluentQtSpatialRenderer LINK_LIBRARIES)",
                "get_target_property(sources FluentQtSpatialRenderer SOURCES)",
                'file(WRITE "${CMAKE_BINARY_DIR}/renderer.txt" "${definitions}\n${links}\n${sources}")',
            ]
            (root / "CMakeLists.txt").write_text("\n".join(source) + "\n", encoding="utf-8")
            result = subprocess.run([ARGS.cmake, "-S", str(root), "-B", str(root / "build")],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            definitions, links, sources = (root / "build/renderer.txt").read_text().splitlines()
            rhi = "FLUENT_QT_HAS_RHI=1" in definitions.split(";")
            self.assertEqual("Qt6::GuiPrivate" in links.split(";"), rhi)
            self.assertEqual("spatial_rhi.qrc" in sources, rhi)
            return rhi, "FLUENT_QT_HAS_WIDGETS_PRIVATE=1" in definitions.split(";")

    def test_missing_private_target_uses_public_update_path(self):
        for module in ("Core", "Gui", "Widgets"):
            with self.subTest(module=module):
                self.assertFalse(self.configure(missing_target=module,
                                                widget_header="private/qwidget_p.h"))

    def test_exported_target_with_missing_include_directory_uses_public_update_path(self):
        for module in ("Core", "Gui", "Widgets"):
            with self.subTest(module=module):
                self.assertFalse(self.configure(missing_directory=module,
                                                widget_header="private/qwidget_p.h"))

    def test_existing_include_directory_without_widget_header_uses_public_update_path(self):
        self.assertFalse(self.configure())

    def test_qt5_and_qt6_with_matching_private_headers_enable_optimization(self):
        for major in (5, 6):
            for header in ("private/qwidget_p.h", "QtWidgets/private/qwidget_p.h"):
                with self.subTest(major=major, header=header):
                    self.assertTrue(self.configure(major=major, widget_header=header))

    def test_conditional_qt_exports_preserve_available_private_headers(self):
        self.assertTrue(self.configure(conditional=True, widget_header="private/qwidget_p.h"))

    def test_conditional_qt_exports_require_enabled_and_installed_headers(self):
        for module in ("Core", "Gui", "Widgets"):
            with self.subTest(module=module, reason="disabled"):
                self.assertFalse(self.configure(conditional=True, disabled_headers=module,
                                                widget_header="private/qwidget_p.h"))
            with self.subTest(module=module, reason="missing"):
                self.assertFalse(self.configure(conditional=True, missing_directory=module,
                                                widget_header="private/qwidget_p.h"))

    def test_renderer_missing_gui_private_target_keeps_opengl(self):
        self.assertEqual(self.configure_renderer(missing_target="Gui"), (False, False))

    def test_renderer_missing_private_include_directory_keeps_opengl(self):
        for conditional in (False, True):
            for module in ("Core", "Gui"):
                with self.subTest(module=module, conditional=conditional):
                    self.assertEqual(self.configure_renderer(missing_directory=module,
                                                             conditional=conditional), (False, False))

    def test_renderer_requires_all_rhi_headers(self):
        for header in RHI_HEADERS:
            with self.subTest(missing_header=header):
                remaining = tuple(item for item in RHI_HEADERS if item != header)
                self.assertEqual(self.configure_renderer(rhi_headers=remaining), (False, True))

    def test_renderer_conditional_exports_require_enabled_headers(self):
        for module in ("Core", "Gui"):
            with self.subTest(module=module):
                self.assertEqual(self.configure_renderer(conditional=True, disabled_headers=module),
                                 (False, False))

    def test_renderer_complete_sdk_keeps_rhi_with_plain_and_conditional_exports(self):
        for conditional in (False, True):
            for platform in ("windows", "macos"):
                with self.subTest(conditional=conditional, platform=platform):
                    self.assertEqual(self.configure_renderer(conditional=conditional, platform=platform),
                                     (True, True))

    def test_renderer_rhi_does_not_require_widgets_private(self):
        self.assertEqual(self.configure_renderer(missing_target="Widgets"), (True, False))

    def test_renderer_keeps_existing_qt_and_platform_boundary(self):
        for major, version, platform in ((5, "5.15.2", "windows"), (6, "6.6.3", "windows"),
                                         (6, "6.9.3", "linux"), (6, "6.7.0", "windows")):
            with self.subTest(major=major, version=version, platform=platform):
                self.assertEqual(self.configure_renderer(major=major, version=version, platform=platform),
                                 (version == "6.7.0", True))


if __name__ == "__main__":
    unittest.main(argv=[__file__, *UNITTEST_ARGS])
