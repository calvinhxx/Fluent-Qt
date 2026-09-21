"""GPU composition of the Gallery shell, matching GallerySpatialController.cpp.

The live widgets retain their parents and keyboard focus. Two cached surfaces
share one OpenGL canvas; pointer events are mapped back into the native layout.
Only the optional Spatial entry path imports this module.
"""

import os
import math
import ctypes
import sys
import struct

import fluentqt
from PySide6.QtCore import (
    QAbstractAnimation, QEasingCurve, QEvent, QLineF, QObject, QPoint, QPointF,
    QRect, QRectF, QSize, Qt, QTimer, QVariantAnimation, Signal, Slot,
)
from PySide6.QtGui import (
    QBrush, QColor, QContextMenuEvent, QEnterEvent, QGuiApplication, QHelpEvent,
    QImage, QLinearGradient, QMatrix4x4, QMouseEvent, QOffscreenSurface, QOpenGLContext,
    QPaintEngine, QPainter, QPainterPath, QPen, QPixmap, QPolygonF, QRegion, QTransform,
    QVector2D, QVector3D, QSurfaceFormat, QWheelEvent,
)
from PySide6.QtOpenGL import (
    QOpenGLFramebufferObject, QOpenGLFramebufferObjectFormat, QOpenGLPaintDevice,
    QOpenGLBuffer, QOpenGLShader, QOpenGLShaderProgram, QOpenGLVertexArrayObject,
)
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import (
    QApplication, QFrame, QGraphicsEffect, QProxyStyle, QStyle, QStyleOption,
    QStyleOptionButton, QStyleOptionFrame, QWidget,
)
from shiboken6 import isValid

from .settings import gallery_settings


def _alive(widget):
    return widget is not None and isValid(widget)


def _software(renderer):
    return not renderer or any(name in renderer.lower() for name in (
        "llvmpipe", "softpipe", "swiftshader", "software", "basic render driver",
        "warp", "gdi generic",
    ))


def _renderer(context):
    value = context.functions().glGetString(0x1F01)  # GL_RENDERER
    return value.decode() if isinstance(value, bytes) else str(value or "")


def _session_unavailable_reason():
    if os.environ.get("FLUENT_QT_GALLERY_DISABLE_3D", "0") != "0":
        return "3D is disabled for this session."
    if QGuiApplication.platformName() in ("offscreen", "minimal", "vnc"):
        return "This display uses the 2D Gallery."
    return ""


def _unavailable_reason():
    context = QOpenGLContext()
    if not context.create():
        return "3D acceleration is unavailable. Using the 2D Gallery."
    surface = QOffscreenSurface()
    surface.setFormat(context.format())
    surface.create()
    if not surface.isValid() or not context.makeCurrent(surface):
        return "3D acceleration is unavailable. Using the 2D Gallery."
    renderer = _renderer(context)
    context.doneCurrent()
    if _software(renderer):
        return "Hardware acceleration is unavailable. Using the 2D Gallery."
    return ""


def _alpha(color, opacity):
    result = QColor(color)
    result.setAlphaF(max(0., min(1., opacity)))
    return result


# Same aggregate cache budget and sampling ladder as GallerySpatialRenderPolicy.h.
_CACHE_BUDGET_BYTES = 192 * 1024 * 1024
_CACHE_BYTES_PER_PIXEL = 4
_PAINT_SAMPLES = 2


def _cache_plan(panels, native_dpr, max_dimension, max_extra=2., budget=_CACHE_BUDGET_BYTES,
                paint_samples=_PAINT_SAMPLES):
    if (not math.isfinite(native_dpr) or native_dpr <= 0 or max_dimension <= 0
            or budget <= 0 or paint_samples <= 1):
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
        rows = (budget - texture_bytes) // row_bytes
        if rows < min(32, paint_size.height()):
            continue
        paint_size.setHeight(min(paint_size.height(), rows))
        return dpr, sizes, paint_size
    return None


