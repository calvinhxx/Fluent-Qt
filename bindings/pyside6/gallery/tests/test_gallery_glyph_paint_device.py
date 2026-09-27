"""CPU contracts for the private low-DPI Gallery glyph adapter."""
import sys
import unittest
import weakref
from unittest.mock import patch

from PySide6.QtCore import QLineF, QPoint, QPointF, QRect, QRectF, Qt
from PySide6.QtGui import (
    QColor, QFont, QImage, QLinearGradient, QPaintDevice, QPaintEngine, QPainter, QPainterPath, QPen, QRegion,
    QTransform,
)
from PySide6.QtWidgets import QApplication, QWidget

from fluentqt_gallery.glyph_paint_device import GlyphPaintDevice, needs_native_glyph_coverage


class GalleryGlyphPaintDeviceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.errors = []
        self.old_hook = sys.excepthook
        sys.excepthook = lambda *error: self.errors.append(error)

    def tearDown(self):
        sys.excepthook = self.old_hook
        self.assertEqual(self.errors, [])

    def render(self, draw, adapted, dpr=1, opaque=False):
        image = QImage(960, 360, QImage.Format_ARGB32_Premultiplied)
        image.setDevicePixelRatio(dpr)
        image.fill(Qt.white if opaque else Qt.transparent)
        device = GlyphPaintDevice(image, 1) if adapted else image
        painter = QPainter(device)
        try:
            draw(painter)
        finally:
            painter.end()
        return image, device

    def test_adapter_does_not_advertise_unsupported_backend_operations(self):
        class Engine(QPaintEngine):
            def __init__(self):
                super().__init__(QPaintEngine.AlphaBlend | QPaintEngine.PorterDuff | QPaintEngine.PainterPaths)

        class Device(QPaintDevice):
            def __init__(self):
                super().__init__()
                self.engine = Engine()

            def paintEngine(self):
                return self.engine

        target = Device()
        adapter = GlyphPaintDevice(target, 1)
        for bit in range(32):
            feature = QPaintEngine.PaintEngineFeature(1 << bit)
            self.assertEqual(adapter.paintEngine().hasFeature(feature), target.engine.hasFeature(feature))
        self.assertFalse(adapter.paintEngine().hasFeature(QPaintEngine.RasterOpModes))

    def test_platform_gate_delegates_to_shared_native_runtime(self):
        # Platform/DPR decisions belong to SpatialRuntime, not a Python-only fork.
        with patch("fluentqt.spatial.SpatialRuntime") as runtime:
            for dpr in (.75, 1, 1.5, 2):
                for expected in (False, True):
                    runtime.needsNativeGlyphCoverage.return_value = expected
                    self.assertEqual(needs_native_glyph_coverage(dpr), expected)
                    runtime.needsNativeGlyphCoverage.assert_called_with(dpr)

    def test_non_text_transform_clip_opacity_and_composition_are_forwarded(self):
        def draw(painter):
            painter.setRenderHint(QPainter.Antialiasing)
            painter.fillRect(QRect(4, 5, 80, 20), Qt.red)
            painter.save()
            painter.translate(60, 38)
            painter.rotate(11)
            clip = QPainterPath()
            clip.addRoundedRect(QRectF(0, 0, 95, 50), 7, 7)
            painter.setClipPath(clip)
            painter.setOpacity(.45)
            painter.fillRect(QRect(-20, -20, 140, 90), QColor("#2060a0"))
            painter.setPen(QPen(Qt.black, 2))
            painter.drawEllipse(QRectF(15, 10, 65, 30))
            painter.restore()
            painter.setClipRect(QRect(0, 90, 120, 40))
            painter.setCompositionMode(QPainter.CompositionMode_Source)
            painter.fillRect(QRect(20, 80, 150, 50), QColor(20, 80, 100, 90))
            painter.setClipping(False)
            painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
            painter.drawLine(QPoint(140, 80), QPoint(160, 110))
            painter.drawRects([QRect(210, 10, 20, 15), QRect(260, 15, 30, 25)])
            painter.drawLines([QLineF(215, 80, 230, 95), QLineF(240, 75, 250, 90)])
            painter.drawPoints([QPoint(270, 80), QPoint(280, 100)])
            painter.drawPolygon([QPoint(300, 10), QPoint(330, 30), QPoint(300, 55)])
        for dpr in (1, 1.5, 2):
            with self.subTest(dpr=dpr):
                expected, _ = self.render(draw, False, dpr)
                actual, device = self.render(draw, True, dpr)
                self.assertEqual(actual, expected)
                self.assertEqual(device.engine.glyph_items, 0)

    def test_shaped_fallback_bold_rtl_and_long_text_keep_native_coverage(self):
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(22)
            font.setBold(True)
            font.setUnderline(True)
            painter.setFont(font)
            painter.setPen(QColor(20, 30, 40, 210))
            painter.drawText(QPointF(13, 42), "Gallery 标题  العربية שלום fi ffi")
            font.setBold(False)
            font.setUnderline(False)
            painter.setFont(font)
            painter.setOpacity(.65)
            painter.drawText(QPointF(13, 90), "abcdefghijklmnopqrstuvwxyz 0123456789 " * 3)
        expected, _ = self.render(draw, False)
        actual, device = self.render(draw, True)
        self.assertGreater(device.engine.glyph_items, 2)
        # Two source-over operations may differ by one premultiplied channel bit;
        # glyph positions, fallback selection, decorations and coverage cannot move.
        self.assertLessEqual(max(abs(a - b) for a, b in zip(
            memoryview(actual.constBits()).cast("B"), memoryview(expected.constBits()).cast("B"))), 1)

    def test_patterned_and_rotated_text_keep_original_painter_path(self):
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(22)
            painter.setFont(font)
            painter.translate(30, 30)
            painter.rotate(8)
            painter.drawText(QPointF(10, 20), "rotated text")
            painter.resetTransform()
            gradient = QLinearGradient(0, 0, 240, 0)
            gradient.setColorAt(0, Qt.red)
            gradient.setColorAt(1, Qt.blue)
            painter.setPen(QPen(gradient, 1))
            painter.drawText(QPointF(10, 95), "gradient text")
        expected, _ = self.render(draw, False)
        actual, device = self.render(draw, True)
        self.assertEqual(actual, expected)
        self.assertEqual(device.engine.glyph_items, 0)

    def test_text_hints_and_opaque_background_are_preserved(self):
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(18)
            painter.setFont(font)
            painter.setRenderHint(QPainter.TextAntialiasing, False)
            painter.setPen(Qt.black)
            painter.drawText(QPointF(10, 30), "Pixel hint 012345")
            painter.setBackground(QColor(20, 100, 80, background_alpha))
            painter.setBackgroundMode(Qt.OpaqueMode)
            painter.drawText(QPointF(10, 65), "Opaque background")
        for background_alpha in (255, 120):
            with self.subTest(background_alpha=background_alpha):
                expected, _ = self.render(draw, False)
                actual, device = self.render(draw, True)
                self.assertEqual(actual, expected)
                self.assertEqual(device.engine.glyph_items, 1,
                                 "Only ordinary text uses a transparent glyph tile")

    def test_opacity_is_applied_before_native_glyph_coverage(self):
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(22)
            painter.setFont(font)
            painter.setPen(QColor(20, 30, 40, alpha))
            painter.setOpacity(opacity)
            painter.drawText(QPointF(13, 90), "abcdefghijklmnopqrstuvwxyz 0123456789 " * 3)
        for alpha in (96, 210, 255):
            for opacity in (.1, .65, 1):
                with self.subTest(alpha=alpha, opacity=opacity):
                    expected, _ = self.render(draw, False)
                    actual, device = self.render(draw, True)
                    self.assertGreater(device.engine.glyph_items, 0)
                    self.assertEqual(actual, expected)

    def test_widget_system_clip_does_not_leak_children(self):
        class Fill(QWidget):
            def paintEvent(self, event):
                painter = QPainter(self)
                painter.fillRect(QRect(-500, -500, 1500, 1500), Qt.red)
                painter.end()
        parent = QWidget()
        parent.resize(240, 140)
        child = Fill(parent)
        child.setGeometry(80, 20, 40, 50)
        def draw(painter):
            painter.translate(13, 7)
            parent.render(painter, QPoint(), QRegion(QRect(0, 0, 100, 100)), QWidget.DrawChildren)
        expected, _ = self.render(draw, False)
        actual, _ = self.render(draw, True)
        self.assertEqual(actual, expected)

    def test_paint_device_has_no_engine_reference_cycle(self):
        image = QImage(40, 40, QImage.Format_ARGB32_Premultiplied)
        device = GlyphPaintDevice(image, 1)
        painter = QPainter(device)
        painter.fillRect(QRect(0, 0, 40, 40), Qt.red)
        painter.end()
        reference = weakref.ref(device)
        del painter, device
        self.assertIsNone(reference(), "Dirty-cache strips must release without a GC collection")

    def test_widget_clip_is_reused_without_losing_changes_to_paint_state(self):
        class Dots(QWidget):
            def paintEvent(self, event):
                painter = QPainter(self)
                painter.setRenderHint(QPainter.Antialiasing)
                for number in range(160):
                    painter.setPen(QPen(QColor(number, 40, 200), 1))
                    painter.setBrush(QColor(80, number, 40))
                    painter.setOpacity(.4 + number / 400)
                    painter.drawEllipse(QRectF(number % 16 * 4 - 10, number // 16 * 4 - 8, 5, 5))
                painter.setClipRect(QRect(0, 0, 35, 28))
                painter.fillRect(QRect(-5, -5, 70, 70), QColor(30, 120, 80, 90))
                painter.translate(3, 4)
                painter.setClipping(False)
                painter.setOpacity(1)
                painter.fillRect(QRect(20, 20, 80, 80), Qt.blue)
                painter.end()
        parent = QWidget()
        parent.resize(240, 140)
        children = [Dots(parent), Dots(parent)]
        children[0].setGeometry(10, 10, 48, 44)
        children[1].setGeometry(85, 55, 53, 42)
        def draw(painter):
            painter.translate(13, 7)
            parent.render(painter, QPoint(), QRegion(QRect(0, 0, 120, 110)), QWidget.DrawChildren)
        for dpr in (1, 1.5, 2):
            with self.subTest(dpr=dpr):
                expected, _ = self.render(draw, False, dpr)
                actual, device = self.render(draw, True, dpr)
                self.assertEqual(actual, expected)
                self.assertLess(device.engine.system_clip_applications, 20,
                                "Hundreds of primitives must not rebuild the same system clip")


if __name__ == "__main__":
    unittest.main()
