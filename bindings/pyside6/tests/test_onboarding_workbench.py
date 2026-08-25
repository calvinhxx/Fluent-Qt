#!/usr/bin/env python3

from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class PySide6WorkbenchInspectorTest(unittest.TestCase):
    project_root: Path

    def generate_workbench(self, root: Path) -> Path:
        create_script = self.project_root / "tools/onboarding/fluentqt_create.py"
        specification = importlib.util.spec_from_file_location(
            "fluentqt_create_for_pyside_test", create_script
        )
        self.assertIsNotNone(specification)
        self.assertIsNotNone(specification.loader)
        module = importlib.util.module_from_spec(specification)
        sys.modules[specification.name] = module
        specification.loader.exec_module(module)

        target = root / "generated-pyside-workbench"
        report = module.create_project(
            target=target,
            application="Inspector Workbench",
            identifier="inspector-workbench",
            language="pyside6",
            starter="workbench",
            accent="#2457D6",
        )
        self.assertEqual(report["status"], "created")
        return target

    def test_generated_workbench_emits_native_inspector_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            target = self.generate_workbench(Path(temporary))

            environment = dict(os.environ)
            package_paths = [str(target / "src")]
            existing_path = environment.get("PYTHONPATH")
            if existing_path:
                package_paths.append(existing_path)
            environment["PYTHONPATH"] = os.pathsep.join(package_paths)
            result = subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "inspector_workbench.app.main",
                    "--quality-report",
                ],
                cwd=target,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            quality_report = json.loads(result.stdout)
            self.assertEqual(quality_report["schema_version"], 1)
            self.assertEqual(quality_report["tool"], "FluentQt Inspector")
            self.assertEqual(quality_report["summary"]["findings"], 0)

    def test_generated_workbench_passes_application_scenes(self) -> None:
        import fluentqt
        from PySide6.QtCore import QEvent, QEventLoop, QTimer
        from PySide6.QtWidgets import QApplication

        manifest = json.loads(
            (self.project_root / "docs/ai/evals/application-scenes.json").read_text(
                encoding="utf-8"
            )
        )
        scenes = [
            scene
            for scene in manifest["scenes"]
            if scene["application"] == "fluentqt_pyside6_workbench"
            and scene["automation"] == "automated"
        ]
        self.assertEqual(len(scenes), 3)

        with tempfile.TemporaryDirectory() as temporary:
            target = self.generate_workbench(Path(temporary))
            source_path = str(target / "src")
            sys.path.insert(0, source_path)
            try:
                controller_module = importlib.import_module(
                    "inspector_workbench.application.workspace_controller"
                )
                repository_module = importlib.import_module(
                    "inspector_workbench.infrastructure.local_workspace_repository"
                )
                domain_module = importlib.import_module(
                    "inspector_workbench.domain.workspace"
                )
                window_module = importlib.import_module(
                    "inspector_workbench.ui.shell.main_window"
                )
                theme_module = importlib.import_module(
                    "inspector_workbench.ui.theme.brand_theme"
                )
                application = QApplication.instance() or QApplication([])
                fluentqt.initialize_resources()

                class EmptyRepository:
                    def current_workspace(self):
                        return domain_module.Workspace("", "")

                for scene in scenes:
                    with self.subTest(scene=scene["id"]):
                        repository = (
                            EmptyRepository()
                            if "empty" in scene["coverage"]
                            else repository_module.LocalWorkspaceRepository()
                        )
                        controller = controller_module.WorkspaceController(repository)
                        dark = scene["theme"] == "dark"
                        if dark:
                            controller.toggle_theme()
                        theme_module.apply_brand_theme(dark)
                        window = window_module.MainWindow(controller)
                        viewport = scene["viewport"]
                        window.resize(viewport["width"], viewport["height"])
                        window.show()
                        QApplication.sendPostedEvents(None, QEvent.Type.LayoutRequest)
                        application.processEvents(QEventLoop.ProcessEventsFlag.AllEvents)
                        settle_ms = scene.get("settle_ms", 0)
                        if settle_ms:
                            settle_loop = QEventLoop()
                            QTimer.singleShot(settle_ms, settle_loop.quit)
                            settle_loop.exec()

                        self.assertEqual(
                            (window.width(), window.height()),
                            (viewport["width"], viewport["height"]),
                        )
                        report = fluentqt.inspect_widget(window.contentWidget())
                        budget = scene["inspector"]
                        self.assertLessEqual(
                            report["summary"]["findings"], budget["max_findings"]
                        )
                        allowed_codes = set(budget["allowed_codes"])
                        for finding in report["findings"]:
                            self.assertIn(finding["code"], allowed_codes)
                        window.close()
                        application.processEvents()
            finally:
                sys.path.remove(source_path)
                for module_name in list(sys.modules):
                    if module_name == "inspector_workbench" or module_name.startswith(
                        "inspector_workbench."
                    ):
                        del sys.modules[module_name]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=Path, required=True)
    arguments, unittest_arguments = parser.parse_known_args()
    PySide6WorkbenchInspectorTest.project_root = arguments.project_root.resolve()
    program = unittest.main(argv=[sys.argv[0], *unittest_arguments], exit=False)
    return 0 if program.result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
