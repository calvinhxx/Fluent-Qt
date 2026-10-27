"""Qt-free CMake contracts for test-only font discovery and CTest propagation."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--cmake", default=shutil.which("cmake") or "cmake")
ARGS, UNITTEST_ARGS = parser.parse_known_args()


class TestFontEnvironmentContract(unittest.TestCase):
    def configure(self, *, windows, macos=False, fonts=(), configured=None, registrations=False):
        temporary = tempfile.TemporaryDirectory(prefix="fluentqt-test-fonts-")
        self.addCleanup(temporary.cleanup)
        # CMake expands Windows 8.3 temp paths; compare the same canonical path.
        root = Path(temporary.name).resolve()
        font_dir = root / ("System/Library/Fonts" if macos else "Windows/Fonts")
        font_dir.mkdir(parents=True)
        for name in fonts:
            (font_dir / name).write_bytes(b"CMake checks paths; Qt fixtures validate font contents.")
        override = ""
        if configured is not None:
            path = root / configured if configured else ""
            if configured == "custom.ttf":
                path.write_bytes(b"Configured font path fixture")
            if configured == "directory":
                path.mkdir()
            override = f'set(FLUENT_QT_TEST_MONOSPACE_FONT "{Path(path).as_posix() if path else ""}" CACHE FILEPATH "")\n'
        registration_source = ""
        if registrations:
            registration_source = '''
enable_testing()
add_test(NAME test_pyside6_gallery COMMAND "${CMAKE_COMMAND}" -E true)
add_test(NAME test_pyside6_gallery_glyph_paint_device COMMAND "${CMAKE_COMMAND}" -E true)
add_test(NAME test_pyside6_gallery_spatial_native COMMAND "${CMAKE_COMMAND}" -E true)
add_test(NAME unrelated_offscreen COMMAND "${CMAKE_COMMAND}" -E true)
set_tests_properties(test_pyside6_gallery test_pyside6_gallery_glyph_paint_device
    unrelated_offscreen PROPERTIES ENVIRONMENT "PYTHONPATH=original;QT_QPA_PLATFORM=offscreen;KEEP=1")
set_tests_properties(test_pyside6_gallery_spatial_native PROPERTIES ENVIRONMENT "KEEP=native")
fluentqt_apply_headless_test_font_environment("${font_environment}" "^test_pyside6_gallery($|_)")
'''
        (root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(TestFonts NONE)\n'
            + f'set(CMAKE_HOST_WIN32 {"TRUE" if windows else "FALSE"})\n'
            + f'set(CMAKE_HOST_APPLE {"TRUE" if macos else "FALSE"})\n'
            + f'set(CMAKE_FIND_ROOT_PATH "{root.as_posix()}")\n'
            + 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n'
            + override
            + f'include("{(ROOT / "cmake/FluentQtTestFonts.cmake").as_posix()}")\n'
            + 'fluentqt_test_font_environment(font_environment)\n'
            + 'file(WRITE "${CMAKE_BINARY_DIR}/font-environment.txt" "${font_environment}")\n'
            + registration_source,
            encoding="utf-8",
        )
        env = dict(os.environ, WINDIR=str(font_dir.parent))
        result = subprocess.run([ARGS.cmake, "-S", str(root), "-B", str(root / "build")],
                                env=env, text=True, capture_output=True)
        return root, result

    def test_windows_without_a_font_fails_instead_of_leaving_empty_environment(self):
        _, result = self.configure(windows=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Windows test fixtures require a monospace font", result.stderr)

    def test_windows_empty_cached_override_also_fails(self):
        _, result = self.configure(windows=True, fonts=("cour.ttf",), configured="")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("empty cached override", result.stderr)

    def test_windows_discovers_each_supported_fallback(self):
        for fonts in (("cour.ttf", "consola.ttf", "lucon.ttf"), ("consola.ttf", "lucon.ttf"), ("lucon.ttf",)):
            with self.subTest(fonts=fonts):
                root, result = self.configure(windows=True, fonts=fonts)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((root / "build/font-environment.txt").read_text(),
                                 "FLUENT_QT_TEST_MONOSPACE_FONT=" + (root / "Windows/Fonts" / fonts[0]).as_posix())

    def test_explicit_font_overrides_host_discovery(self):
        root, result = self.configure(windows=True, fonts=("cour.ttf",), configured="custom.ttf")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((root / "build/font-environment.txt").read_text(),
                         "FLUENT_QT_TEST_MONOSPACE_FONT=" + (root / "custom.ttf").as_posix())

    def test_macos_discovers_system_fixed_font_without_bundling_it(self):
        for fonts in (("Menlo.ttc", "Monaco.ttf"), ("Monaco.ttf",)):
            with self.subTest(fonts=fonts):
                root, result = self.configure(windows=False, macos=True, fonts=fonts)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((root / "build/font-environment.txt").read_text(),
                                 "FLUENT_QT_TEST_MONOSPACE_FONT=" + (root / "System/Library/Fonts" / fonts[0]).as_posix())

    def test_macos_requires_a_font_and_respects_explicit_overrides(self):
        for configured in (None, "", "custom.ttf"):
            with self.subTest(configured=configured):
                root, result = self.configure(windows=False, macos=True, configured=configured,
                                              fonts=("Menlo.ttc",) if configured == "" else ())
                if configured == "custom.ttf":
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual((root / "build/font-environment.txt").read_text(),
                                     "FLUENT_QT_TEST_MONOSPACE_FONT=" + (root / "custom.ttf").as_posix())
                else:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("macOS test fixtures require a monospace font", result.stderr)

    def test_invalid_explicit_paths_fail(self):
        for configured in ("missing.ttf", "directory"):
            with self.subTest(configured=configured):
                _, result = self.configure(windows=False, configured=configured)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("is not a font file", result.stderr)

    def test_other_hosts_can_use_the_system_fixed_font(self):
        root, result = self.configure(windows=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((root / "build/font-environment.txt").read_text(), "")

    def test_only_registered_headless_gallery_tests_inherit_font(self):
        root, result = self.configure(windows=True, fonts=("cour.ttf",), registrations=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        ctest = str(Path(ARGS.cmake).with_name("ctest" + Path(ARGS.cmake).suffix))
        result = subprocess.run([ctest, "--test-dir", str(root / "build"), "-C", "Debug", "--show-only=json-v1"],
                                text=True, capture_output=True, check=True)
        tests = {row["name"]: {p["name"]: p["value"] for p in row["properties"]}
                 for row in json.loads(result.stdout)["tests"]}
        expected = "FLUENT_QT_TEST_MONOSPACE_FONT=" + (root / "Windows/Fonts/cour.ttf").as_posix()
        for name in ("test_pyside6_gallery", "test_pyside6_gallery_glyph_paint_device"):
            self.assertIn(name, tests, result.stdout)
            self.assertEqual(tests[name]["ENVIRONMENT"],
                             ["PYTHONPATH=original", "QT_QPA_PLATFORM=offscreen", "KEEP=1", expected])
        self.assertEqual(tests["test_pyside6_gallery_spatial_native"]["ENVIRONMENT"], ["KEEP=native"])
        self.assertNotIn(expected, tests["unrelated_offscreen"]["ENVIRONMENT"])


if __name__ == "__main__":
    unittest.main(argv=[__file__, *UNITTEST_ARGS])
