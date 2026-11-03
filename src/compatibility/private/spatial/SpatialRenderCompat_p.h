#ifndef FLUENTQT_SPATIALRENDERCOMPAT_H
#define FLUENTQT_SPATIALRENDERCOMPAT_H

#include <QGuiApplication>
#include <QtMath>
#include "compatibility/QtFeatures.h"

namespace compatibility::detail {

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
    const bool linuxDesktop = platform == QLatin1String("xcb") || platform.startsWith(QLatin1String("wayland"));
    const bool cocoa = platform == QLatin1String("cocoa");
    SpatialRenderCapabilities result;
    result.openGLDisplay = platform != QLatin1String("offscreen") && platform != QLatin1String("minimal")
        && platform != QLatin1String("vnc");
    result.prepareNativeSurface = FLUENT_OPENGL_WIDGET_CAN_REPLACE_NATIVE_WINDOW && (windows || linuxDesktop || cocoa);
    // A failed QOpenGLWidget can leave the whole native window unable to compose.
    // Check desktop drivers before exposing a GPU child, including Windows ARM VMs.
    result.probeBeforeExposure = windows || linuxDesktop || cocoa;
    // Windows needs the same native coverage across the whole zoom range.
    // Switching to OpenGL glyphs below 1x or at 2x changes stroke weight even
    // when the font and pen stay unchanged. Other measured paths are retained.
    // zh_CN: Windows 各缩放倍率统一使用原生字形覆盖率；低于 1 或达到 2 时切换到 OpenGL
    // 会在字体和画笔未变时改变笔画重量。其余平台保留已验证的路径。
    result.nativeGlyphCoverage = (windows && qIsFinite(nativeDpr) && nativeDpr > 0)
        || (linuxDesktop && qFuzzyCompare(nativeDpr, 1.));
    return result;
}
} // namespace compatibility::detail
#endif
