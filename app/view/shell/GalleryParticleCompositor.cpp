#include "GalleryParticleCompositor.h"

#ifdef FLUENT_QT_HAS_SPATIAL
#include "GalleryGlyphPaintDevice.h"
#include "components/layout/ParticleBackdrop.h"
#include "components/spatial/ParticleLayer.h"

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QPainter>
#include <QPointer>
#include <QVector4D>
#include <QVector2D>
#include <QWidget>
#include <QtMath>
#include <array>
#include <vector>

namespace fluent::gallery::spatial_render {
namespace {
using layout::ParticleBackdrop;
using spatial::ParticleLayer;

bool unsupportedAncestors(QWidget* widget, QWidget* root)
{
    for (QWidget* parent = widget->parentWidget(); parent && parent != root;
         parent = parent->parentWidget())
        if (!parent->mask().isEmpty() || parent->graphicsEffect())
            return true;
    return false;
}

QRect clippedRect(QWidget* widget, QWidget* root)
{
    if (!widget || !root || widget->isWindow() || !widget->isVisibleTo(root) || !root->isVisible())
        return {};
    QRect clip(widget->mapTo(root, QPoint()), widget->size());
    for (QWidget* parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (parent != root && (!parent->mask().isEmpty() || parent->graphicsEffect()))
            return {}; // Preserve unrelated nonrectangular/effect composition on CPU.
        clip &= QRect(parent->mapTo(root, QPoint()), parent->size());
        if (parent == root)
            return clip;
    }
    return {};
}

QList<QWidget*> foregroundWidgets(ParticleBackdrop* backdrop, QWidget* root)
{
    QList<QWidget*> result;
    for (auto* child : backdrop->children())
        if (auto* widget = qobject_cast<QWidget*>(child);
            widget && !widget->isWindow() && !widget->isHidden())
            result.append(widget);
    QWidget* previous = backdrop;
    for (QWidget* parent = previous->parentWidget(); parent; parent = parent->parentWidget()) {
        bool after = false;
        for (auto* child : parent->children()) {
            if (child == previous) {
                after = true;
                continue;
            }
            if (auto* widget = qobject_cast<QWidget*>(child);
                after && widget && !widget->isWindow() && !widget->isHidden())
                result.append(widget);
        }
        if (parent == root)
            break;
        previous = parent;
    }
    return result;
}

QSize physicalSize(const QSize& logical, qreal dpr)
{
    return QSize(qCeil(logical.width() * dpr), qCeil(logical.height() * dpr));
}

void prepareSampling(QOpenGLFramebufferObject* target)
{
    if (!target || !target->isValid())
        return;
    auto* gl = QOpenGLContext::currentContext()->functions();
    gl->glBindTexture(GL_TEXTURE_2D, target->texture());
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
}

class InsertionSampler {
public:
    bool create()
    {
        auto* context = QOpenGLContext::currentContext();
        const bool es = context->isOpenGLES();
        const QByteArray glVersion(
            reinterpret_cast<const char*>(context->functions()->glGetString(GL_VERSION)));
        const bool es3 = context->format().majorVersion() >= 3 ||
                         glVersion.contains("OpenGL ES 3.") || glVersion.contains("WebGL 2.");
        const bool modern = es ? es3 : context->format().profile() == QSurfaceFormat::CoreProfile;
        const QByteArray version = modern ? (es ? "#version 300 es\n" : "#version 150\n") : "";
        const QByteArray precision = es ? "precision highp float;\n" : "";
        const QByteArray vertex =
            QByteArray(modern ? "in vec2 position; out vec2 uv;\n"
                              : "attribute vec2 position; varying vec2 uv;\n") +
            "void main(){ uv=(position+1.0)*0.5; gl_Position=vec4(position,0.0,1.0); }\n";
        const QByteArray fragment =
            QByteArray(modern ? "in vec2 uv; out vec4 result;\n#define SAMPLE texture\n"
                                "#define OUTPUT result\n"
                              : "varying vec2 uv;\n#define SAMPLE texture2D\n"
                                "#define OUTPUT gl_FragColor\n") +
            "uniform sampler2D baseImage, particleImage, foregroundImage;\n"
            "uniform vec2 panelSize; uniform vec4 particleRect, clipRect;\n"
            "void main(){\n"
            "vec4 b=SAMPLE(baseImage,uv); vec2 at=vec2(uv.x,1.0-uv.y)*panelSize;\n"
            "vec2 fp=(at-clipRect.xy)/clipRect.zw;\n"
            "if(all(greaterThanEqual(fp,vec2(0.0)))&&all(lessThanEqual(fp,vec2(1.0)))){\n"
            "vec2 pp=(at-particleRect.xy)/particleRect.zw;\n"
            "vec4 p=SAMPLE(particleImage,vec2(pp.x,1.0-pp.y));\n"
            "vec4 f=SAMPLE(foregroundImage,vec2(fp.x,1.0-fp.y));\n"
            // b = f + background*(1-f.a). Insert p without redrawing/blending f twice.
            "b=b+p*(1.0-f.a)-(b-f)*p.a;\n"
            "}\nOUTPUT=clamp(b,0.0,1.0); }\n";
        if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex, version + precision + vertex) ||
            !program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                             version + precision + fragment))
            return false;
        program.bindAttributeLocation("position", 0);
        if (!program.link() || !vertices.create())
            return false;
        vao.create();
        QOpenGLVertexArrayObject::Binder binding(&vao);
        vertices.bind();
        const GLfloat quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
        vertices.allocate(quad, sizeof(quad));
        vertices.release();
        return true;
    }

    bool draw(GLuint base, GLuint particle, GLuint foreground, const QSizeF& panel,
              const QRect& rect, const QRect& clip)
    {
        auto* gl = QOpenGLContext::currentContext()->functions();
        QOpenGLVertexArrayObject::Binder binding(&vao);
        if (!program.bind())
            return false;
        vertices.bind();
        program.enableAttributeArray(0);
        program.setAttributeBuffer(0, GL_FLOAT, 0, 2);
        program.setUniformValue("baseImage", 0);
        program.setUniformValue("particleImage", 1);
        program.setUniformValue("foregroundImage", 2);
        program.setUniformValue("panelSize", QVector2D(panel.width(), panel.height()));
        program.setUniformValue("particleRect",
                                QVector4D(rect.x(), rect.y(), rect.width(), rect.height()));
        program.setUniformValue("clipRect",
                                QVector4D(clip.x(), clip.y(), clip.width(), clip.height()));
        const GLuint textures[] = {base, particle, foreground};
        for (int i = 0; i < 3; ++i) {
            gl->glActiveTexture(GL_TEXTURE0 + i);
            gl->glBindTexture(GL_TEXTURE_2D, textures[i]);
        }
        gl->glDisable(GL_BLEND);
        gl->glDisable(GL_DEPTH_TEST);
        gl->glDisable(GL_STENCIL_TEST);
        gl->glDisable(GL_SCISSOR_TEST);
        gl->glColorMask(true, true, true, true);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        for (int i = 2; i >= 0; --i) {
            gl->glActiveTexture(GL_TEXTURE0 + i);
            gl->glBindTexture(GL_TEXTURE_2D, 0);
        }
        program.disableAttributeArray(0);
        vertices.release();
        program.release();
        return gl->glGetError() == GL_NO_ERROR;
    }

