"""Gallery-only insertion of optional native particle layers into static panels.

Particle simulation and drawing remain in FluentQt. This module only preserves
QWidget paint order while composing their textures in the existing GL context.
Its contract mirrors GalleryParticleCompositor.cpp; 2D never imports it.
"""

import math
import struct

import fluentqt
from fluentqt._qt_compat import delete_qobject
from PySide6.QtCore import QEvent, QObject, QPoint, QPointF, QRect, QRectF, QSize, QSizeF, Qt, Signal, Slot
from PySide6.QtGui import QOpenGLContext, QPainter, QRegion, QSurfaceFormat, QVector2D, QVector4D
from PySide6.QtOpenGL import (
    QOpenGLBuffer, QOpenGLFramebufferObject, QOpenGLFramebufferObjectFormat,
    QOpenGLPaintDevice, QOpenGLShader, QOpenGLShaderProgram, QOpenGLVertexArrayObject,
)
from PySide6.QtWidgets import QApplication, QWidget
from shiboken6 import isValid

from .glyph_paint_device import GlyphPaintDevice, needs_native_glyph_coverage

_SOURCE_CHANGE_EVENTS = frozenset((
    QEvent.ChildAdded, QEvent.ChildRemoved, QEvent.Show, QEvent.Hide,
    QEvent.ParentChange, QEvent.Move, QEvent.Resize, QEvent.ZOrderChange, QEvent.LayoutRequest,
))


def _particle_layer_type():
    # This module itself is loaded only when the optional 3D surface is created.
    # Older or base-only bindings remain a working CPU path.
    try:
        from fluentqt.spatial import ParticleLayer
    except (ImportError, AttributeError):
        return None
    return ParticleLayer


def _alive(value):
    return value is not None and isValid(value)


def _unsupported_ancestors(widget, root):
    parent = widget.parentWidget()
    while parent is not None and parent != root:
        if not parent.mask().isEmpty() or parent.graphicsEffect() is not None:
            return True
        parent = parent.parentWidget()
    return False


def _clipped_rect(widget, root):
    if (not _alive(widget) or not _alive(root) or widget.isWindow()
            or not widget.isVisibleTo(root) or not root.isVisible()):
        return QRect()
    clip = QRect(widget.mapTo(root, QPoint()), widget.size())
    parent = widget.parentWidget()
    while parent is not None:
        if parent != root and (not parent.mask().isEmpty() or parent.graphicsEffect() is not None):
            return QRect()
        clip &= QRect(parent.mapTo(root, QPoint()), parent.size())
        if parent == root:
            return clip
        parent = parent.parentWidget()
    return QRect()


def _foreground_widgets(backdrop, root):
    if not _alive(backdrop) or not _alive(root):
        return []
    result = [child for child in backdrop.children()
              if isinstance(child, QWidget) and not child.isWindow() and not child.isHidden()]
    previous = backdrop
    parent = previous.parentWidget()
    while parent is not None:
        after = False
        for child in parent.children():
            if child == previous:
                after = True
                continue
            if after and isinstance(child, QWidget) and not child.isWindow() and not child.isHidden():
                result.append(child)
        if parent == root:
            break
        previous, parent = parent, parent.parentWidget()
    return result


def _physical_size(logical, dpr):
    return QSize(math.ceil(logical.width() * dpr), math.ceil(logical.height() * dpr))


def _texture_bytes(size):
    return size.width() * size.height() * 4


def _prepare_sampling(target):
    if not target.isValid():
        return
    context = QOpenGLContext.currentContext()
    gl = context.functions()
    gl.glBindTexture(0x0DE1, target.texture())
    for name, value in ((0x2801, 0x2601), (0x2800, 0x2601),
                        (0x2802, 0x812F), (0x2803, 0x812F)):
        gl.glTexParameteri(0x0DE1, name, value)
    gl.glBindTexture(0x0DE1, 0)


