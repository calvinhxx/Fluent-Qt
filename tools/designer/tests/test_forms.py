"""Exercise actual form loading for every widget supplied by the installed bindings."""
import json
import os
from pathlib import Path
import sys
import unittest
from xml.etree import ElementTree as ET

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from PySide6.QtCore import QBuffer, QCoreApplication, QEvent, QIODevice
from PySide6.QtGui import QStandardItemModel
from PySide6.QtUiTools import QUiLoader
from PySide6.QtWidgets import QApplication, QDialogButtonBox, QWidget
import shiboken6
from fluentqt import initialize_resources
from fluentqt_designer_catalog import DIRECTORY, WIDGETS, widget_xml
from fluentqt_designer import available_widgets
from fluentqt_designer_forms import make_form_host, show_form_host


class DesignerForms(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        initialize_resources()
        cls.loader = QUiLoader(cls.app)
        cls.widgets = {entry["name"]: widget for entry, widget in available_widgets()}
        for widget in cls.widgets.values():
            cls.loader.registerCustomWidget(widget)

    def dispose(self, widget):
        widget.deleteLater()
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
        self.assertFalse(shiboken6.isValid(widget))
        self.assertTrue(shiboken6.isValid(self.loader))
        self.assertIs(QApplication.instance(), self.app)

    def test_catalog_forms(self):
        for entry in WIDGETS:
            name = entry["name"]
            with self.subTest(name=name):
                if name not in self.widgets:
                    self.assertIn("extension", entry)
                    continue
                widget_type = self.widgets[name]
                root = ET.fromstring('<ui version="4.0"><class>Form</class><widget class="QWidget" name="Form"/></ui>')
                root.find("widget").append(ET.fromstring(widget_xml(entry)).find("widget"))
                stream = QBuffer()
                stream.setData(ET.tostring(root, encoding="utf-8"))
                self.assertTrue(stream.open(QIODevice.OpenModeFlag.ReadOnly))
                form = self.loader.load(stream)
                control = form.findChild(widget_type)
                self.assertIsNotNone(control)
                self.assertEqual(control.metaObject().className(), name)
                if "property" in entry:
                    self.assertEqual(control.property(entry["property"]), entry["value"])
                self.assertTrue((DIRECTORY / "icons" / (name + ".png")).is_file())
                self.dispose(form)

    def test_grid_sample_model_is_reused_and_leaves_application_data_alone(self):
        grid = self.widgets["FluentDataGrid"]()
        data = json.dumps({"headers": ["端口", "说明"], "rows": [["A", "a & b"], ["B"]]})
        grid.setProperty("formTable", data)
        sample = grid.model()
        self.assertEqual(sample.index(0, 1).data(), "a & b")
        external = QStandardItemModel(1, 1)
        grid.setModel(external)
        grid.setProperty("formTable", '{"rows":[1]}')
        self.assertIs(grid.model(), external)
        self.assertEqual(grid.property("formTable"), data)
        grid.setProperty("formTable", "")
        self.assertIs(grid.model(), sample)
        self.assertEqual(sample.rowCount(), 0)
        self.assertEqual(external.rowCount(), 1)
        self.dispose(grid)

    def test_accordion_keeps_designer_pages_when_removed(self):
        from PySide6.QtWidgets import QLabel
        host = self.widgets["FluentAccordion"]()
        page = QLabel("Release notes")
        explicit = self.widgets["FluentExpander"]()
        host.addFormSection(explicit)
        host.insertFormSection(0, page)
        self.assertIs(host.formSectionPage(0), page)
        self.assertEqual(host.itemAt(0).headerText(), "Release notes")
        host.removeFormSection(0)
        self.assertIs(host.formSectionPage(0), explicit)
        self.assertTrue(shiboken6.isValid(page))
        self.assertEqual(host.count(), 1)
        self.dispose(host)
        self.dispose(page)

    def test_overlay_anchors(self):
        for entry, widget_type in available_widgets():
            if "surface" not in entry:
                continue
            kind = entry["surface"]
            with self.subTest(kind=kind):
                owner = QWidget()
                owner.resize(580, 360)
                anchor = widget_type(owner)
                owner.show()
                self.app.processEvents()
                if entry["adapter"] == "Commands":
                    anchor.setActionTexts(["First", "Second"])
                else:
                    anchor.setSurfaceText("Edited in Designer")
                if entry["adapter"] == "TimedMessage":
                    anchor.setDuration(0)
                surface = anchor.surface()
                anchor.click()
                self.app.processEvents()
                self.assertTrue(surface.isVisible())
                self.assertIs(anchor.surface(), surface)
                if entry["adapter"] == "Commands":
                    def actions():
                        if kind == "CommandPalette":
                            return surface.commands()
                        if kind == "CommandBarFlyout":
                            return surface.primaryActions()
                        return surface.actions()
                    received = []
                    anchor.actionTriggered.connect(received.append)
                    actions()[0].trigger()
                    self.assertEqual(received, ["First"])
                    anchor.setActionTexts(["Replacement"])
                    self.assertEqual([action.text() for action in actions()], ["Replacement"])
                if kind == "Toast":
                    self.assertEqual(surface.message(), "Edited in Designer")
                    self.assertEqual(surface.duration(), 0)
                if kind == "ToolTip":
                    anchor.setSurfaceText("Updated tip")
                    self.assertEqual(surface.text(), "Updated tip")
                self.dispose(anchor)
                QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
                self.assertFalse(shiboken6.isValid(surface))
                self.dispose(owner)

    def test_duration_changed(self):
        from fluentqt_designer import FluentSplashScreenButton, FluentToastButton

        for widget_type in (FluentSplashScreenButton, FluentToastButton):
            with self.subTest(widget=widget_type.__name__):
                anchor = widget_type()
                meta = anchor.metaObject()
                prop = meta.property(meta.indexOfProperty("duration"))
                self.assertEqual(bytes(prop.notifySignal().methodSignature()), b"durationChanged(int)")
                values = []
                anchor.durationChanged.connect(values.append)
                anchor.setProperty("duration", 700)
                anchor.surface()
                anchor.setDuration(700)
                anchor.setDuration(-1)
                anchor.setProperty("duration", 0)
                self.assertEqual(anchor.property("duration"), 0)
                self.assertEqual(values, [700, 0])
                self.dispose(anchor)

    def test_form_templates(self):
        from importlib.util import find_spec
        from fluentqt.dialogs_flyouts import ContentDialog, Dialog
        from fluentqt.windowing import Window
        hosts = {"Window": Window, "Dialog": Dialog, "ContentDialog": ContentDialog}
        if find_spec("fluentqt.spatial") is not None:
            from fluentqt.spatial import SpatialView
            hosts["SpatialItem"] = SpatialView
        for kind, host_type in hosts.items():
            with self.subTest(kind=kind):
                body = self.loader.load(str(DIRECTORY / "templates" / (kind + ".ui")))
                with self.assertRaises(ValueError):
                    make_form_host(body, body)
                host = make_form_host(body)
                self.assertIsInstance(host, host_type)
                show_form_host(host)
                self.app.processEvents()
                self.assertTrue(host.isVisible())
                if kind != "SpatialItem":
                    self.assertTrue(body.isVisibleTo(host))
                    self.assertTrue(host.isAncestorOf(body))
                if kind == "SpatialItem":
                    item, = host.items()
                    self.assertIs(item.widget(), body)
                    self.assertAlmostEqual(item.scale(), 0.9)
                    self.assertEqual(item.rotation().y(), 12)
                elif kind == "ContentDialog":
                    self.assertTrue(host.isOpen())
                    self.assertEqual(host.primaryButtonText(), "Apply")
                elif kind == "Dialog":
                    self.assertTrue(host.isOpen())
                    host.setAnimationEnabled(False)
                    box = body.findChild(QDialogButtonBox, "buttonBox")
                    box.button(QDialogButtonBox.StandardButton.Ok).click()
                    self.assertFalse(host.isOpen())
                host.close()
                self.dispose(host)

    def test_sample_connection(self):
        form = self.loader.load(str(DIRECTORY / "preview.ui"))
        name = form.findChild(QWidget, "projectName")
        self.assertTrue(name.text())
        choose = form.findChild(QWidget, "chooseName")
        choices = choose.surface().actions()
        choices[0].trigger()
        self.assertEqual(name.text(), choices[0].text())
        self.assertTrue(form.findChild(QWidget, "reopen").isChecked())
        form.findChild(QWidget, "clearName").click()
        self.assertEqual(name.text(), "")
        self.dispose(form)


if __name__ == "__main__":
    unittest.main()
