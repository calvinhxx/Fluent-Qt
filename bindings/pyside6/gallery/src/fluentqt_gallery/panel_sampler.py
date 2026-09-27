"""Private panel resampler; shared production and native A/B test implementation.

Keep the four footprint taps, cache size and premultiplied-alpha contract. The
monotone cubic phase has no negative weights or extra texture reads.
"""

import struct

from PySide6.QtGui import QOpenGLContext, QSurfaceFormat, QVector2D, QVector4D
from PySide6.QtOpenGL import QOpenGLBuffer, QOpenGLShader, QOpenGLShaderProgram, QOpenGLVertexArrayObject


class _PanelSampler:
    """Four footprint taps with bounded, monotone in-texel reconstruction."""

    def __init__(self):
        self.program = QOpenGLShaderProgram()
        self.vertices = QOpenGLBuffer()
        self.vao = QOpenGLVertexArrayObject()

    def create(self, *, linear_reference=False):
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
        # The explicit legacy mode is a same-context regression oracle, not a setting.
        reconstruction = "" if linear_reference else """
    vec2 texel = coordinate * sourceSize - 0.5;
    vec2 phase = fract(texel);
    phase = phase * phase * (3.0 - 2.0 * phase);
    coordinate = (floor(texel) + phase + 0.5) / sourceSize;
"""
        fragment = ("in vec2 uv; out vec4 color;\n#define SAMPLE texture\n#define OUTPUT color\n"
                    if modern else "varying highp vec2 uv;\n#define SAMPLE texture2D\n#define OUTPUT gl_FragColor\n") + """
uniform sampler2D source;
uniform vec2 sourceSize;
uniform vec2 panelSize;
uniform vec4 panelShape;
uniform vec4 materialColor;
uniform vec4 reflectionColor;
vec4 samplePanel(vec2 coordinate) {
""" + reconstruction + """
    return SAMPLE(source, coordinate);
}
void main() {
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec2 span = clamp(vec2(length(dx * sourceSize), length(dy * sourceSize)) - 1.0, 0.0, 1.0);
    dx *= 0.25 * span.x; dy *= 0.25 * span.y;
    vec4 pixel = 0.25 * (samplePanel(uv - dx - dy) + samplePanel(uv + dx - dy)
                      + samplePanel(uv - dx + dy) + samplePanel(uv + dx + dy));
    if (panelSize.x > 0.0) {
        vec2 point = vec2(uv.x, 1.0 - uv.y) * panelSize;
        vec2 halfSize = max(vec2(0.0), (panelSize - 1.0) * 0.5);
        float radius = min(panelShape.x, min(halfSize.x, halfSize.y));
        vec2 q = abs(point - panelSize * 0.5) - halfSize + radius;
        float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
        float aa = max(0.0001, 0.5 * fwidth(distance));
        float coverage = 1.0 - smoothstep(-aa, aa, distance);
        vec2 diagonal = max(vec2(1.0), panelSize - 1.0);
        float t = clamp(dot(point - 0.5, diagonal) / dot(diagonal, diagonal), 0.0, 1.0);
        float alpha = (materialColor.a - 0.12 * t) * panelShape.y;
        pixel = (pixel + vec4(materialColor.rgb * alpha, alpha) * (1.0 - pixel.a)) * coverage;
        float rim = 1.0 - smoothstep(0.5 - aa, 0.5 + aa, abs(distance));
        float rimAlpha = (t < 0.35 ? mix(reflectionColor.a, 0.12, t / 0.35)
                         : mix(0.12, 0.0, (t - 0.35) / 0.65)) * panelShape.y * rim;
        pixel = vec4(reflectionColor.rgb * rimAlpha, rimAlpha) + pixel * (1.0 - rimAlpha);
    }
    OUTPUT = pixel;
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

    def blit(self, texture, size, target, appearance=None):
        gl = QOpenGLContext.currentContext().functions()
        vao = QOpenGLVertexArrayObject.Binder(self.vao)
        self.program.bind()
        self.vertices.bind()
        self.program.enableAttributeArray(0)
        self.program.setAttributeBuffer(0, 0x1406, 0, 2)
        self.program.setUniformValue("target", target)
        # PySide may select the floating-point overload for a Python int.
        # Sampler uniforms require glUniform1i; GL rejects the float overload.
        gl.glUniform1i(self.program.uniformLocation("source"), 0)
        self.program.setUniformValue("sourceSize", QVector2D(size.width(), size.height()))
        self.program.setUniformValue("panelSize", QVector2D(appearance[0].width(), appearance[0].height())
                                     if appearance else QVector2D())
        if appearance:
            _, radius, progress, material, reflection, opacity, reflection_opacity = appearance
            self.program.setUniformValue("panelShape", QVector4D(radius, progress, 0., 0.))
            self.program.setUniformValue("materialColor", QVector4D(
                material.redF(), material.greenF(), material.blueF(), opacity))
            self.program.setUniformValue("reflectionColor", QVector4D(
                reflection.redF(), reflection.greenF(), reflection.blueF(), reflection_opacity))
        gl.glActiveTexture(0x84C0)
        gl.glBindTexture(0x0DE1, texture)
        gl.glDrawArrays(0x0005, 0, 4)
        gl.glBindTexture(0x0DE1, 0)
        self.program.disableAttributeArray(0)
        self.vertices.release()
        self.program.release()
        del vao
