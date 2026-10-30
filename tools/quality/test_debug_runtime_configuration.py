"""Qt-free contracts for Debug project code with release Qt dependencies."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--cmake", default=shutil.which("cmake") or "cmake")
ARGS, UNITTEST_ARGS = parser.parse_known_args()


class DebugRuntimeConfigurationContract(unittest.TestCase):
    def test_standard_desktop_presets_use_release_dependencies(self):
        presets = {preset["name"]: preset for preset in
                   json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))["configurePresets"]}

        def runtime_setting(name):
            preset = presets[name]
            variables = preset.get("cacheVariables", {})
            if "FLUENT_QT_DEBUG_RELEASE_RUNTIME" in variables:
                return variables["FLUENT_QT_DEBUG_RELEASE_RUNTIME"]
            parents = preset.get("inherits", [])
            if isinstance(parents, str):
                parents = [parents]
            for parent in parents:
                value = runtime_setting(parent)
                if value is not None:
                    return value
            return None

        for name in ("vcpkg-windows", "vcpkg-windows-arm64", "vcpkg-osx",
                     "vcpkg-osx-x64", "vcpkg-linux", "vcpkg-linux-arm64"):
            with self.subTest(preset=name):
                value = runtime_setting(name)
                self.assertEqual(value, "ON", f"{name} must default to release Qt dependencies")
                msvc = name.startswith("vcpkg-windows")
                result, state = self.configure(enabled=value == "ON", msvc=msvc)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(state, [
                    "MultiThreadedDLL" if msvc else "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL",
                    "Release;RelWithDebInfo;MinSizeRel;None;", "OFF" if msvc else "ON"])

    def configure(self, *, enabled=True, top_level=True, msvc=True, cmake_version="3.21",
                  runtime="MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"):
        with tempfile.TemporaryDirectory(prefix="fluentqt-debug-runtime-") as temporary:
            root = Path(temporary).resolve()
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.16)\nproject(DebugRuntime NONE)\n"
                + f"set(CMAKE_VERSION {cmake_version})\n"
                + f"set(MSVC {'TRUE' if msvc else 'FALSE'})\n"
                + f"set(FLUENT_QT_IS_TOP_LEVEL {'TRUE' if top_level else 'FALSE'})\n"
                + f"set(FLUENT_QT_DEBUG_RELEASE_RUNTIME {'ON' if enabled else 'OFF'})\n"
                + f'set(CMAKE_MSVC_RUNTIME_LIBRARY "{runtime}")\n'
                + "set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG HostConfig)\n"
                + "set(VCPKG_APPLOCAL_DEPS ON)\n"
                + f'include("{(ROOT / "cmake/FluentQtTargetOptions.cmake").as_posix()}")\n'
                + "fluent_qt_configure_debug_runtime()\n"
                + 'file(WRITE "${CMAKE_BINARY_DIR}/runtime.txt" '
                '"${CMAKE_MSVC_RUNTIME_LIBRARY}\n${CMAKE_MAP_IMPORTED_CONFIG_DEBUG}\n${VCPKG_APPLOCAL_DEPS}")\n',
                encoding="utf-8",
            )
            result = subprocess.run([ARGS.cmake, "-S", str(root), "-B", str(root / "build")],
                                    capture_output=True, text=True)
            state = root / "build/runtime.txt"
            return result, state.read_text().splitlines() if state.exists() else None

    def test_top_level_msvc_uses_release_crt_imports_and_matching_deployment(self):
        result, state = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(state, ["MultiThreadedDLL", "Release;RelWithDebInfo;MinSizeRel;None;", "OFF"])

    def test_embedded_build_rejects_partial_runtime_override(self):
        for msvc in (True, False):
            with self.subTest(msvc=msvc):
                result, state = self.configure(top_level=False, msvc=msvc)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("only supported in top-level FluentQt builds", " ".join(result.stderr.split()))
                self.assertIsNone(state)

    def test_disabled_runtime_preserves_host_configuration(self):
        for top_level, msvc in ((True, True), (False, True), (True, False), (False, False)):
            with self.subTest(top_level=top_level, msvc=msvc):
                result, state = self.configure(enabled=False, top_level=top_level, msvc=msvc)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(state, ["MultiThreaded$<$<CONFIG:Debug>:Debug>DLL", "HostConfig", "ON"])

    def test_non_msvc_remaps_dependencies_without_changing_runtime_or_deployment(self):
        result, state = self.configure(msvc=False, cmake_version="3.16")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(state, ["MultiThreaded$<$<CONFIG:Debug>:Debug>DLL",
                                 "Release;RelWithDebInfo;MinSizeRel;None;", "ON"])

    def test_imported_dependencies_select_release_and_support_system_qt_layouts(self):
        cases = (
            (("DEBUG", "RELEASE"), True, "RELEASE"),
            (("DEBUG", "RELWITHDEBINFO"), True, "RELWITHDEBINFO"),
            (("MINSIZEREL",), True, "MINSIZEREL"),
            (("NONE",), True, "NONE"),
            (("",), True, ""),
            (("DEBUG",), True, None),
            (("DEBUG", "RELEASE"), False, "DEBUG"),
        )
        for configurations, enabled, expected in cases:
            with self.subTest(configurations=configurations, enabled=enabled):
                with tempfile.TemporaryDirectory(prefix="fluentqt-imported-runtime-") as temporary:
                    root = Path(temporary).resolve()
                    definitions = []
                    for configuration in configurations:
                        location = root / f"{configuration or 'unqualified'}.bin"
                        location.touch()
                        suffix = f"_{configuration}" if configuration else ""
                        definitions.append(
                            f'set_property(TARGET Dependency PROPERTY IMPORTED_LOCATION{suffix} "{location.as_posix()}")\n')
                        if configuration:
                            definitions.append(
                                f'set_property(TARGET Dependency APPEND PROPERTY IMPORTED_CONFIGURATIONS {configuration})\n')
                    (root / "CMakeLists.txt").write_text(
                        "cmake_minimum_required(VERSION 3.16)\nproject(ImportedRuntime NONE)\n"
                        "set(MSVC FALSE)\nset(FLUENT_QT_IS_TOP_LEVEL TRUE)\n"
                        + f"set(FLUENT_QT_DEBUG_RELEASE_RUNTIME {'ON' if enabled else 'OFF'})\n"
                        + f'include("{(ROOT / "cmake/FluentQtTargetOptions.cmake").as_posix()}")\n'
                        + "fluent_qt_configure_debug_runtime()\nadd_library(Dependency UNKNOWN IMPORTED)\n"
                        + "".join(definitions)
                        + 'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/selected-$<CONFIG>.txt" '
                          'CONTENT "$<TARGET_FILE:Dependency>")\n',
                        encoding="utf-8",
                    )
                    result = subprocess.run(
                        [ARGS.cmake, "-S", str(root), "-B", str(root / "build"), "-DCMAKE_BUILD_TYPE=Debug"],
                        capture_output=True, text=True)
                    if expected is None:
                        # With CMake's pre-CMP0111 behavior a missing import is
                        # diagnosed at build time. Neither path may pick Debug.
                        if result.returncode == 0:
                            selected = (root / "build/selected-Debug.txt").read_text()
                            self.assertEqual(selected, "Dependency-NOTFOUND")
                        else:
                            self.assertIn("IMPORTED_LOCATION", result.stdout + result.stderr)
                    else:
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        selected = (root / "build/selected-Debug.txt").read_text()
                        self.assertEqual(Path(selected).name, f"{expected or 'unqualified'}.bin")

    def test_static_crt_request_is_rejected(self):
        result, state = self.configure(runtime="MultiThreaded$<$<CONFIG:Debug>:Debug>")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires the dynamic MSVC runtime", result.stderr)
        self.assertIsNone(state)

    def test_older_cmake_rejects_the_opt_in_runtime(self):
        result, state = self.configure(cmake_version="3.20")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires CMake 3.21 or newer", result.stderr)
        self.assertIsNone(state)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *UNITTEST_ARGS])
