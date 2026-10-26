#!/usr/bin/env python3
"""Qt-free tests for cache integrity, exact patching, and read-only SDK handling."""
from __future__ import annotations

import difflib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("qt_accessibility_overlay", Path(__file__).with_name("qt_accessibility_overlay.py"))
OVERLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OVERLAY)


def digest(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def archive(entries: list[tuple[str, bytes]]) -> bytes:
    # BSD extended member names keep this fixture independent of LLVM's CLI.
    result = b"!<arch>\n"
    for name, content in entries:
        encoded = name.encode()
        payload = encoded + content
        header = (f"#1/{len(encoded)}".ljust(16) + "0".ljust(12) + "0".ljust(6)
                  + "0".ljust(6) + "100644".ljust(8) + str(len(payload)).ljust(10) + "`\n").encode()
        result += header + payload + (b"\n" if len(payload) % 2 else b"")
    return result


class OverlayIntegrityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = self.root / "data"
        self.cache = self.root / "cache"
        self.sdk = self.root / "sdk"
        self.objects = self.root / "objects"
        for path in (self.data, self.cache, self.sdk, self.objects):
            path.mkdir()
        self.manifest = {"qt_version": "6.9.3", "emscripten_version": "3.1.70",
                         "qtbase_commit": OVERLAY.QTBASE_COMMIT, "files": {},
                         "sdk_headers": {}, "sdk_archives": {}, "patches": {}}
        for source, member, library in (("src/gui/cache.cpp", "qaccessiblecache.cpp.o", "lib/libQt6Gui.a"),
                                        ("src/wasm/bridge.cpp", "qwasmaccessibility.cpp.o", "plugins/platforms/libqwasm.a")):
            original = "// SPDX-License-Identifier: MIT\nvoid example() {}\n"
            changed = "// SPDX-License-Identifier: MIT\nvoid example() { retire(); }\n"
            source_file = self.cache / source
            source_file.parent.mkdir(parents=True, exist_ok=True)
            source_file.write_text(original, encoding="utf-8")
            self.manifest["files"][source] = {"sha256": OVERLAY.sha256(source_file), "license": "MIT"}
            patch_file = self.data / (Path(source).name + ".patch")
            patch_file.write_text("".join(difflib.unified_diff(original.splitlines(True), changed.splitlines(True),
                fromfile="a/" + source, tofile="b/" + source)), encoding="utf-8")
            self.manifest["patches"][source] = {"file": patch_file.name, "sha256": OVERLAY.sha256(patch_file)}
            library_file = self.sdk / library
            library_file.parent.mkdir(parents=True, exist_ok=True)
            library_file.write_bytes(archive([(member, b"old code"), ("untouched.cpp.o", b"native code unchanged")]))
            self.manifest["sdk_archives"][library] = {"sha256": OVERLAY.sha256(library_file), "member": member}
            (self.objects / member).write_bytes(b"replacement code")
        header = self.sdk / "include/cache_p.h"
        header.parent.mkdir()
        header.write_bytes(b"// original SDK class declaration\n")
        self.manifest["sdk_headers"]["include/cache_p.h"] = {"sha256": OVERLAY.sha256(header)}
        self.manifest_file = self.data / "manifest.json"
        self.write_manifest()

    def write_manifest(self):
        self.manifest_file.write_text(json.dumps(self.manifest), encoding="utf-8")

    def sdk_snapshot(self):
        return {str(path.relative_to(self.sdk)): path.read_bytes() for path in self.sdk.rglob("*") if path.is_file()}

    def fake_archiver(self, command, check):
        self.assertTrue(check)
        self.assertEqual(command[1], "rcsD")
        destination, object_file = Path(command[2]), Path(command[3])
        self.assertFalse(destination.resolve().is_relative_to(self.sdk.resolve()))
        destination.write_bytes(archive([(object_file.name, object_file.read_bytes()),
                                         ("untouched.cpp.o", b"native code unchanged")]))

    def test_verified_offline_cache_never_opens_network(self):
        with patch.object(OVERLAY.urllib.request, "urlopen", side_effect=AssertionError("network used")):
            OVERLAY.fetch_sources(self.manifest, self.cache, offline=True)
            OVERLAY.fetch_sources(self.manifest, self.cache)

    def test_missing_offline_source_reports_action_without_download(self):
        relative = next(iter(self.manifest["files"]))
        (self.cache / relative).unlink()
        with patch.object(OVERLAY.urllib.request, "urlopen", side_effect=AssertionError("network used")):
            with self.assertRaisesRegex(RuntimeError, "Missing offline Qt source.*fetch command"):
                OVERLAY.fetch_sources(self.manifest, self.cache, offline=True)

    def test_tampered_cache_and_download_are_rejected_without_publication(self):
        relative = next(iter(self.manifest["files"]))
        source = self.cache / relative
        source.write_bytes(b"tampered cache")
        with self.assertRaisesRegex(RuntimeError, "Hash mismatch"):
            OVERLAY.fetch_sources(self.manifest, self.cache)
        self.assertEqual(source.read_bytes(), b"tampered cache")
        source.unlink()
        with patch.object(OVERLAY.urllib.request, "urlopen", return_value=io.BytesIO(b"wrong download")):
            with self.assertRaisesRegex(RuntimeError, "unexpected hash"):
                OVERLAY.fetch_sources(self.manifest, self.cache)
        self.assertFalse(source.exists())

    def test_manifest_paths_cannot_escape_cache(self):
        for name in ("../source.cpp", "/source.cpp", "C:/source.cpp", "source\\file.cpp"):
            with self.subTest(name=name), self.assertRaisesRegex(RuntimeError, "Unsafe|escapes"):
                OVERLAY.child_path(self.cache, name)

    def test_unreviewed_sdk_or_commit_is_rejected(self):
        self.manifest["qtbase_commit"] = "0" * 40
        self.write_manifest()
        with self.assertRaisesRegex(RuntimeError, "Unsupported Qt source revision"):
            OVERLAY.load_manifest(self.manifest_file)

    def test_exact_patch_rejects_fuzz_and_wrong_hunk_counts(self):
        patch_file = next(self.data.glob("*.patch"))
        contents = patch_file.read_text(encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "context"):
            OVERLAY.apply_patch_exact("changed upstream\n", contents)
        with self.assertRaisesRegex(RuntimeError, "lengths"):
            OVERLAY.apply_patch_exact("// SPDX-License-Identifier: MIT\nvoid example() {}\n", contents.replace(",2 ", ",3 ", 1))

    def test_zero_context_insertion_preserves_the_exact_source_boundary(self):
        before, after = "first\nsecond\n", "first\ninserted\nsecond\n"
        change = "".join(difflib.unified_diff(before.splitlines(True), after.splitlines(True), n=0))
        self.assertEqual(OVERLAY.apply_patch_exact(before, change), after)

    def test_prepare_is_offline_repeatable_and_keeps_sdk_bytes(self):
        before = self.sdk_snapshot()
        destination = self.root / "prepared"
        with patch.object(OVERLAY.urllib.request, "urlopen", side_effect=AssertionError("network used")):
            first = OVERLAY.prepare_sources(self.manifest_file, self.cache, self.sdk, destination)
            times = {p.name: p.stat().st_mtime_ns for p in destination.iterdir()}
            second = OVERLAY.prepare_sources(self.manifest_file, self.cache, self.sdk, destination)
        self.assertEqual(first, second)
        self.assertEqual(times, {p.name: p.stat().st_mtime_ns for p in destination.iterdir()})
        self.assertIn("retire();", (destination / "cache.cpp").read_text())
        self.assertEqual(before, self.sdk_snapshot())

    def test_sdk_hash_mismatch_fails_before_outputs_exist(self):
        library = self.sdk / next(iter(self.manifest["sdk_archives"]))
        library.write_bytes(b"different native library")
        destination = self.root / "prepared"
        with self.assertRaisesRegex(RuntimeError, "Hash mismatch.*SDK"):
            OVERLAY.prepare_sources(self.manifest_file, self.cache, self.sdk, destination)
        self.assertFalse(destination.exists())
        self.assertEqual(library.read_bytes(), b"different native library")

    def test_outputs_inside_sdk_are_rejected_before_writes(self):
        before = self.sdk_snapshot()
        for operation in (lambda: OVERLAY.prepare_sources(self.manifest_file, self.cache, self.sdk, self.sdk / "output"),
                          lambda: OVERLAY.publish_archives(self.manifest_file, self.sdk, self.cache, self.objects, self.sdk / "output", Path("ar"))):
            with self.assertRaisesRegex(RuntimeError, "read-only"):
                operation()
        self.assertEqual(before, self.sdk_snapshot())

    def test_publish_changes_only_two_project_copies(self):
        before = self.sdk_snapshot()
        destination = self.root / "archives"
        with patch.object(OVERLAY.subprocess, "run", side_effect=self.fake_archiver):
            record = OVERLAY.publish_archives(self.manifest_file, self.sdk, self.cache, self.objects, destination, Path("ar"))
        self.assertTrue(record["sdk_unchanged"])
        self.assertEqual(len(record["archives"]), 2)
        self.assertEqual(before, self.sdk_snapshot())
        for relative, spec in self.manifest["sdk_archives"].items():
            OVERLAY.verify_replacement(self.sdk / relative, destination / Path(relative).name, spec["member"], self.objects / spec["member"])

    def test_unrelated_archive_change_cannot_publish_a_partial_pair(self):
        destination = self.root / "archives"
        destination.mkdir()
        for relative in self.manifest["sdk_archives"]:
            (destination / Path(relative).name).write_bytes(b"previous complete output")
        before = self.sdk_snapshot()
        def corrupt_second(command, check):
            self.fake_archiver(command, check)
            if Path(command[3]).name == "qwasmaccessibility.cpp.o":
                Path(command[2]).write_bytes(archive([(Path(command[3]).name, b"replacement code"),
                                                     ("untouched.cpp.o", b"unexpected change")]))
        with patch.object(OVERLAY.subprocess, "run", side_effect=corrupt_second):
            with self.assertRaisesRegex(RuntimeError, "only the reviewed"):
                OVERLAY.publish_archives(self.manifest_file, self.sdk, self.cache, self.objects, destination, Path("ar"))
        self.assertEqual(before, self.sdk_snapshot())
        self.assertEqual(sorted(p.name for p in destination.iterdir()), ["libQt6Gui.a", "libqwasm.a"])
        self.assertTrue(all(p.read_bytes() == b"previous complete output" for p in destination.iterdir()))

    def test_installed_driver_uses_adjacent_manifest_outside_checkout(self):
        installed = self.root / "installed/wasm-accessibility"
        installed.mkdir(parents=True)
        tool = installed / "qt_accessibility_overlay.py"
        shutil.copy2(Path(__file__).with_name(tool.name), tool)
        shutil.copytree(self.data, installed / "data")
        result = subprocess.run([sys.executable, str(tool), "fetch", "--cache", str(self.cache), "--offline"],
                                cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("verified", result.stdout)

    def test_native_cmake_helper_has_no_python_or_qt_dependency(self):
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake, "CMake is required for the native no-op configuration contract")
        helper = Path(__file__).resolve().parents[2] / "cmake/FluentQtWasmAccessibility.cmake"
        source = self.root / "native"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.16)\nproject(NativeOverlayGuard NONE)\n'
            f'include("{helper.as_posix()}")\n'
            'fluent_qt_configure_wasm_accessibility_overlay("missing-tool" "missing-data" "missing-project")\n'
            'if(TARGET fluent_qt_wasm_accessibility_overlay OR DEFINED Python3_EXECUTABLE)\n'
            '  message(FATAL_ERROR "Native configuration discovered an overlay dependency")\nendif()\n', encoding="utf-8")
        result = subprocess.run([cmake, "-S", str(source), "-B", str(self.root / "native-build")],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def configure_sibling_consumers(self, conflict=None, configurations=("RELEASE",)):
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake, "CMake is required for the imported-target scope contract")
        repository = Path(__file__).resolve().parents[2]
        helper = repository / "cmake/FluentQtWasmAccessibility.cmake"
        driver = Path(__file__).with_name("qt_accessibility_overlay.py").resolve()
        source = self.root / "siblings"
        source.mkdir()
        project = self.root / "overlay-project"
        project.mkdir()
        (project / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\nproject(OverlayFixture NONE)\n", encoding="utf-8")
        compiler = self.root / ("fixture-emcc.cmd" if os.name == "nt" else "fixture-emcc")
        compiler.write_text("@echo emcc 3.1.70\n" if os.name == "nt" else "#!/bin/sh\nprintf 'emcc 3.1.70\\n'\n", encoding="utf-8")
        compiler.chmod(0o755)
        # Only the compiler identity is simulated. CMake's imported-target
        # scopes, ExternalProject registration, and source/SDK verification run.
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.25)\nproject(SiblingConsumers NONE)\n'
            'set(EMSCRIPTEN TRUE)\nset(Qt6_VERSION "6.9.3")\n'
            f'set(CMAKE_CXX_COMPILER "{compiler.as_posix()}")\n'
            'set(_fluent_qt_overlay_ar "${CMAKE_COMMAND}")\n'
            f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
            f'set(FLUENT_QT_WASM_SOURCE_CACHE "{self.cache.as_posix()}")\n'
            f'include("{helper.as_posix()}")\n'
            'add_subdirectory(one)\nadd_subdirectory(two)\n', encoding="utf-8")
        second_sdk, second_data = self.sdk, self.data
        if conflict == "sdk":
            second_sdk = self.root / "different-sdk"
            shutil.copytree(self.sdk, second_sdk)
        elif conflict == "manifest":
            second_data = self.root / "different-data"
            shutil.copytree(self.data, second_data)
            manifest = dict(self.manifest, description="A different reviewed overlay")
            (second_data / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        for name, sdk, data in (("one", self.sdk, self.data), ("two", second_sdk, second_data)):
            child = source / name
            child.mkdir()
            target_properties = ""
            for target, library in (("Qt6::Gui", "lib/libQt6Gui.a"),
                                    ("Qt6::QWasmIntegrationPlugin", "plugins/platforms/libqwasm.a")):
                location = (sdk / library).as_posix()
                # Qt's AdditionalTargetInfo files copy Release into these
                # configuration-specific and configuration-less properties.
                target_properties += (
                    f'set_target_properties({target} PROPERTIES\n'
                    '  IMPORTED_CONFIGURATIONS "RELEASE;RelWithDebInfo;MINSIZEREL;NOCONFIG;CUSTOM"\n'
                    f'  IMPORTED_LOCATION "{location}"\n'
                    + "".join(f'  IMPORTED_LOCATION_{config} "{location}"\n'
                              for config in ("RELEASE", "RELWITHDEBINFO", "MINSIZEREL", "NOCONFIG", "CUSTOM"))
                    + '  MAP_IMPORTED_CONFIG_PROFILE "Missing;RelWithDebInfo"\n'
                    '  MAP_IMPORTED_CONFIG_DEFAULT "")\n')
            (child / "CMakeLists.txt").write_text(
                'add_library(Qt6::Gui STATIC IMPORTED)\n'
                'add_library(Qt6::QWasmIntegrationPlugin STATIC IMPORTED)\n'
                + target_properties +
                'add_library(GuiAlias ALIAS Qt6::Gui)\n'
                'add_library(PluginAlias ALIAS Qt6::QWasmIntegrationPlugin)\n'
                'set_target_properties(Qt6::Gui PROPERTIES\n'
                '  _qt_sbom_spdx_repo_document_namespace "qtbase-be09b211")\n'
                f'fluent_qt_configure_wasm_accessibility_overlay("{driver.as_posix()}" "{data.as_posix()}" "{project.as_posix()}")\n'
                f'fluent_qt_configure_wasm_accessibility_overlay("{driver.as_posix()}" "{data.as_posix()}" "{project.as_posix()}")\n'
                'foreach(target Qt6::Gui Qt6::QWasmIntegrationPlugin GuiAlias PluginAlias)\n'
                '  get_target_property(configs ${target} IMPORTED_CONFIGURATIONS)\n'
                '  get_target_property(profile ${target} MAP_IMPORTED_CONFIG_PROFILE)\n'
                '  get_target_property(default ${target} MAP_IMPORTED_CONFIG_DEFAULT)\n'
                '  if(NOT configs STREQUAL "RELEASE;RelWithDebInfo;MINSIZEREL;NOCONFIG;CUSTOM"\n'
                '      OR NOT profile STREQUAL "Missing;RelWithDebInfo" OR NOT default STREQUAL "")\n'
                '    message(FATAL_ERROR "The overlay changed imported configuration selection rules")\n'
                '  endif()\n'
                f'  foreach(config {" ".join(configurations)})\n'
                '    get_target_property(location ${target} LOCATION_${config})\n'
                '    if(NOT location MATCHES "^${CMAKE_BINARY_DIR}/fluentqt-wasm-accessibility/archives/")\n'
                '      message(FATAL_ERROR "${target} ${config} resolved outside the overlay: ${location}")\n'
                '    endif()\n'
                '  endforeach()\n'
                '  get_target_property(dependencies ${target} MANUALLY_ADDED_DEPENDENCIES)\n'
                '  if(NOT "fluent_qt_wasm_accessibility_overlay" IN_LIST dependencies)\n'
                '    message(FATAL_ERROR "Local imported target lacks the overlay build dependency")\n'
                '  endif()\nendForeach()\n', encoding="utf-8")
        return subprocess.run([cmake, "-S", str(source), "-B", str(self.root / "siblings-build")],
                              capture_output=True, text=True)

    def test_sibling_imported_targets_share_one_overlay_with_local_dependencies(self):
        before = self.sdk_snapshot()
        result = self.configure_sibling_consumers()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(before, self.sdk_snapshot())

    def test_each_imported_configuration_and_alias_resolves_the_overlay(self):
        before = self.sdk_snapshot()
        result = self.configure_sibling_consumers(configurations=(
            "RELEASE", "RELWITHDEBINFO", "MINSIZEREL", "NOCONFIG", "CUSTOM", "PROFILE", "DEFAULT"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(before, self.sdk_snapshot())

    def test_sibling_consumers_reject_conflicting_manifest(self):
        result = self.configure_sibling_consumers("manifest")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Conflicting Qt Wasm accessibility overlay manifest or SDK", result.stdout + result.stderr)

    def test_sibling_consumers_reject_conflicting_sdk(self):
        result = self.configure_sibling_consumers("sdk")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Conflicting Qt Wasm accessibility overlay manifest or SDK", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
