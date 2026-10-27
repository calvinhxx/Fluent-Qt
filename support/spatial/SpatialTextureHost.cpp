#include "SpatialTextureHost.h"
#include "components/spatial/SpatialRuntime.h"
#include "components/windowing/Window.h"
// Parse Fluent declarations before Qt OpenGL headers expose the Windows `small` macro.
// zh_CN: 先解析 Fluent 声明，避免 Qt OpenGL 头文件引入的 Windows small 宏污染字段名。
#include "compatibility/private/spatial/SpatialFramebufferCompat_p.h"
#include "compatibility/private/spatial/renderer/SpatialTextureBackend_p.h"
#include <QResizeEvent>
#include <utility>

namespace fluent::spatial {
namespace {
using TextureBackend = ::compatibility::detail::SpatialTextureBackend;
}

struct SpatialTextureHost::Private {
    std::unique_ptr<TextureBackend> backend;
};

SpatialTextureHost::SpatialTextureHost(QWidget* parent) : QWidget(parent), d(new Private)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    ::compatibility::detail::SpatialTextureCallbacks callbacks;
    callbacks.synchronizeFrame = [this] { synchronizeFrame(); };
    callbacks.paintLayer = [this](QPainter* painter, int index, const QRegion& region) {
        paintLayer(painter, index, region);
    };
    callbacks.initialized = [this] { emit initialized(); };
    callbacks.renderingFailed = [this] { emit renderingFailed(); };
    callbacks.cacheUnavailable = [this] { emit cacheUnavailable(); };
    callbacks.isHardwareRenderer = &SpatialRuntime::isHardwareRenderer;
    d->backend = std::make_unique<TextureBackend>(this, std::move(callbacks));
}

SpatialTextureHost::~SpatialTextureHost() = default;

int SpatialTextureHost::currentPaintSampleBytes()
{
    return ::compatibility::detail::currentPaintSampleBytes();
}

bool SpatialTextureHost::isSupported()
{
    return TextureBackend::isSupported();
}

bool SpatialTextureHost::isPreferred()
{
    return TextureBackend::isPreferred();
}

bool SpatialTextureHost::prepareWindow(windowing::Window* window)
{
    if (!isPreferred())
        return SpatialRuntime::prepareWindow(window);
    const auto surface = TextureBackend::preferredSurfaceType();
    return surface && SpatialRuntime::prepareWindow(window, *surface);
}

QString SpatialTextureHost::preflightFailure()
{
    return TextureBackend::hasHardwareDevice(&SpatialRuntime::isHardwareRenderer)
        ? QString()
        : tr("A hardware graphics device is unavailable.");
}

bool SpatialTextureHost::isReady() const
{
    return d->backend->isReady();
}

QString SpatialTextureHost::rendererName() const
{
    return d->backend->rendererName();
}

QVariantMap SpatialTextureHost::statistics() const
{
    return d->backend->statistics();
}

QImage SpatialTextureHost::grabFramebuffer() const
{
    return d->backend->grabFramebuffer();
}

void SpatialTextureHost::requestFrame()
{
    d->backend->requestFrame();
}

void SpatialTextureHost::clearFrameCaches()
{
    d->backend->clearFrameCaches();
}

void SpatialTextureHost::setLayerCount(int count)
{
    d->backend->setLayerCount(count);
}

void SpatialTextureHost::setLayer(int index,
    const QRectF& source,
    const QTransform& transform,
    quint64 revision,
    qreal radius,
    const QColor& material,
    const QColor& reflection,
    qreal progress,
    qreal shadowOpacity)
{
    d->backend->setLayer(index, source, transform, revision, radius, material, reflection, progress, shadowOpacity);
}

void SpatialTextureHost::setSceneClip(const QRectF& clip)
{
    d->backend->setSceneClip(clip);
}

void SpatialTextureHost::synchronizeFrame() {}
void SpatialTextureHost::paintLayer(QPainter*, int, const QRegion&) {}

void SpatialTextureHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    d->backend->resize(rect());
}
} // namespace fluent::spatial
