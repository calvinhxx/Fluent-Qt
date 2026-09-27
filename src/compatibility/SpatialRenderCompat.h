#ifndef FLUENTQT_SPATIALRENDERCOMPAT_H
#define FLUENTQT_SPATIALRENDERCOMPAT_H

#include <QGuiApplication>
#include <QtMath>
#include "QtCompat.h"

namespace compatibility {

// Platform differences live here; consumers select one common rendering flow.
struct SpatialRenderCapabilities {
    bool openGLDisplay = false;
    bool prepareNativeSurface = false;
    bool probeBeforeExposure = false;
    bool nativeGlyphCoverage = false;
};

inline SpatialRenderCapabilities spatialRenderCapabilities(qreal nativeDpr = 1)
{
    const QString platform = QGuiApplication::platformName();
    const bool windows = platform == QLatin1String("windows");
    const bool linuxDesktop =
        platform == QLatin1String("xcb") || platform.startsWith(QLatin1String("wayland"));
    const bool cocoa = platform == QLatin1String("cocoa");
    SpatialRenderCapabilities result;
    result.openGLDisplay = platform != QLatin1String("offscreen") &&
                           platform != QLatin1String("minimal") && platform != QLatin1String("vnc");
    result.prepareNativeSurface =
        FLUENT_OPENGL_WIDGET_CAN_REPLACE_NATIVE_WINDOW && (windows || linuxDesktop || cocoa);
    result.probeBeforeExposure = cocoa;
    // Keep the measured font paths: Windows low-DPI and integer Linux need
    // native coverage; fractional Linux/Cocoa already preserve their coverage.
    result.nativeGlyphCoverage = (windows && nativeDpr >= 1 && nativeDpr < 2) ||
                                 (linuxDesktop && qFuzzyCompare(nativeDpr, 1.));
    return result;
}
} // namespace compatibility
#endif
