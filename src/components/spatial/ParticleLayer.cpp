#include "ParticleLayer.h"

#include <QEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QPointer>
#include <QVariant>
#include <QtMath>
#include <cmath>

#include "components/layout/ParticleBackdrop.h"
#include "components/layout/ParticleBackdrop_p.h"

namespace fluent::spatial {
namespace {
using Access = layout::ParticleBackdropRenderAccess;

// Preserve the caller's framebuffer and viewport across allocation/resolve.
class TargetScope {
public:
    explicit TargetScope(QOpenGLContext* current) : context(current), gl(current->functions())
    {
        gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        gl->glGetIntegerv(GL_VIEWPORT, viewport);
    }
    ~TargetScope()
    {
        if (!context || QOpenGLContext::currentContext() != context)
            return;
        gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(framebuffer));
        gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

private:
    QPointer<QOpenGLContext> context;
    QOpenGLFunctions* gl;
    GLint framebuffer = 0;
    GLint viewport[4] = {};
};

int actualPaintSamples(QOpenGLContext* context)
{
    // Qt may round the requested MSAA count upwards. Probe the identical
    // format before allocating full-size targets, and retain the result only
    // for this context's lifetime.
    constexpr auto key = "_fluentqt_particleLayerPaintSamples";
    const QVariant cached = context->property(key);
    if (cached.isValid())
        return cached.toInt();
    TargetScope scope(context);
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setInternalTextureFormat(GL_RGBA8);
    format.setSamples(4);
    QOpenGLFramebufferObject probe(QSize(1, 1), format);
    const int samples = probe.isValid() ? probe.format().samples() : 0;
    context->setProperty(key, samples);
    return samples;
}
} // namespace

struct ParticleLayer::Private {
    // During QWidget destruction a typed QPointer may still reference the
    // object after its ParticleBackdrop pimpl has gone. Re-check dynamic type.
    QPointer<QObject> backdrop;
    QPointer<QOpenGLContext> context;
    QMetaObject::Connection contextDestroyed;
    std::unique_ptr<QOpenGLFramebufferObject> paintTarget;
    std::unique_ptr<QOpenGLFramebufferObject> texture;
    quint64 revision = 0;
    quint64 bytes = 0;
    quint64 frames = 0;
    qreal dpr = 0;
    bool active = false;
};

ParticleLayer::ParticleLayer(layout::ParticleBackdrop* backdrop, QObject* parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->backdrop = backdrop;
    if (backdrop) {
        backdrop->installEventFilter(this);
        connect(backdrop, &layout::ParticleBackdrop::gpuAccelerationEnabledChanged, this,
                [this](bool enabled) {
                    if (!enabled)
                        release();
                });
        connect(backdrop, &QObject::destroyed, this, [this] {
            d->backdrop.clear();
            release();
        });
    }
}

ParticleLayer::~ParticleLayer()
{
    // QObject destruction is the lifetime notification. Do not emit a regular
    // state signal from a partially destructed object to reentrant callers.
    d->active = false;
    release();
}

layout::ParticleBackdrop* ParticleLayer::backdrop() const
{
    return qobject_cast<layout::ParticleBackdrop*>(d->backdrop.data());
}

bool ParticleLayer::isActive() const
{
    return d->active && Access::isClaimedBy(backdrop(), this);
}

quint64 ParticleLayer::estimatedBytes(const QSize& logicalSize, qreal devicePixelRatio)
{
    auto* context = QOpenGLContext::currentContext();
    if (!context || !context->isValid() || logicalSize.isEmpty() ||
        !std::isfinite(devicePixelRatio) || devicePixelRatio <= 0 || devicePixelRatio > 4)
        return 0;
    auto* gl = context->functions();
    GLint maxTexture = 0, maxRenderbuffer = 0;
    gl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    gl->glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbuffer);
    const int maxDimension = qMin(maxTexture, maxRenderbuffer);
    if (logicalSize.width() * devicePixelRatio > maxDimension ||
        logicalSize.height() * devicePixelRatio > maxDimension)
        return 0;
    const QSize pixels(qCeil(logicalSize.width() * devicePixelRatio),
                       qCeil(logicalSize.height() * devicePixelRatio));
    const int samples = actualPaintSamples(context);
    if (samples < 2)
        return 0;
    // Conservatively allow separate depth/stencil storage, as the Gallery
    // aggregate policy does, plus the resolved RGBA texture.
    return quint64(pixels.width()) * quint64(pixels.height()) * quint64(samples * 12 + 4);
}

