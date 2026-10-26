"""Private glyph coverage adapter for the low-DPI GL panel cache.

Qt has already shaped each QTextItem. Unhinted outlines use the cache density;
hinted fonts retain the display's pixel grid. Geometry, images, panel resolution
and multisampling still use the target device.
"""

import math
import weakref
from collections import OrderedDict
from hashlib import sha256

from fluentqt._qt_painting import (
    device_pixel_ratio_metric, forward_paint_array, forward_paint_polygon, forwarding_paint_engine_features,
    image_device_pixel_ratio, region_rects,
)

from PySide6.QtCore import QPoint, QPointF, QRectF, QSize, QSizeF, Qt
from PySide6.QtGui import (
    QFont, QFontMetricsF, QImage, QPaintDevice, QPaintEngine, QPainter,
    QPainterPath, QRegion, QTransform,
)


def needs_native_glyph_coverage(native_dpr):
    from fluentqt.spatial import SpatialRuntime
    return SpatialRuntime.needsNativeGlyphCoverage(native_dpr)


def glyph_raster_dpr(font, native_dpr, cache_dpr):
    # Hinting fits strokes to physical pixels; oversampling would fit a
    # different grid. Unhinted outlines can retain their real cache detail.
    return cache_dpr if font.hintingPreference() == QFont.PreferNoHinting else native_dpr


GLYPH_IMAGE_CACHE_BYTES = 2 * 1024 * 1024


class GlyphImageCache:
    """Retain exact raster values and texture keys within a bounded working set."""

    _ENTRY_METADATA_BYTES = 1024

    def __init__(self, budget=GLYPH_IMAGE_CACHE_BYTES):
        self._budget = min(GLYPH_IMAGE_CACHE_BYTES, max(0, int(budget)))
        self._images = OrderedDict()
        self._cost = 0

    def retain(self, image):
        if image.isNull() or image.format() != QImage.Format_ARGB32_Premultiplied:
            return QImage(image)
        cost = image.sizeInBytes() + self._ENTRY_METADATA_BYTES
        if cost > self._budget:
            return QImage(image)
        key = (sha256(memoryview(image.constBits()).cast("B")).digest(),
               image.width(), image.height(), image.devicePixelRatioF())
        cached = self._images.get(key)
        if cached is not None:
            if cached[0] == image:
                self._images.move_to_end(key)
                return QImage(cached[0])
            # A digest match alone must never return different raster pixels.
            self._cost -= cached[1]
            del self._images[key]
        while self._images and self._cost + cost > self._budget:
            _, (_, retired_cost) = self._images.popitem(last=False)
            self._cost -= retired_cost
        retained = QImage(image)
        self._images[key] = retained, cost
        self._cost += cost
        return QImage(retained)

    def clear(self):
        self._images.clear()
        self._cost = 0

    def cost(self):
        return self._cost

    def entry_count(self):
        return len(self._images)


class GlyphPaintDevice(QPaintDevice):
    def __init__(self, target, native_dpr, raster_origin=QPointF(), glyph_cache=None):
        super().__init__()
        self.target = target
        self.native_dpr = native_dpr
        self.raster_origin = QPointF(raster_origin)
        self.glyph_cache = glyph_cache
        self.engine = _GlyphPaintEngine(self)

    def raster_dpr(self, font):
        return glyph_raster_dpr(font, self.native_dpr, self.target.devicePixelRatioF())

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
        return device_pixel_ratio_metric(metric, self.target.devicePixelRatioF())


