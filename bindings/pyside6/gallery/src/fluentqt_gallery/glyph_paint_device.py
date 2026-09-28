"""Private native-DPI glyph coverage adapter for the low-DPI GL panel cache.

Qt has already shaped each QTextItem. Only that item is rasterized; geometry,
images, panel resolution and multisampling continue to use the target device.
"""

import weakref

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import (
    QFontMetricsF, QImage, QPaintDevice, QPaintEngine, QPainter,
    QPainterPath, QRegion, QTransform,
)


def needs_native_glyph_coverage(native_dpr):
    from fluentqt.spatial import SpatialRuntime
    return SpatialRuntime.needsNativeGlyphCoverage(native_dpr)


class GlyphPaintDevice(QPaintDevice):
    def __init__(self, target, native_dpr, raster_origin=QPointF()):
        super().__init__()
        self.target = target
        self.native_dpr = native_dpr
        self.raster_origin = QPointF(raster_origin)
        self.engine = _GlyphPaintEngine(self)

    def paintEngine(self):
        return self.engine

    def metric(self, metric):
        methods = {
            QPaintDevice.PdmWidth: self.target.width,
            QPaintDevice.PdmHeight: self.target.height,
            QPaintDevice.PdmWidthMM: self.target.widthMM,
            QPaintDevice.PdmHeightMM: self.target.heightMM,
            QPaintDevice.PdmDpiX: self.target.logicalDpiX,
            QPaintDevice.PdmDpiY: self.target.logicalDpiY,
            QPaintDevice.PdmPhysicalDpiX: self.target.physicalDpiX,
            QPaintDevice.PdmPhysicalDpiY: self.target.physicalDpiY,
            QPaintDevice.PdmDepth: self.target.depth,
            QPaintDevice.PdmNumColors: self.target.colorCount,
        }
        if metric in methods:
            return methods[metric]()
        if metric == QPaintDevice.PdmDevicePixelRatio:
            return round(self.target.devicePixelRatioF())
        if metric == QPaintDevice.PdmDevicePixelRatioScaled:
            return round(self.target.devicePixelRatioF() * 65536)
        # Qt 6.8's encoded-double metrics fall back to the scaled metric.
        return 0


