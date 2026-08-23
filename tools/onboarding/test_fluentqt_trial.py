#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("fluentqt_trial.py")
LAUNCHER = Path(__file__).with_name("fluentqt")
SCHEMA = Path(__file__).with_name("first-window-report.schema.json")
SPEC = importlib.util.spec_from_file_location("fluentqt_trial", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def ready_doctor(profile: str) -> str:
    return json.dumps(
        {
            "schema_version": 1,
            "profile": profile,
            "ready": True,
            "summary": {"passed": 3, "warnings": 0, "failures": 0},
            "checks": [],
        }
    )


class RecordingRunner:
    def __init__(self, *, doctor_ready: bool = True, fail_step: str | None = None):
        self.doctor_ready = doctor_ready
        self.fail_step = fail_step
        self.commands: list[list[str]] = []

    def __call__(self, command, cwd, environ, timeout_seconds):
        command = list(command)
        self.commands.append(command)
        joined = " ".join(command)
        if " doctor " in f" {joined} ":
            profile = "python" if "python" in command else "cpp"
            if self.doctor_ready:
                return MODULE.CommandOutput(0, ready_doctor(profile))
            report = json.loads(ready_doctor(profile))
            report["ready"] = False
            report["summary"]["failures"] = 1
            return MODULE.CommandOutput(1, json.dumps(report))
        if self.fail_step and self.fail_step in joined:
            return MODULE.CommandOutput(1, f"{self.fail_step} failed")
        if " create " in f" {joined} ":
            return MODULE.CommandOutput(0, '{"status":"created"}')
        return MODULE.CommandOutput(0, "")


class FluentQtTrialTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def test_cpp_trial_records_complete_first_window_path(self):
        runner = RecordingRunner()
        report = MODULE.run_trial(
            profile="cpp",
            starter="workbench",
            target=self.root / "cpp-trial",
            fluentqt_source=self.root / "Fluent-QT",
            runner=runner,
        )
        self.assertEqual(report["status"], "passed")
        self.assertTrue(report["first_window_reached"])
        self.assertEqual(
            [step["id"] for step in report["steps"]],
            ["doctor", "create", "configure", "build", "tests", "window"],
        )
        configure = runner.commands[2]
        self.assertTrue(
            any(value.startswith("-DFLUENTQT_SOURCE_DIR=") for value in configure)
        )
        self.assertIn("_(application_test|quality_report)$", runner.commands[-2])
        self.assertIn("_ui_smoke$", runner.commands[-1])

    def test_python_existing_qt_uses_demo_window(self):
        runner = RecordingRunner()
        report = MODULE.run_trial(
            profile="python",
            starter="existing-qt",
            target=self.root / "python-trial",
            runner=runner,
        )
        self.assertEqual(report["status"], "passed")
        self.assertEqual(
            [step["id"] for step in report["steps"]],
            ["doctor", "create", "tests", "window"],
        )
        self.assertIn("first_window_trial.app.demo", runner.commands[-1])

    def test_blocked_doctor_skips_mutating_steps(self):
        runner = RecordingRunner(doctor_ready=False)
        report = MODULE.run_trial(
            profile="cpp",
            starter="workbench",
            target=self.root / "blocked",
            runner=runner,
        )
        self.assertEqual(report["status"], "blocked")
        self.assertFalse(report["first_window_reached"])
        self.assertEqual(len(runner.commands), 1)
        self.assertEqual(report["steps"][0]["status"], "failed")
        self.assertTrue(
            all(step["status"] == "skipped" for step in report["steps"][1:])
        )

    def test_failed_build_does_not_claim_window_success(self):
        runner = RecordingRunner(fail_step="--build")
        report = MODULE.run_trial(
            profile="cpp",
            starter="workbench",
            target=self.root / "failed",
            runner=runner,
        )
        self.assertEqual(report["status"], "failed")
        self.assertFalse(report["first_window_reached"])
        statuses = {step["id"]: step["status"] for step in report["steps"]}
        self.assertEqual(statuses["build"], "failed")
        self.assertEqual(statuses["tests"], "skipped")
        self.assertEqual(statuses["window"], "skipped")

    def test_report_matches_versioned_top_level_contract(self):
        report = MODULE.run_trial(
            profile="python",
            starter="workbench",
            target=self.root / "schema",
            runner=RecordingRunner(),
        )
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        self.assertEqual(set(report), set(schema["required"]))
        self.assertEqual(report["schema_version"], 1)
        self.assertIn(report["status"], schema["properties"]["status"]["enum"])
        self.assertEqual(
            len({step["id"] for step in report["steps"]}),
            len(report["steps"]),
        )

    def test_launcher_lists_trial_command(self):
        result = subprocess.run(
            [sys.executable, str(LAUNCHER), "--help"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("trial", result.stdout)


if __name__ == "__main__":
    unittest.main()
