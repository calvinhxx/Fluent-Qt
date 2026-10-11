"""Source integrity and recovery checks without a Qt installation."""

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from fluent_qt_preview_editor import Field
from fluent_qt_preview_editor.history import EditHistory
from fluent_qt_preview_editor.source import SourceDocument


SOURCE = ('# 保留注释\r\nPREVIEW_EDITS = {"通知": {"padding": 18, "strong": True}} # untouched\r\n'
          'application_value = "do not edit"\r\n').encode("utf-8")
FIELDS = (Field("padding", "Padding", "int", minimum=4, maximum=28), Field("strong", "Strong", "bool"))


class PreviewEditorSourceTest(unittest.TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / "scene.py"
        self.path.write_bytes(SOURCE)
        self.history = EditHistory(self.path, self.path.parent / "history")

    def begin(self, value=8):
        document = SourceDocument(self.path)
        changed = document.patch("通知", {"padding": value}, FIELDS, document.revision)
        return self.history.begin(document.content, changed, region="通知", title="Notifications", changes={"padding": value})

    def test_patches_only_literals_preserving_unicode_comments_and_crlf(self):
        document = SourceDocument(self.path)
        changed = document.patch("通知", {"padding": 8, "strong": False}, FIELDS, document.revision)
        self.assertEqual(changed, SOURCE.replace(b'18, "strong": True', b'8, "strong": False'))
        self.assertEqual(document.location("通知", "padding")["line"], 2)
        self.assertEqual(document.location("通知", "padding")["column"],
                         SOURCE.decode("utf-8").splitlines()[1].index("18") + 1)

    def test_rejects_unregistered_fields_invalid_types_and_stale_revision(self):
        document = SourceDocument(self.path)
        for changes in ({"arbitrary": 1}, {"padding": True}, {"padding": 100}, {"strong": 1}, {}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                document.patch("通知", changes, FIELDS, document.revision)
        with self.assertRaises(RuntimeError):
            document.patch("通知", {"padding": 8}, FIELDS, "stale")
        self.assertEqual(self.path.read_bytes(), SOURCE)

    def test_rejects_ambiguous_or_computed_source_dictionary(self):
        for source in ('PREVIEW_EDITS = dict(a=1)', 'PREVIEW_EDITS = {"a":{}, "a":{}}',
                       'PREVIEW_EDITS = {}\nPREVIEW_EDITS = {}', 'PREVIEW_EDITS = other = {}'):
            self.path.write_text(source)
            with self.subTest(source=source), self.assertRaises(ValueError):
                SourceDocument(self.path)

    def test_pending_survives_restart_and_reverts_exact_bytes(self):
        self.begin()
        reopened = EditHistory(self.path, self.history.directory)
        self.assertIsNotNone(reopened.pending)
        reopened.restore()
        self.assertEqual(self.path.read_bytes(), SOURCE)

    def test_kept_edits_can_be_undone_in_order(self):
        self.begin(8)
        self.history.accept()
        first = self.path.read_bytes()
        self.begin(12)
        self.history.accept()
        self.history.restore(undo=True)
        self.assertEqual(self.path.read_bytes(), first)
        self.history.restore(undo=True)
        self.assertEqual(self.path.read_bytes(), SOURCE)

    def test_conflicting_external_edits_are_preserved(self):
        self.begin()
        manual = self.path.read_bytes() + b'# external change\n'
        self.path.write_bytes(manual)
        with self.assertRaises(RuntimeError):
            self.history.restore()
        with self.assertRaises(RuntimeError):
            self.history.accept()
        self.history.dismiss()
        self.assertEqual(self.path.read_bytes(), manual)

    def test_pending_edit_blocks_a_second_mutation(self):
        self.begin()
        with self.assertRaises(RuntimeError):
            self.begin(12)
        self.history.restore()
        self.assertEqual(self.path.read_bytes(), SOURCE)


if __name__ == "__main__":
    unittest.main()
