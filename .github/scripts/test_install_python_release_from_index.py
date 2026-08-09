#!/usr/bin/env python3
"""Regression tests for package-index installation retries."""

from __future__ import annotations

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest


SCRIPT_PATH = Path(__file__).with_name("install-python-release-from-index.py")
SPEC = importlib.util.spec_from_file_location(
    "install_python_release_from_index", SCRIPT_PATH
)
INSTALLER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = INSTALLER
SPEC.loader.exec_module(INSTALLER)


class PackageIndexInstallTest(unittest.TestCase):
    def test_build_command_pins_wheels_and_test_index(self):
        command = INSTALLER.build_command(
            "/venv/bin/python",
            ("FluentQt==1.6.1", "FluentQt-Gallery==1.6.1"),
            index_url="https://test.pypi.org/simple",
            no_deps=True,
        )

        self.assertEqual(command[:4], ["/venv/bin/python", "-m", "pip", "install"])
        self.assertIn("--no-cache-dir", command)
        self.assertIn("--only-binary=:all:", command)
        self.assertIn("--no-deps", command)
        self.assertEqual(
            command[-2:], ["FluentQt==1.6.1", "FluentQt-Gallery==1.6.1"]
        )

    def test_install_returns_without_sleep_after_success(self):
        calls = []
        sleeps = []

        def runner(command, *, check):
            calls.append((command, check))
            return SimpleNamespace(returncode=0)

        INSTALLER.install_with_retry(
            ["python", "-m", "pip"],
            3,
            2,
            runner=runner,
            sleeper=sleeps.append,
        )

        self.assertEqual(len(calls), 1)
        self.assertEqual(sleeps, [])

    def test_install_retries_transient_index_failure(self):
        exit_codes = iter((1, 1, 0))
        sleeps = []

        def runner(command, *, check):
            del command, check
            return SimpleNamespace(returncode=next(exit_codes))

        INSTALLER.install_with_retry(
            ["python", "-m", "pip"],
            3,
            2.5,
            runner=runner,
            sleeper=sleeps.append,
        )

        self.assertEqual(sleeps, [2.5, 2.5])

    def test_install_fails_after_bounded_attempts(self):
        calls = []
        sleeps = []

        def runner(command, *, check):
            calls.append((command, check))
            return SimpleNamespace(returncode=7)

        with self.assertRaisesRegex(
            INSTALLER.ReleaseInstallError, "failed after 3 attempts"
        ):
            INSTALLER.install_with_retry(
                ["python", "-m", "pip"],
                3,
                1,
                runner=runner,
                sleeper=sleeps.append,
            )

        self.assertEqual(len(calls), 3)
        self.assertEqual(sleeps, [1, 1])

    def test_cli_requires_exact_package_version(self):
        with self.assertRaises(SystemExit):
            INSTALLER.parse_args(
                ["--python", "python", "--package", "FluentQt>=1.6"]
            )


if __name__ == "__main__":
    unittest.main()