class _GpuCompatibleNativeStyle(QProxyStyle):
    """Keep Cocoa's CGContext-only style primitives out of the OpenGL painter."""

    def __init__(self, base):
        name = base.objectName()
        super().__init__(base)
        self.setObjectName(name)
        self.setProperty("galleryGpuCompatibleStyle", True)

    @staticmethod
    def _gpu(painter):
        return painter.paintEngine().type() == QPaintEngine.OpenGL2

    def _paint_native(self, option, painter, paint):
        if not self._gpu(painter):
            paint(option, painter)
            return
        # Translate the option, not the painter: Cocoa draws NSViews in option coordinates.
        if isinstance(option, (QStyleOptionFrame, QStyleOptionButton)) or option.type == QStyleOption.SO_Default:
            option = type(option)(option)
            bounds = option.rect.adjusted(-4, -4, 4, 4)
            rect = option.rect
            rect.translate(-bounds.topLeft())
            option.rect = rect
        else:
            bounds = QRect(0, 0, max(1, option.rect.right() + 5), max(1, option.rect.bottom() + 5))
        size = bounds.size()
        dpr = painter.device().devicePixelRatioF()
        image = QImage(QSize(math.ceil(size.width() * dpr), math.ceil(size.height() * dpr)),
                       QImage.Format_ARGB32_Premultiplied)
        image.setDevicePixelRatio(dpr)
        image.fill(Qt.transparent)
        native = QPainter(image)
        native.setFont(painter.font())
        native.setPen(painter.pen())
        native.setRenderHints(painter.renderHints())
        paint(option, native)
        native.end()
        painter.drawImage(bounds.topLeft(), image)

    def drawPrimitive(self, element, option, painter, widget=None):
        if self._gpu(painter) and element == QStyle.PE_Widget:
            return  # QMacStyle delegates this no-op to QCommonStyle.
        self._paint_native(option, painter, lambda local, target:
                           QProxyStyle.drawPrimitive(self, element, local, target, widget))

    def drawControl(self, element, option, painter, widget=None):
        if (self._gpu(painter) and element == QStyle.CE_ShapedFrame
                and getattr(option, "frameShape", None) == QFrame.NoFrame):
            return
        self._paint_native(option, painter, lambda local, target:
                           QProxyStyle.drawControl(self, element, local, target, widget))

    def drawComplexControl(self, control, option, painter, widget=None):
        self._paint_native(option, painter, lambda local, target:
                           QProxyStyle.drawComplexControl(self, control, local, target, widget))


def _prepare_native_style():
    app = QApplication.instance()
    style = app.style()
    if (app.platformName() == "cocoa" and "mac" in style.objectName().lower()
            and not style.property("galleryGpuCompatibleStyle")):
        app._gallery_gpu_style = _GpuCompatibleNativeStyle(style)
        app.setStyle(app._gallery_gpu_style)


class _Capture(QGraphicsEffect):
    def __init__(self, invalidated, parent):
        super().__init__(parent)
        self.invalidated = invalidated
        self.rendering = self.composing = False
        self.setEnabled(False)

    def draw(self, painter):
        if self.rendering:
            self.drawSource(painter)
        elif not self.composing:
            self.invalidated()


class _SceneTheme(fluentqt.FluentWidget):
    changed = Signal()

    def on_theme_updated(self):
        super().on_theme_updated()
        self.changed.emit()


