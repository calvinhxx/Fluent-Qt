#!/usr/bin/env python3
"""Exercise the verifier's script entry point outside the repository cwd."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("fluent_qt_gui_verify.py").resolve()


class GuiVerifierCliTest(unittest.TestCase):
    def invoke(self, directory, *arguments):
        environment = dict(os.environ)
        environment.pop("PYTHONPATH", None)
        return subprocess.run(
            [sys.executable, str(SCRIPT), *arguments],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )

    def test_subcommands_load_without_repository_on_pythonpath(self):
        with tempfile.TemporaryDirectory() as directory:
            for command in ("run", "approve", "finalize"):
                with self.subTest(command=command):
                    result = self.invoke(directory, command, "--help")
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn(f"{command} ", result.stdout)

    def test_malformed_recipe_fails_before_creating_output(self):
        with tempfile.TemporaryDirectory() as directory:
            recipe = Path(directory) / "recipe.json"
            output = Path(directory) / "output"
            recipe.write_text('{"schema_version": 1,', encoding="utf-8")
            result = self.invoke(
                directory, "run", "--recipe", str(recipe),
                "--output-dir", str(output), "--no-build",
            )
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn("Could not parse JSON", result.stderr)
            self.assertNotIn("Traceback", result.stderr)
            self.assertFalse(output.exists())

    def test_module_entry_point_loads_from_repository_root(self):
        result = subprocess.run(
            [sys.executable, "-m", "tools.dev.fluent_qt_gui_verify", "--help"],
            cwd=SCRIPT.parents[2],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("{run,approve,finalize}", result.stdout)


if __name__ == "__main__":
    unittest.main()
