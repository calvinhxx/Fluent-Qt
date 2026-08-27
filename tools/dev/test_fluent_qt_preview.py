#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("fluent_qt_preview.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_preview", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtPreviewToolTest(unittest.TestCase):
    def test_default_presets_follow_host_architecture(self):
        self.assertEqual(MODULE.default_preset("Darwin", "arm64"), "vcpkg-osx")
        self.assertEqual(
            MODULE.default_preset("Darwin", "x86_64"), "vcpkg-osx-x64"
        )
        self.assertEqual(MODULE.default_preset("Linux", "x86_64"), "vcpkg-linux")
        self.assertEqual(
            MODULE.default_preset("Windows", "arm64"), "vcpkg-windows-arm64"
        )

    def test_resolves_macos_bundle_and_direct_unix_executables(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            mac_binary = (
                build
                / "app"
                / "Fluent-Qt Gallery.app"
                / "Contents"
                / "MacOS"
                / "Fluent-Qt Gallery"
            )
            mac_binary.parent.mkdir(parents=True)
            mac_binary.write_bytes(b"binary")
            self.assertEqual(
                MODULE.resolve_gallery_executable(build), mac_binary.resolve()
            )

        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            unix_binary = build / "app" / "fluent_qt_gallery"
            unix_binary.parent.mkdir(parents=True)
            unix_binary.write_bytes(b"binary")
            self.assertEqual(
                MODULE.resolve_gallery_executable(build), unix_binary.resolve()
            )

    def test_preview_arguments_preserve_scene_and_artifact_contract(self):
        args = MODULE.parse_args(
            [
                "--route",
                "button",
                "--sample",
                "button-styles",
                "--theme",
                "dark",
                "--rtl",
                "--size",
                "920x680",
                "--report",
                "-",
                "--no-build",
            ]
        )
        command = MODULE.preview_arguments(args)
        self.assertEqual(command[0], "--preview")
        self.assertIn("button", command)
        self.assertIn("button-styles", command)
        self.assertIn("dark", command)
        self.assertIn("920x680", command)
        self.assertIn("--rtl", command)
        self.assertEqual(command[command.index("--report") + 1], "-")

if __name__ == "__main__":
    unittest.main()
