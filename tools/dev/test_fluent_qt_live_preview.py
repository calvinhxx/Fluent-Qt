#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import importlib.util
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("fluent_qt_live_preview.py")
SPEC = importlib.util.spec_from_file_location("fluent_qt_live_preview", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtLivePreviewLauncherTest(unittest.TestCase):
    def test_resolves_binding_build_and_cached_python(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            build = project / "build" / "pyside6-6.9.3"
            package = build / "python" / "fluentqt"
            package.mkdir(parents=True)
            (package / "_fluentqt.test.so").write_bytes(b"extension")
            cache = build / "CMakeCache.txt"
            cache.write_text(
                "Python_EXECUTABLE:FILEPATH={0}\n".format(sys.executable),
                encoding="utf-8",
            )

            runtime = MODULE.resolve_runtime(None, None, project_root=project)
            self.assertEqual(runtime.build_directory, build.resolve())
            self.assertEqual(
                runtime.python_executable, Path(sys.executable).absolute()
            )
            self.assertEqual(runtime.python_path, (build / "python").resolve())

    def test_explicit_build_must_contain_native_binding(self):
        with tempfile.TemporaryDirectory() as temporary:
            build = Path(temporary)
            with self.assertRaises(FileNotFoundError):
                MODULE.resolve_build_directory(build)

    def test_host_arguments_preserve_live_scene_contract(self):
        args = MODULE.parse_args(
            [
                "--scene",
                "scene.preview.py",
                "--theme",
                "dark",
                "--size",
                "720x540",
                "--rtl",
                "--no-watch",
                "--report",
                "build/live/report.json",
                "--keep-open",
            ]
        )
        command = MODULE.host_arguments(args)
        self.assertEqual(command[0], str(MODULE.LIVE_HOST))
        self.assertIn("--scene", command)
        self.assertIn("dark", command)
        self.assertIn("720x540", command)
        self.assertIn("--rtl", command)
        self.assertIn("--no-watch", command)
        self.assertIn("--report", command)
        self.assertIn("--keep-open", command)

    def test_runtime_environment_prefers_live_source_then_built_binding(self):
        runtime = MODULE.PySideRuntime(
            build_directory=Path("/tmp/build"),
            python_executable=Path(sys.executable),
            python_path=Path("/tmp/build/python"),
        )
        environment = MODULE.runtime_environment(
            runtime,
            base={"PYTHONPATH": "/existing"},
            source_package_root=Path("/source/gallery"),
        )
        self.assertEqual(
            environment["PYTHONPATH"].split(os.pathsep),
            ["/source/gallery", "/tmp/build/python", "/existing"],
        )
        self.assertEqual(environment["PYTHONDONTWRITEBYTECODE"], "1")

    def test_catalog_selection_and_default_fork_path(self):
        contract = {
            "schema_version": 1,
            "components": [
                {
                    "id": "button",
                    "title": "Button",
                    "samples": [
                        {"id": "button-styles", "title": "Button styles"},
                        {"id": "button-sizes", "title": "Button sizes"},
                    ],
                }
            ],
        }
        component, sample = MODULE.resolve_catalog_selection(
            contract, "button", None
        )
        self.assertEqual(sample["id"], "button-styles")
        document = MODULE.catalog_routes_document(contract)
        self.assertEqual(document["routes"][0]["samples"], [
            "button-styles",
            "button-sizes",
        ])

        args = MODULE.parse_args(["--route", "button", "--fork-scene"])
        runtime = MODULE.PySideRuntime(
            build_directory=Path("/tmp/pyside-build"),
            python_executable=Path(sys.executable),
            python_path=Path("/tmp/pyside-build/python"),
        )
        scene, mode = MODULE.catalog_scene_paths(args, runtime, component, sample)
        self.assertEqual(mode, "fork")
        self.assertEqual(scene.name, "button--button-styles.preview.py")

        managed_args = MODULE.parse_args(["--route", "button"])
        managed_scene, managed_mode = MODULE.catalog_scene_paths(
            managed_args, runtime, component, sample
        )
        self.assertEqual(managed_mode, "managed")
        self.assertEqual(managed_scene.parent.name, "button")
        self.assertEqual(managed_scene.name, "button-styles.preview.py")

    def test_catalog_listing_argument_contract(self):
        routes = MODULE.parse_args(["--list-routes"])
        self.assertTrue(routes.list_routes)
        samples = MODULE.parse_args(["--list-samples", "--route", "button"])
        self.assertTrue(samples.list_samples)
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                MODULE.parse_args(["--sample", "button-styles"])
            with self.assertRaises(SystemExit):
                MODULE.parse_args(["--scene", "scene.py", "--fork-scene"])

    def test_rejects_too_small_size(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                MODULE.parse_args(["--scene", "scene.py", "--size", "400x300"])


if __name__ == "__main__":
    unittest.main()
