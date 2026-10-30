"""Qt-free installed SDK runtime contracts, using real CMake imported targets."""

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
RELEASE_CONFIGS = "Release;RelWithDebInfo;MinSizeRel;None;"
DEFAULT_CRT = "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"


class SdkRuntimeConfigurationContract(unittest.TestCase):
    def configure(self, *, adopt=True, msvc=True, release=True, runtime="MultiThreadedDLL",
                  late_target=False, child=False, prior_directory=False, before_adopt="", wrapper=False):
        with tempfile.TemporaryDirectory(prefix="fluentqt-sdk-runtime-") as temporary:
            root = Path(temporary).resolve()
            package = root / "sdk/lib/cmake/FluentQt"
            qt = root / "sdk/lib/cmake/Qt6"
            package.mkdir(parents=True)
            qt.mkdir(parents=True)
            for module in ("FluentQtRuntime.cmake", "FluentQtApplePlatformDependencies.cmake"):
                shutil.copyfile(ROOT / "cmake" / module, package / module)
            for name in ("sdk-debug.lib", "qt-debug.lib", "qt-release.lib", "tool-debug.exe", "tool-release.exe"):
                (root / name).write_text("Configuration fixture only.\n", encoding="utf-8")
            (qt / "Qt6Config.cmake").write_text(f'''
set(Qt6_FOUND TRUE)
set(Qt6Widgets_FOUND TRUE)
add_library(Qt6::Core UNKNOWN IMPORTED)
set_target_properties(Qt6::Core PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    IMPORTED_LOCATION_DEBUG "{root.as_posix()}/qt-debug.lib"
    IMPORTED_LOCATION_RELEASE "{root.as_posix()}/qt-release.lib")
add_library(Qt6::Widgets INTERFACE IMPORTED)
set_target_properties(Qt6::Widgets PROPERTIES INTERFACE_LINK_LIBRARIES Qt6::Core)
add_library(Qt::Core ALIAS Qt6::Core)
add_executable(Qt6::tool IMPORTED)
set_target_properties(Qt6::tool PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    IMPORTED_LOCATION_DEBUG "{root.as_posix()}/tool-debug.exe"
    IMPORTED_LOCATION_RELEASE "{root.as_posix()}/tool-release.exe")
''', encoding="utf-8")
            (package / "FluentQtTargets.cmake").write_text(f'''
add_library(FluentQt::FluentQt STATIC IMPORTED)
set_target_properties(FluentQt::FluentQt PROPERTIES
    IMPORTED_CONFIGURATIONS DEBUG
    IMPORTED_LOCATION_DEBUG "{root.as_posix()}/sdk-debug.lib"
    INTERFACE_LINK_LIBRARIES Qt6::Widgets)
add_library(FluentQt::Alias ALIAS FluentQt::FluentQt)
''', encoding="utf-8")
            (root / "dummy.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            source = [
                "cmake_minimum_required(VERSION 3.21)",
                "project(SdkRuntime LANGUAGES CXX)",
                "include(CMakePackageConfigHelpers)",
                f"set(MSVC {'TRUE' if msvc else 'FALSE'})",
                # Test the producer's actual property extraction, including default/static modes.
                f'include("{(ROOT / "cmake/FluentQtRuntime.cmake").as_posix()}")',
                "add_library(metadata_probe INTERFACE)",
            ]
            if runtime is not None:
                source.append(f'set_property(TARGET metadata_probe PROPERTY MSVC_RUNTIME_LIBRARY "{runtime}")')
            source += [
                "_fluent_qt_msvc_runtime_for_target(metadata_probe FLUENT_QT_SDK_MSVC_RUNTIME_LIBRARY)",
                f"set(FLUENT_QT_DEBUG_RELEASE_RUNTIME {'ON' if release else 'OFF'})",
                "set(QT_VERSION_MAJOR 6)",
                "set(FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT OFF)",
                "set(FLUENT_QT_BUILD_PLUGINS OFF)",
                "set(FLUENT_QT_BUILD_PARTICLES OFF)",
                "set(FLUENT_QT_BUILD_SPATIAL OFF)",
                "set(FLUENT_QT_BUILD_QML_BRIDGE OFF)",
                f'configure_package_config_file("{(ROOT / "cmake/FluentQtConfig.cmake.in").as_posix()}"',
                f'  "{(package / "FluentQtConfig.cmake").as_posix()}" INSTALL_DESTINATION lib/cmake/FluentQt)',
                'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL" CACHE STRING "Host CRT" FORCE)',
                'set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG "Debug" CACHE STRING "Host imports" FORCE)',
                f'set(Qt6_DIR "{qt.as_posix()}")',
                f'find_package(FluentQt CONFIG REQUIRED PATHS "{package.as_posix()}" NO_DEFAULT_PATH)',
                'file(WRITE "${CMAKE_BINARY_DIR}/before.txt" "${CMAKE_MSVC_RUNTIME_LIBRARY}|${CMAKE_MAP_IMPORTED_CONFIG_DEBUG}")',
            ]
            if late_target:
                source.append("add_library(existing STATIC dummy.cpp)")
            source.append(before_adopt)
            if child or prior_directory:
                (root / "child").mkdir()
                (root / "child/CMakeLists.txt").write_text(
                    "fluent_qt_use_sdk_runtime()\n" if child else "add_library(config_only INTERFACE)\n", encoding="utf-8")
                source.append("add_subdirectory(child)")
            if adopt and not child:
                if wrapper:
                    source += ["function(wrapped_adoption)", "fluent_qt_use_sdk_runtime()",
                               "endfunction()", "wrapped_adoption()"]
                else:
                    source.append("fluent_qt_use_sdk_runtime()")
            source += [
                "add_library(application STATIC dummy.cpp)",
                "add_library(infrastructure STATIC dummy.cpp)",
                "add_executable(consumer dummy.cpp)",
                "target_link_libraries(consumer PRIVATE application infrastructure FluentQt::FluentQt)",
                "get_target_property(app_runtime application MSVC_RUNTIME_LIBRARY)",
                "get_target_property(infrastructure_runtime infrastructure MSVC_RUNTIME_LIBRARY)",
                "get_target_property(consumer_runtime consumer MSVC_RUNTIME_LIBRARY)",
                "get_target_property(qt_map Qt::Core MAP_IMPORTED_CONFIG_DEBUG)",
                "get_target_property(sdk_map FluentQt::Alias MAP_IMPORTED_CONFIG_DEBUG)",
                "get_target_property(tool_map Qt6::tool MAP_IMPORTED_CONFIG_DEBUG)",
                "get_property(cached_runtime CACHE CMAKE_MSVC_RUNTIME_LIBRARY PROPERTY VALUE)",
                "get_property(cached_map CACHE CMAKE_MAP_IMPORTED_CONFIG_DEBUG PROPERTY VALUE)",
                'file(WRITE "${CMAKE_BINARY_DIR}/state.txt"',
                ' "${FluentQt_MSVC_RUNTIME_LIBRARY}\n${app_runtime}\n${infrastructure_runtime}\n${consumer_runtime}\n"',
                ' "${qt_map}\n${sdk_map}\n${tool_map}\n${CMAKE_MAP_IMPORTED_CONFIG_DEBUG}\n${cached_runtime}\n${cached_map}\n")',
                'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/locations-$<CONFIG>.txt"',
                ' CONTENT "$<TARGET_FILE:Qt6::Core>\n$<TARGET_FILE:FluentQt::FluentQt>\n$<TARGET_FILE:Qt6::tool>\n")',
            ]
            (root / "CMakeLists.txt").write_text("\n".join(source) + "\n", encoding="utf-8")
            result = subprocess.run([ARGS.cmake, "-S", str(root), "-B", str(root / "build"),
                                     "-DCMAKE_BUILD_TYPE=Debug"], capture_output=True, text=True)
            build = root / "build"
            read = lambda name: (build / name).read_text().splitlines() if (build / name).is_file() else None
            return result, read("state.txt"), read("before.txt"), read("locations-Debug.txt")

    def test_find_package_only_describes_runtime_without_changing_host(self):
        result, state, before, locations = self.configure(adopt=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(before, ["MultiThreadedDebugDLL|Debug"])
        self.assertEqual(state, ["MultiThreadedDLL", *(["MultiThreadedDebugDLL"] * 3),
                                 "Debug", "Debug", "Debug", "Debug", "MultiThreadedDebugDLL", "Debug"])
        self.assertTrue(locations[0].endswith("qt-debug.lib"))

    def test_adoption_matches_all_new_application_targets_without_remapping_sdk_or_tools(self):
        result, state, before, locations = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(before, ["MultiThreadedDebugDLL|Debug"])
        self.assertEqual(state, [*(["MultiThreadedDLL"] * 4), RELEASE_CONFIGS,
                                 "Debug", "Debug", "Debug", "MultiThreadedDebugDLL", "Debug"])
        self.assertTrue(locations[0].endswith("qt-release.lib"))
        self.assertTrue(locations[1].endswith("sdk-debug.lib"))
        self.assertTrue(locations[2].endswith("tool-debug.exe"))

    def test_debug_and_static_sdk_metadata_preserve_the_producer_runtime(self):
        for requested, expected in ((None, DEFAULT_CRT), ("MultiThreaded$<$<CONFIG:Debug>:Debug>",
                                                         "MultiThreaded$<$<CONFIG:Debug>:Debug>"), ("", "")):
            with self.subTest(runtime=requested):
                result, state, _before, locations = self.configure(release=False, runtime=requested)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(state[:4], [expected] * 4)
                self.assertEqual(state[4:8], ["Debug"] * 4)
                self.assertTrue(locations[0].endswith("qt-debug.lib"))

    def test_non_msvc_metadata_is_empty_and_does_not_change_host_crt(self):
        result, state, _before, locations = self.configure(msvc=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(state[:4], ["", *(["MultiThreadedDebugDLL"] * 3)])
        self.assertEqual(state[4], RELEASE_CONFIGS)
        self.assertTrue(locations[0].endswith("qt-release.lib"))

    def test_late_adoption_rejects_existing_targets_before_changes(self):
        result, state, before, _locations = self.configure(late_target=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("before creating application targets", " ".join(result.stderr.split()))
        self.assertEqual(before, ["MultiThreadedDebugDLL|Debug"])
        self.assertIsNone(state)

    def test_embedded_adoption_is_rejected(self):
        result, state, _before, _locations = self.configure(child=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("top-level application", result.stderr)
        self.assertIsNone(state)

    def test_adoption_after_subdirectory_is_rejected_without_scanning_its_targets(self):
        result, state, _before, _locations = self.configure(prior_directory=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("before add_subdirectory()", " ".join(result.stderr.split()))
        self.assertIsNone(state)

    def test_older_cmake_is_rejected_only_when_the_helper_is_called(self):
        result, state, before, _locations = self.configure(before_adopt="set(CMAKE_VERSION 3.20)")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires CMake 3.21 or newer", result.stderr)
        self.assertEqual(before, ["MultiThreadedDebugDLL|Debug"])
        self.assertIsNone(state)

    def test_missing_sdk_metadata_has_an_explicit_error(self):
        result, state, _before, _locations = self.configure(before_adopt="unset(FluentQt_MSVC_RUNTIME_LIBRARY)")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires runtime metadata from an installed", " ".join(result.stderr.split()))
        self.assertIsNone(state)

    def test_function_scoped_adoption_is_rejected_before_state_can_escape_partially(self):
        result, state, before, _locations = self.configure(wrapper=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not inside a function", " ".join(result.stderr.split()))
        self.assertEqual(before, ["MultiThreadedDebugDLL|Debug"])
        self.assertIsNone(state)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *UNITTEST_ARGS])
