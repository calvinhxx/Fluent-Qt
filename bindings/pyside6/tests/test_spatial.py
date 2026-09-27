"""Optional Spatial binding ownership, runtime and input contracts."""
import gc
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
import weakref

from PySide6.QtCore import QEventLoop, QObject, QPoint, QPointF, QSize, QTimer, Qt
from PySide6.QtGui import QAction, QPolygonF, QTransform, QVector3D
from PySide6.QtTest import QSignalSpy, QTest
from PySide6.QtWidgets import QApplication, QGraphicsView, QVBoxLayout, QWidget
from PySide6.QtOpenGLWidgets import QOpenGLWidget
import shiboken6
import fluentqt
from fluentqt._qt_compat import delete_qobject
from fluentqt.spatial import ParticleLayer, SpatialItem, SpatialRuntime, SpatialView


def _qwait(milliseconds):
    loop = QEventLoop()
    QTimer.singleShot(milliseconds, loop.quit)
    loop.exec()


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
            delete_qobject(self.view)
        self.app.processEvents()
        gc.collect()

    def test_manifest(self):
        manifest = json.loads((Path(__file__).parents[1] / "api-manifest.json").read_text())
        contract = manifest["optional_modules"]["fluentqt.spatial"]
        for name, methods in contract["methods"].items():
            cls = {"SpatialView": SpatialView, "SpatialItem": SpatialItem,
                   "ParticleLayer": ParticleLayer, "SpatialRuntime": SpatialRuntime}[name]
            for method in methods:
                self.assertTrue(hasattr(cls, method), f"{name}.{method}")

    def test_fresh_import_does_not_require_opengl_python_modules(self):
        result = subprocess.run([sys.executable, "-c", """
import sys
import fluentqt
from fluentqt.spatial import ParticleLayer, SpatialView
assert "PySide6.QtOpenGL" not in sys.modules
assert "PySide6.QtOpenGLWidgets" not in sys.modules
"""], env=os.environ.copy(), capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_embedded_command_surfaces_reopen_and_restore_2d(self):
        window = QWidget()
        window.resize(900, 650)
        self.view.setParent(window)
        self.view.setGeometry(20, 20, 860, 600)
        self.view.setPointerTrackingEnabled(False)
        card = fluentqt.Card()
        card.setFixedSize(260, 110)
        bar = fluentqt.CommandBar(card)
        bar.setGeometry(10, 5, 240, 48)
        anchor = fluentqt.Button("Commands", card)
        anchor.setGeometry(10, 65, 180, 36)
        flyout = fluentqt.CommandBarFlyout(card)
        flyout.setAnimationEnabled(False)
        for index in range(5):
            action = QAction(f"Command {index}", bar)
            self.assertTrue(bar.addSecondaryAction(action))
            self.assertTrue(flyout.addSecondaryAction(action))
        item = self.view.addOwnedWidget(card)
        item.setRotation(QVector3D(5, -12, 0))
        try:
            window.show()
            for spatial in (True, False, True):
                self.view.setSpatialEnabled(spatial)
                _qwait(30)
                bar.setOverflowOpen(True)
                self.app.processEvents()
                popup = window.findChild(QWidget, "FluentCommandBar.OverflowPopup")
                self.assertIsNotNone(popup)
                self.assertTrue(bar.isOverflowOpen())
                self.assertIs(popup.parentWidget(), window)
                self.assertGreater(popup.height(), card.height())
                bar.setOverflowOpen(False)
                for point in (QPoint(20, 10), QPoint(100, 20)):
                    flyout.showAtPoint(anchor, point, fluentqt.CommandBarFlyout.ShowMode.Transient)
                    self.app.processEvents()
                    self.assertTrue(flyout.isOpen())
                    self.assertIs(flyout.parentWidget(), window)
                    self.assertIsNone(flyout.graphicsProxyWidget())
                    self.assertGreater(flyout.height(), card.height())
                    self.assertTrue(window.rect().contains(flyout.geometry().center()))
                    evidence = os.environ.get("FLUENTQT_SPATIAL_EVIDENCE_DIR")
                    if evidence and self.app.platformName() not in ("offscreen", "minimal"):
                        output = Path(evidence)
                        output.mkdir(parents=True, exist_ok=True)
                        self.assertTrue(window.grab().save(str(output / f"commands-spatial-{spatial}.png")))
                    flyout.close()
                flyout.setAnimationEnabled(True)
                flyout.showAt(anchor)
                flyout.close()
                self.assertFalse(flyout.isOpen())
                self.assertTrue(flyout.isVisible())
                flyout.showAtPoint(anchor, QPoint(30, 10))
                self.assertTrue(flyout.isOpen())
                # Settle via the normal animation completion path, not a sleep.
                flyout.setAnimationEnabled(False)
                self.assertTrue(flyout.isOpen())
                self.assertTrue(flyout.isVisible())
                flyout.close()
        finally:
            delete_qobject(window)

    def test_embedded_chart_readout_uses_native_host_through_updates_and_2d(self):
        window = QWidget()
        window.resize(900, 650)
        self.view.setParent(window)
        self.view.setGeometry(40, 50, 800, 540)
        self.view.setPointerTrackingEnabled(False)
        card = fluentqt.Card()
        card.setFixedSize(340, 300)
        chart = fluentqt.DonutChart(card)
        chart.setGeometry(20, 16, 300, 230)
        chart.setLegendVisible(False)
        model = fluentqt.ChartModel(card)
        model.setPoints([QPointF(0, 65), QPointF(1, 35)], ["Used", "Free"])
        chart.setModel(model)
        item = self.view.addOwnedWidget(card)
        item.setRotation(QVector3D(5, -12, 0))
        try:
            window.show()
            _qwait(60)
            chart.setCurrentPoint(0, 0)
            # Internal C++ popups are native wrappers, not instances of the
            # convenience Python Popup subclass.
            readout = window.findChild(QWidget, "FluentChartReadout")
            self.assertIsNotNone(readout)
            self.assertTrue(readout.isOpen())
            self.assertIs(readout.parentWidget(), window)
            self.assertIsNone(readout.graphicsProxyWidget())
            for spatial in (True, False, True):
                self.view.setSpatialEnabled(spatial)
                chart.setCurrentPoint(0, 1)
                _qwait(30)
                self.assertIs(readout.parentWidget(), window)
                self.assertTrue(window.rect().contains(readout.geometry()))
                chart.setCurrentPoint(0, 0)
            self.view.hide()
            _qwait(30)
            self.assertFalse(readout.isOpen())
        finally:
            delete_qobject(window)

    def test_particle_layer_keeps_cpu_default_and_borrows_source(self):
        parent = QWidget()
        source = fluentqt.ParticleBackdrop(parent)
        layer = ParticleLayer(source)
        self.assertIs(layer.backdrop(), source)
        self.assertIs(source.parentWidget(), parent)
        self.assertFalse(layer.isActive())
        self.assertEqual(ParticleLayer.estimatedBytes(QSize(240, 160), 1.), 0)
        self.assertFalse(layer.render(1.))
        self.assertEqual(layer.textureId(), 0)
        self.assertEqual(layer.allocatedBytes(), 0)
        spy = QSignalSpy(layer.activeChanged)
        layer.release()
        layer.release()
        self.assertEqual(spy.count(), 0)
        delete_qobject(parent)
        self.assertIsNone(layer.backdrop())
        self.assertFalse(layer.render(1.))
        delete_qobject(layer)

    def test_particle_layer_python_source_collection_is_safe(self):
        source = fluentqt.ParticleBackdrop()
        source_reference = weakref.ref(source)
        layer = ParticleLayer(source)
        del source
        gc.collect()
        self.assertIsNone(source_reference())
        self.assertIsNone(layer.backdrop())
        self.assertFalse(layer.render(1.))
        delete_qobject(layer)

    def test_particle_layer_parent_wrapper_survives_explicit_child_deletion(self):
        for owner_type in (QObject, QWidget):
            with self.subTest(owner=owner_type):
                owner = owner_type()
                source = fluentqt.ParticleBackdrop()
                layer = ParticleLayer(source, owner)
                layer.release()
                delete_qobject(layer)
                self.assertFalse(shiboken6.isValid(layer))
                self.assertEqual(owner.children(), [])
                self.assertTrue(shiboken6.isValid(source))
                delete_qobject(owner)
                self.assertTrue(shiboken6.isValid(source))
                delete_qobject(source)

    def test_particle_layer_native_current_context_and_source_teardown(self):
        if self.app.platformName() in ("offscreen", "minimal"):
            self.skipTest("Requires the native OpenGL compositor path")
        context = QOpenGLWidget()
        context.resize(320, 220)
        parent = QWidget()
        parent.resize(240, 160)
        source = fluentqt.ParticleBackdrop(parent)
        source.resize(parent.size())
        source.setAnimationEnabled(False)
        source.setGpuAccelerationEnabled(True)
        source.setEffect(fluentqt.ParticleBackdrop.Effect.Starfield)
        layer = ParticleLayer(source)
        spy = QSignalSpy(layer.activeChanged)
        try:
            context.show()
            parent.show()
            self.assertTrue(QTest.qWaitForWindowExposed(context))
            self.assertTrue(QTest.qWaitForWindowExposed(parent))
            context.makeCurrent()
            self.assertTrue(context.isValid())
            estimate = ParticleLayer.estimatedBytes(source.size(), 1.)
            self.assertGreater(estimate, 0)
            self.assertTrue(layer.render(1., estimate))
            self.assertTrue(layer.isActive())
            self.assertEqual(layer.allocatedBytes(), estimate)
            self.assertGreater(layer.textureId(), 0)
            self.assertEqual(layer.textureSize(), source.size())
            count = layer.renderedFrameCount()
            self.assertTrue(layer.render(1., estimate))
            self.assertEqual(layer.renderedFrameCount(), count)
            self.assertIs(layer.backdrop(), source)
            self.assertFalse(layer.render(1., 1))
            self.assertEqual(layer.allocatedBytes(), 0)
            self.assertEqual(source.effect(), fluentqt.ParticleBackdrop.Effect.Starfield)
            self.assertFalse(source.isAnimationEnabled())
            self.assertTrue(layer.render(1., estimate))
            delete_qobject(source)
            self.assertIsNone(layer.backdrop())
            self.assertFalse(layer.isActive())
            self.assertEqual(layer.textureId(), 0)
            self.assertEqual(layer.allocatedBytes(), 0)
            self.assertEqual(spy.count(), 4)
            self.assertEqual(len(parent.findChildren(QOpenGLWidget)), 0)
        finally:
            layer.release()
            delete_qobject(layer)
            context.doneCurrent()
            delete_qobject(parent)
            delete_qobject(context)

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
        delete_qobject(widget)

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

    def test_rejected_content_preserves_parent_layout(self):
        for method in ("addBorrowedWidget", "addOwnedWidget", "addReparentedWidget"):
            with self.subTest(method=method):
                parent = QWidget()
                layout = QVBoxLayout(parent)
                widget = QOpenGLWidget()
                layout.addWidget(widget)
                try:
                    self.assertIsNone(getattr(self.view, method)(widget))
                    self.assertIs(widget.parentWidget(), parent)
                    self.assertEqual(layout.count(), 1)
                    self.assertIs(layout.itemAt(0).widget(), widget)
                finally:
                    delete_qobject(parent)

    def test_original_parent_destruction_preserves_hosted_content(self):
        for spatial in (False, True):
            self.view.setSpatialEnabled(spatial)
            for method in ("addBorrowedWidget", "addOwnedWidget", "addReparentedWidget"):
                with self.subTest(spatial=spatial, method=method):
                    parent = QWidget()
                    layout = QVBoxLayout(parent)
                    widget = fluentqt.Button("Hosted")
                    layout.addWidget(widget)
                    item = getattr(self.view, method)(widget)
                    self.assertIsNotNone(item)
                    delete_qobject(parent)
                    self.assertTrue(shiboken6.isValid(widget))
                    self.assertIs(item.widget(), widget)
                    self.view.releaseItem(item)
                    if method == "addOwnedWidget":
                        self.assertFalse(shiboken6.isValid(widget))
                    else:
                        self.assertTrue(shiboken6.isValid(widget))
                        self.assertIsNone(widget.parentWidget())
                        delete_qobject(widget)

    def test_reparented_restore_and_take(self):
        parent = QWidget()
        widget = fluentqt.Button("Restore", parent)
        item = self.view.addReparentedWidget(widget)
        self.view.releaseItem(item)
        self.assertIs(widget.parentWidget(), parent)
        item = self.view.addReparentedWidget(widget)
        self.assertIs(self.view.takeWidget(item), widget)
        self.assertIsNone(widget.parentWidget())
        delete_qobject(parent)
        self.assertTrue(shiboken6.isValid(widget))
        delete_qobject(widget)

    def test_view_destruction_respects_ownership(self):
        parent = QWidget()
        borrowed = fluentqt.Button("Borrowed")
        restored = fluentqt.Button("Restored", parent)
        owned = fluentqt.Button("Owned")
        self.view.addWidget(borrowed)
        self.view.addReparentedWidget(restored)
        self.view.addOwnedWidget(owned)
        delete_qobject(self.view)
        self.assertTrue(shiboken6.isValid(borrowed))
        self.assertIsNone(borrowed.parentWidget())
        self.assertIs(restored.parentWidget(), parent)
        self.assertFalse(shiboken6.isValid(owned))
        delete_qobject(parent)
        delete_qobject(borrowed)

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
        _qwait(100)
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
