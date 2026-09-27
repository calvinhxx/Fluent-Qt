"""GPU composition of the Gallery shell, matching GallerySpatialController.cpp.

The live widgets retain their parents and keyboard focus. Two cached surfaces
share one OpenGL canvas; pointer events are mapped back into the native layout.
Only the optional Spatial entry path imports this module.
"""

import os
import math
import weakref
from functools import partial

import fluentqt
from fluentqt.spatial import SpatialRuntime
from PySide6.QtCore import (
    QAbstractAnimation, QEasingCurve, QEvent, QLineF, QObject, QPoint, QPointF,
    QRect, QRectF, QSize, Qt, QTimer, QVariantAnimation, Signal, Slot,
)
from PySide6.QtGui import (
    QColor, QContextMenuEvent, QEnterEvent, QGuiApplication, QHelpEvent,
    QImage, QMatrix4x4, QMouseEvent, QOffscreenSurface, QOpenGLContext,
    QPaintEngine, QPainter, QPainterPath, QPixmap, QPolygonF, QRegion, QTransform,
    QVector3D, QSurfaceFormat, QWheelEvent,
)
from PySide6.QtOpenGL import (
    QOpenGLFramebufferObject, QOpenGLFramebufferObjectFormat, QOpenGLPaintDevice,
    QOpenGLTexture,
)
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import (
    QApplication, QFrame, QGraphicsEffect, QMenu, QProxyStyle, QStyle, QStyleOption,
    QStyleOptionButton, QStyleOptionFrame, QWidget,
)
from shiboken6 import VoidPtr, invalidate, isValid

from .settings import gallery_settings
from .glyph_paint_device import GlyphPaintDevice, needs_native_glyph_coverage
from .panel_sampler import _PanelSampler


def _alive(widget):
    return widget is not None and isValid(widget)


_PRESENTATION_TRANSFORM = "_fluent_qt_overlay_presentation_transform"
_PRESENTED_MENU_SOURCE = "_fluent_qt_presented_menu_source"
_PRESENTED_MENU_POINT = "_fluent_qt_presented_menu_point"


def _invalidate_context_functions(reference):
    functions = reference()
    if _alive(functions):
        # Qt owns the C++ functions. Only retire their borrowed Python wrapper:
        # native-created contexts do not invalidate it reliably on older PySide.
        invalidate(functions)


def _mouse_global_position(receiver, event):
    # Nested Qt hover events can overwrite a native mouse event's shared global
    # point. Receiver-local position survives; synthetic callers own their global.
    if event.spontaneous():
        return receiver.mapToGlobal(event.position().toPoint())
    return event.globalPosition().toPoint()


# Runtime/driver compatibility is shared with the native library and C++ Gallery.
def _software(renderer):
    return not SpatialRuntime.isHardwareRenderer(renderer)


def _renderer(context):
    return SpatialRuntime.currentRendererName()


def _session_unavailable_reason():
    if os.environ.get("FLUENT_QT_GALLERY_DISABLE_3D", "0") != "0":
        return "3D is disabled for this session."
    if not SpatialRuntime.supportsOpenGLDisplay():
        return "This display uses the 2D Gallery."
    return ""


def _unavailable_reason():
    return SpatialRuntime.preflightFailure()


def _maximum_cache_dimension(context):
    return SpatialRuntime.maximumTextureDimension()


# Same aggregate cache budget and sampling ladder as GallerySpatialRenderPolicy.h.
_CACHE_BUDGET_BYTES = 192 * 1024 * 1024
_CACHE_BYTES_PER_PIXEL = 4
_PAINT_SAMPLES = 2
_GLYPH_SCRATCH_BYTES = 512 * 1024


def _cache_plan(panels, native_dpr, max_dimension, max_extra=2., budget=_CACHE_BUDGET_BYTES,
                paint_samples=_PAINT_SAMPLES, backdrop_bytes=0, glyph_scratch_bytes=0):
    if (not math.isfinite(native_dpr) or native_dpr <= 0 or max_dimension <= 0
            or budget <= 0 or paint_samples <= 1 or backdrop_bytes < 0 or backdrop_bytes >= budget
            or glyph_scratch_bytes < 0 or glyph_scratch_bytes >= budget - backdrop_bytes):
        return None
    for extra in (2., 1.75, 1.5, 1.25, 1.):
        if extra > max_extra:
            continue
        dpr = native_dpr * extra
        sizes = []
        for panel in panels:
            if panel.isEmpty():
                sizes.append(QSize())
                continue
            width, height = panel.width() * dpr, panel.height() * dpr
            if (not math.isfinite(width) or not math.isfinite(height)
                    or width > max_dimension or height > max_dimension):
                break
            sizes.append(QSize(math.ceil(width), math.ceil(height)))
        pixels = sum(size.width() * size.height() for size in sizes if not size.isEmpty())
        paint_size = QSize()
        for size in sizes:
            paint_size = paint_size.expandedTo(size)
        if len(sizes) != len(panels) or paint_size.isEmpty():
            continue
        texture_bytes = pixels * _CACHE_BYTES_PER_PIXEL
        row_bytes = paint_size.width() * (12 * paint_samples + 4)
        rows = (budget - backdrop_bytes - glyph_scratch_bytes - texture_bytes) // row_bytes
        if rows < min(32, paint_size.height()):
            continue
        paint_size.setHeight(min(paint_size.height(), rows))
        return dpr, sizes, paint_size
    return None




class _Capture(QGraphicsEffect):
    def __init__(self, invalidated, parent):
        super().__init__(parent)
        self.invalidated = invalidated
        self.rendering = self.composing = False
        self.setEnabled(False)

    def draw(self, painter):
        if self.rendering:
            self.drawSource(painter)
        elif not self.composing and self.invalidated:
            self.invalidated()


class _SceneTheme(fluentqt.FluentWidget):
    changed = Signal()

    def on_theme_updated(self):
        super().on_theme_updated()
        self.changed.emit()


