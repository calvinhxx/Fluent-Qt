#!/usr/bin/env python3

import compileall
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("fluentqt_create.py")
LAUNCHER = Path(__file__).with_name("fluentqt")
SCHEMA = Path(__file__).with_name("create-report.schema.json")
VALIDATOR = (
    Path(__file__).resolve().parents[2]
    / ".agents"
    / "skills"
    / "build-fluentqt-gui"
    / "scripts"
    / "validate_project_structure.py"
)
SPEC = importlib.util.spec_from_file_location("fluentqt_create", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class FluentQtCreateTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def create(self, language: str, starter: str) -> tuple[Path, dict]:
        target = self.root / f"sample-{language}-{starter}"
        report = MODULE.create_project(
            target=target,
            application="Sample Desktop",
            identifier=f"sample-{language.replace('pyside6', 'python')}",
            language=language,
            starter=starter,
            accent="#2457D6",
        )
        return target, report

    def assert_structure_passes(self, target: Path) -> None:
        result = subprocess.run(
            [
                sys.executable,
                str(VALIDATOR),
                "--project-root",
                str(target),
                "--strict",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_all_starters_render_without_placeholders_or_absolute_paths(self):
        for language in ("cpp", "pyside6"):
            for starter in ("existing-qt", "workbench"):
                with self.subTest(language=language, starter=starter):
                    target, report = self.create(language, starter)
                    self.assertEqual(report["status"], "created")
                    self.assertTrue((target / ".fluentqt/architecture.json").is_file())
                    for path in target.rglob("*"):
                        if not path.is_file():
                            continue
                        text = path.read_text(encoding="utf-8")
                        self.assertNotRegex(text, MODULE.PLACEHOLDER)
                        self.assertNotIn(str(target), text)
                    self.assert_structure_passes(target)

    def test_python_starters_compile_and_run_application_tests(self):
        for starter in ("existing-qt", "workbench"):
            with self.subTest(starter=starter):
                target, _ = self.create("pyside6", starter)
                self.assertTrue(compileall.compile_dir(target / "src", quiet=1))
                environment = dict(os.environ)
                environment["PYTHONPATH"] = str(target / "src")
                result = subprocess.run(
                    [
                        sys.executable,
                        "-m",
                        "unittest",
                        "discover",
                        "-s",
                        "tests",
                    ],
                    cwd=target,
                    env=environment,
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("Ran 1 test", result.stdout + result.stderr)

    def test_existing_target_is_never_overwritten(self):
        target = self.root / "existing"
        target.mkdir()
        marker = target / "keep.txt"
        marker.write_text("keep", encoding="utf-8")
        with self.assertRaisesRegex(MODULE.CreateError, "already exists"):
            MODULE.create_project(
                target=target,
                application="Existing",
                identifier="existing",
                language="cpp",
                starter="workbench",
                accent="#2457D6",
            )
        self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

    def test_dry_run_reports_files_without_writing(self):
        target = self.root / "planned"
        report = MODULE.create_project(
            target=target,
            application="Planned",
            identifier="planned",
            language="cpp",
            starter="workbench",
            accent="#2457D6",
            dry_run=True,
        )
        self.assertEqual(report["status"], "planned")
        self.assertIn(".fluentqt/architecture.json", report["files"])
        self.assertFalse(target.exists())

    def test_invalid_identifier_and_accent_are_rejected(self):
        for identifier, accent, message in (
            ("Bad Name", "#2457D6", "Identifier"),
            ("valid-name", "blue", "Accent"),
        ):
            with self.subTest(identifier=identifier, accent=accent):
                with self.assertRaisesRegex(MODULE.CreateError, message):
                    MODULE.create_project(
                        target=self.root / f"invalid-{message}",
                        application="Invalid",
                        identifier=identifier,
                        language="cpp",
                        starter="workbench",
                        accent=accent,
                    )

    def test_json_report_has_stable_shape(self):
        _, report = self.create("cpp", "existing-qt")
        decoded = json.loads(json.dumps(report))
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        self.assertEqual(decoded["schema_version"], 1)
        self.assertEqual(set(decoded), set(schema["required"]))
        self.assertIn(decoded["status"], schema["properties"]["status"]["enum"])
        self.assertIn(
            decoded["template"], schema["properties"]["template"]["enum"]
        )
        self.assertIn(
            decoded["language"], schema["properties"]["language"]["enum"]
        )
        self.assertIn(decoded["profile"], schema["properties"]["profile"]["enum"])
        self.assertEqual(decoded["files"], sorted(set(decoded["files"])))

    def test_portable_launcher_dispatches_create(self):
        target = self.root / "launcher-project"
        result = subprocess.run(
            [
                sys.executable,
                str(LAUNCHER),
                "create",
                str(target),
                "--language",
                "cpp",
                "--starter",
                "existing-qt",
                "--format",
                "json",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)["status"], "created")
        self.assertTrue((target / "CMakeLists.txt").is_file())


if __name__ == "__main__":
    unittest.main()
