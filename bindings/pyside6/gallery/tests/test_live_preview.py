"""Focused behavior tests for the persistent FluentQt Live Scene host."""

from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
from textwrap import dedent
import unittest

import fluentqt
import shiboken6

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import (  # noqa: E402
    QCoreApplication,
    QElapsedTimer,
    QEvent,
    QEventLoop,
    QTimer,
    Qt,
)
from PySide6.QtGui import QPalette  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

LIVE_HOST = (
    Path(__file__).resolve().parents[4] / "tools" / "dev" / "fluent_qt_live_host.py"
)
sys.path.insert(0, str(LIVE_HOST.parent))
from fluent_qt_preview_session import PythonPreviewSession  # noqa: E402
SPEC = importlib.util.spec_from_file_location("fluent_qt_live_host", LIVE_HOST)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
LivePreviewWindow = MODULE.LivePreviewWindow


def scene_source(
    title: str,
    *,
    default_on: bool = False,
    default_note: str = "",
) -> str:
    return dedent(
        """
        import fluentqt
        from PySide6.QtWidgets import QVBoxLayout, QWidget

        SCENE_TITLE = {scene_title!r}

        def build(parent):
            root = QWidget(parent)
            root.setObjectName("testLiveScene")
            layout = QVBoxLayout(root)

            title = fluentqt.Label({title!r}, root)
            title.setObjectName("declarativeTitle")
            layout.addWidget(title)

            toggle = fluentqt.ToggleSwitch(root)
            toggle.setObjectName("stateToggle")
            toggle.setIsOn({default_on!r})
            layout.addWidget(toggle)

            note = fluentqt.LineEdit(root)
            note.setObjectName("stateNote")
            note.setText({default_note!r})
            layout.addWidget(note)
            return root
        """
    ).format(
        scene_title=title,
        title=title,
        default_on=default_on,
        default_note=default_note,
    )


def wait_until(predicate, timeout_ms: int = 4000) -> bool:
    timer = QElapsedTimer()
    timer.start()
    while timer.elapsed() < timeout_ms:
        QApplication.processEvents()
        if predicate():
            return True
        wait_for_events(20)
    QApplication.processEvents()
    return predicate()


def wait_for_events(duration_ms: int) -> None:
    loop = QEventLoop()
    QTimer.singleShot(duration_ms, loop.quit)
    loop.exec()


def flush_deferred_deletes() -> None:
    QApplication.processEvents()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    QApplication.processEvents()


class LivePreviewTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        if not fluentqt.initialize_resources():
            raise RuntimeError("FluentQt resources could not be initialized")
        fluentqt.reset_theme_tokens()
        fluentqt.set_theme(fluentqt.Theme.Light)
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    def close_window(self, window: LivePreviewWindow) -> None:
        window.close()
        window.deleteLater()
        flush_deferred_deletes()

    def test_session_operates_live_widgets_and_invalidates_ids_after_reload(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "session.preview.py"
            scene.write_text(scene_source("Session"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            session = PythonPreviewSession(window, Path(temporary) / "session")
            try:
                nodes = session.execute("inspect")["nodes"]
                note = next(n for n in nodes if n["object_name"] == "stateNote")
                session.execute("action", {"target": note["id"], "action": "type_text", "text": "User note"})
                self.assertEqual(session.resolve("stateNote").text(), "User note")
                session.execute("action", {"target": note["id"], "action": "key", "key": "space"})
                session.execute("action", {"target": note["id"], "action": "type_text", "text": "你好 👋"})
                self.assertEqual(session.resolve("stateNote").text(), "User note 你好 👋")
                session.resolve("stateNote").setText("User note")
                session.execute("action", {"target": "stateToggle", "action": "click"})
                self.assertTrue(session.resolve("stateToggle").isOn())
                session.execute("set_data", {"target": "stateNote", "property": "text", "value": "Temporary"})
                self.assertEqual(session.execute("reset_data")["restored"], 1)
                self.assertEqual(session.resolve("stateNote").text(), "User note")
                with self.assertRaises(ValueError):
                    session.execute("set_data", {"target": "stateNote", "property": "width", "value": 50})
                old_session = session.session_id
                scene.write_text("def build(:\n", encoding="utf-8")
                self.assertFalse(window.reload_scene())
                self.assertEqual(session.session_id, old_session)
                scene.write_text(scene_source("Changed"), encoding="utf-8")
                self.assertTrue(window.reload_scene())
                self.assertNotEqual(session.session_id, old_session)
                with self.assertRaises(ValueError):
                    session.resolve(note["id"])
                self.assertEqual(session.resolve("stateNote").text(), "User note")
                self.assertEqual(session.execute("screenshot")["mime_type"], "image/png")
            finally:
                session.close()
                self.close_window(window)

    def test_session_replies_to_file_commands_and_redacts_password(self):
        import json
        from fluent_qt_preview_client import write_json
        from PySide6.QtWidgets import QLineEdit

        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "session.preview.py"
            scene.write_text(scene_source("Session"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            directory = Path(temporary) / "session"
            session = PythonPreviewSession(window, directory)
            try:
                editor = session.resolve("stateNote")
                editor.setEchoMode(QLineEdit.EchoMode.Password)
                editor.setText("private")
                write_json(directory / "requests" / "probe.json", {
                    "id": "probe", "session_id": session.session_id,
                    "method": "inspect", "parameters": {},
                })
                response = directory / "responses" / "probe.json"
                self.assertTrue(wait_until(response.exists))
                result = json.loads(response.read_text(encoding="utf-8"))["result"]
                note = next(n for n in result["nodes"] if n["object_name"] == "stateNote")
                self.assertNotIn("text", note["data"])
            finally:
                session.close()
                self.close_window(window)

    def test_reset_keeps_failed_originals_and_retries_them(self):
        from PySide6.QtWidgets import QLineEdit

        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "reset.preview.py"
            scene.write_text(scene_source("Reset", default_note="original"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            session = PythonPreviewSession(window, Path(temporary) / "session")
            try:
                editor = session.resolve("stateNote")
                session.execute("set_data", {"target": "stateNote", "property": "text", "value": "temporary"})
                session.execute("set_data", {"target": "stateNote", "property": "placeholderText", "value": "hint"})
                editor.setEchoMode(QLineEdit.EchoMode.Password)
                result = session.execute("reset_data")
                self.assertIn("error", result)
                self.assertEqual((result["restored"], result["failed"], result["pending"]), (1, 1, 1))
                self.assertEqual(editor.text(), "temporary")
                self.assertEqual(editor.placeholderText(), "")
                editor.setEchoMode(QLineEdit.EchoMode.Normal)
                result = session.execute("reset_data")
                self.assertNotIn("error", result)
                self.assertEqual(result["restored"], 1)
                self.assertEqual(result["pending"], 0)
                self.assertEqual(editor.text(), "original")
                self.assertEqual(session.execute("reset_data")["restored"], 0)
            finally:
                session.close()
                self.close_window(window)

    def test_reset_checks_actual_values_after_setter_clamping(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "clamp.preview.py"
            scene.write_text(scene_source("Clamp"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            progress = fluentqt.ProgressBar(window.scene_widget)
            progress.setObjectName("progress")
            progress.setValue(80)
            session = PythonPreviewSession(window, Path(temporary) / "session")
            try:
                session.execute("set_data", {"target": "progress", "property": "value", "value": 20})
                progress.setMaximum(50)
                result = session.execute("reset_data")
                self.assertIn("error", result)
                self.assertEqual(result["pending"], 1)
                self.assertEqual(result["restored"], 0)
                progress.setMaximum(100)
                self.assertEqual(session.execute("reset_data")["restored"], 1)
                self.assertEqual(progress.value(), 80)
            finally:
                session.close()
                self.close_window(window)

    def test_removed_targets_make_click_key_and_text_fail(self):
        for action in ("click", "key", "type_text"):
            with self.subTest(action=action), TemporaryDirectory() as temporary:
                scene = Path(temporary) / "removal.preview.py"
                scene.write_text(scene_source("Removal"), encoding="utf-8")
                window = LivePreviewWindow(scene, watch=False)
                window.show()
                self.assertTrue(window.reload_scene())
                session = PythonPreviewSession(window, Path(temporary) / "session")
                try:
                    clicked = []
                    if action == "click":
                        target = fluentqt.Button("Remove", window.scene_widget)
                        target.setObjectName("target")
                        target.pressed.connect(target.deleteLater)
                        target.clicked.connect(lambda: clicked.append(True))
                        window.scene_widget.layout().addWidget(target)
                        target.show()
                    else:
                        target = session.resolve("stateNote")
                        target.setObjectName("target")
                        target.textChanged.connect(target.deleteLater)
                    QApplication.processEvents()
                    with self.assertRaisesRegex(RuntimeError, "removed"):
                        session.execute("action", {"target": "target", "action": action,
                                                   "key": "x", "text": "x"})
                    self.assertFalse(shiboken6.isValid(target))
                    self.assertEqual(clicked, [])
                finally:
                    session.close()
                    self.close_window(window)

    def test_source_helpers_use_the_selected_generated_contract(self):
        import json
        import subprocess
        from fluent_qt_live_preview import PySideRuntime, runtime_environment

        package_root = Path(fluentqt.__file__).resolve().parent.parent
        runtime = PySideRuntime(package_root.parent, Path(sys.executable), package_root)
        environment = runtime_environment(runtime)
        probe = """
import json, fluentqt
fluentqt.prepare_high_dpi_application()
from PySide6.QtWidgets import QApplication
app = QApplication([])
from fluentqt_gallery import catalog, foundation_pages
print(json.dumps({'helper': foundation_pages.__file__, 'catalog': catalog.__file__,
                  'schema': catalog.CONTRACT['schema_version']}))
"""
        result = subprocess.run([sys.executable, "-c", probe], env=environment,
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        source = LIVE_HOST.parents[2] / "bindings/pyside6/gallery/src/fluentqt_gallery"
        self.assertEqual(Path(report["helper"]).parent, source)
        self.assertEqual(Path(report["catalog"]).parent, source)
        self.assertEqual(report["schema"], 1)
        environment["FLUENTQT_LIVE_PREVIEW_CONTRACT"] = str(package_root / "missing-contract.json")
        invalid = subprocess.run([sys.executable, "-c", probe], env=environment,
                                 capture_output=True, text=True, timeout=20)
        self.assertNotEqual(invalid.returncode, 0)
        self.assertIn("missing-contract.json", invalid.stderr)

    def test_reload_keeps_window_and_restores_only_interaction_delta(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "state.preview.py"
            scene.write_text(
                scene_source("First title", default_note="declarative"),
                encoding="utf-8",
            )
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            window_id = int(window.winId())
            first_root = window.scene_widget
            self.assertIsNotNone(first_root)

            toggle = first_root.findChild(fluentqt.ToggleSwitch, "stateToggle")
            note = first_root.findChild(fluentqt.LineEdit, "stateNote")
            self.assertIsNotNone(toggle)
            self.assertIsNotNone(note)
            toggle.setIsOn(True)
            note.setText("typed by the user")
            note.setFocus()
            QApplication.processEvents()

            scene.write_text(
                scene_source("Changed in code", default_note="new default"),
                encoding="utf-8",
            )
            self.assertTrue(window.reload_scene())
            second_root = window.scene_widget
            self.assertIsNot(second_root, first_root)
            self.assertEqual(int(window.winId()), window_id)
            self.assertEqual(window.generation, 2)

            title = second_root.findChild(fluentqt.Label, "declarativeTitle")
            restored_toggle = second_root.findChild(
                fluentqt.ToggleSwitch, "stateToggle"
            )
            restored_note = second_root.findChild(fluentqt.LineEdit, "stateNote")
            self.assertEqual(title.text(), "Changed in code")
            self.assertTrue(restored_toggle.isOn())
            self.assertEqual(restored_note.text(), "typed by the user")

            flush_deferred_deletes()
            self.assertFalse(shiboken6.isValid(first_root))
            self.close_window(window)

    def test_failed_reload_keeps_last_usable_scene_and_can_recover(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "failure.preview.py"
            scene.write_text(scene_source("Usable"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            usable = window.scene_widget
            window_id = int(window.winId())

            scene.write_text("def build(:\n", encoding="utf-8")
            self.assertFalse(window.reload_scene())
            self.assertIs(window.scene_widget, usable)
            self.assertEqual(int(window.winId()), window_id)
            self.assertIn("SyntaxError", window.last_error)
            self.assertTrue(window.error_bar.isOpen())

            scene.write_text(scene_source("Recovered"), encoding="utf-8")
            self.assertTrue(window.reload_scene())
            self.assertEqual(window.generation, 2)
            title = window.scene_widget.findChild(
                fluentqt.Label, "declarativeTitle"
            )
            self.assertEqual(title.text(), "Recovered")
            self.assertFalse(window.error_bar.isOpen())
            self.close_window(window)

    def test_theme_switch_repaints_scene_surface_without_reloading(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "theme.preview.py"
            scene.write_text(scene_source("Theme"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())
            scene_root = window.scene_widget
            generation = window.generation
            viewport = window._scene_host.parentWidget()
            light_inspector = window._last_inspector

            self.assertFalse(
                viewport.testAttribute(
                    Qt.WidgetAttribute.WA_TranslucentBackground
                )
            )
            light_canvas = viewport.palette().color(QPalette.ColorRole.Window)

            window._toggle_theme()
            QApplication.processEvents()

            self.assertEqual(fluentqt.current_theme(), fluentqt.Theme.Dark)
            self.assertEqual(window._theme_button.text(), "Use light theme")
            self.assertIs(window.scene_widget, scene_root)
            self.assertEqual(window.generation, generation)
            self.assertIsNot(window._last_inspector, light_inspector)
            self.assertTrue(viewport.autoFillBackground())
            dark_canvas = viewport.palette().color(QPalette.ColorRole.Window)
            self.assertLess(dark_canvas.lightness(), light_canvas.lightness())

            window._toggle_theme()
            QApplication.processEvents()
            self.assertEqual(fluentqt.current_theme(), fluentqt.Theme.Light)
            self.assertIs(window.scene_widget, scene_root)
            self.assertEqual(window.generation, generation)
            self.close_window(window)

    def test_high_contrast_uses_dark_backed_binary_preview_contract(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "high-contrast.preview.py"
            scene.write_text(scene_source("High contrast"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())

            fluentqt.set_theme(fluentqt.Theme.HighContrast)
            QApplication.processEvents()
            window._refresh_theme_button()

            self.assertEqual(window._theme_button.text(), "Use light theme")
            self.assertEqual(window.report_payload()["window"]["theme"], "dark")

            window._toggle_theme()
            QApplication.processEvents()
            self.assertEqual(fluentqt.current_theme(), fluentqt.Theme.Light)
            self.close_window(window)

    def test_file_polling_survives_atomic_save_and_reloads_in_process(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "watched.preview.py"
            scene.write_text(scene_source("Before save"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=True, debounce_ms=40)
            window.show()
            self.assertTrue(window.reload_scene())
            window_id = int(window.winId())

            replacement = scene.with_suffix(".replacement")
            replacement.write_text(scene_source("After save"), encoding="utf-8")
            os.replace(replacement, scene)

            self.assertTrue(wait_until(lambda: window.generation >= 2))
            wait_for_events(300)
            QApplication.processEvents()
            self.assertEqual(window.generation, 2)
            self.assertEqual(int(window.winId()), window_id)
            title = window.scene_widget.findChild(
                fluentqt.Label, "declarativeTitle"
            )
            self.assertEqual(title.text(), "After save")
            self.close_window(window)

    def test_report_exposes_stable_process_and_inspector_contract(self):
        with TemporaryDirectory() as temporary:
            scene = Path(temporary) / "report.preview.py"
            scene.write_text(scene_source("Report"), encoding="utf-8")
            window = LivePreviewWindow(scene, watch=False)
            window.show()
            self.assertTrue(window.reload_scene())

            report = window.report_payload()
            self.assertEqual(report["schema_version"], 1)
            self.assertEqual(report["tool"], "FluentQt Live Scene")
            self.assertEqual(report["process"]["pid"], os.getpid())
            self.assertEqual(report["reload"]["generation"], 1)
            self.assertEqual(report["reload"]["failures"], 0)
            self.assertEqual(report["inspector"]["schema_version"], 1)
            self.close_window(window)


if __name__ == "__main__":
    unittest.main()