private:
    QOpenGLShaderProgram program;
    QOpenGLBuffer vertices;
    QOpenGLVertexArrayObject vao;
};
} // namespace

struct GalleryParticleCompositor::Private {
    struct Layer {
        std::unique_ptr<ParticleLayer> renderer;
        bool previousAcceleration = false;
        QRect rect, clip;
        std::unique_ptr<QOpenGLFramebufferObject> foreground;
        quint64 foregroundRevision = 0;
    };
    QPointer<QWidget> root;
    QPointer<QOpenGLContext> context;
    QList<QPointer<ParticleBackdrop>> sources;
    QList<QRect> sourceRects, sourceClips;
    std::vector<Layer> layers;
    std::unique_ptr<InsertionSampler> sampler;
    std::array<std::unique_ptr<QOpenGLFramebufferObject>, 2> composed;
    QSize panelPixels;
    qreal nativeDpr = 0, cacheDpr = 0;
    quint64 scanRevision = 0, staticRevision = 0, lastFrames = 0;
    quint64 captures = 0, compositions = 0, scans = 0, bytes = 0, maximumBytes = 0;
    GLuint result = 0;
    bool releasing = false;
    bool scanned = false, failed = false;
};

GalleryParticleCompositor::GalleryParticleCompositor(QObject* parent)
    : QObject(parent), d(std::make_unique<Private>())
{}

