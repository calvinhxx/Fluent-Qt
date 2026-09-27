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

// Four footprint taps retain coverage during minification. Monotone reconstruction
// avoids blurring already-antialiased texels again with a linear fractional weight.
// All weights remain in [0, 1]: no negative-lobe sharpening, overshoot or extra taps.
// zh_CN: 四点像素足迹采样保持缩小后的覆盖率；单调重建减少重复线性插值的软化，
// 不使用负权重锐化，不产生过冲，也不增加纹理读取次数。
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
    // LinearFootprint is retained only as the same-context regression reference.
    enum class Reconstruction { Monotone, LinearFootprint };
    bool create(Reconstruction reconstruction = Reconstruction::Monotone)
    {
        auto* context = QOpenGLContext::currentContext();
        const bool es = context->isOpenGLES();
        // A Qt WebAssembly sharing wrapper can still report the requested ES 2
        // format while its current browser context is already WebGL 2 / ES 3.
        const QByteArray glVersion(
            reinterpret_cast<const char*>(context->functions()->glGetString(GL_VERSION)));
        const bool es3 = context->format().majorVersion() >= 3 ||
                         glVersion.startsWith("OpenGL ES 3.") || glVersion.contains("WebGL 2.");
        const bool modern = es ? es3 : context->format().profile() == QSurfaceFormat::CoreProfile;
        const QByteArray version =
            modern ? (es ? "#version 300 es\n" : "#version 150\n")
                   : (es ? "#extension GL_OES_standard_derivatives : enable\n" : "");
        const QByteArray precision = es ? "precision highp float;\n" : "";
        const QByteArray vertex =
            (modern ? "in vec2 position; out vec2 uv;\n"
                    : "attribute highp vec2 position; varying highp vec2 uv;\n") +
            QByteArray("uniform mat4 target; void main() {\n"
                       "uv = (position + 1.0) * 0.5;\n"
                       "gl_Position = target * vec4(position, 0.0, 1.0); }\n");
        const QByteArray reconstructionCode =
            reconstruction == Reconstruction::Monotone
                ? "vec2 texel = coordinate * sourceSize - 0.5;\n"
                  "vec2 phase = fract(texel);\n"
                  "phase = phase * phase * (3.0 - 2.0 * phase);\n"
                  "coordinate = (floor(texel) + phase + 0.5) / sourceSize;\n"
                : "";
        const QByteArray fragment =
            (modern ? "in vec2 uv; out vec4 color;\n#define SAMPLE texture\n#define OUTPUT color\n"
                    : "varying highp vec2 uv;\n#define SAMPLE texture2D\n#define OUTPUT "
                      "gl_FragColor\n") +
            QByteArray("uniform sampler2D source; uniform vec2 sourceSize;\n"
                       "uniform vec2 panelSize; uniform vec4 panelShape;\n"
                       "uniform vec4 materialColor; uniform vec4 reflectionColor;\n"
                       "vec4 samplePanel(vec2 coordinate) {\n") +
            reconstructionCode +
            QByteArray(
                "return SAMPLE(source, coordinate); }\n"
                "void main() {\n"
                "vec2 dx = dFdx(uv), dy = dFdy(uv);\n"
                "vec2 span = clamp(vec2(length(dx * sourceSize), length(dy * sourceSize)) - 1.0, "
                "0.0, 1.0);\n"
                "dx *= 0.25 * span.x; dy *= 0.25 * span.y;\n"
                "vec4 pixel = 0.25 * (samplePanel(uv - dx - dy) + samplePanel(uv + dx - dy)\n"
                " + samplePanel(uv - dx + dy) + samplePanel(uv + dx + dy));\n"
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
        if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               version + precision + vertex) ||
            !m_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                               version + precision + fragment))
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
    void blit(GLuint texture, const QSize& size, const QMatrix4x4& target,
              const Appearance* appearance = nullptr)
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
        m_program.setUniformValue(
            "panelSize", appearance ? QVector2D(appearance->size.width(), appearance->size.height())
                                    : QVector2D());
        if (appearance) {
            m_program.setUniformValue("panelShape",
                                      QVector4D(appearance->radius, appearance->progress, 0, 0));
            const QColor material = appearance->material;
            const QColor reflection = appearance->reflection;
            m_program.setUniformValue("materialColor",
                                      QVector4D(material.redF(), material.greenF(),
                                                material.blueF(), appearance->opacity));
            m_program.setUniformValue("reflectionColor",
                                      QVector4D(reflection.redF(), reflection.greenF(),
                                                reflection.blueF(), appearance->reflectionOpacity));
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