bool ParticleLayer::render(qreal devicePixelRatio, quint64 maximumBytes)
{
    auto* context = QOpenGLContext::currentContext();
    QPointer<layout::ParticleBackdrop> source(backdrop());
    if (!context || !context->isValid() || !source || !source->isGpuAccelerationEnabled() ||
        !Access::isInViewport(source)) {
        release();
        return false;
    }
    auto* gl = context->functions();
    TargetScope targetScope(context);
    const quint64 upperBytes = estimatedBytes(source->size(), devicePixelRatio);
    if (!upperBytes || upperBytes > maximumBytes) {
        release();
        return false;
    }
    const QSize pixels(qCeil(source->width() * devicePixelRatio),
                       qCeil(source->height() * devicePixelRatio));

    if (d->context != context || !d->texture || d->texture->size() != pixels ||
        d->dpr != devicePixelRatio) {
        QPointer<ParticleLayer> guard(this);
        release();
        if (!guard || !source || QOpenGLContext::currentContext() != context ||
            !Access::isInViewport(source))
            return false;
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setInternalTextureFormat(GL_RGBA8);
        format.setSamples(actualPaintSamples(context));
        auto paintTarget = std::make_unique<QOpenGLFramebufferObject>(pixels, format);
        QOpenGLFramebufferObjectFormat textureFormat;
        textureFormat.setInternalTextureFormat(GL_RGBA8);
        auto texture = std::make_unique<QOpenGLFramebufferObject>(pixels, textureFormat);
        if (!paintTarget->isValid() || !texture->isValid() || gl->glGetError() != GL_NO_ERROR)
            return false;
        const quint64 allocated = quint64(pixels.width()) * quint64(pixels.height()) *
                                  quint64(qMax(1, paintTarget->format().samples()) * 12 + 4);
        if (allocated > maximumBytes)
            return false;
        gl->glBindTexture(GL_TEXTURE_2D, texture->texture());
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl->glBindTexture(GL_TEXTURE_2D, 0);
        d->paintTarget = std::move(paintTarget);
        d->texture = std::move(texture);
        d->context = context;
        d->dpr = devicePixelRatio;
        d->bytes = allocated;
        d->contextDestroyed =
            connect(context, &QOpenGLContext::aboutToBeDestroyed, this, [this] { release(); });
    }

    const quint64 revision = Access::revision(source);
    if (isActive() && d->revision == revision)
        return true;
    d->paintTarget->bind();
    gl->glViewport(0, 0, pixels.width(), pixels.height());
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glColorMask(true, true, true, true);
    gl->glStencilMask(0xFFFFFFFF);
    gl->glClearColor(0, 0, 0, 0);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    QOpenGLPaintDevice device(pixels);
    device.setDevicePixelRatio(devicePixelRatio);
    QPainter painter(&device);
    if (!painter.isActive()) {
        release();
        return false;
    }
    painter.save();
    Access::paintIsolated(source, painter);
    painter.restore();
    painter.end();
    gl->glDisable(GL_SCISSOR_TEST);
    QOpenGLFramebufferObject::blitFramebuffer(d->texture.get(), d->paintTarget.get());
    if (gl->glGetError() != GL_NO_ERROR) {
        release();
        return false;
    }
    const bool active = isActive();
    if (!Access::claim(source, this, [this] { emit frameRequested(); })) {
        release();
        return false;
    }
    d->revision = revision;
    d->active = true;
    ++d->frames;
    if (!active)
        emit activeChanged(true);
    return true;
}

unsigned int ParticleLayer::textureId() const
{
    return d->texture ? d->texture->texture() : 0;
}

QSize ParticleLayer::textureSize() const
{
    return d->texture ? d->texture->size() : QSize();
}

quint64 ParticleLayer::allocatedBytes() const
{
    return d->bytes;
}

quint64 ParticleLayer::renderedFrameCount() const
{
    return d->frames;
}

void ParticleLayer::release()
{
    const bool active = d->active;
    d->active = false;
    Access::release(backdrop(), this);
    disconnect(d->contextDestroyed);
    d->texture.reset();
    d->paintTarget.reset();
    d->context.clear();
    d->revision = 0;
    d->bytes = 0;
    d->dpr = 0;
    if (active)
        emit activeChanged(false);
}

bool ParticleLayer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == d->backdrop &&
        (event->type() == QEvent::Hide || event->type() == QEvent::ParentChange)) {
        const QPointer<ParticleLayer> guard(this);
        const QPointer<QObject> receiver(watched);
        release();
        if (!guard || !receiver)
            return true;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace fluent::spatial
