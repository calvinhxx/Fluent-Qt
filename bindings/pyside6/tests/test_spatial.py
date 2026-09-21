"""Optional Spatial binding ownership, runtime and input contracts."""
import gc
import json
from pathlib import Path
import unittest
import weakref

from PySide6.QtCore import QPointF, Qt
from PySide6.QtGui import QPolygonF, QTransform, QVector3D
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QGraphicsView, QWidget
import shiboken6
import fluentqt
from fluentqt.spatial import SpatialItem, SpatialView


class SpatialBindingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        fluentqt.initialize_resources()

    def setUp(self):
        fluentqt.set_motion_mode(fluentqt.MotionMode.Full)
        self.view = SpatialView()
        self.view.setRenderMode(SpatialView.RenderMode.Raster)

    def tearDown(self):
        if shiboken6.isValid(self.view):
            shiboken6.delete(self.view)
        self.app.processEvents()
        gc.collect()

    def test_manifest(self):
        manifest = json.loads((Path(__file__).parents[1] / "api-manifest.json").read_text())
        contract = manifest["optional_modules"]["fluentqt.spatial"]
        for name, methods in contract["methods"].items():
            cls = {"SpatialView": SpatialView, "SpatialItem": SpatialItem}[name]
            for method in methods:
                self.assertTrue(hasattr(cls, method), f"{name}.{method}")

    def test_borrowed_content_and_repeated_add(self):
        widget = fluentqt.Button("Borrowed")
        item = self.view.addWidget(widget)
        self.assertIs(self.view.addWidget(widget), item)
        self.assertEqual(self.view.itemCount(), 1)
        self.assertIs(item.widget(), widget)
        self.view.releaseItem(item)
        self.assertTrue(shiboken6.isValid(widget))
        self.assertIsNone(widget.parentWidget())
        self.assertFalse(shiboken6.isValid(item))
        shiboken6.delete(widget)

    def test_owned_content_survives_gc_then_is_destroyed(self):
        class Button(fluentqt.Button):
            pass
        widget = Button("Owned")
        reference = weakref.ref(widget)
        item = self.view.addOwnedWidget(widget)
        del widget
        gc.collect()
        self.assertIsNotNone(reference())
        widget = item.widget()
        self.assertIsInstance(widget, Button)
        self.view.releaseItem(item)
        self.assertFalse(shiboken6.isValid(widget))

    def test_reparented_restore_and_take(self):
        parent = QWidget()
        widget = fluentqt.Button("Restore", parent)
        item = self.view.addReparentedWidget(widget)
        self.view.releaseItem(item)
        self.assertIs(widget.parentWidget(), parent)
        item = self.view.addReparentedWidget(widget)
        self.assertIs(self.view.takeWidget(item), widget)
        self.assertIsNone(widget.parentWidget())
        shiboken6.delete(parent)
        self.assertTrue(shiboken6.isValid(widget))
        shiboken6.delete(widget)

    def test_view_destruction_respects_ownership(self):
        parent = QWidget()
        borrowed = fluentqt.Button("Borrowed")
        restored = fluentqt.Button("Restored", parent)
        owned = fluentqt.Button("Owned")
        self.view.addWidget(borrowed)
        self.view.addReparentedWidget(restored)
        self.view.addOwnedWidget(owned)
        shiboken6.delete(self.view)
        self.assertTrue(shiboken6.isValid(borrowed))
        self.assertIsNone(borrowed.parentWidget())
        self.assertIs(restored.parentWidget(), parent)
        self.assertFalse(shiboken6.isValid(owned))
        shiboken6.delete(parent)
        shiboken6.delete(borrowed)

    def test_live_input_pose_and_2d_roundtrip(self):
        button = fluentqt.ToggleSwitch()
        button.setFixedSize(160, 40)
        item = self.view.addOwnedWidget(button)
        item.setRotation(QVector3D(6, -12, 0))
        item.setSurfaceIntensity(.35)
        item.setHoverLift(2)
        self.view.setPointerTrackingEnabled(False)
        self.view.resize(640, 400)
        self.view.show()
        QTest.qWait(100)
        points = item.projectedPolygon()
        self.assertIn(len(points), (4, 5))
        points = QPolygonF([points[index] for index in range(4)])
        # A switch's indicator is near its left edge, map the widget into its quad.
        source = QPolygonF([QPointF(0, 0), QPointF(160, 0), QPointF(160, 40), QPointF(0, 40)])
        transform = QTransform()
        self.assertTrue(QTransform.quadToQuad(source, points, transform))
        canvas = self.view.findChild(QGraphicsView)
        QTest.mouseClick(canvas.viewport(), Qt.LeftButton,
                         pos=transform.map(QPointF(20, 20)).toPoint())
        self.assertTrue(button.isOn())
        self.view.setSpatialEnabled(False)
        self.assertTrue(button.isOn())
        self.view.setSpatialEnabled(True)
        self.assertTrue(button.isOn())
        spy = QSignalSpy(item.positionChanged)
        item.setPosition(QVector3D(1, 2, 30))
        self.assertEqual(spy.count(), 1)
        fluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)
        self.assertFalse(self.view.isSpatialEnabled())


if __name__ == "__main__":
    unittest.main()