class _GlyphPaintEngine(QPaintEngine):
    def __init__(self, device):
        # Match the delegate: advertising RasterOpModes makes native editors
        # select XOR caret painting, which the OpenGL engine cannot execute.
        engine = device.target.paintEngine()
        features = QPaintEngine.PaintEngineFeatures(0)
        if engine is not None:
            # Bit 31 is not a Qt paint feature and overflows the signed enum
            # converter in PySide6 6.2 on Windows.
            for bit in range(31):
                feature = QPaintEngine.PaintEngineFeature(1 << bit)
                if engine.hasFeature(feature):
                    features |= feature
        super().__init__(features)
        # The paint device owns this engine; do not retain a cycle per dirty strip.
        self.device = weakref.proxy(device)
        self.painter = None
        self.glyph_items = 0
        self.last_system_clip = QRegion()
        self.system_clip_path = QPainterPath()
        self.system_clip_applied = False
        self.system_clip_applications = 0

    def type(self):
        return QPaintEngine.User

    def begin(self, device):
        self.painter = QPainter(self.device.target)
        self.setActive(self.painter.isActive())
        return self.isActive()

    def end(self):
        if self.system_clip_applied:
            self.painter.restore()
        self.system_clip_applied = False
        ended = self.painter.end()
        self.setActive(False)
        return ended

    def updateState(self, state):
        dirty, painter = state.state(), self.painter
        if dirty & (QPaintEngine.DirtyTransform | QPaintEngine.DirtyClipRegion
                    | QPaintEngine.DirtyClipPath | QPaintEngine.DirtyClipEnabled):
            self._restore_system_clip()
        if dirty & QPaintEngine.DirtyTransform:
            inverse_dpr = 1 / self.device.target.devicePixelRatioF()
            painter.setWorldTransform(state.transform() *
                                      QTransform.fromScale(inverse_dpr, inverse_dpr))
        if dirty & QPaintEngine.DirtyPen:
            painter.setPen(state.pen())
        if dirty & QPaintEngine.DirtyBrush:
            painter.setBrush(state.brush())
        if dirty & QPaintEngine.DirtyBrushOrigin:
            painter.setBrushOrigin(state.brushOrigin())
        if dirty & QPaintEngine.DirtyFont:
            painter.setFont(state.font())
        if dirty & QPaintEngine.DirtyBackground:
            painter.setBackground(state.backgroundBrush())
        if dirty & QPaintEngine.DirtyBackgroundMode:
            painter.setBackgroundMode(state.backgroundMode())
        if dirty & QPaintEngine.DirtyHints:
            painter.setRenderHints(painter.renderHints(), False)
            painter.setRenderHints(state.renderHints())
        if dirty & QPaintEngine.DirtyCompositionMode:
            painter.setCompositionMode(state.compositionMode())
        if dirty & QPaintEngine.DirtyOpacity:
            painter.setOpacity(state.opacity())
        if dirty & QPaintEngine.DirtyClipRegion:
            painter.setClipRegion(state.clipRegion(), state.clipOperation())
        if dirty & QPaintEngine.DirtyClipPath:
            painter.setClipPath(state.clipPath(), state.clipOperation())
        if dirty & QPaintEngine.DirtyClipEnabled:
            painter.setClipping(state.isClipEnabled())

    def _restore_system_clip(self):
        if not self.system_clip_applied:
            return
        painter = self.painter
        values = (painter.pen(), painter.brush(), painter.background(), painter.brushOrigin(),
                  painter.font(), painter.backgroundMode(), painter.renderHints(),
                  painter.compositionMode(), painter.opacity(), painter.worldTransform())
        painter.restore()
        self.system_clip_applied = False
        painter.setPen(values[0])
        painter.setBrush(values[1])
        painter.setBackground(values[2])
        painter.setBrushOrigin(values[3])
        painter.setFont(values[4])
        painter.setBackgroundMode(values[5])
        painter.setRenderHints(painter.renderHints(), False)
        painter.setRenderHints(values[6])
        painter.setCompositionMode(values[7])
        painter.setOpacity(values[8])
        painter.setWorldTransform(values[9])

    def _forward(self, draw):
        clip = self.systemClip()
        if clip != self.last_system_clip or (not self.system_clip_applied and not clip.isEmpty()):
            self._restore_system_clip()
        if clip != self.last_system_clip:
            self.last_system_clip = clip
            self.system_clip_path = QPainterPath()
            dpr = self.device.target.devicePixelRatioF()
            for rect in clip.rects() if hasattr(clip, "rects") else clip:
                self.system_clip_path.addRect(QRectF(rect.x() / dpr, rect.y() / dpr,
                                                     rect.width() / dpr, rect.height() / dpr))
        if not self.system_clip_applied and not clip.isEmpty():
            # Keep the device-space stencil across primitives in the same widget.
            # Transform/user-clip changes remove this layer without losing state.
            painter = self.painter
            painter.save()
            transform = painter.worldTransform()
            painter.resetTransform()
            painter.setClipPath(self.system_clip_path, Qt.IntersectClip)
            painter.setWorldTransform(transform)
            self.system_clip_applied = True
            self.system_clip_applications += 1
        draw()

    def _array(self, method, values, count, *arguments):
        if count:
            # Qt 6.2 exposes (first-element wrapper, count) only on QPaintEngine,
            # while QPainter accepts sequences. Keep the original C++ array intact.
            engine = self.painter.paintEngine()
            engine.syncState()
            getattr(engine, method)(values, *count, *arguments)
        else:
            getattr(self.painter, method)(values, *arguments)

    def drawRects(self, rects, *count):
        self._forward(lambda: self._array("drawRects", rects, count))

    def drawLines(self, lines, *count):
        self._forward(lambda: self._array("drawLines", lines, count))

    def drawPoints(self, points, *count):
        self._forward(lambda: self._array("drawPoints", points, count))

    def drawEllipse(self, rect):
        self._forward(lambda: self.painter.drawEllipse(rect))

    def drawPath(self, path):
        self._forward(lambda: self.painter.drawPath(path))

    def drawPixmap(self, rect, pixmap, source):
        self._forward(lambda: self.painter.drawPixmap(rect, pixmap, source))

    def drawImage(self, rect, image, source, flags):
        self._forward(lambda: self.painter.drawImage(rect, image, source, flags))

    def drawTiledPixmap(self, rect, pixmap, offset):
        self._forward(lambda: self.painter.drawTiledPixmap(rect, pixmap, offset))

    def drawPolygon(self, points, *arguments):
        count, mode = arguments[:-1], arguments[-1]
        def draw():
            if count:
                self._array("drawPolygon", points, count, mode)
            elif mode == QPaintEngine.PolylineMode:
                self.painter.drawPolyline(points, *count)
            elif mode == QPaintEngine.ConvexMode:
                self.painter.drawConvexPolygon(points, *count)
            else:
                self.painter.drawPolygon(points, *count, Qt.WindingFill if mode == QPaintEngine.WindingMode
                                         else Qt.OddEvenFill)
        self._forward(draw)

    def drawTextItem(self, position, item):
        def draw():
            painter = self.painter
            source_painter = QPaintEngine.painter(self)
            if (painter.worldTransform().type() not in (QTransform.TxNone, QTransform.TxTranslate)
                    or painter.pen().brush().style() != Qt.SolidPattern
                    or (source_painter is not None and source_painter.backgroundMode() == Qt.OpaqueMode)
                    or painter.compositionMode() != QPainter.CompositionMode_SourceOver):
                painter.paintEngine().syncState()
                painter.paintEngine().drawTextItem(position, item)
                return
            dpr = self.device.native_dpr
            origin = QPointF(painter.worldTransform().dx(), painter.worldTransform().dy())
            origin += self.device.raster_origin
            baseline = position + origin
            ink = QFontMetricsF(item.font()).boundingRect(item.text())
            ink = ink.united(QRectF(0, -item.ascent(), item.width(), item.ascent() + item.descent()))
            ink = ink.adjusted(-2, -2, 2, 2).translated(baseline)
            pixels = QTransform.fromScale(dpr, dpr).mapRect(ink).toAlignedRect()
            self.glyph_items += 1
            # One 256 KiB image plus its upload is reserved by the panel cache plan.
            # Release every tile immediately; no persistent or per-frame glyph cache.
            for top in range(pixels.top(), pixels.bottom() + 1, 128):
                for left in range(pixels.left(), pixels.right() + 1, 512):
                    glyph = QImage(min(512, pixels.right() - left + 1),
                                   min(128, pixels.bottom() - top + 1),
                                   QImage.Format_ARGB32_Premultiplied)
                    if glyph.isNull():
                        continue
                    glyph.setDevicePixelRatio(dpr)
                    glyph.fill(Qt.transparent)
                    tile_origin = QPointF(left / dpr, top / dpr)
                    raster = QPainter(glyph)
                    raster.setPen(painter.pen())
                    # Apply opacity in Qt's glyph rasterizer, not a second image
                    # blend: native font backends may use opacity-dependent coverage.
                    raster.setOpacity(painter.opacity())
                    raster.setRenderHints(raster.renderHints(), False)
                    raster.setRenderHints(painter.renderHints())
                    raster.setBackground(painter.background())
                    raster.setBackgroundMode(painter.backgroundMode())
                    # Preserve shaping, fallback fonts, bidi order and decorations.
                    # QPainter already draws decorations on the outer device.
                    # Its drawTextItem wrapper would paint underline/strikeout twice.
                    raster.paintEngine().syncState()
                    raster.paintEngine().drawTextItem(baseline - tile_origin, item)
                    raster.end()
                    del raster
                    painter.save()
                    painter.setOpacity(1)
                    painter.setRenderHint(QPainter.SmoothPixmapTransform, False)
                    painter.drawImage(tile_origin - origin, glyph)
                    painter.restore()
                    del glyph
        self._forward(draw)
