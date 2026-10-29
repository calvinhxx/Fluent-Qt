#!/usr/bin/env python3
"""Regression tests for the stable-ABI multi-interpreter wheel smoke."""

from __future__ import annotations

import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT_PATH = Path(__file__).with_name("smoke-pyside-wheel-interpreters.py")
SPEC = importlib.util.spec_from_file_location(
    "smoke_pyside_wheel_interpreters", SCRIPT_PATH
)
SMOKE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = SMOKE
SPEC.loader.exec_module(SMOKE)


class StableAbiWheelSmokeTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wheelhouse = self.root / "wheelhouse"
        self.gallery = self.root / "gallery-wheelhouse"
        self.wheelhouse.mkdir()
        self.gallery.mkdir()
        self.scenario = SMOKE.load_scenario(
            SMOKE.DEFAULT_CATALOG, "windows-x64-qt693-cp311-abi3"
        )
        (
            self.wheelhouse
            / f"fluentqt-1.10.3-{self.scenario['publish_wheel_suffix']}.whl"
        ).write_bytes(b"core")
        (self.gallery / "fluentqt_gallery-1.10.3-py3-none-any.whl").write_bytes(
            b"gallery"
        )

    def tearDown(self):
        self.temporary.cleanup()

    def argv(self, *interpreters):
        return [
            "--scenario",
            "windows-x64-qt693-cp311-abi3",
            "--wheel-dir",
            str(self.wheelhouse),
            "--gallery-wheel-dir",
            str(self.gallery),
            "--work-dir",
            str(self.root / "work"),
            *(f"--interpreter={value}" for value in interpreters),
        ]

    def test_every_release_lane_tests_its_newer_interpreters(self):
        catalog = SMOKE.json.loads(SMOKE.DEFAULT_CATALOG.read_text(encoding="utf-8"))
        required = {
            scenario["id"]: SMOKE.required_versions(scenario)
            for scenario in catalog["scenarios"]
            if scenario["release"] is True
        }

        self.assertEqual(
            required,
            {
                "linux-x64-qt693-cp311-abi3": ["3.12", "3.13"],
                "linux-arm64-qt693-cp312-abi3": ["3.13"],
                "windows-x64-qt693-cp311-abi3": ["3.12", "3.13"],
                "windows-arm64-qt693-cp311-abi3": ["3.12", "3.13"],
                "macos-x64-qt693-cp311-abi3": ["3.12", "3.13"],
                "macos-arm64-qt693-cp311-abi3": ["3.12", "3.13"],
            },
        )

    def test_compatibility_lane_is_not_a_stable_abi_target(self):
        with self.assertRaisesRegex(SMOKE.SmokeError, "not a stable-ABI"):
            SMOKE.load_scenario(SMOKE.DEFAULT_CATALOG, "linux-x64-qt624-cp310")

    def test_skipped_setup_python_output_is_ignored(self):
        self.assertEqual(
            SMOKE.parse_interpreters(["3.12=", "3.13=/opt/python3.13"]),
            {"3.13": Path("/opt/python3.13")},
        )

    def test_version_specific_wheel_is_rejected(self):
        for path in self.wheelhouse.iterdir():
            path.rename(path.with_name(path.name.replace("-abi3-", "-cp311-")))

        with self.assertRaisesRegex(SMOKE.SmokeError, "tagged cp311-abi3-win_amd64"):
            SMOKE.single_wheel(
                self.wheelhouse, "fluentqt", self.scenario["publish_wheel_suffix"]
            )

    def test_missing_interpreter_fails_before_running_anything(self):
        stderr = io.StringIO()
        with mock.patch.object(SMOKE.subprocess, "run") as run:
            with contextlib.redirect_stderr(stderr):
                result = SMOKE.main(self.argv("3.12=/opt/python3.12", "3.13="))

        self.assertEqual(result, 1)
        run.assert_not_called()
        self.assertIn("requires interpreters for ['3.12', '3.13']", stderr.getvalue())

    def test_clean_environment_drops_build_tree_paths(self):
        environment = SMOKE.clean_environment(
            {
                "PATH": "/usr/bin",
                "PYTHONPATH": "/build/python",
                "DYLD_LIBRARY_PATH": "/build/lib",
                "QT_PLUGIN_PATH": "/qt/plugins",
                "FLUENTQT_REQUIRE_SPATIAL": "1",
            },
            "1.10.3",
        )

        self.assertEqual(
            environment,
            {
                "PATH": "/usr/bin",
                "FLUENTQT_REQUIRE_SPATIAL": "1",
                "QT_QPA_PLATFORM": "offscreen",
                "FLUENTQT_EXPECTED_VERSION": "1.10.3",
            },
        )

    def test_each_interpreter_installs_and_smokes_the_same_wheel(self):
        commands = []

        def fake_run(command, **kwargs):
            commands.append((command, kwargs))
            if "-c" in command and command[-1] == SMOKE.INTERPRETER_PROBE:
                version = "3.12" if "3.12" in command[0] else "3.13"
                stdout = f"cpython {version}\n"
            elif "-c" in command:
                stdout = "1.10.3\n"
            else:
                stdout = ""
            return mock.Mock(stdout=stdout)

        stdout = io.StringIO()
        with mock.patch.object(SMOKE.subprocess, "run", side_effect=fake_run):
            with contextlib.redirect_stdout(stdout):
                result = SMOKE.main(
                    self.argv("3.12=/opt/python3.12", "3.13=/opt/python3.13")
                )

        self.assertEqual(result, 0)
        installs = [
            command for command, _kwargs in commands if "install" in command
        ]
        self.assertEqual(len(installs), 2)
        for install in installs:
            self.assertTrue(install[-2].endswith("cp311-abi3-win_amd64.whl"))
            self.assertTrue(install[-1].endswith("py3-none-any.whl"))
        smoke_runs = [
            (command, kwargs)
            for command, kwargs in commands
            if command[-1].endswith("test_wheel_smoke.py")
        ]
        self.assertEqual(len(smoke_runs), 2)
        for _command, kwargs in smoke_runs:
            self.assertEqual(kwargs["env"]["QT_QPA_PLATFORM"], "offscreen")
            self.assertEqual(kwargs["env"]["FLUENTQT_EXPECTED_VERSION"], "1.10.3")
            self.assertNotIn("PYTHONPATH", kwargs["env"])
        self.assertIn("in addition to build interpreter 3.11", stdout.getvalue())

    def test_wrong_interpreter_version_is_rejected(self):
        with mock.patch.object(
            SMOKE.subprocess, "run", return_value=mock.Mock(stdout="cpython 3.11\n")
        ), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(SMOKE.SmokeError, "expected 'cpython 3.12'"):
                SMOKE.probe_interpreter(Path("/opt/python3.12"), "3.12")


if __name__ == "__main__":
    unittest.main()