class _PanelSampler:
    """Integrate a projected pixel's footprint without mipmap upsampling blur."""

    def __init__(self):
        self.program = QOpenGLShaderProgram()
        self.vertices = QOpenGLBuffer()
        self.vao = QOpenGLVertexArrayObject()

    def create(self):
        context = QOpenGLContext.currentContext()
        es = context.isOpenGLES()
        version_string = context.functions().glGetString(0x1F02)  # GL_VERSION
        if isinstance(version_string, bytes):
            version_string = version_string.decode("ascii", errors="replace")
        version_string = str(version_string)
        es3 = (context.format().majorVersion() >= 3
               or version_string.startswith("OpenGL ES 3.") or "WebGL 2." in version_string)
        modern = (es3 if es
                  else context.format().profile() == QSurfaceFormat.CoreProfile)
        version = ("#version 300 es\n" if es else "#version 150\n") if modern else (
            "#extension GL_OES_standard_derivatives : enable\n" if es else "")
        precision = "precision highp float;\n" if es else ""
        vertex = ("in vec2 position; out vec2 uv;\n" if modern else
                  "attribute highp vec2 position; varying highp vec2 uv;\n") + """
uniform mat4 target;
void main() {
    uv = (position + 1.0) * 0.5;
    gl_Position = target * vec4(position, 0.0, 1.0);
}
"""
        fragment = ("in vec2 uv; out vec4 color;\n#define SAMPLE texture\n#define OUTPUT color\n"
                    if modern else "varying highp vec2 uv;\n#define SAMPLE texture2D\n#define OUTPUT gl_FragColor\n") + """
uniform sampler2D source;
uniform vec2 sourceSize;
void main() {
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec2 span = clamp(vec2(length(dx * sourceSize), length(dy * sourceSize)) - 1.0, 0.0, 1.0);
    dx *= 0.25 * span.x; dy *= 0.25 * span.y;
    OUTPUT = 0.25 * (SAMPLE(source, uv - dx - dy) + SAMPLE(source, uv + dx - dy)
                  + SAMPLE(source, uv - dx + dy) + SAMPLE(source, uv + dx + dy));
}
"""
        if (not self.program.addShaderFromSourceCode(QOpenGLShader.Vertex, version + precision + vertex)
                or not self.program.addShaderFromSourceCode(QOpenGLShader.Fragment, version + precision + fragment)):
            return False
        self.program.bindAttributeLocation("position", 0)
        if not self.program.link() or not self.vertices.create():
            return False
        self.vao.create()
        vao = QOpenGLVertexArrayObject.Binder(self.vao)
        self.vertices.bind()
        data = struct.pack("8f", -1, -1, 1, -1, -1, 1, 1, 1)
        self.vertices.allocate(data, len(data))
        self.vertices.release()
        del vao
        return True

    def isCreated(self):
        return self.program.isLinked() and self.vertices.isCreated()

    def destroy(self):
        self.vertices.destroy()
        self.vao.destroy()
        self.program.removeAllShaders()

    def blit(self, texture, size, target):
        gl = QOpenGLContext.currentContext().functions()
        vao = QOpenGLVertexArrayObject.Binder(self.vao)
        self.program.bind()
        self.vertices.bind()
        self.program.enableAttributeArray(0)
        self.program.setAttributeBuffer(0, 0x1406, 0, 2)
        self.program.setUniformValue("target", target)
        self.program.setUniformValue("source", 0)
        self.program.setUniformValue("sourceSize", QVector2D(size.width(), size.height()))
        gl.glActiveTexture(0x84C0)
        gl.glBindTexture(0x0DE1, texture)
        gl.glDrawArrays(0x0005, 0, 4)
        gl.glBindTexture(0x0DE1, 0)
        self.program.disableAttributeArray(0)
        self.vertices.release()
        self.program.release()
        del vao


