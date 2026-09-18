#!/usr/bin/env python3
"""Regressions for omissions that otherwise reach the remote build matrix."""

from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SPEC = importlib.util.spec_from_file_location(
    "fluent_qt_preflight", Path(__file__).with_name("fluent_qt_preflight.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class LocalPreflightTest(unittest.TestCase):
    def test_diff_includes_rename_sources_staged_unstaged_and_untracked_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            def git(*args):
                subprocess.run(["git", *args], cwd=root, check=True, capture_output=True)
            git("init", "-b", "main")
            git("config", "user.name", "Preflight Test")
            git("config", "user.email", "test@example.invalid")
            for name in ("old.cpp", "edit.cpp", "staged.cpp"):
                (root / name).write_text("before\n")
            git("add", ".")
            git("commit", "-m", "seed")
            git("tag", "base")
            git("mv", "old.cpp", "renamed.cpp")
            git("commit", "-m", "rename")
            (root / "edit.cpp").write_text("unstaged\n")
            (root / "staged.cpp").write_text("staged\n")
            git("add", "staged.cpp")
            (root / "new file.cpp").write_text("untracked\n")
            self.assertEqual(MODULE.changed_paths(root, "base"), [
                "edit.cpp", "new file.cpp", "old.cpp", "renamed.cpp", "staged.cpp",
            ])

    def test_component_changes_use_ci_selection_and_include_bindings(self):
        cpp, pyside = MODULE.selection_for(["src/components/charts/ChartView.cpp"])
        self.assertEqual(cpp.targets, ("fluent_qt_charts_tests",))
        self.assertEqual(cpp.label_regex, "^(charts)$")
        self.assertTrue(pyside)

    def test_gallery_changes_include_native_shell_and_python_gallery(self):
        cpp, pyside = MODULE.selection_for(["app/view/ContentPresenter.cpp"])
        self.assertEqual(cpp.targets, ("fluent_qt_gallery_tests",))
        self.assertEqual(cpp.label_regex, "^(gallery)$")
        self.assertEqual(cpp, MODULE.CLASSIFIER.select_cpp_tests(["app/view/ContentPresenter.cpp"]))
        self.assertTrue(pyside)

    def test_docs_and_tooling_do_not_force_a_native_rebuild(self):
        cpp, pyside = MODULE.selection_for([
            "docs/development/ci-workflow.md", "tools/dev/fluent_qt_preflight.py",
            ".github/workflows/ci.yml",
        ])
        self.assertEqual(cpp.scope, "none")
        self.assertFalse(pyside)

    def test_shared_native_code_falls_back_to_full_host_tests(self):
        cpp, _ = MODULE.selection_for(["src/compatibility/QtCompat.h"])
        self.assertEqual(cpp.targets, ("fluent_qt_all_tests",))

    def test_runtime_commands_error_on_zero_tests_and_preserve_headless_exclusions(self):
        cpp, _ = MODULE.selection_for(["src/components/status_info/Toast.cpp"])
        commands = MODULE.lane_commands({"kind": "cpp", "build_dir": "build/qt"}, cpp, "Debug")
        self.assertEqual(commands[0], ["cmake", "-S", str(MODULE.ROOT), "-B", "build/qt"])
        self.assertIn("fluent_qt_status_info_tests", commands[1])
        self.assertIn("--no-tests=error", commands[2])
        self.assertIn("^(status_info)$", commands[2])
        self.assertIn("manual_visual", " ".join(commands[2]))

    def test_newer_sdk_success_does_not_claim_minimum_version_coverage(self):
        required = MODULE.required_versions(MODULE.ROOT, True, True)
        self.assertEqual(required["cpp"], ["5.15", "6.2"])
        lanes = [{"kind": kind, "qt": "6.9.3", "status": "passed"} for kind in ("cpp", "pyside")]
        missing = MODULE.missing_coverage(required, lanes)
        self.assertIn("cpp/Qt 5.15", missing)
        self.assertIn("cpp/Qt 6.2", missing)
        self.assertIn("pyside/Qt 6.2", missing)
        self.assertNotIn("pyside/Qt 6.9", missing)

    def test_failed_and_merely_configured_builds_do_not_count_as_validated(self):
        for status in ("not_run", "failed"):
            self.assertEqual(MODULE.missing_coverage(
                {"cpp": ["5.15"]}, [{"kind": "cpp", "qt": "5.15.2", "status": status}]
            ), ["cpp/Qt 5.15"])

    def make_build(self, root, kind="cpp"):
        build = root / "build"
        sdk = root / "qt"
        build.mkdir()
        sdk.mkdir()
        (sdk / "Qt6CoreConfigVersionImpl.cmake").write_text('set(PACKAGE_VERSION "6.9.3")\n')
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={root}\nQt6Core_DIR:PATH={sdk}\n"
            "BUILD_TESTING:BOOL=ON\nFLUENT_QT_BUILD_TESTS:BOOL=ON\n"
            "FLUENT_QT_BUILD_PYSIDE6_BINDINGS:BOOL=ON\n"
            "FLUENT_QT_BUILD_PYSIDE6_GALLERY:BOOL=ON\nPython_EXECUTABLE:FILEPATH=python\n"
        )
        return build

    def test_build_metadata_comes_from_the_configured_sdk(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = self.make_build(root)
            self.assertEqual(MODULE.read_build(build, "cpp", root)["qt"], "6.9.3")
            self.assertEqual(MODULE.read_build(build, "pyside", root)["python"], "python")
            with self.assertRaisesRegex(ValueError, "different checkout"):
                MODULE.read_build(build, "cpp", root / "other")
            cache = build / "CMakeCache.txt"
            cache.write_text(cache.read_text().replace("FLUENT_QT_BUILD_TESTS:BOOL=ON", "FLUENT_QT_BUILD_TESTS:BOOL=OFF"))
            with self.assertRaisesRegex(ValueError, "FLUENT_QT_BUILD_TESTS=ON"):
                MODULE.read_build(build, "cpp", root)

    def test_mixed_binding_runtime_and_sdk_are_rejected(self):
        output = json.dumps({"pyside": "6.2.4", "shiboken": "6.2.4", "qt_runtime": "6.2.4"})
        with mock.patch.object(MODULE.subprocess, "run", return_value=mock.Mock(stdout=output)):
            with self.assertRaisesRegex(ValueError, "versions differ"):
                MODULE.check_python_runtime({"python": "python", "qt": "6.9.3", "build_dir": "build/qt"})

    def test_gallery_cannot_be_omitted_when_shared_changes_expand_the_selection(self):
        lane = {"kind": "cpp", "build_dir": "build/qt", "qt": "6.9.3",
                "gallery_enabled": False, "status": "not_run"}
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE, "changed_paths", return_value=["app/main.cpp", "src/design/FluentTheme.cpp"]), \
                mock.patch.object(MODULE, "read_build", return_value=lane), \
                mock.patch.object(MODULE, "run_logged") as runner, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            report = Path(temporary) / "report.json"
            self.assertEqual(MODULE.main(["--build-dir", "build/qt", "--report", str(report)]), 1)
            runner.assert_not_called()
            result = json.loads(report.read_text())
            self.assertEqual(result["cpp_scope"], "all")
            self.assertIn("FLUENT_QT_BUILD_GALLERY=ON", result["error"])

    def test_runtime_failure_does_not_mark_the_sdk_as_validated(self):
        lane = {"kind": "cpp", "build_dir": "build/qt515", "qt": "5.15.2", "status": "not_run"}
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE, "changed_paths", return_value=["tests/components/status_info/TestToast.cpp"]), \
                mock.patch.object(MODULE.RELEASE, "integration_checks", return_value=[]), \
                mock.patch.object(MODULE, "read_build", return_value=lane), \
                mock.patch.object(MODULE, "run_logged", side_effect=[{"returncode": 0}, {"returncode": 0}, {"returncode": 1}]), \
                redirect_stdout(io.StringIO()):
            report = Path(temporary) / "report.json"
            self.assertEqual(MODULE.main(["--build-dir", "build/qt515", "--report", str(report)]), 1)
            result = json.loads(report.read_text())
            self.assertEqual(result["lanes"][0]["status"], "failed")
            self.assertEqual(result["status"], "failed")

    def test_missing_runtime_environment_returns_incomplete_after_cheap_checks(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE, "changed_paths", return_value=["src/components/charts/ChartView.cpp"]), \
                mock.patch.object(MODULE.RELEASE, "integration_checks", return_value=[]), \
                redirect_stdout(io.StringIO()):
            report = Path(temporary) / "report.json"
            self.assertEqual(MODULE.main(["--report", str(report)]), 2)
            result = json.loads(report.read_text())
            self.assertEqual(result["status"], "incomplete")
            self.assertIn("pyside/Qt 6.2", result["missing_coverage"])

    def test_reconfigure_cannot_silently_change_the_reported_qt_version(self):
        before = {"kind": "cpp", "build_dir": "build/qt", "qt": "5.15.2", "status": "not_run"}
        after = dict(before, qt="6.9.3")
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE, "changed_paths", return_value=["tests/components/status_info/TestToast.cpp"]), \
                mock.patch.object(MODULE.RELEASE, "integration_checks", return_value=[]), \
                mock.patch.object(MODULE, "read_build", side_effect=[before, after]), \
                mock.patch.object(MODULE, "run_logged", return_value={"returncode": 0}) as runner, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            report = Path(temporary) / "report.json"
            self.assertEqual(MODULE.main(["--build-dir", "build/qt", "--report", str(report)]), 1)
            runner.assert_called_once()
            result = json.loads(report.read_text())
            self.assertIn("toolchain changed", result["error"])
            self.assertNotEqual(result["lanes"][0]["status"], "passed")

    def test_plan_does_not_execute_checks_or_claim_a_pass(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE, "changed_paths", return_value=[]), \
                mock.patch.object(MODULE, "run_logged") as runner, \
                redirect_stdout(io.StringIO()):
            report = Path(temporary) / "plan.json"
            self.assertEqual(MODULE.main(["--plan", "--report", str(report)]), 0)
            runner.assert_not_called()
            self.assertEqual(json.loads(report.read_text())["status"], "planned")

    def test_first_failure_stops_and_preserves_its_log(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(MODULE.RELEASE, "integration_checks", return_value=[("first", ["one"]), ("second", ["two"])]), \
                mock.patch.object(MODULE, "run_logged", return_value={"returncode": 1, "log": "failure.log"}) as runner, \
                redirect_stdout(io.StringIO()):
            report = Path(temporary) / "report.json"
            self.assertEqual(MODULE.main(["--checks-only", "--report", str(report)]), 1)
            runner.assert_called_once()
            result = json.loads(report.read_text())
            self.assertEqual(result["status"], "failed")
            self.assertEqual(result["checks"][0]["log"], "failure.log")


if __name__ == "__main__":
    unittest.main()
