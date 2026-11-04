#pragma once

#include "QtTestEnvironment.h"
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPointer>
#include <QScopeGuard>

namespace tests::support {
// Probe Qt itself before exercising the product. A working software context also
// supports QOpenGLWidget; only Spatial requires a hardware renderer.
inline QString nativeOpenGLContextUnavailableReason(QString (*checkRenderer)() = nullptr)
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
    const QString reason = checkRenderer ? checkRenderer() : QString();
    probe.doneCurrent();
    return reason;
}
} // namespace tests::support