class _Surface(QOpenGLWidget):
    def __init__(self, owner, parent):
        super().__init__(parent)
        self.owner = owner
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
        self.blitter = _PanelSampler()
        self.blitter.create()
        self.caches = [{}, {}]
        self.paint_target = None
        self.resolve_target = None
        self.plan = None
        self.max_extra = 2.
        self.cache_failure_pending = False
        gl = self.context().functions()
        # PySide 6.9's array-return overload requires NumPy and can crash without it.
        # Query the two viewport values through the context's standard GL entry point.
        convention = ctypes.WINFUNCTYPE if sys.platform == "win32" else ctypes.CFUNCTYPE
        address = self.context().getProcAddress(b"glGetIntegerv")
        viewport = (ctypes.c_int * 2)()
        if address:
            query = convention(None, ctypes.c_uint, ctypes.POINTER(ctypes.c_int))(address)
            query(0x0D3A, viewport)
        self.max_dimension = min(gl.glGetIntegerv(0x0D33), gl.glGetIntegerv(0x84E8),
                                 *viewport)
        sample_format = QOpenGLFramebufferObjectFormat()
        sample_format.setAttachment(QOpenGLFramebufferObject.CombinedDepthStencil)
        sample_format.setInternalTextureFormat(0x8058)  # GL_RGBA8
        sample_format.setSamples(_PAINT_SAMPLES)
        probe = QOpenGLFramebufferObject(QSize(1, 1), sample_format)
        self.paint_samples = probe.format().samples() if probe.isValid() else 0
        del probe
        self.context().aboutToBeDestroyed.connect(self.release_context)
        self.owner.renderer_initialized = True
        self.owner.queue_check()

    def clear_frame_caches(self):
        self.makeCurrent()
        self.caches = [{}, {}]
        self.paint_target = None
        self.resolve_target = None
        self.plan = None
        self.max_extra = 2.
        self.cache_failure_pending = False
        self.doneCurrent()

    def release_context(self):
        self.makeCurrent()
        self.caches = [{}, {}]
        self.paint_target = None
        self.resolve_target = None
        self.blitter.destroy()
        self.doneCurrent()
        self.owner.context_lost()

    def prepare_caches(self):
        if self.paint_samples <= 1:
            return False
        panels = [rect.size() for rect, _ in self.owner.panels]
        while True:
            plan = _cache_plan(panels, self.devicePixelRatioF(), self.max_dimension,
                               self.max_extra, paint_samples=self.paint_samples)
            if plan is None:
                return False
            if plan == self.plan:
                return True
            # Free the old pair before allocating replacements, including on resize.
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
        outside = QPainterPath()
        local = QRectF(QPointF(), rect.size())
        outside.addRect(local)
        outside.addRoundedRect(local.adjusted(.5, .5, -.5, -.5), 12, 12)
        outside.setFillRule(Qt.OddEvenFill)
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
            painter = QPainter(device)
            painter.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
            painter.translate(0, -paint_top / dpr)
            strip = QRectF(0, paint_top / dpr, rect.width(), paint_height / dpr)
            painter.setClipRect(strip)
            origin = rect.topLeft() if index == 0 else QPointF()
            painter.translate(-origin)
            owner.render_widgets(painter, index == 0, QRegion(strip.translated(origin).toAlignedRect()))
            painter.translate(origin)
            painter.setCompositionMode(QPainter.CompositionMode_DestinationOut)
            painter.fillPath(outside, Qt.black)
            painter.end()
            gl.glDisable(0x0C11)
            # GLES requires matching rectangles/formats when resolving multisampling.
            QOpenGLFramebufferObject.blitFramebuffer(self.resolve_target, self.paint_target)
            QOpenGLFramebufferObject.blitFramebuffer(
                cache["texture"], QRect(0, pixels.height() - top - height, pixels.width(), height),
                self.resolve_target, QRect(0, paint_bottom - top - height, pixels.width(), height))
        cache["key"] = key
        return True

    def draw_texture(self, painter, index):
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
        self.blitter.blit(cache["texture"].texture(), cache["texture"].size(),
                          projection * QMatrix4x4(transform) * quad)
        painter.endNativePainting()

    def paintGL(self):
        gl = self.context().functions()
        gl.glDisable(0x0C11)  # GL_SCISSOR_TEST
        gl.glColorMask(True, True, True, True)
        gl.glClearColor(0, 0, 0, 0)
        gl.glClear(0x4000)  # GL_COLOR_BUFFER_BIT
        if self.property("presenting"):
            if not self.prepare_caches():
                if not self.cache_failure_pending:
                    self.cache_failure_pending = True
                    QTimer.singleShot(0, self.owner, self.owner.release_oversized_presentation)
                return
            if (not self.blitter.isCreated() or not self.update_texture(1)
                    or not self.update_texture(0)):
                QTimer.singleShot(0, self.owner, self.owner.rendering_failed)
                return
            gl.glBindFramebuffer(0x8D40, self.defaultFramebufferObject())
            gl.glViewport(0, 0, round(self.width() * self.devicePixelRatioF()),
                          round(self.height() * self.devicePixelRatioF()))
        painter = QPainter(self)
        if not self.owner.backdrop.isNull():
            painter.drawPixmap(0, 0, self.owner.backdrop)
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
        self.popup_positions = {}
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
        self.settings.spatialModeEnabledChanged.connect(self.apply_mode)
        self.settings.themeModeChanged.connect(self.refresh)
        self.settings.accentColorChanged.connect(self.refresh)
        self.settings.windowEffectChanged.connect(self.refresh)
        fluentqt.motion_policy().modeChanged.connect(self.refresh)
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
            _prepare_native_style()
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

    @Slot()
    def release_oversized_presentation(self):
        # Resource pressure can recover; keep the GPU available for another attempt.
        self.settings.set_spatial_mode_enabled(False)
        self.settle()

    @Slot()
    def rendering_failed(self):
        self.disable("3D rendering is unavailable. Using the 2D Gallery.")

    def disable(self, reason):
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
        for child in self.window.children():
            if (isinstance(child, QWidget) and child.isVisible() and not child.isWindow()
                    and self.is_native_overlay(child)):
                return child
        return None

    def native_overlay_at(self, global_pos):
        if not self.first_overlay():
            return False
        # Native hit testing respects masks and skips mouse-transparent dim-only scrims.
        hit = self.window.childAt(self.window.mapFromGlobal(global_pos))
        while hit and hit != self.window:
            if self.is_native_overlay(hit):
                return True
            hit = hit.parentWidget()
        return False

    def raise_presentation(self):
        overlay = self.first_overlay()
        if overlay:
            self.canvas.stackUnder(overlay)
        else:
            self.canvas.raise_()

    @Slot()
    def refresh(self, *args):
        self.backdrop_dirty = True
        if fluentqt.current_theme() == fluentqt.Theme.HighContrast:
            self.settings.set_spatial_mode_enabled(False)
        self.apply_mode(self.settings.spatial_mode_enabled)

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
        self.canvas.setProperty("galleryRotationAxis", "X" if self.top else "Y")
        self.canvas.setProperty("galleryNavigationRotation", (3. if self.top else 5.) * self.progress)
        self.canvas.setProperty("galleryContentRotation", -(3. if self.top else 5.) * self.progress)
        self.canvas.setProperty("galleryPointerTilt", self.pointer)
        self.canvas.setProperty("galleryDepthProgress", self.progress)
        self.canvas.update()

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
            material = QLinearGradient(body.topLeft(), body.bottomRight())
            opacity = (.32 if dark else .34) if index == 0 else (.62 if dark else .56)
            material.setColorAt(0, _alpha(colors.bgLayerAlt, opacity * self.progress))
            material.setColorAt(1, _alpha(colors.bgLayerAlt, (opacity - .12) * self.progress))
            painter.fillPath(path, material)
            self.canvas.draw_texture(painter, index)
            if self.progress > 0:
                rim = QLinearGradient(body.topLeft(), body.bottomRight())
                rim.setColorAt(0, _alpha(colors.grey10, (.18 if dark else .72) * self.progress))
                rim.setColorAt(.35, _alpha(colors.grey10, .12 * self.progress))
                rim.setColorAt(1, _alpha(colors.grey10, 0))
                painter.setBrush(Qt.NoBrush)
                painter.setPen(QPen(rim, 1))
                painter.drawPath(path)
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
        if self.forwarding or not _alive(self.canvas) or not _alive(self.window):
            return False
        kind = event.type()
        nav = self.navigation
        if watched == self.window and not self.renderer_ready and kind in (QEvent.Show, QEvent.UpdateRequest):
            self.queue_check()
        if not isinstance(watched, QWidget):
            return False
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
        if kind in (QEvent.Show, QEvent.Move) and isinstance(watched, fluentqt.Popup):
            # Native ComboBox flyouts are not instances of the Python Flyout facade.
            anchor = watched.anchor() if callable(getattr(watched, "anchor", None)) else None
            if anchor and nav.isAncestorOf(anchor) and watched.parentWidget() == self.window:
                if self.popup_positions.get(id(watched)) != watched.pos():
                    delta = self.projected_position(anchor, anchor.rect().center()) - anchor.mapTo(self.window, anchor.rect().center())
                    self.forwarding = True
                    watched.move(watched.pos() + delta)
                    self.popup_positions[id(watched)] = watched.pos()
                    self.forwarding = False
        outside_move = (not in_source and watched != self.canvas and kind == QEvent.MouseMove
                        and (watched != self.window or not nav.rect().contains(nav.mapFromGlobal(event.globalPosition().toPoint()))))
        if (watched == self.window and kind == QEvent.Leave) or outside_move:
            self.forwarding = True
            self.hover(None, QPoint())
            self.forwarding = False
            if not self.grabbed and not self.first_overlay():
                self.follow_pointer(QPointF())
        if watched == self.window and kind in (QEvent.WindowDeactivate, QEvent.Hide):
            self.settle()
        if in_source and watched.isWindow() and kind == QEvent.Show:
            # Popups keep their native input/focus; only their anchor is projected.
            source = nav.mapFromGlobal(watched.pos())
            panel = 1 if self.in_content(watched) else 0
            presented = self.panels[panel][1].map(QPointF(source)).toPoint()
            watched.move(nav.mapToGlobal(presented))
        if watched.window() != self.window.window() or (not in_source and watched != self.window):
            return False
        if in_source and kind in (QEvent.Enter, QEvent.Leave, QEvent.HoverEnter, QEvent.HoverLeave, QEvent.HoverMove):
            return True
        mouse = kind in (QEvent.MouseMove, QEvent.MouseButtonPress, QEvent.MouseButtonRelease, QEvent.MouseButtonDblClick)
        wheel = kind == QEvent.Wheel
        tooltip = kind == QEvent.ToolTip
        context = kind == QEvent.ContextMenu
        if not (mouse or wheel or tooltip or context):
            return False
        global_pos = event.globalPosition().toPoint() if mouse or wheel else event.globalPos()
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
