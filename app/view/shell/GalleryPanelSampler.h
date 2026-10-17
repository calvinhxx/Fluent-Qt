#ifndef GALLERYPANELSAMPLER_H
#define GALLERYPANELSAMPLER_H

#include <QColor>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QSizeF>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector4D>

namespace fluent::gallery::spatial_render {

// Reconstruct nearby texels with bounded cubic interpolation. Clamp to the four
// central texels to prevent ringing, and retain footprint filtering when shrinking.
// zh_CN: 三次插值重建邻近像素，并限制到中心四个像素的范围以避免光晕；缩小时保留足迹滤波。
class PanelSampler {
public:
    struct Appearance {
        QSizeF size;
        qreal radius = 0;
        qreal progress = 0;
        QColor material, reflection;
        qreal opacity = 0;
        qreal reflectionOpacity = 0;
    };
    // Previous filters remain available only for same-context regression comparisons.
    enum class Reconstruction { BoundedCubic, Monotone, LinearFootprint };
    bool create(Reconstruction reconstruction = Reconstruction::BoundedCubic)
    {
        auto* context = QOpenGLContext::currentContext();
        const bool es = context->isOpenGLES();
        // A Qt WebAssembly sharing wrapper can still report the requested ES 2
        // format while its current browser context is already WebGL 2 / ES 3.
        const QByteArray glVersion(reinterpret_cast<const char*>(context->functions()->glGetString(GL_VERSION)));
        const bool es3 = context->format().majorVersion() >= 3 || glVersion.startsWith("OpenGL ES 3.")
            || glVersion.contains("WebGL 2.");
        const bool modern = es ? es3 : context->format().profile() == QSurfaceFormat::CoreProfile;
        const QByteArray version = modern
            ? (es ? "#version 300 es\n" : "#version 150\n")
            : (es ? "#extension GL_OES_standard_derivatives : enable\n" : "");
        const QByteArray precision = es ? "precision highp float;\n" : "";
        const QByteArray vertex =
            (modern ? "in vec2 position; out vec2 uv;\n" : "attribute highp vec2 position; varying highp vec2 uv;\n")
            + QByteArray(
                "uniform mat4 target; void main() {\n"
                "uv = (position + 1.0) * 0.5;\n"
                "gl_Position = target * vec4(position, 0.0, 1.0); }\n");
        const QByteArray reconstructionCode = reconstruction != Reconstruction::LinearFootprint
            ? "vec2 texel = coordinate * sourceSize - 0.5;\n"
              "vec2 phase = fract(texel);\n"
              "phase = phase * phase * (3.0 - 2.0 * phase);\n"
              "coordinate = (floor(texel) + phase + 0.5) / sourceSize;\n"
            : "";
        const QByteArray boundedCubic = reconstruction == Reconstruction::BoundedCubic
            ? R"(
vec4 cubicWeights(float t) {
    float t2 = t * t, t3 = t2 * t;
    return vec4(-0.5*t + t2 - 0.5*t3, 1.0 - 2.5*t2 + 1.5*t3,
                0.5*t + 2.0*t2 - 1.5*t3, -0.5*t2 + 0.5*t3);
}
vec4 sampleBoundedCubic(vec2 coordinate) {
    vec2 texel = coordinate * sourceSize - 0.5;
    vec2 cell = floor(texel), phase = fract(texel);
    vec4 wx = cubicWeights(phase.x), wy = cubicWeights(phase.y);
    vec4 value = vec4(0.0), low = vec4(1.0), high = vec4(0.0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec4 pixel = SAMPLE(source, (cell + vec2(float(x)-0.5, float(y)-0.5)) / sourceSize);
            value += pixel * wx[x] * wy[y];
            if (x > 0 && x < 3 && y > 0 && y < 3) {
                low = min(low, pixel); high = max(high, pixel);
            }
        }
    }
    value = clamp(value, low, high);
    value.rgb = min(value.rgb, value.aaa);
    return value;
}
)"
            : "";
        const QByteArray pixelSampling = reconstruction == Reconstruction::BoundedCubic
            ? R"(
    float filtering = smoothstep(1.75, 2.5, max(footprint.x, footprint.y));
    vec4 pixel;
    if (filtering < 1.0)
        pixel = sampleBoundedCubic(uv);
    if (filtering > 0.0) {
        vec4 filtered = sampleFootprint(uv, dx, dy);
        pixel = filtering >= 1.0 ? filtered : mix(pixel, filtered, filtering);
    }
)"
            : "vec4 pixel = sampleFootprint(uv, dx, dy);\n";
        const QByteArray fragment =
            (modern ? "in vec2 uv; out vec4 color;\n#define SAMPLE texture\n#define OUTPUT color\n"
                    : "varying highp vec2 uv;\n#define SAMPLE texture2D\n#define OUTPUT "
                      "gl_FragColor\n")
            + QByteArray(
                "uniform sampler2D source; uniform vec2 sourceSize;\n"
                "uniform vec2 panelSize; uniform vec4 panelShape;\n"
                "uniform vec4 materialColor; uniform vec4 reflectionColor;\n"
                "vec4 samplePanel(vec2 coordinate) {\n")
            + reconstructionCode + QByteArray("return SAMPLE(source, coordinate); }\n") + boundedCubic
            + QByteArray(
                "vec4 sampleFootprint(vec2 coordinate, vec2 dx, vec2 dy) {\n"
                "return 0.25 * (samplePanel(coordinate - dx - dy) + samplePanel(coordinate + dx - "
                "dy)\n"
                " + samplePanel(coordinate - dx + dy) + samplePanel(coordinate + dx + dy)); }\n"
                "void main() {\n"
                "vec2 dx = dFdx(uv), dy = dFdy(uv);\n"
                "vec2 footprint = vec2(length(dx * sourceSize), length(dy * sourceSize));\n"
                "vec2 span = clamp(footprint - 1.0, 0.0, 1.0);\n"
                "dx *= 0.25 * span.x; dy *= 0.25 * span.y;\n")
            + pixelSampling
            + QByteArray(
                "if (panelSize.x > 0.0) {\n"
                "vec2 point = vec2(uv.x, 1.0 - uv.y) * panelSize;\n"
                "vec2 halfSize = max(vec2(0.0), (panelSize - 1.0) * 0.5);\n"
                "float radius = min(panelShape.x, min(halfSize.x, halfSize.y));\n"
                "vec2 q = abs(point - panelSize * 0.5) - halfSize + radius;\n"
                "float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;\n"
                "float aa = max(0.0001, 0.5 * fwidth(distance));\n"
                "float coverage = 1.0 - smoothstep(-aa, aa, distance);\n"
                "vec2 diagonal = max(vec2(1.0), panelSize - 1.0);\n"
                "float t = clamp(dot(point - 0.5, diagonal) / dot(diagonal, diagonal), 0.0, 1.0);\n"
                "float alpha = (materialColor.a - 0.12 * t) * panelShape.y;\n"
                "pixel = (pixel + vec4(materialColor.rgb * alpha, alpha) * (1.0 - pixel.a)) * "
                "coverage;\n"
                "float rim = 1.0 - smoothstep(0.5 - aa, 0.5 + aa, abs(distance));\n"
                "float rimAlpha = (t < 0.35 ? mix(reflectionColor.a, 0.12, t / 0.35)\n"
                " : mix(0.12, 0.0, (t - 0.35) / 0.65)) * panelShape.y * rim;\n"
                "pixel = vec4(reflectionColor.rgb * rimAlpha, rimAlpha) + pixel * (1.0 - "
                "rimAlpha);\n"
                "}\nOUTPUT = pixel; }\n");
        if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, version + precision + vertex)
            || !m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, version + precision + fragment))
            return false;
        m_program.bindAttributeLocation("position", 0);
        if (!m_program.link() || !m_vertices.create())
            return false;
        m_vao.create();
        QOpenGLVertexArrayObject::Binder vao(&m_vao);
        m_vertices.bind();
        const GLfloat vertices[] = {-1, -1, 1, -1, -1, 1, 1, 1};
        m_vertices.allocate(vertices, sizeof(vertices));
        m_vertices.release();
        return true;
    }

    bool isCreated() const { return m_program.isLinked() && m_vertices.isCreated(); }
    void blit(GLuint texture, const QSize& size, const QMatrix4x4& target, const Appearance* appearance = nullptr)
    {
        auto* gl = QOpenGLContext::currentContext()->functions();
        QOpenGLVertexArrayObject::Binder vao(&m_vao);
        m_program.bind();
        m_vertices.bind();
        m_program.enableAttributeArray(0);
        m_program.setAttributeBuffer(0, GL_FLOAT, 0, 2);
        m_program.setUniformValue("target", target);
        m_program.setUniformValue("source", 0);
        m_program.setUniformValue("sourceSize", QVector2D(size.width(), size.height()));
        m_program.setUniformValue("panelSize",
            appearance ? QVector2D(appearance->size.width(), appearance->size.height()) : QVector2D());
        if (appearance) {
            m_program.setUniformValue("panelShape", QVector4D(appearance->radius, appearance->progress, 0, 0));
            const QColor material = appearance->material;
            const QColor reflection = appearance->reflection;
            m_program.setUniformValue("materialColor",
                QVector4D(material.redF(), material.greenF(), material.blueF(), appearance->opacity));
            m_program.setUniformValue("reflectionColor",
                QVector4D(reflection.redF(), reflection.greenF(), reflection.blueF(), appearance->reflectionOpacity));
        }
        gl->glActiveTexture(GL_TEXTURE0);
        gl->glBindTexture(GL_TEXTURE_2D, texture);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        gl->glBindTexture(GL_TEXTURE_2D, 0);
        m_program.disableAttributeArray(0);
        m_vertices.release();
        m_program.release();
    }

private:
    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vertices;
    QOpenGLVertexArrayObject m_vao;
};

} // namespace fluent::gallery::spatial_render
#endif
