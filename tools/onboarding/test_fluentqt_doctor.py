#!/usr/bin/env python3

import importlib.metadata
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("fluentqt_doctor.py")
SCHEMA = Path(__file__).with_name("doctor-report.schema.json")
SPEC = importlib.util.spec_from_file_location("fluentqt_doctor", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtDoctorTest(unittest.TestCase):
    def setUp(self):
        self.commands = {
            "cmake": "/tools/cmake",
            "clang++": "/tools/clang++",
            "qmake6": "/tools/qmake6",
        }

    def which(self, name):
        return self.commands.get(name)

    def ready_runner(self, command, _timeout):
        if command == ["/tools/cmake", "--version"]:
            return MODULE.CommandOutput(0, "cmake version 3.30.2")
        if command == ["/tools/qmake6", "-query", "QT_INSTALL_PREFIX"]:
            return MODULE.CommandOutput(0, "/opt/Qt/6.9.3/macos")
        if command[0] == "/tools/cmake" and "-S" in command:
            return MODULE.CommandOutput(
                0,
                "\n".join(
                    (
                        "-- FLUENTQT_DOCTOR_QT_MAJOR=6",
                        "-- FLUENTQT_DOCTOR_QT_VERSION=6.9.3",
                        "-- FLUENTQT_DOCTOR_QT_DIR=/opt/Qt/6.9.3/macos/lib/cmake/Qt6",
                        "-- FLUENTQT_DOCTOR_CXX_COMPILER=/tools/clang++",
                    )
                ),
            )
        return MODULE.CommandOutput(1, f"unexpected command: {command}")

    def test_command_timeout_is_preserved(self):
        with mock.patch.object(
            MODULE.subprocess,
            "run",
            side_effect=subprocess.TimeoutExpired(["cmake"], 7),
        ):
            output = MODULE.run_command(["cmake"], 7)

        self.assertEqual(output.returncode, 124)
        self.assertTrue(output.timed_out)
        self.assertEqual(output.output, "Timed out after 7 seconds.")

    def test_cpp_profile_reports_ready_consumer(self):
        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=self.ready_runner,
        )

        self.assertTrue(report["ready"])
        self.assertEqual(report["summary"]["failures"], 0)
        self.assertEqual(
            [check["id"] for check in report["checks"]],
            ["cmake", "cxx_compiler", "qt_widgets"],
        )
        self.assertIn("Qt 6 Widgets 6.9.3", report["checks"][2]["summary"])

    def test_cpp_profile_exposes_cmake_qt_failure(self):
        def runner(command, timeout):
            if command[0] == "/tools/cmake" and "-S" in command:
                return MODULE.CommandOutput(
                    1,
                    "Could not find a package configuration file provided by Qt6",
                )
            return self.ready_runner(command, timeout)

        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=runner,
        )

        self.assertFalse(report["ready"])
        self.assertEqual(report["summary"]["failures"], 1)
        self.assertEqual(report["checks"][2]["status"], "fail")
        self.assertIn("--cmake-prefix-path", report["checks"][2]["hint"])

    def test_cpp_profile_reports_qt_probe_timeout_without_claiming_qt_is_missing(self):
        def runner(command, timeout):
            if command[0] == "/tools/cmake" and "-S" in command:
                return MODULE.CommandOutput(
                    124,
                    f"Timed out after {timeout} seconds.",
                    timed_out=True,
                )
            return self.ready_runner(command, timeout)

        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=runner,
            timeout_seconds=60,
        )

        qt_check = report["checks"][2]
        self.assertFalse(report["ready"])
        self.assertEqual(qt_check["summary"], "The Qt Widgets CMake probe timed out.")
        self.assertIn("--timeout-seconds 120", qt_check["hint"])
        self.assertNotIn("Install Qt", qt_check["hint"])

    def test_cpp_profile_rejects_unsupported_qt(self):
        def runner(command, timeout):
            output = self.ready_runner(command, timeout)
            if command[0] == "/tools/cmake" and "-S" in command:
                return MODULE.CommandOutput(
                    0,
                    output.output.replace("QT_VERSION=6.9.3", "QT_VERSION=6.1.3"),
                )
            return output

        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=runner,
        )

        self.assertFalse(report["ready"])
        self.assertEqual(report["checks"][2]["status"], "fail")
        self.assertIn("Qt 5.15+ or Qt 6.2+", report["checks"][2]["hint"])

    def test_missing_path_compiler_defers_to_cmake_probe(self):
        self.commands.pop("clang++")

        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=self.ready_runner,
        )

        self.assertTrue(report["ready"])
        self.assertEqual(report["checks"][1]["status"], "warn")

    def test_old_cmake_blocks_the_cpp_profile(self):
        def runner(command, timeout):
            if command == ["/tools/cmake", "--version"]:
                return MODULE.CommandOutput(0, "cmake version 3.15.7")
            return self.ready_runner(command, timeout)

        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=runner,
        )

        self.assertFalse(report["ready"])
        self.assertEqual(report["checks"][0]["status"], "fail")
        self.assertEqual(report["checks"][2]["status"], "fail")

    def test_python_wheel_profile_is_ready(self):
        versions = {"PySide6": "6.9.3", "FluentQt": "1.7.1"}
        report = MODULE.run_doctor(
            profile="python",
            python_version=(3, 13, 2),
            module_finder=lambda _name: object(),
            version_reader=lambda name: versions[name],
        )

        self.assertTrue(report["ready"])
        self.assertEqual(report["summary"]["passed"], 3)

    def test_python_310_is_source_only_warning(self):
        report = MODULE.run_doctor(
            profile="python",
            python_version=(3, 10, 14),
            module_finder=lambda _name: object(),
            version_reader=lambda _name: "test",
        )

        self.assertTrue(report["ready"])
        self.assertEqual(report["summary"]["warnings"], 1)
        self.assertEqual(report["checks"][0]["status"], "warn")

    def test_missing_fluentqt_blocks_python_profile(self):
        def module_finder(name):
            return object() if name == "PySide6" else None

        report = MODULE.run_doctor(
            profile="python",
            python_version=(3, 12, 4),
            module_finder=module_finder,
            version_reader=lambda _name: "6.9.3",
        )

        self.assertFalse(report["ready"])
        self.assertEqual(report["checks"][2]["status"], "fail")

    def test_human_and_json_outputs_keep_stable_shape(self):
        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=self.ready_runner,
        )
        human = MODULE.render_human(report)
        encoded = json.dumps(report)
        decoded = json.loads(encoded)

        self.assertIn("FluentQt doctor (cpp)", human)
        self.assertIn("Ready: 3 passed", human)
        self.assertEqual(decoded["schema_version"], 1)
        self.assertEqual(decoded["checks"][2]["id"], "qt_widgets")

    def test_json_report_matches_versioned_schema_contract(self):
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        report = MODULE.run_doctor(
            profile="cpp",
            environ={},
            which=self.which,
            runner=self.ready_runner,
        )

        self.assertEqual(set(report), set(schema["required"]))
        self.assertEqual(report["schema_version"], 1)
        self.assertIn(report["profile"], schema["properties"]["profile"]["enum"])
        self.assertEqual(
            set(report["host"]), set(schema["$defs"]["host"]["required"])
        )
        self.assertEqual(
            set(report["summary"]), set(schema["$defs"]["summary"]["required"])
        )
        for check in report["checks"]:
            self.assertEqual(set(check), set(schema["$defs"]["check"]["required"]))
            self.assertIn(
                check["status"],
                schema["$defs"]["check"]["properties"]["status"]["enum"],
            )

    def test_unavailable_distribution_version_is_non_blocking(self):
        def missing_version(_name):
            raise importlib.metadata.PackageNotFoundError

        result = MODULE.check_python_package(
            check_id="fluentqt",
            module="fluentqt",
            distribution="FluentQt",
            module_finder=lambda _name: object(),
            version_reader=missing_version,
        )

        self.assertEqual(result.status, "pass")
        self.assertIn("unknown version", result.summary)


if __name__ == "__main__":
    unittest.main()