GalleryParticleCompositor::~GalleryParticleCompositor()
{
    releaseGpu(false);
}

bool GalleryParticleCompositor::prepare(QWidget* content, quint64 revision,
                                        const QSize& panelPixels, qreal nativeDpr, qreal cacheDpr,
                                        quint64 maximumBytes, bool enabled)
{
    auto* context = QOpenGLContext::currentContext();
    if (!enabled || !content || !context || panelPixels.isEmpty() || nativeDpr <= 0 ||
        cacheDpr <= 0) {
        release();
        return false;
    }
    if (d->root != content || d->context != context || d->panelPixels != panelPixels ||
        d->nativeDpr != nativeDpr || d->cacheDpr != cacheDpr) {
        release();
        d->root = content;
        d->context = context;
        d->panelPixels = panelPixels;
        d->nativeDpr = nativeDpr;
        d->cacheDpr = cacheDpr;
    }
    if (!d->scanned || d->scanRevision != revision) {
        ++d->scans;
        // Hidden prewarmed pages are not traversed. A clean no-particle page
        // retains this negative result until its source revision changes.
        QList<ParticleBackdrop*> candidates;
        std::function<void(QWidget*)> visit = [&](QWidget* parent) {
            for (auto* child : parent->children()) {
                auto* widget = qobject_cast<QWidget*>(child);
                if (!widget || widget->isWindow() || widget->isHidden())
                    continue;
                if (auto* backdrop = qobject_cast<ParticleBackdrop*>(widget))
                    candidates.append(backdrop);
                visit(widget);
            }
        };
        visit(content);
        QList<QPointer<ParticleBackdrop>> visible;
        QList<QRect> rects, clips;
        for (auto* backdrop : candidates) {
            bool supported = backdrop->mask().isEmpty() && !backdrop->graphicsEffect() &&
                             !clippedRect(backdrop, content).isEmpty();
            for (auto* foreground : foregroundWidgets(backdrop, content)) {
                if (foreground->graphicsEffect() || !foreground->mask().isEmpty() ||
                    unsupportedAncestors(foreground, content)) {
                    supported = false;
                    break;
                }
            }
            if (supported) {
                visible.append(backdrop);
                rects.append(QRect(backdrop->mapTo(content, QPoint()), backdrop->size()));
                clips.append(clippedRect(backdrop, content));
            }
        }
        if (visible != d->sources || rects != d->sourceRects || clips != d->sourceClips) {
            releaseGpu();
            d->sources = visible;
            d->sourceRects = rects;
            d->sourceClips = clips;
            d->failed = false;
        }
        d->scanRevision = revision;
        d->scanned = true;
    }
    if (d->sources.empty())
        return false;
    if (d->failed && d->maximumBytes == maximumBytes)
        return false; // Retry only a geometry/source/context/preference/budget change.
    d->maximumBytes = maximumBytes;
    if (d->layers.empty()) {
        for (int i = 0; i < d->sources.size(); ++i) {
            auto* backdrop = d->sources[i].data();
            Private::Layer layer;
            layer.previousAcceleration = backdrop->isGpuAccelerationEnabled();
            backdrop->setGpuAccelerationEnabled(true);
            layer.renderer = std::make_unique<ParticleLayer>(backdrop);
            layer.rect = d->sourceRects[i];
            layer.clip = d->sourceClips[i];
            connect(layer.renderer.get(), &ParticleLayer::frameRequested, this,
                    &GalleryParticleCompositor::frameRequested);
            connect(layer.renderer.get(), &ParticleLayer::activeChanged, this, [this] {
                if (!d->releasing)
                    emit staticContentInvalidated();
            });
            d->layers.push_back(std::move(layer));
        }
        d->result = 0;
    }

    const int outputs = d->layers.size() > 1 ? 2 : 1;
    quint64 auxiliaryBytes =
        quint64(panelPixels.width()) * quint64(panelPixels.height()) * 4 * outputs;
    for (const auto& layer : d->layers) {
        const QSize foregroundPixels = physicalSize(layer.clip.size(), cacheDpr);
        auxiliaryBytes +=
            quint64(foregroundPixels.width()) * quint64(foregroundPixels.height()) * 4;
    }
    quint64 required = auxiliaryBytes;
    for (const auto& layer : d->layers) {
        const quint64 estimated = ParticleLayer::estimatedBytes(layer.rect.size(), nativeDpr);
        if (!estimated || required > maximumBytes || estimated > maximumBytes - required) {
            releaseGpu();
            d->failed = true;
            return false;
        }
        required += estimated;
    }
    if (required > maximumBytes) {
        releaseGpu();
        d->failed = true;
        return false;
    }
    if (!d->sampler) {
        auto sampler = std::make_unique<InsertionSampler>();
        if (!sampler->create()) {
            releaseGpu();
            d->failed = true;
            return false;
        }
        d->sampler = std::move(sampler);
    }
    QOpenGLFramebufferObjectFormat textureFormat;
    textureFormat.setInternalTextureFormat(GL_RGBA8);
    if (!d->composed[0]) {
        for (int i = 0; i < outputs; ++i) {
            d->composed[size_t(i)] =
                std::make_unique<QOpenGLFramebufferObject>(panelPixels, textureFormat);
            prepareSampling(d->composed[size_t(i)].get());
        }
        for (auto& layer : d->layers) {
            layer.foreground = std::make_unique<QOpenGLFramebufferObject>(
                physicalSize(layer.clip.size(), cacheDpr), textureFormat);
            prepareSampling(layer.foreground.get());
        }
    }
    bool valid = true;
    for (int i = 0; i < outputs; ++i)
        valid &= d->composed[size_t(i)]->isValid();
    quint64 allocated = auxiliaryBytes;
    for (auto& layer : d->layers) {
        valid &= layer.foreground->isValid();
        if (valid) {
            valid &= layer.renderer->render(nativeDpr, maximumBytes - allocated);
            allocated += layer.renderer->allocatedBytes();
        }
    }
    if (!valid) {
        releaseGpu();
        d->failed = true;
        return false;
    }
    if (allocated > maximumBytes) {
        releaseGpu();
        d->failed = true;
        return false;
    }
    d->bytes = allocated;
    return true;
}

