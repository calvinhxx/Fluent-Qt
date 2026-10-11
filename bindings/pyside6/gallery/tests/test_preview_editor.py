"""Exercise modular source edits against the real settings scene and host."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from test_live_preview import LivePreviewWindow, flush_deferred_deletes, wait_until
import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from fluent_qt_preview_client import PreviewClient
from fluent_qt_preview_session import PythonPreviewSession
from fluent_qt_preview_editor.controller import PreviewEditor
from fluent_qt_preview_editor.panel import EditorPanel
from fluent_qt_preview_editor.picker import RegionPicker


SCENE = Path(__file__).resolve().parents[4] / "tools/dev/scenes/settings_page.preview.py"


class PreviewEditorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        fluentqt.initialize_resources()
        fluentqt.set_theme(fluentqt.Theme.Light)
        cls.app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.scene = self.folder / "settings.py"
        self.scene.write_bytes(SCENE.read_bytes())
        self.window = LivePreviewWindow(self.scene, watch=False, content_only=True, window_size=(1240, 760))
        self.editor = PreviewEditor(self.window, self.folder / "edits")
        self.window.preview_editor = self.editor
        self.picker = self.editor.picker = RegionPicker(self.editor)
        self.panel = EditorPanel(self.editor, self.picker)
        self.window.mount_preview_tools(self.panel)
        self.window.show()
        self.assertTrue(self.window.reload_scene())
        self.session = PythonPreviewSession(self.window, self.folder / "session")
        self.addCleanup(self.close)

    def close(self):
        self.session.close()
        self.window.close()
        self.window.deleteLater()
        flush_deferred_deletes()

    def select(self):
        return self.editor.select("notifications")

    def test_pick_descendant_does_not_toggle_then_edit_compare_keep_and_undo(self):
        self.window.scene_widget.show_page(2)
        toggle = self.window.scene_widget.notifications
        self.picker.set_picking(True)
        QTest.mouseClick(toggle, Qt.MouseButton.LeftButton)
        self.assertTrue(toggle.isOn())
        self.assertEqual(self.editor.selected, "notifications")
        self.assertFalse(self.picker.picking)
        self.window.scene_widget.name.setText("保留用户输入")
        self.window.scene_widget.notifications.setIsOn(False)
        before_source = self.scene.read_bytes()
        before_height = self.editor.regions()["notifications"].widget.height()
        state = self.editor.inspect()
        result = self.editor.apply({"row_padding": 6, "heading_emphasis": False}, state["revision"], "notifications")
        self.assertTrue(result["pending"])
        self.assertEqual(self.window.scene_widget.name.text(), "保留用户输入")
        self.assertEqual(self.window.scene_widget.pages.currentIndex(), 2)
        self.assertEqual(self.window.scene_widget.nav[2].fluentStyle(), fluentqt.Button.ButtonStyle.Standard)
        self.assertFalse(self.window.scene_widget.sound_row.isEnabled())
        self.assertAlmostEqual(self.window.scene_widget.sound.property("knobPosition"), 1.0)
        self.assertLess(self.editor.regions()["notifications"].widget.height(), before_height)
        comparison = self.editor.comparison()
        self.assertTrue(Path(comparison["before_image"]).is_file())
        self.assertTrue(Path(comparison["after_image"]).is_file())
        self.assertIn('"row_padding": 6', comparison["diff"])
        self.panel.show_comparison()
        QApplication.processEvents()
        self.assertTrue(self.panel.comparison_window.isVisible())
        self.panel.comparison_window.close()
        self.editor.accept()
        self.assertEqual(result["pending"]["status"], "pending")
        self.editor.restore(undo=True)
        self.assertEqual(self.scene.read_bytes(), before_source)
        self.assertEqual(self.window.scene_widget.name.text(), "保留用户输入")

    def test_file_transport_survives_reload_and_expires_old_widget_ids(self):
        self.select()
        old_id = next(node["id"] for node in self.session.inspect()["nodes"] if node["object_name"] == "allowNotifications")
        old_session = self.session.session_id
        state = self.editor.inspect()
        with ThreadPoolExecutor(max_workers=1) as executor:
            future = executor.submit(PreviewClient(self.session.directory).request, "editor_apply",
                                     {"region": "notifications", "revision": state["revision"], "changes": {"row_padding": 8}})
            self.assertTrue(wait_until(future.done))
            self.assertIsNotNone(future.result()["pending"])
        self.assertNotEqual(self.session.session_id, old_session)
        with self.assertRaises(ValueError):
            self.session.resolve(old_id)
        self.editor.restore()

    def test_external_changes_are_not_overwritten_and_second_region_is_independent(self):
        self.editor.select("workspace")
        state = self.editor.inspect()
        self.editor.apply({"row_padding": 8}, state["revision"], "workspace")
        self.assertIn('"notifications": {"row_padding": 18', self.scene.read_text())
        manual = self.scene.read_bytes() + b'\n# Keep this external change\n'
        self.scene.write_bytes(manual)
        with self.assertRaises(RuntimeError):
            self.editor.restore()
        self.assertEqual(self.scene.read_bytes(), manual)
        self.assertTrue(self.editor.inspect()["conflict"])
        self.editor.execute("editor_dismiss", {})

    def test_failed_candidate_restores_source_and_keeps_user_state(self):
        source = self.scene.read_text().replace('def build(parent):\n',
            'def build(parent):\n    if PREVIEW_EDITS["notifications"]["row_padding"] == 4:\n        raise RuntimeError("rejected test value")\n')
        self.scene.write_text(source)
        self.assertTrue(self.window.reload_scene())
        self.select()
        self.window.scene_widget.name.setText("still here")
        with self.assertRaisesRegex(RuntimeError, "could not load"):
            self.editor.apply({"row_padding": 4}, self.editor.inspect()["revision"], "notifications")
        self.assertEqual(self.scene.read_text(), source)
        self.assertEqual(self.window.scene_widget.name.text(), "still here")
        self.assertIsNone(self.editor.history.pending)

    def test_invalid_external_source_can_end_comparison_without_overwriting(self):
        state = self.select()
        self.editor.apply({"row_padding": 8}, state["revision"], "notifications")
        broken = b'def build(:\n'
        self.scene.write_bytes(broken)
        self.assertFalse(self.window.reload_scene())
        self.assertTrue(self.panel.dismiss.isVisible())
        self.assertFalse(self.panel.revert.isEnabled())
        self.assertTrue(self.editor.execute("editor_dismiss", {})["dismissed"])
        self.assertEqual(self.scene.read_bytes(), broken)

    def test_source_must_match_the_loaded_scene_before_applying(self):
        self.select()
        self.scene.write_bytes(self.scene.read_bytes() + b'\n# newly edited source\n')
        state = self.editor.inspect()
        self.assertFalse(state["synchronized"])
        with self.assertRaisesRegex(RuntimeError, "not loaded"):
            self.editor.apply({"row_padding": 8}, state["revision"], "notifications")
        self.assertTrue(self.window.reload_scene())
        self.assertTrue(self.editor.inspect()["synchronized"])


if __name__ == "__main__":
    unittest.main()
