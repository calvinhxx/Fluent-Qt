"""Atomic QMLPlus cards execute their Python source and use native semantics."""

import unittest

import fluentqt
from PySide6.QtCore import QCoreApplication, QEvent
from PySide6.QtWidgets import QApplication, QWidget

from fluentqt_gallery.catalog import CONTRACT
from fluentqt_gallery.qmlplus_samples import build_example, python_source, sample_ids
from fluentqt_gallery.window import GalleryWindow


class GalleryQmlPlusTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.app.setProperty("fluentqtGalleryAutomated", True)
        fluentqt.initialize_resources()

    def setUp(self):
        self.old_motion = fluentqt.current_motion_mode()
        fluentqt.set_motion_mode(fluentqt.MotionMode.Disabled)
        self.panels = []

    def tearDown(self):
        for panel in self.panels:
            panel.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        QApplication.processEvents()
        fluentqt.set_motion_mode(self.old_motion)

    def example(self, name):
        panel = build_example("qmlplus-" + name)
        self.panels.append(panel)
        return panel._qmlplus_namespace

    def test_all_twelve_cards_follow_native_order_and_python_only_source(self):
        samples = CONTRACT["foundation_samples"]["foundation-qmlplus"]
        self.assertEqual(len(samples), 12)
        self.assertEqual(tuple(sample["id"] for sample in samples), sample_ids())
        window = GalleryWindow(startup_visuals=False)
        self.panels.append(window)
        page = window._ensure_page("foundation-qmlplus")[1]
        cards = page.findChildren(QWidget, "gallerySampleCard")
        self.assertEqual([card.property("gallerySampleId") for card in cards], list(sample_ids()))
        for card in cards:
            sample_id = card.property("gallerySampleId")
            code = card.findChild(fluentqt.CodeBlock, "galleryCodeBlock")
            self.assertIsNotNone(code)
            self.assertIsNone(code.findChild(QWidget, "galleryCodeBlockLanguageSelector"))
            code.copy()
            source = QApplication.clipboard().text()
            self.assertEqual(source, python_source(sample_id))
            self.assertLessEqual(len(source.strip().splitlines()), 20)
            self.assertNotIn("auto*", source)
            namespace = {"panel": QWidget(), "fluentqt": fluentqt}
            self.panels.append(namespace["panel"])
            exec(compile(source, "<copied-atomic-qmlplus>", "exec"), namespace)

    def test_one_way_two_way_computed_and_pause(self):
        n = self.example("binding-one-way")
        n["slider"].setValue(63)
        self.assertEqual(n["bar"].value(), 63)
        n = self.example("binding-two-way")
        n["first"].setIsOn(True)
        self.assertTrue(n["second"].isOn())
        n["second"].setIsOn(False)
        self.assertFalse(n["first"].isOn())
        n = self.example("binding-computed")
        self.assertEqual(n["result"].text(), "20 + 30 = 50")
        n["first"].setValue(25)
        n["second"].setValue(70)
        self.assertEqual(n["result"].text(), "25 + 70 = 95")
        n = self.example("binding-pause")
        n["pause"].setIsOn(True)
        n["slider"].setValue(72)
        self.assertEqual(n["bar"].value(), 40)
        n["pause"].setIsOn(False)
        self.assertEqual(n["bar"].value(), 72)

    def test_manual_conditional_inherited_and_binding_restore_states(self):
        for name in ("state-manual", "state-conditional"):
            n = self.example(name)
            n["toggle"].setIsOn(True)
            self.assertEqual(n["label"].text(), "Active")
            n["toggle"].setIsOn(False)
            self.assertEqual(n["label"].text(), "Default")
        n = self.example("state-inheritance")
        n["toggle"].setIsOn(True)
        self.assertEqual(n["button"].text(), "Saving...")
        self.assertFalse(n["button"].isEnabled())
        n["toggle"].setIsOn(False)
        self.assertEqual(n["button"].text(), "Ready")
        self.assertTrue(n["button"].isEnabled())
        n = self.example("state-binding-restore")
        n["toggle"].setIsOn(True)
        n["slider"].setValue(72)
        self.assertEqual(n["bar"].value(), 100)
        n["toggle"].setIsOn(False)
        self.assertEqual(n["bar"].value(), 72)

    def test_behavior_and_transition_resolve_disabled_motion_immediately(self):
        n = self.example("behavior")
        n["slider"].setValue(58)
        self.assertEqual(n["bar"].value(), 58)
        n = self.example("transition")
        n["toggle"].setIsOn(True)
        self.assertEqual(n["bar"].value(), 80)
        n["toggle"].setIsOn(False)
        self.assertEqual(n["bar"].value(), 20)

    def test_anchor_examples_resize_and_restore_the_original_relation(self):
        centered = self.example("anchors-center")
        moved = self.example("anchors-state")
        for width in (640, 320):
            for n in (centered, moved):
                n["panel"].resize(width, 220)
                n["panel"].show()
            QApplication.processEvents()
            self.assertAlmostEqual(centered["button"].geometry().center().x(),
                                   centered["surface"].rect().center().x(), delta=1)
            self.assertEqual(moved["button"].x(), 12)
            moved["toggle"].setIsOn(True)
            QApplication.processEvents()
            self.assertEqual(moved["button"].x() + moved["button"].width(),
                             moved["surface"].width() - 12)
            moved["toggle"].setIsOn(False)
            QApplication.processEvents()
            self.assertEqual(moved["button"].x(), 12)


if __name__ == "__main__":
    unittest.main()