class _InsertionSampler:
    def __init__(self):
        self.program = QOpenGLShaderProgram()
        self.vertices = QOpenGLBuffer()
        self.vao = QOpenGLVertexArrayObject()

    def create(self):
        context = QOpenGLContext.currentContext()
        es = context.isOpenGLES()
        version_string = context.functions().glGetString(0x1F02)
        if isinstance(version_string, bytes):
            version_string = version_string.decode("ascii", errors="replace")
        version_string = str(version_string)
        es3 = (context.format().majorVersion() >= 3 or "OpenGL ES 3." in version_string
               or "WebGL 2." in version_string)
        modern = es3 if es else context.format().profile() == QSurfaceFormat.CoreProfile
        version = ("#version 300 es\n" if es else "#version 150\n") if modern else ""
        precision = "precision highp float;\n" if es else ""
        vertex = ("in vec2 position; out vec2 uv;\n" if modern else
                  "attribute vec2 position; varying vec2 uv;\n") + """
void main() { uv=(position+1.0)*0.5; gl_Position=vec4(position,0.0,1.0); }
"""
        fragment = ("in vec2 uv; out vec4 result;\n#define SAMPLE texture\n#define OUTPUT result\n"
                    if modern else "varying vec2 uv;\n#define SAMPLE texture2D\n#define OUTPUT gl_FragColor\n") + """
uniform sampler2D baseImage, particleImage, foregroundImage;
uniform vec2 panelSize;
uniform vec4 particleRect, clipRect;
void main() {
    vec4 b=SAMPLE(baseImage,uv);
    vec2 at=vec2(uv.x,1.0-uv.y)*panelSize;
    vec2 fp=(at-clipRect.xy)/clipRect.zw;
    if(all(greaterThanEqual(fp,vec2(0.0))) && all(lessThanEqual(fp,vec2(1.0)))) {
        vec2 pp=(at-particleRect.xy)/particleRect.zw;
        vec4 p=SAMPLE(particleImage,vec2(pp.x,1.0-pp.y));
        vec4 f=SAMPLE(foregroundImage,vec2(fp.x,1.0-fp.y));
        // b already contains f: insert particles without blending text twice.
        b=b+p*(1.0-f.a)-(b-f)*p.a;
    }
    OUTPUT=clamp(b,0.0,1.0);
}
"""
        if (not self.program.addShaderFromSourceCode(QOpenGLShader.Vertex, version + precision + vertex)
                or not self.program.addShaderFromSourceCode(QOpenGLShader.Fragment, version + precision + fragment)):
            return False
        self.program.bindAttributeLocation("position", 0)
        if not self.program.link() or not self.vertices.create():
            return False
        self.vao.create()
        binding = QOpenGLVertexArrayObject.Binder(self.vao)
        self.vertices.bind()
        data = struct.pack("8f", -1, -1, 1, -1, -1, 1, 1, 1)
        self.vertices.allocate(data, len(data))
        self.vertices.release()
        del binding
        return True

    def destroy(self):
        self.vertices.destroy()
        self.vao.destroy()
        self.program.removeAllShaders()

    def draw(self, base, particle, foreground, panel, rect, clip):
        context = QOpenGLContext.currentContext()
        gl = context.functions()
        binding = QOpenGLVertexArrayObject.Binder(self.vao)
        if not self.program.bind():
            del binding
            return False
        self.vertices.bind()
        self.program.enableAttributeArray(0)
        self.program.setAttributeBuffer(0, 0x1406, 0, 2)
        for unit, name in enumerate(("baseImage", "particleImage", "foregroundImage")):
            # PySide 6.2 may resolve setUniformValue(int) to the float overload.
            gl.glUniform1i(self.program.uniformLocation(name), unit)
        self.program.setUniformValue("panelSize", QVector2D(panel.width(), panel.height()))
        self.program.setUniformValue("particleRect", QVector4D(rect.x(), rect.y(), rect.width(), rect.height()))
        self.program.setUniformValue("clipRect", QVector4D(clip.x(), clip.y(), clip.width(), clip.height()))
        for unit, texture in enumerate((base, particle, foreground)):
            gl.glActiveTexture(0x84C0 + unit)
            gl.glBindTexture(0x0DE1, texture)
        for capability in (0x0BE2, 0x0B71, 0x0B90, 0x0C11):
            gl.glDisable(capability)
        gl.glColorMask(True, True, True, True)
        gl.glDrawArrays(0x0005, 0, 4)
        for unit in (2, 1, 0):
            gl.glActiveTexture(0x84C0 + unit)
            gl.glBindTexture(0x0DE1, 0)
        self.program.disableAttributeArray(0)
        self.vertices.release()
        self.program.release()
        del binding
        return gl.glGetError() == 0