class _GlyphPaintEngine(QPaintEngine):
    def __init__(self, device):
        # Match the delegate: advertising RasterOpModes makes native editors
        # select XOR caret painting, which the OpenGL engine cannot execute.
        engine = device.target.paintEngine()
        super().__init__(forwarding_paint_engine_features(engine))
        # The paint device owns this engine; do not retain a cycle per dirty strip.
        self.device = weakref.proxy(device)
        self.painter = None
        self.background_mode = Qt.TransparentMode
        self.glyph_items = 0
        self.glyph_tiles = 0
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
        # Qt can emulate an opaque background without marking the delegate's
        # mode dirty. Retain the source state for the text fallback decision.
        self.background_mode = state.backgroundMode()
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
            for rect in region_rects(clip):
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
        forward_paint_array(self.painter, method, values, count, *arguments)

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
        self._forward(lambda: forward_paint_polygon(self.painter, points, *arguments))

    def drawTextItem(self, position, item):
        def draw():
            painter = self.painter
            # Use the captured state: querying QPaintEngine.painter() makes
            # PySide 6.2 reparent its wrapper through the engine, so a nested
            # widget painter can invalidate its caller.
            if (painter.worldTransform().type() not in (QTransform.TxNone, QTransform.TxTranslate)
                    or painter.pen().brush().style() != Qt.SolidPattern
                    or self.background_mode == Qt.OpaqueMode
                    or painter.compositionMode() != QPainter.CompositionMode_SourceOver):
                painter.paintEngine().syncState()
                painter.paintEngine().drawTextItem(position, item)
                return
            dpr = image_device_pixel_ratio(self.device.raster_dpr(item.font()))
            origin = QPointF(painter.worldTransform().dx(), painter.worldTransform().dy())
            origin += self.device.raster_origin
            baseline = position + origin
            metrics = QFontMetricsF(item.font())
            # Guard the leading edges without exceeding the 512 x 128 budget.
            # Fixed-point truncation can change the phase when a crossing
            # glyph's raster origin becomes negative inside the next tile.
            guard_width = (metrics.maxWidth() - min(0, metrics.minRightBearing())) * dpr + 2
            guard_height = max(metrics.descent(), item.descent()) * dpr + 2
            if guard_width > 511 or guard_height > 127:
                painter.paintEngine().syncState()
                painter.paintEngine().drawTextItem(position, item)
                return
            guard = QSize(math.ceil(guard_width), math.ceil(guard_height))
            payload = QSize(512 - guard.width(), 128 - guard.height())
            ink = metrics.boundingRect(item.text())
            ink = ink.united(QRectF(0, -item.ascent(), item.width(), item.ascent() + item.descent()))
            ink = ink.adjusted(-2, -2, 2, 2).translated(baseline)
            self.glyph_items += 1
            # Native fallback fonts can overestimate ink by orders of magnitude.
            # Bound work to this paint strip and its active clip before tiling.
            target = self.device.target
            target_dpr = target.devicePixelRatioF()
            visible = QRectF(0, 0, target.width() / target_dpr, target.height() / target_dpr)
            if painter.hasClipping():
                visible = visible.intersected(painter.worldTransform().mapRect(painter.clipBoundingRect()))
            ink = ink.intersected(visible.translated(self.device.raster_origin))
            if ink.isEmpty():
                return
            pixels = QTransform.fromScale(dpr, dpr).mapRect(ink).toAlignedRect()
            # The plan reserves one tile/upload plus the optional bounded cache.
            for top in range(pixels.top(), pixels.bottom() + 1, payload.height()):
                for left in range(pixels.left(), pixels.right() + 1, payload.width()):
                    size = QSize(min(payload.width(), pixels.right() - left + 1),
                                 min(payload.height(), pixels.bottom() - top + 1))
                    tile = QPoint(max(0, left - guard.width()), max(0, top - guard.height()))
                    prefix = QPoint(left, top) - tile
                    glyph = QImage(size + QSize(prefix.x(), prefix.y()),
                                   QImage.Format_ARGB32_Premultiplied)
                    if glyph.isNull():
                        continue
                    self.glyph_tiles += 1
                    glyph.fill(Qt.transparent)
                    tile_origin = QPointF(left / dpr, top / dpr)
                    raster = QPainter(glyph)
                    raster.setFont(item.font())
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
                    # Keep the shaped position intact: Qt's outline path can
                    # quantize it before applying the strip/tile transform.
                    raster.setWorldTransform(QTransform(dpr, 0, 0, dpr,
                                                        origin.x() * dpr - tile.x(),
                                                        origin.y() * dpr - tile.y()))
                    raster.paintEngine().syncState()
                    raster.paintEngine().drawTextItem(position, item)
                    raster.end()
                    del raster
                    glyph.setDevicePixelRatio(dpr)
                    if self.device.glyph_cache is not None:
                        glyph = self.device.glyph_cache.retain(glyph)
                    painter.save()
                    painter.setOpacity(1)
                    painter.setRenderHint(QPainter.SmoothPixmapTransform, False)
                    painter.drawImage(QRectF(tile_origin - origin,
                                             QSizeF(size.width() / dpr, size.height() / dpr)),
                                      glyph, QRectF(prefix, size))
                    painter.restore()
                    del glyph
        self._forward(draw)