class _Surface(QOpenGLWidget):
    def __init__(self, owner, parent):
        super().__init__(parent)
        from .particle_compositor import GalleryParticleCompositor
        self.owner = owner
        self.particles = GalleryParticleCompositor(self)
        self.particles.frameRequested.connect(self.update)
        self.particles.staticContentInvalidated.connect(owner._capture_content)
        self.presented_content_texture = 0
        self._functions = None
        self.backdrop_cache = {}
        self.backdrop_uploads = 0
        # A Python subclass has no C++ destructor override. Release GL resources
        # before the parent destroys its child QOpenGLWidget/context; its own
        # aboutToBeDestroyed connection is too late once the receiver is deleted.
        owner.window.destroyed.connect(self.release_window_context)
        self.setObjectName("gallerySpatialSurface")
        self.setProperty("galleryGpuComposition", True)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        self.setAttribute(Qt.WA_NoSystemBackground)
        self.setFocusPolicy(Qt.NoFocus)
        self.setUpdateBehavior(QOpenGLWidget.UpdateBehavior.NoPartialUpdate)
        fmt = self.format()
        samples = 4
        if os.environ.get("FLUENT_QT_SPATIAL_BENCHMARK", "0") != "0":
            requested = os.environ.get("FLUENT_QT_SPATIAL_SAMPLES")
            if requested in ("0", "2", "4"):
                samples = int(requested)
        fmt.setSamples(samples)
        fmt.setAlphaBufferSize(8)
        self.setFormat(fmt)

    def initializeGL(self):
        context = self.context()
        # Keep one registered wrapper for this context, including on PySide
        # versions that otherwise recreate it for each functions() query.
        self._functions = context.functions()
        self.blitter = _PanelSampler()
        self.blitter.create()
        self.caches = [{}, {}]
        self.paint_target = None
        self.resolve_target = None
        self.plan = None
        self.max_extra = 2.
        self.cache_failure_pending = False
        self.max_dimension = 0
        self.paint_samples = 0
        context.aboutToBeDestroyed.connect(self.release_context)
        # No controller/context capture: this also runs if Qt has already
        # invalidated the Python QOpenGLWidget receiver during destruction.
        context.aboutToBeDestroyed.connect(partial(
            _invalidate_context_functions, weakref.ref(self._functions)))
        try:
            self.max_dimension = _maximum_cache_dimension(self.context())
        except (AttributeError, OSError, TypeError, ValueError):
            self.owner.render_failure_timer.start(0)
            return
        sample_format = QOpenGLFramebufferObjectFormat()
        sample_format.setAttachment(QOpenGLFramebufferObject.CombinedDepthStencil)
        sample_format.setInternalTextureFormat(0x8058)  # GL_RGBA8
        sample_format.setSamples(_PAINT_SAMPLES)
        probe = QOpenGLFramebufferObject(QSize(1, 1), sample_format)
        self.paint_samples = probe.format().samples() if probe.isValid() else 0
        del probe
        self.owner.renderer_initialized = True
        self.owner.queue_check()

    def clear_frame_caches(self):
        self.makeCurrent()
        self.particles.release()
        self.presented_content_texture = 0
        self.clear_backdrop()
        self.caches = [{}, {}]
        self.paint_target = None
        self.resolve_target = None
        self.plan = None
        self.max_extra = 2.
        self.cache_failure_pending = False
        self.doneCurrent()

    def release_window_context(self):
        self.clear_frame_caches()
        self.invalidate_functions()

    def invalidate_functions(self):
        if self._functions is not None:
            _invalidate_context_functions(weakref.ref(self._functions))
            self._functions = None

    def release_context(self):
        self.makeCurrent()
        self.particles.release()
        self.presented_content_texture = 0
        self.clear_backdrop()
        self.caches = [{}, {}]
        self.paint_target = None
        self.resolve_target = None
        self.plan = None
        self.blitter.destroy()
        self.doneCurrent()
        self.invalidate_functions()
        self.owner.context_lost()

    def prepare_caches(self):
        if self.paint_samples <= 1:
            return False
        panels = [rect.size() for rect, _ in self.owner.panels]
        backdrop = self.owner.backdrop
        backdrop_bytes = backdrop.width() * backdrop.height() * 4
        while True:
            plan = _cache_plan(panels, self.devicePixelRatioF(), self.max_dimension,
                               self.max_extra, paint_samples=self.paint_samples,
                               backdrop_bytes=backdrop_bytes,
                               glyph_scratch_bytes=(_GLYPH_SCRATCH_BYTES
                                   if needs_native_glyph_coverage(self.devicePixelRatioF()) else 0))
            if plan is None:
                return False
            if plan == self.plan:
                return True
            # Free the old pair before allocating replacements, including on resize.
            self.particles.release()
            self.presented_content_texture = 0
            self.caches = [{}, {}]
            self.paint_target = None
            self.resolve_target = None
            self.plan = None
            allocated = True
            paint_format = QOpenGLFramebufferObjectFormat()
            paint_format.setAttachment(QOpenGLFramebufferObject.CombinedDepthStencil)
            paint_format.setInternalTextureFormat(0x8058)
            paint_format.setSamples(self.paint_samples)
            self.paint_target = QOpenGLFramebufferObject(plan[2], paint_format)
            texture_format = QOpenGLFramebufferObjectFormat()
            texture_format.setAttachment(QOpenGLFramebufferObject.NoAttachment)
            texture_format.setInternalTextureFormat(0x8058)
            self.resolve_target = QOpenGLFramebufferObject(plan[2], texture_format)
            allocated = self.paint_target.isValid() and self.resolve_target.isValid()
            for index, size in enumerate(plan[1]):
                if not allocated:
                    break
                if size.isEmpty():
                    continue
                texture = QOpenGLFramebufferObject(size, texture_format)
                if not texture.isValid():
                    allocated = False
                    del texture
                    break
                self.caches[index] = {"texture": texture}
                gl = self.context().functions()
                gl.glBindTexture(0x0DE1, texture.texture())
                gl.glTexParameteri(0x0DE1, 0x2801, 0x2601)
                gl.glTexParameteri(0x0DE1, 0x2800, 0x2601)
                gl.glBindTexture(0x0DE1, 0)
                del texture
            if allocated:
                self.plan = plan
                return True
            self.caches = [{}, {}]
            self.max_extra = plan[0] / self.devicePixelRatioF() - .25

    def static_cache_bytes(self):
        backdrop = self.owner.backdrop
        total = backdrop.width() * backdrop.height() * 4
        if self.plan is None:
            return total
        total += sum(size.width() * size.height() * 4 for size in self.plan[1])
        total += self.plan[2].width() * self.plan[2].height() * (12 * self.paint_samples + 4)
        if needs_native_glyph_coverage(self.devicePixelRatioF()):
            total += _GLYPH_SCRATCH_BYTES
        return total

    def update_texture(self, index):
        owner = self.owner
        rect = owner.panels[index][0]
        revision = owner.navigation_revision if index == 0 else owner.content_revision
        if not revision or rect.isEmpty():
            return True
        cache = self.caches[index]
        dpr = self.plan[0]
        key = (revision, rect, dpr)
        if cache.get("key") == key:
            return True
        if not cache or not cache["texture"].isValid():
            return False
        pixels = cache["texture"].size()
        guard = max(1, math.ceil(dpr))
        stride = (pixels.height() if pixels.height() <= self.plan[2].height()
                  else max(1, self.plan[2].height() - 2 * guard))
        for top in range(0, pixels.height(), stride):
            height = min(stride, pixels.height() - top)
            paint_top = max(0, top - guard)
            paint_bottom = min(pixels.height(), top + height + guard)
            paint_height = paint_bottom - paint_top
            self.paint_target.bind()
            gl = self.context().functions()
            gl.glDisable(0x0C11)
            gl.glColorMask(True, True, True, True)
            gl.glStencilMask(0xFFFFFFFF)
            gl.glClearColor(0, 0, 0, 0)
            gl.glClear(0x4000 | 0x0400)
            device = QOpenGLPaintDevice(QSize(pixels.width(), paint_height))
            device.setDevicePixelRatio(dpr)
            glyph_device = GlyphPaintDevice(device, self.devicePixelRatioF(),
                                            QPointF(0, paint_top / dpr))
            painter = QPainter(glyph_device if needs_native_glyph_coverage(self.devicePixelRatioF())
                               else device)
            painter.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
            painter.translate(0, -paint_top / dpr)
            strip = QRectF(0, paint_top / dpr, rect.width(), paint_height / dpr)
            painter.setClipRect(strip)
            origin = rect.topLeft() if index == 0 else QPointF()
            painter.translate(-origin)
            owner.render_widgets(painter, index == 0, QRegion(strip.translated(origin).toAlignedRect()))
            painter.translate(origin)
            painter.end()
            gl.glDisable(0x0C11)
            # GLES requires matching rectangles/formats when resolving multisampling.
            QOpenGLFramebufferObject.blitFramebuffer(self.resolve_target, self.paint_target)
            QOpenGLFramebufferObject.blitFramebuffer(
                cache["texture"], QRect(0, pixels.height() - top - height, pixels.width(), height),
                self.resolve_target, QRect(0, paint_bottom - top - height, pixels.width(), height))
        cache["key"] = key
        return True

    def draw_texture(self, painter, index, colors, dark):
        cache = self.caches[index]
        if not cache:
            return
        rect, transform = self.owner.panels[index]
        painter.beginNativePainting()
        gl = self.context().functions()
        gl.glEnable(0x0BE2)  # GL_BLEND; premultiplied alpha
        gl.glBlendFunc(1, 0x0303)
        projection = QMatrix4x4()
        projection.ortho(0., float(self.width()), float(self.height()), 0., -1., 1.)
        quad = QMatrix4x4()
        quad.translate(rect.center().x(), rect.center().y())
        quad.scale(rect.width() / 2, -rect.height() / 2)
        appearance = (rect.size(), 12., self.owner.progress, colors.bgLayerAlt, colors.grey10,
                      (.32 if dark else .34) if index == 0 else (.62 if dark else .56),
                      .18 if dark else .72)
        texture = (self.presented_content_texture if index == 1 and self.presented_content_texture
                   else cache["texture"].texture())
        self.blitter.blit(texture, cache["texture"].size(),
                          projection * QMatrix4x4(transform) * quad, appearance)
        painter.endNativePainting()

    def clear_backdrop(self):
        if self.backdrop_cache:
            self.backdrop_cache["texture"].destroy()
        self.backdrop_cache = {}

    def invalidate_backdrop(self):
        pixmap = self.owner.backdrop
        key = (pixmap.cacheKey(), pixmap.size(), pixmap.devicePixelRatioF(), self.context())
        if self.backdrop_cache.get("key") != key:
            self.clear_backdrop()

    def update_backdrop(self):
        pixmap = self.owner.backdrop
        if pixmap.isNull() or self.backdrop_cache:
            return True
        size = pixmap.size()
        if (not self.blitter.isCreated() or size.width() > self.max_dimension
                or size.height() > self.max_dimension
                or size.width() * size.height() * 4 > _CACHE_BUDGET_BYTES):
            return False
        image = pixmap.toImage().convertToFormat(QImage.Format_RGBA8888_Premultiplied)
        if image.isNull():
            return False
        texture = QOpenGLTexture(QOpenGLTexture.Target2D)
        if not texture.create():
            return False
        texture.setFormat(QOpenGLTexture.RGBA8_UNorm)
        texture.setSize(size.width(), size.height())
        texture.setMipLevels(1)
        texture.setMinMagFilters(QOpenGLTexture.Linear, QOpenGLTexture.Linear)
        texture.setWrapMode(QOpenGLTexture.ClampToEdge)
        texture.allocateStorage(QOpenGLTexture.RGBA, QOpenGLTexture.UInt8)
        if not texture.isStorageAllocated():
            texture.destroy()
            return False
        # PySide advertises this void* overload as int but rejects 64-bit integers.
        # The explicit buffer pointer works across supported PySide versions.
        # Keep the QImage alive through this synchronous upload.
        try:
            texture.setData(QOpenGLTexture.RGBA, QOpenGLTexture.UInt8, VoidPtr(image.bits()))
        except (TypeError, ValueError, RuntimeError):
            texture.destroy()
            return False
        if self.context().functions().glGetError():
            texture.destroy()
            return False
        self.backdrop_cache = {
            "texture": texture,
            "key": (pixmap.cacheKey(), size, pixmap.devicePixelRatioF(), self.context()),
        }
        self.backdrop_uploads += 1
        return True

    def draw_backdrop(self, painter):
        if not self.backdrop_cache:
            return
        _, size, dpr, _ = self.backdrop_cache["key"]
        painter.beginNativePainting()
        gl = self.context().functions()
        gl.glEnable(0x0BE2)
        gl.glBlendFunc(1, 0x0303)
        projection = QMatrix4x4()
        projection.ortho(0., float(self.width()), float(self.height()), 0., -1., 1.)
        quad = QMatrix4x4()
        quad.translate(size.width() / dpr / 2, size.height() / dpr / 2)
        # QImage upload rows start at the top, unlike the panel FBO textures.
        quad.scale(size.width() / dpr / 2, size.height() / dpr / 2)
        self.blitter.blit(self.backdrop_cache["texture"].textureId(), size, projection * quad)
        painter.endNativePainting()

    def paintGL(self):
        gl = self.context().functions()
        gl.glDisable(0x0C11)  # GL_SCISSOR_TEST
        gl.glColorMask(True, True, True, True)
        gl.glClearColor(0, 0, 0, 0)
        gl.glClear(0x4000)  # GL_COLOR_BUFFER_BIT
        self.invalidate_backdrop()
        if self.property("presenting"):
            # Layout may still be pending during first exposure or a resize.
            # An empty scene is not an allocation failure or a user 2D choice.
            if all(rect.isEmpty() for rect, _ in self.owner.panels):
                return
            if not self.prepare_caches():
                if not self.cache_failure_pending:
                    self.cache_failure_pending = True
                    self.owner.cache_failure_timer.start(0)
                return
            accelerated_particles = self.particles.prepare(
                self.owner.navigation.contentHost(), self.owner.content_revision, self.plan[1][1],
                self.devicePixelRatioF(), self.plan[0],
                max(0, _CACHE_BUDGET_BYTES - self.static_cache_bytes()),
                True)
            if (not self.blitter.isCreated() or not self.update_texture(1)
                    or not self.update_texture(0)):
                self.owner.render_failure_timer.start(0)
                return
            self.presented_content_texture = self.caches[1]["texture"].texture() if self.caches[1] else 0
            if accelerated_particles:
                revision = self.owner.content_revision
                self.presented_content_texture = self.particles.compose(
                    self.presented_content_texture, revision, self.paint_target, self.resolve_target,
                    self.owner.render_foreground)
                if revision != self.owner.content_revision and not self.particles.activeLayerCount():
                    if not self.update_texture(1):
                        self.owner.render_failure_timer.start(0)
                        return
                    self.presented_content_texture = self.caches[1]["texture"].texture() if self.caches[1] else 0
            gl.glBindFramebuffer(0x8D40, self.defaultFramebufferObject())
            gl.glViewport(0, 0, round(self.width() * self.devicePixelRatioF()),
                          round(self.height() * self.devicePixelRatioF()))
        if not self.update_backdrop():
            self.owner.render_failure_timer.start(0)
            return
        painter = QPainter(self)
        self.draw_backdrop(painter)
        if self.property("presenting"):
            self.owner.paint(painter)
        painter.end()


