#pragma once

#include "QtTestEnvironment.h"
#include "components/spatial/SpatialRuntime.h"
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPointer>
#include <QScopeGuard>

namespace tests::support {
// Probe Qt independently so product regressions still fail on a hardware GPU host.
inline QString nativeOpenGLUnavailableReason()
{
    if (isHeadlessPlatform())
        return QStringLiteral("Requires a native OpenGL viewport");
    const QPointer<QOpenGLContext> previous = QOpenGLContext::currentContext();
    QSurface* previousSurface = previous ? previous->surface() : nullptr;
    const auto restore = qScopeGuard([previous, previousSurface] {
        if (previous && previousSurface)
            previous->makeCurrent(previousSurface);
    });
    QOpenGLContext probe;
    if (!probe.create())
        return QStringLiteral("The host cannot create a Qt OpenGL context");
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    if (!surface.isValid() || !probe.makeCurrent(&surface))
        return QStringLiteral("The host cannot make a Qt OpenGL context current");
    const auto renderer = fluent::spatial::SpatialRuntime::currentRendererName();
    probe.doneCurrent();
    if (!fluent::spatial::SpatialRuntime::isHardwareRenderer(renderer))
        return QStringLiteral("Requires hardware OpenGL; renderer: ") + renderer;
    return {};
}
} // namespace tests::support