unsigned int GalleryParticleCompositor::compose(unsigned int base, quint64 revision,
                                                QOpenGLFramebufferObject* paintTarget,
                                                QOpenGLFramebufferObject* resolveTarget,
                                                const RenderWidget& renderWidget)
{
    if (!d->root || d->layers.empty() || !base)
        return base;
    auto* context = QOpenGLContext::currentContext();
    if (!context || context != d->context || !paintTarget || !resolveTarget ||
        !paintTarget->isValid() || !resolveTarget->isValid() ||
        paintTarget->size() != resolveTarget->size()) {
        release();
        return base;
    }
    const quint64 frames = particleFrameCount();
    if (d->result && d->staticRevision == revision && d->lastFrames == frames)
        return d->result;
    auto* gl = context->functions();
    for (auto& layer : d->layers) {
        if (layer.foregroundRevision == revision)
            continue;
        const QSize pixels = layer.foreground->size();
        if (pixels.width() > paintTarget->width()) {
            releaseGpu();
            d->failed = true;
            return base;
        }
        const int guard = qMax(1, qCeil(d->cacheDpr));
        const int stride = pixels.height() <= paintTarget->height()
                               ? pixels.height()
                               : qMax(1, paintTarget->height() - 2 * guard);
        const auto foreground = foregroundWidgets(layer.renderer->backdrop(), d->root);
        for (int top = 0; top < pixels.height(); top += stride) {
            const int height = qMin(stride, pixels.height() - top);
            const int paintTop = qMax(0, top - guard);
            const int paintBottom = qMin(pixels.height(), top + height + guard);
            const int paintHeight = paintBottom - paintTop;
            paintTarget->bind();
            gl->glDisable(GL_SCISSOR_TEST);
            gl->glColorMask(true, true, true, true);
            gl->glStencilMask(0xFFFFFFFF);
            gl->glClearColor(0, 0, 0, 0);
            gl->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            QOpenGLPaintDevice device(QSize(pixels.width(), paintHeight));
            device.setDevicePixelRatio(d->cacheDpr);
            const QPointF origin = layer.clip.topLeft() + QPointF(0, paintTop / d->cacheDpr);
            GalleryGlyphPaintDevice glyphDevice(device, d->nativeDpr, origin);
            QPaintDevice* target = needsNativeGlyphCoverage(d->nativeDpr)
                                       ? static_cast<QPaintDevice*>(&glyphDevice)
                                       : &device;
            QPainter painter(target);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            painter.translate(-origin);
            const QRectF strip(origin, QSizeF(layer.clip.width(), paintHeight / d->cacheDpr));
            painter.setClipRect(strip);
            for (auto* widget : foreground) {
                const QRect clip =
                    clippedRect(widget, d->root) & layer.clip & strip.toAlignedRect();
                if (clip.isEmpty())
                    continue;
                const QPoint widgetOrigin = widget->mapTo(d->root, QPoint());
                painter.save();
                painter.setClipRect(clip, Qt::IntersectClip);
                painter.translate(widgetOrigin);
                renderWidget(painter, widget, QRegion(clip.translated(-widgetOrigin)));
                painter.restore();
            }
            painter.end();
            gl->glDisable(GL_SCISSOR_TEST);
            // Resolve identical MSAA rectangles first, then copy the guarded
            // strip. These are the canvas's existing targets, not new caches.
            QOpenGLFramebufferObject::blitFramebuffer(resolveTarget, paintTarget);
            QOpenGLFramebufferObject::blitFramebuffer(
                layer.foreground.get(),
                QRect(0, pixels.height() - top - height, pixels.width(), height), resolveTarget,
                QRect(0, paintBottom - top - height, pixels.width(), height));
        }
        if (gl->glGetError() != GL_NO_ERROR) {
            releaseGpu();
            d->failed = true;
            return base;
        }
        layer.foregroundRevision = revision;
        ++d->captures;
    }
    unsigned int input = base;
    for (size_t i = 0; i < d->layers.size(); ++i) {
        auto& layer = d->layers[i];
        auto* output = d->composed[i % 2].get();
        output->bind();
        gl->glViewport(0, 0, d->panelPixels.width(), d->panelPixels.height());
        if (!d->sampler->draw(input, layer.renderer->textureId(), layer.foreground->texture(),
                              d->root->size(), layer.rect, layer.clip)) {
            releaseGpu();
            d->failed = true;
            return base;
        }
        input = output->texture();
    }
    d->result = input;
    d->staticRevision = revision;
    d->lastFrames = frames;
    ++d->compositions;
    return input;
}

