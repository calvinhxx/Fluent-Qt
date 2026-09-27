"""Paint-order, fallback and real-context contracts for Gallery GPU particles."""
import sys
import unittest

import fluentqt
from fluentqt._qt_compat import delete_qobject as delete
from PySide6.QtCore import QMarginsF, QPoint, QRect, QSize, Qt
from PySide6.QtGui import QColor, QOpenGLContext, QPainter, QPalette, QRegion
from PySide6.QtOpenGL import QOpenGLFramebufferObject, QOpenGLFramebufferObjectFormat, QOpenGLPaintDevice
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QGraphicsOpacityEffect, QWidget

from fluentqt_gallery.glyph_paint_device import GlyphPaintDevice, needs_native_glyph_coverage
from fluentqt_gallery.particle_compositor import (
    GalleryParticleCompositor, _clipped_rect, _foreground_widgets, _particle_layer_type,
)


class _Foreground(QWidget):
    def __init__(self, color, parent):
        super().__init__(parent)
        self.color = color

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.fillRect(self.rect(), self.color)
        painter.setPen(QColor(255, 255, 255, 176))
        painter.drawText(self.rect().adjusted(12, 8, -12, -8), Qt.AlignCenter, "Foreground Aa 123")
        painter.end()


def _format():
    format_ = QOpenGLFramebufferObjectFormat()
    format_.setInternalTextureFormat(0x8058)
    format_.setAttachment(QOpenGLFramebufferObject.CombinedDepthStencil)
    format_.setSamples(2)
    return format_


def _capture(root, dpr=1):
    pixels = QSize(root.width() * dpr, root.height() * dpr)
    paint = QOpenGLFramebufferObject(pixels, _format())
    texture = QOpenGLFramebufferObject(pixels)
    paint.bind()
    context = QOpenGLContext.currentContext()
    gl = context.functions()
    gl.glDisable(0x0C11)
    gl.glColorMask(True, True, True, True)
    gl.glStencilMask(0xFFFFFFFF)
    gl.glClearColor(0, 0, 0, 0)
    gl.glClear(0x4000 | 0x0400)
    device = QOpenGLPaintDevice(pixels)
    device.setDevicePixelRatio(dpr)
    glyph = GlyphPaintDevice(device, 1)
    painter = QPainter(glyph if needs_native_glyph_coverage(1) else device)
    try:
        root.render(painter, QPoint(), QRegion(), QWidget.DrawChildren)
    finally:
        painter.end()
    QOpenGLFramebufferObject.blitFramebuffer(texture, paint)
    return texture


def _render_foreground(painter, widget, region):
    widget.render(painter, region.boundingRect().topLeft(), region, QWidget.DrawChildren)


def _difference(actual, expected, rect):
    # Both paths use the same FBO readback format, including premultiplication.
    a, e = actual.copy(rect), expected.copy(rect)
    av, ev = memoryview(a.constBits()).cast("B"), memoryview(e.constBits()).cast("B")
    return sum(abs(x - y) for x, y in zip(av, ev)) / len(av)


class GalleryParticleCompositorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        fluentqt.initialize_resources()

    def setUp(self):
        self.errors, self.old_hook = [], sys.excepthook
        sys.excepthook = lambda *error: self.errors.append(error)

    def tearDown(self):
        sys.excepthook = self.old_hook
        self.assertEqual(self.errors, [], "unhandled Qt callback error")

    def test_foreground_order_tracks_raise_and_excludes_native_windows(self):
        root = QWidget()
        try:
            below = QWidget(root)
            parent = QWidget(root)
            source = fluentqt.ParticleBackdrop(parent)
            child = QWidget(source)
            sibling = QWidget(parent)
            above = QWidget(root)
            hidden = QWidget(root)
            hidden.hide()
            popup = QWidget(root, Qt.Tool)
            root.show()
            popup.show()
            self.assertEqual(_foreground_widgets(source, root), [child, sibling, above])
            below.raise_()
            self.assertEqual(_foreground_widgets(source, root), [child, sibling, above, below])
        finally:
            delete(root)

    def test_clipping_preserves_ancestors_visibility_and_unsupported_masks(self):
        root = QWidget()
        try:
            root.resize(300, 180)
            parent = QWidget(root)
            parent.setGeometry(20, 25, 100, 80)
            source = fluentqt.ParticleBackdrop(parent)
            source.setGeometry(-10, 10, 150, 90)
            root.show()
            self.assertEqual(_clipped_rect(source, root), QRect(20, 35, 100, 70))
            parent.setMask(QRegion(QRect(0, 0, 100, 80), QRegion.Ellipse))
            self.assertTrue(_clipped_rect(source, root).isEmpty())
            parent.clearMask()
            parent.hide()
            self.assertTrue(_clipped_rect(source, root).isEmpty())
        finally:
            delete(root)

    def test_no_current_context_or_disabled_preference_remains_cpu(self):
        root = QWidget()
        compositor = GalleryParticleCompositor()
        try:
            root.resize(300, 180)
            source = fluentqt.ParticleBackdrop(root)
            source.resize(root.size())
            for enabled in (False, True):
                self.assertFalse(compositor.prepare(root, 1, root.size(), 1, 1, 32 << 20, enabled))
                self.assertEqual(compositor.activeLayerCount(), 0)
                self.assertEqual(compositor.allocatedBytes(), 0)
                self.assertEqual(compositor.sourceScanCount(), 0)
            self.assertFalse(source.isAnimating())
        finally:
            compositor.release()
            delete(compositor)
            delete(root)

    def _native_context(self):
        if QApplication.platformName() in ("offscreen", "minimal", "vnc"):
            self.skipTest("requires a native desktop GL context")
        if _particle_layer_type() is None:
            self.skipTest("optional ParticleLayer binding is not installed")
        surface = QOpenGLWidget()
        surface.resize(420, 280)
        surface.show()
        self.addCleanup(delete, surface)
        self.assertTrue(QTest.qWaitForWindowExposed(surface))
        for _ in range(40):
            if surface.isValid():
                break
            QTest.qWait(25)
        self.assertTrue(surface.isValid())
        surface.makeCurrent()
        return surface

    def test_native_insertion_preserves_foreground_overlap_and_frame_cache(self):
        surface = self._native_context()
        root = QWidget()
        compositor = GalleryParticleCompositor()
        try:
            root.resize(420, 240)
            root.setAutoFillBackground(True)
            palette = root.palette()
            palette.setColor(QPalette.Window, QColor(24, 35, 47))
            root.setPalette(palette)
            lower = fluentqt.ParticleBackdrop(root)
            lower.setGeometry(root.rect())
            lower.setEffect(fluentqt.ParticleBackdrop.FloatingDots)
            lower.setParticleCount(240)
            lower.setAnimationEnabled(False)
            lower.setFadeMargins(QMarginsF(80, 0, 0, 60))
            upper = fluentqt.ParticleBackdrop(root)
            upper.setGeometry(45, 30, 330, 180)
            upper.setEffect(fluentqt.ParticleBackdrop.Starfield)
            upper.setParticleCount(240)
            upper.setAnimationEnabled(False)
            translucent = _Foreground(QColor(210, 60, 25, 128), root)
            translucent.setGeometry(35, 40, 235, 110)
            opaque = _Foreground(QColor(0, 90, 170), root)
            opaque.setGeometry(280, 80, 105, 105)
            root.show()
            self.assertTrue(QTest.qWaitForWindowExposed(root))
            surface.makeCurrent()
            original = _capture(root)
            expected = original.toImage()
            self.assertTrue(compositor.prepare(root, 1, root.size(), 1, 1, 32 << 20, True))
            self.assertEqual(compositor.activeLayerCount(), 2)
            base = _capture(root)
            paint = QOpenGLFramebufferObject(root.size(), _format())
            resolve = QOpenGLFramebufferObject(root.size())
            result = compositor.compose(base.texture(), 2, paint, resolve, _render_foreground)
            self.assertNotEqual(result, base.texture())
            actual = next(target for target in compositor.composed if target.texture() == result).toImage()
            self.assertLess(_difference(actual, expected, root.rect()), 2.)
            self.assertLess(_difference(actual, expected, translucent.geometry()), 1.)
            self.assertLess(_difference(actual, expected, opaque.geometry().adjusted(2, 2, -2, -2)), .1)
            self.assertEqual(compositor.foregroundCaptureCount(), 2)
            self.assertEqual(compositor.compositionCount(), 1)
            self.assertEqual(compositor.compose(base.texture(), 2, paint, resolve, _render_foreground), result)
            self.assertEqual(compositor.foregroundCaptureCount(), 2)
            self.assertEqual(compositor.compositionCount(), 1)
            self.assertEqual(surface.context().functions().glGetError(), 0)
            compositor.release()
            self.assertEqual(compositor.activeLayerCount(), 0)
            self.assertEqual(compositor.allocatedBytes(), 0)
            self.assertEqual(_capture(root).toImage(), expected)
            self.assertFalse(compositor.prepare(root, 3, root.size(), 1, 1, 1, True))
            scans = compositor.sourceScanCount()
            for _ in range(12):
                self.assertFalse(compositor.prepare(root, 3, root.size(), 1, 1, 1, True))
            self.assertEqual(compositor.sourceScanCount(), scans)
            self.assertTrue(compositor.prepare(root, 3, root.size(), 1, 1, 32 << 20, True))
            del original, base, paint, resolve
        finally:
            compositor.release()
            delete(compositor)
            delete(root)
            surface.doneCurrent()

    def test_native_mask_and_foreground_effect_keep_cpu_pixels(self):
        surface = self._native_context()
        root = QWidget()
        compositor = GalleryParticleCompositor()
        try:
            root.resize(300, 180)
            parent = QWidget(root)
            parent.resize(root.size())
            parent.setMask(QRegion(QRect(0, 0, 250, 150), QRegion.Ellipse))
            source = fluentqt.ParticleBackdrop(parent)
            source.resize(parent.size())
            source.setAnimationEnabled(False)
            foreground = _Foreground(QColor(200, 30, 20, 180), parent)
            foreground.setGeometry(20, 20, 160, 70)
            root.show()
            self.assertTrue(QTest.qWaitForWindowExposed(root))
            surface.makeCurrent()
            for revision in (1, 2):
                if revision == 2:
                    parent.clearMask()
                    effect = QGraphicsOpacityEffect(foreground)
                    effect.setOpacity(.5)
                    foreground.setGraphicsEffect(effect)
                expected = _capture(root).toImage()
                self.assertFalse(compositor.prepare(root, revision, root.size(), 1, 1, 32 << 20, True))
                self.assertEqual(compositor.activeLayerCount(), 0)
                self.assertEqual(compositor.allocatedBytes(), 0)
                self.assertEqual(_capture(root).toImage(), expected)
        finally:
            compositor.release()
            delete(compositor)
            delete(root)
            surface.doneCurrent()

    def test_native_supersampled_strips_preserve_clipped_foreground(self):
        surface = self._native_context()
        root = QWidget()
        compositor = GalleryParticleCompositor()
        try:
            root.resize(420, 320)
            parent = QWidget(root)
            parent.setGeometry(17, 11, 380, 240)
            source = fluentqt.ParticleBackdrop(parent)
            source.setGeometry(-20, 10, 420, 300)
            source.setEffect(fluentqt.ParticleBackdrop.FloatingDots)
            source.setAnimationEnabled(False)
            foreground = _Foreground(QColor(25, 90, 150, 128), parent)
            foreground.setGeometry(31, 40, 310, 190)
            root.show()
            self.assertTrue(QTest.qWaitForWindowExposed(root))
            surface.makeCurrent()
            original = _capture(root, 2)
            expected = original.toImage().scaled(root.size(), Qt.IgnoreAspectRatio, Qt.SmoothTransformation)
            pixels = QSize(root.width() * 2, root.height() * 2)
            self.assertTrue(compositor.prepare(root, 1, pixels, 1, 2, 32 << 20, True))
            base = _capture(root, 2)
            # Foreground is taller than this borrowed MSAA target, as on large
            # Gallery pages. Guard rows must not move text at strip boundaries.
            paint = QOpenGLFramebufferObject(QSize(pixels.width(), 128), _format())
            resolve = QOpenGLFramebufferObject(paint.size())
            result = compositor.compose(base.texture(), 2, paint, resolve, _render_foreground)
            self.assertNotEqual(result, base.texture())
            actual = next(target for target in compositor.composed if target.texture() == result).toImage()
            actual = actual.scaled(root.size(), Qt.IgnoreAspectRatio, Qt.SmoothTransformation)
            self.assertLess(_difference(actual, expected, root.rect()), 2.)
            self.assertEqual(compositor.foregroundCaptureCount(), 1)
            self.assertEqual(surface.context().functions().glGetError(), 0)
            del original, base, paint, resolve
        finally:
            compositor.release()
            delete(compositor)
            delete(root)
            surface.doneCurrent()

    def test_native_negative_scan_is_cached_and_deleted_source_returns_cpu(self):
        surface = self._native_context()
        root = QWidget()
        compositor = GalleryParticleCompositor()
        try:
            root.resize(300, 180)
            hidden = QWidget(root)
            source = fluentqt.ParticleBackdrop(hidden)
            hidden.hide()
            root.show()
            self.assertTrue(QTest.qWaitForWindowExposed(root))
            surface.makeCurrent()
            for _ in range(60):
                self.assertFalse(compositor.prepare(root, 1, root.size(), 1, 1, 32 << 20, True))
            self.assertEqual(compositor.sourceScanCount(), 1)
            source.setParent(root)
            source.resize(root.size())
            source.show()
            self.assertTrue(compositor.prepare(root, 2, root.size(), 1, 1, 32 << 20, True))
            delete(source)
            self.assertEqual(compositor.activeLayerCount(), 0)
            self.assertFalse(compositor.prepare(root, 3, root.size(), 1, 1, 32 << 20, True))
            self.assertEqual(compositor.allocatedBytes(), 0)
        finally:
            compositor.release()
            delete(compositor)
            delete(root)
            surface.doneCurrent()


if __name__ == "__main__":
    unittest.main(verbosity=2)