class GalleryParticleCompositor(QObject):
    frameRequested = Signal()
    staticContentInvalidated = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.root = self.context = None
        self.candidates = []
        self.sources, self.source_rects, self.source_clips, self.layers = [], [], [], []
        self.sampler = None
        self.composed = []
        self.panel_pixels = QSize()
        self.native_dpr = self.cache_dpr = 0.
        self.scan_revision = self.static_revision = self.last_frames = 0
        self.captures = self.compositions = self.scans = self.bytes = self.maximum_bytes = 0
        self.result = 0
        self.native_required = 0
        self.releasing = self.scanned = self.validated = self.failed = False

    @Slot(bool)
    def _active_changed(self, _active):
        if not self.releasing:
            self.staticContentInvalidated.emit()

    def eventFilter(self, watched, event):
        if event.type() not in _SOURCE_CHANGE_EVENTS:
            return False
        if (_alive(self.root) and isinstance(watched, QWidget)
                and (watched == self.root or self.root.isAncestorOf(watched))):
            self.scanned = self.validated = False
        return False

    def requiredBytes(self, content, revision, panel_pixels, native_dpr, cache_dpr, enabled):
        context = QOpenGLContext.currentContext()
        layer_type = _particle_layer_type()
        if (not enabled or not _alive(content) or not _alive(context) or layer_type is None
                or panel_pixels.isEmpty() or not math.isfinite(native_dpr) or native_dpr <= 0
                or not math.isfinite(cache_dpr) or cache_dpr <= 0):
            self.release()
            return 0
        if self.root != content or self.context != context or self.native_dpr != native_dpr:
            self.release()
            self.root, self.context = content, context
            self.native_dpr = native_dpr
            QApplication.instance().installEventFilter(self)
        if not self.scanned:
            self.scans += 1
            self.candidates = []

            def visit(parent):
                for child in parent.children():
                    if not isinstance(child, QWidget) or child.isWindow() or child.isHidden():
                        continue
                    if isinstance(child, fluentqt.ParticleBackdrop):
                        self.candidates.append(child)
                    visit(child)

            visit(content)
            self.scanned, self.validated = True, False
        if not self.validated or self.scan_revision != revision:
            visible, rects, clips = [], [], []
            for backdrop in self.candidates:
                if not _alive(backdrop):
                    continue
                clip = _clipped_rect(backdrop, content)
                supported = (backdrop.mask().isEmpty() and backdrop.graphicsEffect() is None
                             and not clip.isEmpty())
                if supported:
                    supported = all(widget.graphicsEffect() is None and widget.mask().isEmpty()
                                    and not _unsupported_ancestors(widget, content)
                                    for widget in _foreground_widgets(backdrop, content))
                if supported:
                    visible.append(backdrop)
                    rects.append(QRect(backdrop.mapTo(content, QPoint()), backdrop.size()))
                    clips.append(clip)
            if visible != self.sources or rects != self.source_rects or clips != self.source_clips:
                self._release_gpu()
                self.sources, self.source_rects, self.source_clips = visible, rects, clips
                self.native_required = 0
                self.failed = False
            self.scan_revision, self.validated = revision, True
        if not self.sources:
            return 0
        if not self.native_required:
            for rect in self.source_rects:
                estimated = layer_type.estimatedBytes(rect.size(), native_dpr)
                if not estimated:
                    self.native_required = (1 << 64) - 1
                    break
                self.native_required += estimated
        return (self.native_required + _texture_bytes(panel_pixels) * (2 if len(self.sources) > 1 else 1)
                + sum(_texture_bytes(_physical_size(clip.size(), cache_dpr)) for clip in self.source_clips))

    def prepare(self, content, revision, panel_pixels, native_dpr, cache_dpr, maximum_bytes, enabled):
        required = self.requiredBytes(content, revision, panel_pixels, native_dpr, cache_dpr, enabled)
        if not required:
            return False
        # Estimating other levels leaves this plan's allocated GPU targets intact.
        if self.panel_pixels != panel_pixels or self.cache_dpr != cache_dpr:
            self._release_gpu()
            self.panel_pixels, self.cache_dpr = QSize(panel_pixels), cache_dpr
            self.failed = False
        if self.failed and self.maximum_bytes == maximum_bytes:
            return False
        self.maximum_bytes = maximum_bytes
        if required > maximum_bytes:
            return self._fail()
        if not self.layers:
            layer_type = _particle_layer_type()
            for source, rect, clip in zip(self.sources, self.source_rects, self.source_clips):
                if not _alive(source):
                    return self._fail()
                previous_acceleration = source.isGpuAccelerationEnabled()
                source.setGpuAccelerationEnabled(True)
                renderer = layer_type(source, self)
                renderer.frameRequested.connect(self.frameRequested.emit)
                renderer.activeChanged.connect(self._active_changed)
                self.layers.append({"renderer": renderer, "rect": rect, "clip": clip,
                                    "foreground": None, "revision": None,
                                    "previous_acceleration": previous_acceleration})
            self.result = 0
        outputs = 2 if len(self.layers) > 1 else 1
        static_bytes = _texture_bytes(panel_pixels) * outputs
        static_bytes += sum(_texture_bytes(_physical_size(layer["clip"].size(), cache_dpr))
                            for layer in self.layers)
        if static_bytes > maximum_bytes:
            return self._fail()
        if self.sampler is None:
            self.sampler = _InsertionSampler()
            if not self.sampler.create():
                return self._fail()
        if not self.composed:
            texture_format = QOpenGLFramebufferObjectFormat()
            texture_format.setInternalTextureFormat(0x8058)
            self.composed = [QOpenGLFramebufferObject(panel_pixels, texture_format)
                             for _ in range(outputs)]
            for target in self.composed:
                _prepare_sampling(target)
            for layer in self.layers:
                layer["foreground"] = QOpenGLFramebufferObject(
                    _physical_size(layer["clip"].size(), cache_dpr), texture_format)
                _prepare_sampling(layer["foreground"])
        if (any(not target.isValid() for target in self.composed)
                or any(not layer["foreground"].isValid() for layer in self.layers)):
            return self._fail()
        allocated = static_bytes
        for layer in self.layers:
            renderer = layer["renderer"]
            # Give each renderer only the remaining aggregate allowance. Its
            # native sample-count probe checks the real driver allocation first.
            if not renderer.render(native_dpr, maximum_bytes - allocated):
                return self._fail()
            allocated += renderer.allocatedBytes()
            if allocated > maximum_bytes:
                return self._fail()
        self.bytes = allocated
        return True

    def compose(self, base, revision, paint_target, resolve_target, render_widget):
        if not _alive(self.root) or not self.layers or not base:
            return base
        context = QOpenGLContext.currentContext()
        if (context is None or context != self.context or paint_target is None or resolve_target is None
                or not paint_target.isValid() or not resolve_target.isValid()
                or paint_target.size() != resolve_target.size()):
            self.release()
            return base
        frames = self.particleFrameCount()
        if self.result and self.static_revision == revision and self.last_frames == frames:
            return self.result
        gl = context.functions()
        for layer in self.layers:
            if layer["revision"] == revision:
                continue
            pixels = layer["foreground"].size()
            if pixels.width() > paint_target.width():
                self._fail()
                return base
            guard = max(1, math.ceil(self.cache_dpr))
            stride = (pixels.height() if pixels.height() <= paint_target.height()
                      else max(1, paint_target.height() - 2 * guard))
            foreground = _foreground_widgets(layer["renderer"].backdrop(), self.root)
            for top in range(0, pixels.height(), stride):
                height = min(stride, pixels.height() - top)
                paint_top, paint_bottom = max(0, top - guard), min(pixels.height(), top + height + guard)
                paint_height = paint_bottom - paint_top
                paint_target.bind()
                gl.glDisable(0x0C11)
                gl.glColorMask(True, True, True, True)
                gl.glStencilMask(0xFFFFFFFF)
                gl.glClearColor(0, 0, 0, 0)
                gl.glClear(0x4000 | 0x0400)
                device = QOpenGLPaintDevice(QSize(pixels.width(), paint_height))
                device.setDevicePixelRatio(self.cache_dpr)
                origin = QPointF(layer["clip"].topLeft()) + QPointF(0, paint_top / self.cache_dpr)
                glyph_device = GlyphPaintDevice(device, self.native_dpr, origin)
                painter = QPainter(glyph_device if needs_native_glyph_coverage(self.native_dpr) else device)
                painter.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
                painter.translate(-origin)
                strip = QRectF(origin, QSizeF(layer["clip"].width(), paint_height / self.cache_dpr))
                painter.setClipRect(strip)
                try:
                    for widget in foreground:
                        clip = _clipped_rect(widget, self.root) & layer["clip"] & strip.toAlignedRect()
                        if clip.isEmpty():
                            continue
                        widget_origin = widget.mapTo(self.root, QPoint())
                        painter.save()
                        try:
                            painter.setClipRect(clip, Qt.IntersectClip)
                            painter.translate(widget_origin)
                            render_widget(painter, widget, QRegion(clip.translated(-widget_origin)))
                        finally:
                            painter.restore()
                finally:
                    painter.end()
                gl.glDisable(0x0C11)
                QOpenGLFramebufferObject.blitFramebuffer(resolve_target, paint_target)
                QOpenGLFramebufferObject.blitFramebuffer(
                    layer["foreground"], QRect(0, pixels.height() - top - height, pixels.width(), height),
                    resolve_target, QRect(0, paint_bottom - top - height, pixels.width(), height))
            if gl.glGetError():
                self._fail()
                return base
            layer["revision"] = revision
            self.captures += 1
        result = base
        for index, layer in enumerate(self.layers):
            output = self.composed[index % 2]
            output.bind()
            gl.glViewport(0, 0, self.panel_pixels.width(), self.panel_pixels.height())
            if not self.sampler.draw(result, layer["renderer"].textureId(), layer["foreground"].texture(),
                                     self.root.size(), layer["rect"], layer["clip"]):
                self._fail()
                return base
            result = output.texture()
        self.result, self.static_revision, self.last_frames = result, revision, frames
        self.compositions += 1
        return result

    def _fail(self):
        self._release_gpu()
        self.failed = True
        return False

    def _release_gpu(self):
        active = self.activeLayerCount() != 0
        self.releasing = True
        for layer in self.layers:
            renderer = layer["renderer"]
            if _alive(renderer):
                source = renderer.backdrop()
                renderer.release()
                delete_qobject(renderer)
                if _alive(source):
                    source.setGpuAccelerationEnabled(layer["previous_acceleration"])
            layer["foreground"] = None
        self.layers.clear()
        self.composed.clear()
        if self.sampler is not None:
            self.sampler.destroy()
            self.sampler = None
        self.static_revision = self.last_frames = self.bytes = self.result = 0
        self.releasing = False
        if active:
            self.staticContentInvalidated.emit()

    def release(self):
        app = QApplication.instance()
        if app is not None:
            app.removeEventFilter(self)
        self.root = self.context = None
        self.candidates = []
        self.sources, self.source_rects, self.source_clips = [], [], []
        self.scan_revision = 0
        self.native_required = 0
        self.scanned = self.validated = self.failed = False
        self._release_gpu()

    def activeLayerCount(self):
        return sum(bool(layer["renderer"].isActive()) for layer in self.layers
                   if _alive(layer["renderer"]))

    def allocatedBytes(self):
        return self.bytes

    def particleFrameCount(self):
        return sum(layer["renderer"].renderedFrameCount() for layer in self.layers
                   if _alive(layer["renderer"]))

    def foregroundCaptureCount(self):
        return self.captures

    def compositionCount(self):
        return self.compositions

    def sourceScanCount(self):
        return self.scans