void GalleryParticleCompositor::releaseGpu(bool notify)
{
    const bool active = activeLayerCount() != 0;
    d->releasing = true;
    for (auto& layer : d->layers) {
        QPointer<ParticleBackdrop> source(layer.renderer->backdrop());
        layer.renderer.reset();
        if (source)
            source->setGpuAccelerationEnabled(layer.previousAcceleration);
    }
    d->layers.clear();
    d->composed = {};
    d->sampler.reset();
    d->staticRevision = d->lastFrames = d->bytes = 0;
    d->result = 0;
    d->releasing = false;
    if (active && notify)
        emit staticContentInvalidated();
}

void GalleryParticleCompositor::release()
{
    d->root.clear();
    d->context.clear();
    d->sources.clear();
    d->sourceRects.clear();
    d->sourceClips.clear();
    d->scanRevision = 0;
    d->scanned = d->failed = false;
    releaseGpu();
}

int GalleryParticleCompositor::activeLayerCount() const
{
    int count = 0;
    for (const auto& layer : d->layers)
        count += layer.renderer->isActive();
    return count;
}

quint64 GalleryParticleCompositor::allocatedBytes() const
{
    return d->bytes;
}
quint64 GalleryParticleCompositor::particleFrameCount() const
{
    quint64 frames = 0;
    for (const auto& layer : d->layers)
        frames += layer.renderer->renderedFrameCount();
    return frames;
}
quint64 GalleryParticleCompositor::foregroundCaptureCount() const
{
    return d->captures;
}
quint64 GalleryParticleCompositor::compositionCount() const
{
    return d->compositions;
}
quint64 GalleryParticleCompositor::sourceScanCount() const
{
    return d->scans;
}

} // namespace fluent::gallery::spatial_render
#endif
