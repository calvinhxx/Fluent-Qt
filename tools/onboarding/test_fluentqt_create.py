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
from unittest.mock import patch


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
            for starter in ("existing-qt", "monitor", "workbench"):
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
        for starter in ("existing-qt", "monitor", "workbench"):
            with self.subTest(starter=starter):
                target, _ = self.create("pyside6", starter)
                self.assertIn(
                    'requires-python = ">=3.10,<3.14"',
                    (target / "pyproject.toml").read_text(encoding="utf-8"),
                )
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

    def test_full_starters_separate_shell_from_product_page(self):
        expectations = {
            ("cpp", "workbench"): (
                "src/ui/components/WorkbenchShell.cpp",
                "src/ui/pages/WorkspacePage.cpp",
                "WorkspacePage",
                "setMaximumWidth(1320)",
            ),
            ("pyside6", "workbench"): (
                "src/sample_python/ui/components/workbench_shell.py",
                "src/sample_python/ui/pages/workspace_page.py",
                "WorkspacePage",
                "setMaximumWidth(1320)",
            ),
            ("cpp", "monitor"): (
                "src/ui/components/WorkbenchShell.cpp",
                "src/ui/pages/MonitorPage.cpp",
                "MonitorPage",
                "setMaximumWidth(1320)",
            ),
            ("pyside6", "monitor"): (
                "src/sample_python/ui/components/workbench_shell.py",
                "src/sample_python/ui/pages/monitor_page.py",
                "MonitorPage",
                "setMaximumWidth(1320)",
            ),
        }
        for (language, starter), (shell_path, page_path, page_symbol, invariant) in expectations.items():
            with self.subTest(language=language, starter=starter):
                target, _ = self.create(language, starter)
                shell = target / shell_path
                page = target / page_path
                self.assertTrue(shell.is_file())
                self.assertTrue(page.is_file())
                self.assertIn(invariant, shell.read_text(encoding="utf-8"))
                self.assertNotIn(page_symbol, shell.read_text(encoding="utf-8"))
                dependency_file = (
                    target / "CMakeLists.txt"
                    if language == "cpp"
                    else target / "pyproject.toml"
                )
                expected_version = "1.8.5" if starter == "monitor" else "1.7.2"
                self.assertIn(expected_version, dependency_file.read_text(encoding="utf-8"))

    def test_publish_retries_a_short_windows_lock_and_preserves_existing_targets(self):
        stage, destination = self.root / "staging", self.root / "project"
        stage.mkdir()
        (stage / "marker").write_text("generated", encoding="utf-8")
        lock = PermissionError("reader holds the staging directory")
        lock.winerror = 32
        rename = Path.rename
        attempts = iter((lock, None))

        def briefly_locked(source, target):
            error = next(attempts)
            if error:
                raise error
            return rename(source, target)

        with patch.object(Path, "rename", briefly_locked), patch.object(MODULE.time, "sleep") as wait:
            MODULE._publish_project(stage, destination)
        wait.assert_called_once()
        self.assertEqual((destination / "marker").read_text(encoding="utf-8"), "generated")

        stage.mkdir()
        with self.assertRaisesRegex(MODULE.CreateError, "Target already exists"):
            MODULE._publish_project(stage, destination)
        self.assertTrue(stage.exists())
        self.assertEqual((destination / "marker").read_text(encoding="utf-8"), "generated")

        with patch.object(Path, "rename", side_effect=lock), patch.object(MODULE.time, "sleep") as wait:
            with self.assertRaisesRegex(MODULE.CreateError, "Could not create"):
                MODULE._publish_project(stage, self.root / "locked")
        self.assertEqual(wait.call_count, 3)

    def test_existing_qt_preserves_original_minimum_versions(self):
        cpp_target, _ = self.create("cpp", "existing-qt")
        self.assertIn(
            "find_package(FluentQt 1.7 CONFIG REQUIRED)",
            (cpp_target / "CMakeLists.txt").read_text(encoding="utf-8"),
        )
        py_target, _ = self.create("pyside6", "existing-qt")
        self.assertIn(
            '"FluentQt>=1.7,<2"',
            (py_target / "pyproject.toml").read_text(encoding="utf-8"),
        )

    def test_quoted_application_name_generates_valid_toml_and_sources(self):
        app_name = 'Ops "North" \\ Lab'
        for language in ("cpp", "pyside6"):
            for starter in ("existing-qt", "monitor", "workbench"):
                with self.subTest(language=language, starter=starter):
                    target = self.root / f"quoted-{language}-{starter}"
                    MODULE.create_project(
                        target=target,
                        application=app_name,
                        identifier=f"quoted-{language.replace('pyside6', 'py')}-{starter}",
                        language=language,
                        starter=starter,
                        accent="#2457D6",
                    )
                    self.assert_structure_passes(target)
                    if language == "pyside6":
                        metadata = (target / "pyproject.toml").read_text(encoding="utf-8")
                        self.assertNotIn(app_name, metadata)
                        if sys.version_info >= (3, 11):
                            import tomllib

                            self.assertIn("project", tomllib.loads(metadata))
                        self.assertTrue(compileall.compile_dir(target / "src", quiet=1))
                        self.assertTrue(compileall.compile_dir(target / "tests", quiet=1))
                    else:
                        main_cpp = (target / "src/app/main.cpp").read_text(encoding="utf-8")
                        self.assertIn('Ops \\"North\\" \\\\ Lab', main_cpp)

    def test_cpp_starter_ci_uses_the_manifest_tag(self):
        tag = json.loads(MODULE.STARTER_MANIFEST.read_text(encoding="utf-8"))["cpp_ci_tag"]
        for starter in ("existing-qt", "monitor", "workbench"):
            with self.subTest(starter=starter):
                target, _ = self.create("cpp", starter)
                workflow = (target / ".github/workflows/ci.yml").read_text(encoding="utf-8")
                self.assertIn(f"repository: calvinhxx/Fluent-Qt\n          ref: '{tag}'\n", workflow)

    def test_missing_or_empty_shared_directory_fails_before_creation(self):
        starters = self.root / "starters"
        workbench = starters / "cpp-workbench"
        workbench.mkdir(parents=True)
        (workbench / "README.md.in").write_text("Workbench", encoding="utf-8")
        shared = starters / "cpp-common"
        target = self.root / "sample"
        for problem in ("Missing", "Empty"):
            with self.subTest(problem=problem):
                if problem == "Empty":
                    shared.mkdir()
                with self.assertRaises(MODULE.CreateError) as failure:
                    MODULE.create_project(
                        target=target,
                        application="Sample",
                        identifier="sample",
                        language="cpp",
                        starter="workbench",
                        accent="#2457D6",
                        starters_root=starters,
                    )
                self.assertEqual(str(failure.exception), f"{problem} starter template directory: {shared}")
                self.assertFalse(target.exists())

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
