"""CPU contracts for the private low-DPI Gallery glyph adapter."""
import faulthandler
import sys
import unittest
import weakref
from unittest.mock import patch

if __name__ == "__main__":
    # Preserve the blocked call stack before CTest's 60-second timeout,
    # including a stall while importing the native bindings.
    faulthandler.enable()
    faulthandler.dump_traceback_later(45)

import fluentqt

from PySide6.QtCore import QLineF, QPoint, QPointF, QRect, QRectF, Qt
from PySide6.QtGui import (
    QColor, QFont, QImage, QLinearGradient, QPaintDevice, QPaintEngine, QPainter, QPainterPath, QPen, QRegion,
    QTransform,
)
from PySide6.QtWidgets import QApplication, QWidget

from fluentqt_gallery.glyph_paint_device import (
    GlyphPaintDevice, glyph_raster_dpr, needs_native_glyph_coverage,
)
from fluentqt_gallery.spatial_support import SPATIAL_AVAILABLE


class GalleryGlyphPaintDeviceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        fluentqt.initialize_resources()

    def setUp(self):
        self.errors = []
        self.old_hook = sys.excepthook
        sys.excepthook = lambda *error: self.errors.append(error)

    def tearDown(self):
        sys.excepthook = self.old_hook
        self.assertEqual(self.errors, [])

    def render(self, draw, adapted, dpr=1, opaque=False, raster_origin=QPointF(), native_dpr=None):
        image = QImage(960, 360, QImage.Format_ARGB32_Premultiplied)
        image.setDevicePixelRatio(dpr)
        image.fill(Qt.white if opaque else Qt.transparent)
        device = GlyphPaintDevice(image, dpr if native_dpr is None else native_dpr,
                                  raster_origin) if adapted else image
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
        for bit in range(31):
            feature = QPaintEngine.PaintEngineFeature(1 << bit)
            self.assertEqual(adapter.paintEngine().hasFeature(feature), target.engine.hasFeature(feature))
        self.assertFalse(adapter.paintEngine().hasFeature(QPaintEngine.RasterOpModes))

    @unittest.skipUnless(SPATIAL_AVAILABLE, "optional Spatial binding is not installed")
    def test_platform_gate_delegates_to_shared_native_runtime(self):
        # Platform/DPR decisions belong to SpatialRuntime, not a Python-only fork.
        with patch("fluentqt.spatial.SpatialRuntime") as runtime:
            for dpr in (.75, 1, 1.5, 2):
                for expected in (False, True):
                    runtime.needsNativeGlyphCoverage.return_value = expected
                    self.assertEqual(needs_native_glyph_coverage(dpr), expected)
                    runtime.needsNativeGlyphCoverage.assert_called_with(dpr)

    def test_raster_density_preserves_each_fonts_hinting_policy(self):
        for hint in (QFont.PreferDefaultHinting, QFont.PreferNoHinting,
                     QFont.PreferVerticalHinting, QFont.PreferFullHinting):
            font = QFont()
            font.setHintingPreference(hint)
            for native, cache in ((1, 2), (1.5, 2.625), (2, 2)):
                with self.subTest(hint=hint, native=native, cache=cache):
                    self.assertEqual(glyph_raster_dpr(font, native, cache),
                                     cache if hint == QFont.PreferNoHinting else native)

    def test_shaped_items_choose_density_independently(self):
        hints = (QFont.PreferDefaultHinting, QFont.PreferNoHinting,
                 QFont.PreferVerticalHinting, QFont.PreferFullHinting)
        observed = []
        def density(font, native, cache):
            result = glyph_raster_dpr(font, native, cache)
            observed.append((font.hintingPreference(), result))
            return result
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(14)
            for row, hint in enumerate(hints):
                font.setHintingPreference(hint)
                painter.setFont(font)
                painter.drawText(QPointF(12, 25 + row * 30), "Independent glyph policy")
        with patch("fluentqt_gallery.glyph_paint_device.glyph_raster_dpr", side_effect=density):
            self.render(draw, True, 2, native_dpr=1)
        self.assertEqual(observed, [(hint, 2 if hint == QFont.PreferNoHinting else 1)
                                    for hint in hints])

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
        for dpr in (1, 1.25, 1.5, 2):
            with self.subTest(dpr=dpr):
                expected, _ = self.render(draw, False, dpr)
                actual, device = self.render(draw, True, dpr)
                self.assertEqual(device.raster_dpr(QFont()), dpr)
                self.assertGreater(device.engine.glyph_items, 2)
                # Two source-over operations may differ by one premultiplied channel bit;
                # shaping and decorations must match direct painting at the cache density.
                self.assertLessEqual(max(abs(a - b) for a, b in zip(
                    memoryview(actual.constBits()).cast("B"),
                    memoryview(expected.constBits()).cast("B"))), 1)

    def test_cache_density_rasterizes_detail_instead_of_enlarging_native_pixels(self):
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(14)
            font.setHintingPreference(QFont.PreferNoHinting)
            painter.setFont(font)
            painter.setPen(Qt.black)
            painter.drawText(QPointF(13, 30), "Small glyph detail 0123456789 " * 3)
        expected, _ = self.render(draw, False, 2)
        actual, device = self.render(draw, True, 2, native_dpr=1)
        self.assertEqual(actual, expected)
        self.assertGreater(device.engine.glyph_tiles, device.engine.glyph_items,
                           "Long text must exercise the bounded tile boundary")
        native, _ = self.render(draw, False)
        enlarged = native.scaled(native.size() * 2, Qt.IgnoreAspectRatio, Qt.FastTransformation)
        enlarged.setDevicePixelRatio(2)
        self.assertNotEqual(actual.copy(0, 0, 900, 90), enlarged.copy(0, 0, 900, 90))

    def test_fractional_tiles_preserve_shaped_font_phase_exactly(self):
        def draw(painter):
            font = fluentqt.font_for_role(fluentqt.FontRole.BodyStrong)
            font.setUnderline(True)
            font.setStrikeOut(True)
            painter.setFont(font)
            painter.setRenderHint(QPainter.TextAntialiasing)
            painter.setPen(QColor(32, 90, 150, 210))
            painter.setOpacity(.65)
            painter.drawText(QPointF(7.25, 76.5), "Gallery 设置 · العربية · Popup 0123456789")
        for dpr in (1.25, 1.5, 2.1875):
            for origin in (QPointF(), QPointF(0, 197 / dpr)):
                with self.subTest(dpr=dpr, origin=origin):
                    expected, _ = self.render(draw, False, dpr)
                    actual, device = self.render(draw, True, dpr, raster_origin=origin)
                    self.assertGreater(device.engine.glyph_items, 1)
                    self.assertEqual(actual, expected,
                                     "Tile offsets must not move Qt's shaped glyph phase")

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

    def test_glyph_tiles_are_bounded_by_visible_clip(self):
        def draw(painter):
            font = QFont("Arial")
            font.setPixelSize(22)
            painter.setFont(font)
            painter.translate(13, 7)
            painter.setClipRect(QRect(0, 0, 64, 50))
            painter.drawText(QPointF(0, 35), "Gallery 标题 العربية שלום " * 100)
        expected, _ = self.render(draw, False)
        for origin in (QPointF(), QPointF(113, 197)):
            with self.subTest(origin=origin):
                with patch("fluentqt_gallery.glyph_paint_device.QFontMetricsF") as metrics:
                    metrics.return_value.boundingRect.return_value = QRectF(-1e6, -1e6, 2e6, 2e6)
                    actual, device = self.render(draw, True, raster_origin=origin)
                self.assertEqual(actual, expected)
                self.assertGreater(device.engine.glyph_tiles, 0)
                self.assertLessEqual(device.engine.glyph_tiles, device.engine.glyph_items,
                                     "The visible clip fits one tile per shaped item, regardless of ink bounds")

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
    try:
        unittest.main(verbosity=2)
    finally:
        faulthandler.cancel_dump_traceback_later()