class GallerySpatialController(QObject):
    def __init__(self, window, navigation):
        super().__init__(window)
        self.setObjectName("gallerySpatialController")
        self.window = window
        self.navigation = navigation
        self.settings = gallery_settings()
        self.canvas = None
        self.capture = self.content_capture = None
        self.navigation_revision = 0
        self.content_revision = 0
        self.backdrop = QPixmap()
        self.backdrop_dirty = True
        self.renderer_initialized = False
        self.renderer_ready = self.renderer_failed = False
        self.renderer_name = ""
        self.progress = 0.
        self.pointer = QPointF()
        self.target = False
        self.top = False
        self.panels = [(QRectF(), QTransform()), (QRectF(), QTransform())]
        self.forwarding = False
        self.filtering = False
        self.grabbed = self.hovered = None
        self.native_popup_anchors = {}
        self._native_background = navigation.testAttribute(Qt.WA_NoSystemBackground)
        # A public FluentWidget supplies inherited semantic tokens and theme changes.
        self.tokens = _SceneTheme(window)
        self.tokens.hide()
        self.motion = QVariantAnimation(self)
        self.motion.setObjectName("galleryAssemblyAnimation")
        self.motion.setEasingCurve(QEasingCurve.InOutCubic)
        self.motion.valueChanged.connect(self._advance)
        self.motion.finished.connect(self.settle)
        self.pointer_motion = QVariantAnimation(self)
        self.pointer_motion.setObjectName("galleryPointerAnimation")
        self.pointer_motion.setDuration(180)
        self.pointer_motion.setEasingCurve(QEasingCurve.OutCubic)
        self.pointer_motion.valueChanged.connect(self._tilt)
        self.sync_timer = QTimer(self)
        self.sync_timer.setSingleShot(True)
        self.sync_timer.timeout.connect(self.sync)
        self.check_timer = QTimer(self)
        self.check_timer.setSingleShot(True)
        self.check_timer.timeout.connect(self.check_renderer)
        self.renderer_timeout = QTimer(self)
        self.renderer_timeout.setSingleShot(True)
        self.renderer_timeout.setInterval(5000)
        self.renderer_timeout.timeout.connect(self.initialization_timed_out)
        self.cache_failure_timer = QTimer(self)
        self.cache_failure_timer.setSingleShot(True)
        self.cache_failure_timer.timeout.connect(self.release_oversized_presentation)
        self.render_failure_timer = QTimer(self)
        self.render_failure_timer.setSingleShot(True)
        self.render_failure_timer.timeout.connect(self.rendering_failed)
        shared_connections = [
            self.settings.spatialModeEnabledChanged.connect(self.apply_mode),
            self.settings.themeModeChanged.connect(self.refresh),
            self.settings.accentColorChanged.connect(self.refresh),
            self.settings.windowEffectChanged.connect(self.refresh),
            fluentqt.motion_policy().modeChanged.connect(self.refresh),
        ]
        if isinstance(window, fluentqt.Window):
            shared_connections.append(window.backdropStateChanged.connect(self._backdrop_state_changed))
        # PySide 6.2 can retain Python slots after their QObject receiver dies.
        # Capture connection handles, not the controller, and disconnect senders
        # which outlive the window before its child timers are destroyed.
        def disconnect_shared_settings(*_args):
            for connection in shared_connections:
                QObject.disconnect(connection)
        self.destroyed.connect(disconnect_shared_settings)
        roots = (weakref.ref(navigation), weakref.ref(navigation.contentHost()))
        def clear_presentation_roots(*_args):
            for root_ref in roots:
                root = root_ref()
                if _alive(root):
                    root.setProperty(_PRESENTATION_TRANSFORM, None)
        self.destroyed.connect(clear_presentation_roots)
        self.tokens.changed.connect(self.refresh, Qt.QueuedConnection)
        self.settings.spatial_available = False
        self.settings.spatial_availability_pending = True
        self.settings.spatialAvailabilityChanged.emit()
        reason = _session_unavailable_reason()
        if reason:
            self.disable(reason)
            return
        self.apply_mode(self.settings.spatial_mode_enabled)

    def set_filtering(self, active):
        if self.filtering == active:
            return
        self.filtering = active
        app = QApplication.instance()
        if active:
            app.installEventFilter(self)
        else:
            app.removeEventFilter(self)

    def ensure_renderer(self):
        if self.renderer_failed:
            return
        if not _alive(self.canvas):
            reason = _unavailable_reason()
            if reason:
                self.disable(reason)
                return
            SpatialRuntime.prepareApplication()
            self.canvas = _Surface(self, self.window)
            self.canvas.lower()
            self.navigation.setMouseTracking(True)
            self.window.setMouseTracking(True)
        self.set_filtering(True)
        self.canvas.setGeometry(QRect(self.navigation.mapTo(self.window, QPoint()), self.navigation.size()))
        self.canvas.show()
        self.sync()
        self.start_presentation()

    def queue_check(self):
        self.check_timer.start(0)

    @Slot()
    def context_lost(self):
        self.renderer_initialized = self.renderer_ready = False
        self.queue_check()

    def can_initialize_renderer(self):
        handle = self.window.window().windowHandle()
        return (self.settings.spatial_mode_enabled and _alive(self.canvas)
                and self.canvas.isVisible() and not self.canvas.size().isEmpty()
                and handle is not None and handle.isExposed())

    @Slot()
    def initialization_timed_out(self):
        self.check_renderer()
        if not self.renderer_ready and self.can_initialize_renderer():
            self.disable("3D could not start. Using the 2D Gallery.")

    @Slot()
    def check_renderer(self):
        if self.renderer_failed or self.renderer_ready or not _alive(self.canvas):
            return
        if not self.can_initialize_renderer():
            self.renderer_timeout.stop()
            return
        # An uninitialized surface is pending, including zero-sized/hidden startup.
        if not self.renderer_timeout.isActive():
            self.renderer_timeout.start()
        if not self.renderer_initialized:
            return
        if not self.canvas.isValid() or not self.canvas.blitter.isCreated():
            self.disable("3D could not start. Using the 2D Gallery.")
            return
        self.canvas.makeCurrent()
        self.renderer_name = _renderer(self.canvas.context())
        self.canvas.doneCurrent()
        if _software(self.renderer_name):
            self.disable("Hardware acceleration is unavailable. Using the 2D Gallery.")
            return
        self.renderer_ready = True
        self.renderer_timeout.stop()
        self.settings.set_spatial_availability(True)
        self.start_presentation()

    @Slot()
    def start_presentation(self):
        if (not self.renderer_ready
                or not self.settings.spatial_mode_enabled
                or self.window._splash is not None
                or self.window._dismissal_splash is not None):
            return
        if self.capture is not None:
            if not self.canvas.property("presenting"):
                self.apply_mode(True)
            return
        self.navigation.setAttribute(Qt.WA_NoSystemBackground)
        self.content_capture = _Capture(self._capture_content, self)
        self.navigation.contentHost().setGraphicsEffect(self.content_capture)
        self.capture = _Capture(self._capture_navigation, self)
        self.navigation.setGraphicsEffect(self.capture)
        self.apply_mode(self.settings.spatial_mode_enabled)

    def _capture_content(self):
        self.content_revision += 1
        self.canvas.update()

    def _capture_navigation(self):
        self.navigation_revision += 1
        self.content_revision += 1
        self.layout()
        self.canvas.update()

    def render_widgets(self, painter, navigation, region):
        # Widgets paint directly into the GPU cache. Suppress the other panel's effect
        # during this pass so a floating drawer never becomes part of the content.
        capture = self.capture if navigation else self.content_capture
        widget = self.navigation if navigation else self.navigation.contentHost()
        self.capture.composing = self.content_capture.composing = True
        capture.rendering = True
        try:
            widget.render(painter, region.boundingRect().topLeft(), region, QWidget.DrawChildren)
        finally:
            capture.rendering = False
            self.capture.composing = self.content_capture.composing = False

    def render_foreground(self, painter, widget, region):
        previous = (self.capture.composing, self.capture.rendering,
                    self.content_capture.composing, self.content_capture.rendering)
        self.capture.composing = self.content_capture.composing = True
        self.capture.rendering = self.content_capture.rendering = True
        try:
            widget.render(painter, region.boundingRect().topLeft(), region, QWidget.DrawChildren)
        finally:
            (self.capture.composing, self.capture.rendering,
             self.content_capture.composing, self.content_capture.rendering) = previous

    @Slot()
    def release_oversized_presentation(self):
        # Resource pressure can recover; keep the GPU available for another attempt.
        self.settings.set_spatial_mode_enabled(False)
        self.settle()

    @Slot()
    def rendering_failed(self):
        self.disable("3D rendering is unavailable. Using the 2D Gallery.")

    def disable(self, reason):
        self.publish_presentation_transforms(False)
        self.renderer_failed = True
        self.renderer_timeout.stop()
        self.set_filtering(False)
        self.renderer_ready = self.target = False
        self.motion.stop()
        self.pointer_motion.stop()
        self.progress = 0.
        if _alive(self.capture):
            self.navigation.setGraphicsEffect(None)
        if _alive(self.content_capture):
            self.navigation.contentHost().setGraphicsEffect(None)
        self.capture = self.content_capture = None
        if _alive(self.canvas):
            self.canvas.clear_frame_caches()
            self.canvas.hide()
            self.canvas.deleteLater()
        self.canvas = None
        self.navigation.setAttribute(Qt.WA_NoSystemBackground, self._native_background)
        self.window.setProperty("gallerySpatialEnabled", False)
        self.settings.set_spatial_availability(False, reason)
        self.navigation.update()

    @staticmethod
    def is_native_overlay(widget):
        return (bool(widget.property("_fluent_qt_overlay_surface"))
                or "Scrim" in widget.metaObject().className()
                or widget.objectName() == "GalleryIntroTour.Scrim")

    def first_overlay(self):
        for child in self.window.window().children():
            if (isinstance(child, QWidget) and child.isVisible() and not child.isWindow()
                    and self.is_native_overlay(child)):
                return child
        return None

    def native_overlay_at(self, global_pos):
        if not self.first_overlay():
            return False
        # Native hit testing respects masks and skips mouse-transparent dim-only scrims.
        top = self.window.window()
        hit = top.childAt(top.mapFromGlobal(global_pos))
        while hit and hit != top:
            if self.is_native_overlay(hit):
                return True
            hit = hit.parentWidget()
        return False

    def raise_presentation(self):
        overlay = self.first_overlay()
        if overlay and overlay.parentWidget() == self.canvas.parentWidget():
            self.canvas.stackUnder(overlay)
        else:
            self.canvas.raise_()

    @Slot()
    def refresh(self, *args):
        self.backdrop_dirty = True
        if fluentqt.current_theme() == fluentqt.Theme.HighContrast:
            self.settings.set_spatial_mode_enabled(False)
        self.apply_mode(self.settings.spatial_mode_enabled)

    @Slot()
    def _backdrop_state_changed(self):
        # Native material removal must see the new opaque GPU background before
        # the later Gallery Settings notification. Do not restart 3D motion.
        self.backdrop_dirty = True
        self.navigation_revision += 1
        self.content_revision += 1
        self.sync()

    @Slot(bool)
    def apply_mode(self, enabled):
        if enabled and (not _alive(self.capture) or not self.renderer_ready):
            self.ensure_renderer()
            return
        if not _alive(self.capture):
            self.renderer_timeout.stop()
            if _alive(self.canvas):
                self.canvas.hide()
            self.set_filtering(False)
            return
        if enabled:
            self.set_filtering(True)
            self.layout()
            self.canvas.show()
            self.navigation.setAttribute(Qt.WA_NoSystemBackground)
        self.motion.stop()
        self.pointer_motion.stop()
        self.pointer = QPointF()
        self.target = (enabled and self.renderer_ready
                       and fluentqt.current_motion_mode() == fluentqt.MotionMode.Full
                       and fluentqt.current_theme() != fluentqt.Theme.HighContrast)
        self.window.setProperty("gallerySpatialEnabled", self.target)
        target = 1. if self.target else 0.
        if (not self.window.isVisible() or abs(self.progress - target) < .001
                or fluentqt.current_motion_mode() != fluentqt.MotionMode.Full):
            self.settle()
            return
        self.content_capture.setEnabled(True)
        self.capture.setEnabled(True)
        self.canvas.setProperty("presenting", True)
        self.raise_presentation()
        self.sync()
        self.motion.setDuration(max(1, round(420 * abs(target - self.progress))))
        self.motion.setStartValue(self.progress)
        self.motion.setEndValue(target)
        self.motion.start()

    @Slot()
    def settle(self):
        self.motion.stop()
        self.pointer_motion.stop()
        self.pointer = QPointF()
        self.progress = 1. if self.target else 0.
        if not _alive(self.capture):
            return
        self.content_capture.setEnabled(self.target)
        self.capture.setEnabled(self.target)
        self.canvas.setProperty("presenting", self.target)
        if self.target:
            self.navigation.setAttribute(Qt.WA_NoSystemBackground)
            self.raise_presentation()
        else:
            self.publish_presentation_transforms(False)
            self.navigation.setAttribute(Qt.WA_NoSystemBackground, self._native_background)
            self.canvas.hide()
            self.canvas.clear_frame_caches()
            self.set_filtering(False)
            self.canvas.setProperty("galleryDepthProgress", 0.)
            self.navigation_revision = 0
            self.content_revision = 0
            self.backdrop = QPixmap()
            self.backdrop_dirty = True
            self.grabbed = self.hovered = None
        self.sync()
        self.navigation.update()
        self.window.update()

    def _advance(self, value):
        self.progress = value
        self.sync()

    def _tilt(self, value):
        self.pointer = value
        self.sync()

    def follow_pointer(self, tilt):
        if self.pointer_motion.state() == QAbstractAnimation.Running:
            self.pointer_motion.setEndValue(tilt)
        elif QLineF(self.pointer, tilt).length() > .001:
            self.pointer_motion.setStartValue(self.pointer)
            self.pointer_motion.setEndValue(tilt)
            self.pointer_motion.start()

    def project(self, rect, angle, navigation):
        if rect.isEmpty() or self.progress == 0:
            return QTransform()
        camera = max(1400., max(rect.width(), rect.height()) * 6.)
        rotation = QMatrix4x4()
        rotation.rotate(((angle if self.top else 0) + self.pointer.y()) * self.progress, 1, 0, 0)
        rotation.rotate(((0 if self.top else angle) + self.pointer.x()) * self.progress, 0, 1, 0)
        original = QPolygonF([rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()])
        projected = QPolygonF()
        for corner in original:
            p = rotation.map(QVector3D(corner.x() - rect.center().x(), corner.y() - rect.center().y(), 0))
            scale = camera / (camera - p.z())
            projected.append(QPointF(p.x() * scale, p.y() * scale))
        horizontal = ((5. if self.top else min(3., rect.width() * 6. / rect.height()))
                      if navigation else 6.) * self.progress
        vertical = (min(.5, rect.height() * 5. / rect.width())
                    if navigation and self.top else 6.) * self.progress
        available = rect.adjusted(horizontal, vertical, -horizontal, -vertical)
        bounds = projected.boundingRect()
        fit = min(available.width() / bounds.width(), available.height() / bounds.height())
        points = QPolygonF([available.center() + (p - bounds.center()) * fit for p in projected])
        transform = QTransform()
        QTransform.quadToQuad(original, points, transform)
        return transform

    def layout(self):
        nav = self.navigation
        self.top = nav.effectiveDisplayMode() == fluentqt.NavigationView.DisplayMode.Top
        content = QRectF(nav.contentHost().geometry())
        width = max((w.geometry().right() + 1 for w in (
            nav.headerChromeWidget(), nav.mainChromeWidget(), nav.footerChromeWidget()
        ) if w and not w.isHidden()), default=0)
        navigation = (QRectF(0, 0, nav.width(), content.top()) if self.top
                      else QRectF(0, 0, width, nav.height()))
        angle = 3. if self.top else 5.
        self.panels = [(navigation, self.project(navigation, angle, True)),
                       (content, self.project(content, -angle, False))]

    @Slot()
    def sync(self):
        if not _alive(self.canvas) or self.canvas.isHidden():
            return
        bounds = QRect(self.navigation.mapTo(self.window, QPoint()), self.navigation.size())
        self.backdrop_dirty |= self.canvas.geometry() != bounds
        self.backdrop_dirty |= (not self.backdrop.isNull()
                                and self.backdrop.devicePixelRatioF() != self.window.devicePixelRatioF())
        self.canvas.setGeometry(bounds)
        # The visible GL surface needs the window material during splash fade-out,
        # before it starts presenting the projected panels.
        if self.backdrop_dirty:
            self.backdrop_dirty = False
            self.backdrop = QPixmap()
            if self.window.backdropState().surfaceMode != fluentqt.BackdropSurfaceMode.CompositedTransparent:
                dpr = self.window.devicePixelRatioF()
                self.backdrop = QPixmap(bounds.size() * dpr)
                self.backdrop.setDevicePixelRatio(dpr)
                self.backdrop.fill(Qt.transparent)
                painter = QPainter(self.backdrop)
                painter.translate(-bounds.topLeft())
                self.window.render(painter, QPoint(), QRegion(), QWidget.RenderFlags())
                painter.end()
        if not self.canvas.property("presenting"):
            self.canvas.update()
            return
        self.layout()
        self.publish_presentation_transforms(True)
        self.canvas.setProperty("galleryRotationAxis", "X" if self.top else "Y")
        self.canvas.setProperty("galleryNavigationRotation", (3. if self.top else 5.) * self.progress)
        self.canvas.setProperty("galleryContentRotation", -(3. if self.top else 5.) * self.progress)
        self.canvas.setProperty("galleryPointerTilt", self.pointer)
        self.canvas.setProperty("galleryDepthProgress", self.progress)
        self.canvas.update()

    def publish_presentation_transforms(self, enabled):
        if not _alive(self.navigation):
            return
        host = self.navigation.contentHost()
        if enabled:
            origin = host.mapTo(self.navigation, QPoint())
            transform = (QTransform.fromTranslate(origin.x(), origin.y()) * self.panels[1][1]
                         * QTransform.fromTranslate(-origin.x(), -origin.y()))
            self.navigation.setProperty(_PRESENTATION_TRANSFORM, self.panels[0][1])
            host.setProperty(_PRESENTATION_TRANSFORM, transform)
        else:
            self.navigation.setProperty(_PRESENTATION_TRANSFORM, None)
            host.setProperty(_PRESENTATION_TRANSFORM, None)
        for key, (popup, source, point, offset) in list(self.native_popup_anchors.items()):
            if not _alive(popup):
                self.native_popup_anchors.pop(key, None)
                continue
            if (not _alive(source) or not popup.isVisible()
                    or source.window() != self.window.window()):
                continue
            presented = (self.window.mapToGlobal(self.projected_position(source, point))
                         if enabled else source.mapToGlobal(point))
            position = presented + offset
            screen = QGuiApplication.screenAt(presented) or popup.screen()
            if screen:
                available = screen.availableGeometry()
                frame = popup.frameGeometry()
                frame_offset = frame.topLeft() - popup.pos()
                frame_position = position + frame_offset
                position = QPoint(
                    max(available.left(), min(frame_position.x(), available.right() - frame.width() + 1)),
                    max(available.top(), min(frame_position.y(), available.bottom() - frame.height() + 1)),
                ) - frame_offset
            was_forwarding = self.forwarding
            self.forwarding = True
            try:
                popup.move(position)
            finally:
                self.forwarding = was_forwarding

    def _settle_native_popup_offset(self, key):
        if not _alive(self):
            return
        entry = self.native_popup_anchors.get(key)
        if not entry:
            return
        popup, source, point, _offset = entry
        if not _alive(popup) or not _alive(source) or not popup.isVisible():
            return
        presented = self.window.mapToGlobal(self.projected_position(source, point))
        self.native_popup_anchors[key] = (popup, source, point, popup.pos() - presented)

    def paint(self, painter):
        if not self.navigation_revision:
            return
        painter.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
        colors = self.tokens.theme_tokens().colors
        dark = fluentqt.theme_uses_dark_appearance(fluentqt.current_theme())
        for index in (1, 0):
            rect, transform = self.panels[index]
            if rect.isEmpty():
                continue
            body = rect.adjusted(.5, .5, -.5, -.5)
            path = QPainterPath()
            path.addRoundedRect(body, 12, 12)
            painter.save()
            if self.progress > 0:
                painter.setClipRect(self.panels[0][0].united(self.panels[1][0]).adjusted(1, 1, -1, -1))
            painter.setTransform(transform)
            shadow = QPainterPath()
            shadow.addRect(body.adjusted(-20, -20, 20, 24))
            shadow.addPath(path)
            shadow.setFillRule(Qt.OddEvenFill)
            painter.save()
            painter.setClipPath(shadow, Qt.IntersectClip)
            painter.setPen(Qt.NoPen)
            for spread in range(10, 0, -1):
                painter.setBrush(QColor(0, 0, 0, round((1.5 if index == 0 else 2.2) * self.progress)))
                painter.drawRoundedRect(body.adjusted(-spread, -spread + 3, spread, spread + 3), 12 + spread, 12 + spread)
            painter.restore()
            # Use the sampler's single projected boundary for widget coverage,
            # material and reflection instead of independently rasterized edges.
            self.canvas.draw_texture(painter, index, colors, dark)
            painter.restore()

    def in_content(self, widget):
        host = self.navigation.contentHost()
        return widget == host or host.isAncestorOf(widget)

    def projected_position(self, widget, point):
        nav = self.navigation
        if _alive(self.capture) and self.capture.isEnabled() and (widget == nav or nav.isAncestorOf(widget)):
            local = widget.mapTo(nav, point)
            panel = 1 if self.in_content(widget) else 0
            return self.panels[panel][1].map(QPointF(local)).toPoint() + nav.mapTo(self.window, QPoint())
        return widget.mapTo(self.window, point)

    def hover(self, target, source):
        if target == self.hovered:
            return
        if _alive(self.hovered):
            QApplication.sendEvent(self.hovered, QEvent(QEvent.Leave))
        self.hovered = target
        if _alive(target):
            local = target.mapFrom(self.navigation, source)
            QApplication.sendEvent(target, QEnterEvent(local, target.mapTo(self.window, local), target.mapToGlobal(local)))

    def eventFilter(self, watched, event):
        native_menu_show = event.type() == QEvent.Show and isinstance(watched, QMenu)
        if ((self.forwarding and not native_menu_show) or not _alive(self.canvas) or not _alive(self.window)
                or not _alive(self.navigation)):
            return False
        kind = event.type()
        nav = self.navigation
        if (kind == QEvent.MouseMove and _alive(self.hovered) and _alive(self.capture)
                and self.capture.isEnabled() and event.buttons() == Qt.NoButton
                and watched == self.window.window().windowHandle()):
            global_pos = event.globalPosition().toPoint()
            presented = nav.mapFromGlobal(global_pos)
            target = None
            if nav.rect().contains(presented) and not self.native_overlay_at(global_pos):
                for rect, transform in self.panels:
                    source = transform.inverted()[0].map(QPointF(presented))
                    if rect.contains(source):
                        target = nav.childAt(source.toPoint())
                        break
            if target != self.hovered:
                # Clear projected hover before Qt dispatches the native QWidget
                # move and starts its tooltip timer. A later synthetic Leave
                # cancels that timer; the forwarded move cannot restart it.
                previous, self.hovered = self.hovered, None
                self.forwarding = True
                try:
                    QApplication.sendEvent(previous, QEvent(QEvent.Leave))
                finally:
                    if _alive(self):
                        self.forwarding = False
                if not _alive(self) or not _alive(watched):
                    return True
        if watched == self.window and not self.renderer_ready and kind in (QEvent.Show, QEvent.UpdateRequest):
            self.queue_check()
        if not isinstance(watched, QWidget):
            return False
        mouse = kind in (QEvent.MouseMove, QEvent.MouseButtonPress, QEvent.MouseButtonRelease, QEvent.MouseButtonDblClick)
        mouse_global = _mouse_global_position(watched, event) if mouse else QPoint()
        if not _alive(self.grabbed):
            self.grabbed = None
        if kind in (QEvent.MouseMove, QEvent.Wheel) and event.buttons() == Qt.NoButton:
            self.grabbed = None
        in_source = watched == nav or nav.isAncestorOf(watched)
        if not in_source and watched != self.window and kind == QEvent.MouseButtonRelease:
            self.grabbed = None
        if watched in (self.window, nav, nav.contentHost(), nav.mainChromeWidget(), nav.footerChromeWidget(), nav.headerChromeWidget()):
            if kind in (QEvent.Resize, QEvent.Move, QEvent.Show, QEvent.LayoutRequest, QEvent.ActivationChange):
                if kind == QEvent.Resize and self.motion.state() == QAbstractAnimation.Running:
                    self.settle()
                if kind in (QEvent.Resize, QEvent.Move):
                    self.pointer_motion.stop()
                    self.pointer = QPointF()
                if kind == QEvent.ActivationChange:
                    self.backdrop_dirty = True
                self.sync_timer.start(0)
        if not _alive(self.capture) or not self.capture.isEnabled():
            return False
        if kind in (QEvent.MouseButtonPress, QEvent.Wheel) or (kind == QEvent.Show and (watched.isWindow() or self.first_overlay())):
            self.pointer_motion.stop()
        outside_move = (not in_source and watched != self.canvas and kind == QEvent.MouseMove
                        and (watched != self.window or not nav.rect().contains(nav.mapFromGlobal(mouse_global))))
        if (watched == self.window and kind == QEvent.Leave) or outside_move:
            self.forwarding = True
            self.hover(None, QPoint())
            self.forwarding = False
            if not self.grabbed and not self.first_overlay():
                self.follow_pointer(QPointF())
        if watched == self.window and kind in (QEvent.WindowDeactivate, QEvent.Hide):
            self.settle()
        if isinstance(watched, QMenu) and kind == QEvent.Show:
            key = id(watched)
            if key in self.native_popup_anchors:
                self.native_popup_anchors[key] = (watched, None, QPoint(), QPoint())
            declared = watched.property(_PRESENTED_MENU_SOURCE)
            source = declared if _alive(declared) else watched.parentWidget()
            # Child menus and other top-levels already have native coordinates.
            if (_alive(source) and source.window() == self.window.window()
                    and (source == nav or nav.isAncestorOf(source))):
                point = (watched.property(_PRESENTED_MENU_POINT) if declared
                         else source.mapFromGlobal(watched.pos()))
                presented = self.window.mapToGlobal(self.projected_position(source, point))
                if not declared:
                    was_forwarding = self.forwarding
                    self.forwarding = True
                    try:
                        watched.move(presented)
                    finally:
                        self.forwarding = was_forwarding
                anchors = self.native_popup_anchors
                if key not in anchors:
                    watched.destroyed.connect(lambda _=None, key=key: anchors.pop(key, None))
                anchors[key] = (watched, source, point, watched.pos() - presented)
                QTimer.singleShot(0, partial(self._settle_native_popup_offset, key))
        if watched.window() != self.window.window() or (not in_source and watched != self.window):
            return False
        if in_source and kind in (QEvent.Enter, QEvent.Leave, QEvent.HoverEnter, QEvent.HoverLeave, QEvent.HoverMove):
            return True
        wheel = kind == QEvent.Wheel
        tooltip = kind == QEvent.ToolTip
        context = kind == QEvent.ContextMenu
        if not (mouse or wheel or tooltip or context):
            return False
        # Qt's tooltip timer may have saved the same overwritten global point.
        global_pos = (mouse_global if mouse else watched.mapToGlobal(event.pos()) if tooltip
                      else event.globalPosition().toPoint() if wheel else event.globalPos())
        if self.native_overlay_at(global_pos):
            # Ignored label/card input may bubble to the host; never forward it behind the overlay.
            self.forwarding = True
            try:
                self.grabbed = None
                self.hover(None, QPoint())
            finally:
                self.forwarding = False
            return True
        presented = nav.mapFromGlobal(global_pos)
        if not in_source and not nav.rect().contains(presented):
            return False
        if (kind == QEvent.MouseMove and event.buttons() == Qt.NoButton
                and self.motion.state() != QAbstractAnimation.Running
                and not QApplication.activePopupWidget() and not self.first_overlay()):
            x = max(-1., min(1., 2. * presented.x() / max(1, nav.width()) - 1))
            y = max(-1., min(1., 2. * presented.y() / max(1, nav.height()) - 1))
            self.follow_pointer(QPointF(x * .65, -y * .45))
        hit, source = None, QPointF()
        for index, (rect, transform) in enumerate(self.panels):
            candidate = transform.inverted()[0].map(QPointF(presented))
            if rect.contains(candidate):
                hit, source = index, candidate
                break
        floating = not self.top and self.panels[0][0].right() > self.panels[1][0].left()
        if not self.grabbed and floating and hit != 0 and kind == QEvent.MouseButtonPress:
            nav.setPaneOpen(False)
            return True
        if self.grabbed:
            index = 1 if self.in_content(self.grabbed) else 0
            source = self.panels[index][1].inverted()[0].map(QPointF(presented))
        target = self.grabbed or (nav.childAt(source.toPoint()) if hit is not None else None)
        self.forwarding = True
        try:
            if not target:
                self.hover(None, QPoint())
                return True
            local = target.mapFrom(nav, source.toPoint())
            source_global = nav.mapToGlobal(source.toPoint())
            if mouse:
                self.hover(target, source.toPoint())
                if kind == QEvent.MouseButtonPress:
                    self.grabbed = target
                forwarded = QMouseEvent(kind, local, target.mapTo(self.window, local), source_global,
                                        event.button(), event.buttons(), event.modifiers(), event.source())
                QApplication.sendEvent(target, forwarded)
                if kind == QEvent.MouseButtonRelease:
                    self.grabbed = None
            elif wheel:
                receiver = target
                while _alive(receiver):
                    forwarded = QWheelEvent(receiver.mapFromGlobal(source_global), source_global,
                                            event.pixelDelta(), event.angleDelta(), event.buttons(),
                                            event.modifiers(), event.phase(), event.inverted(), event.source())
                    forwarded.setTimestamp(event.timestamp())
                    forwarded.ignore()
                    QApplication.sendEvent(receiver, forwarded)
                    if not _alive(receiver) or forwarded.isAccepted() or receiver.isWindow() or receiver.testAttribute(Qt.WA_NoMousePropagation):
                        break
                    receiver = receiver.parentWidget()
            elif tooltip:
                QApplication.sendEvent(target, QHelpEvent(QEvent.ToolTip, local, source_global))
            else:
                QApplication.sendEvent(target, QContextMenuEvent(event.reason(), local, source_global, event.modifiers()))
        finally:
            self.forwarding = False
        return True
