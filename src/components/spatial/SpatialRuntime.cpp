#include "SpatialRuntime.h"
#include "compatibility/SpatialNativeStyle_p.h"
#include "compatibility/SpatialRenderCompat.h"
#include "components/windowing/Window.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QScopeGuard>
#include <QWindow>

namespace fluent::spatial {
void SpatialRuntime::prepareApplication()
{
    compatibility::detail::prepareNativeStyle();
}

bool SpatialRuntime::prepareWindow(windowing::Window* window)
{
    return prepareWindow(window, QSurface::OpenGLSurface);
}

bool SpatialRuntime::prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType)
{
    if (!window || !window->isWindow() || window->isVisible() ||
        !compatibility::spatialRenderCapabilities().prepareNativeSurface)
        return false;
    if (window->windowHandle() && window->windowHandle()->surfaceType() == surfaceType)
        return true;
    window->destroy();
    window->setAttribute(Qt::WA_NativeWindow, false);
    window->setAttribute(Qt::WA_NativeWindow);
    if (auto* surface = window->windowHandle()) {
        surface->setSurfaceType(surfaceType);
        return true;
    }
    return false;
}

bool SpatialRuntime::supportsOpenGLDisplay()
{
    return compatibility::spatialRenderCapabilities().openGLDisplay;
}

bool SpatialRuntime::isHardwareRenderer(const QString& renderer)
{
    const auto name = renderer.toLower();
    return !name.isEmpty() && !name.contains(QStringLiteral("llvmpipe")) &&
           !name.contains(QStringLiteral("softpipe")) &&
           !name.contains(QStringLiteral("swiftshader")) &&
           !name.contains(QStringLiteral("software")) &&
           !name.contains(QStringLiteral("basic render driver")) &&
           !name.contains(QStringLiteral("warp")) && !name.contains(QStringLiteral("gdi generic"));
}

QString SpatialRuntime::currentRendererName()
{
    auto* context = QOpenGLContext::currentContext();
    if (!context)
        return {};
    const auto* name = context->functions()->glGetString(GL_RENDERER);
    return name ? QString::fromLatin1(reinterpret_cast<const char*>(name)) : QString();
}

QString SpatialRuntime::preflightFailure()
{
    if (!compatibility::spatialRenderCapabilities().probeBeforeExposure)
        return {};
    const QPointer<QOpenGLContext> previous = QOpenGLContext::currentContext();
    QSurface* previousSurface = previous ? previous->surface() : nullptr;
    const auto restore = qScopeGuard([previous, previousSurface] {
        if (previous && previousSurface)
            previous->makeCurrent(previousSurface);
    });
    QOpenGLContext probe;
    if (!probe.create())
        return QObject::tr("OpenGL initialization failed.");
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    if (!surface.isValid() || !probe.makeCurrent(&surface))
        return QObject::tr("OpenGL initialization failed.");
    const bool hardware = isHardwareRenderer(currentRendererName());
    probe.doneCurrent();
    return hardware ? QString() : QObject::tr("A hardware OpenGL renderer is unavailable.");
}

int SpatialRuntime::maximumTextureDimension()
{
    auto* context = QOpenGLContext::currentContext();
    if (!context)
        return 0;
    GLint texture = 0, renderbuffer = 0, viewport[2] = {};
    auto* gl = context->functions();
    gl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture);
    gl->glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbuffer);
    gl->glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
    return qMin(qMin(texture, renderbuffer), qMin(viewport[0], viewport[1]));
}

bool SpatialRuntime::needsNativeGlyphCoverage(qreal nativeDpr)
{
    return compatibility::spatialRenderCapabilities(nativeDpr).nativeGlyphCoverage;
}
} // namespace fluent::spatial
