#pragma once

#include "compatibility/WindowChromeCompat.h"

namespace compatibility::detail {

// Every platform implements this internal backend contract.
void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options);
bool handlePlatformNativeEvent(QWidget* window,
    const WindowChromeOptions& options,
    const QByteArray& eventType,
    void* message,
    FluentNativeEventResult* result);
bool beginPlatformSystemMove(QWidget* window, const QPoint& globalPos);
bool beginPlatformSystemResize(QWidget* window, Qt::Edges edges, const QPoint& globalPos);
bool performPlatformTitleBarDoubleClick(QWidget* window, const WindowChromeOptions& options);
bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos);
bool requestPlatformForegroundActivation(QWidget* window);
void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options);
int nativeTitleBarLeadingInset(QWidget* window);
int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options);
bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options);
BackdropCapabilities platformBackdropCapabilities();
BackdropApplyResult
applyPlatformSystemBackdrop(QWidget* window, BackdropEffect effect, bool dark, bool forceRecomposite);
bool flushPlatformWindowBackdropSurface(QWidget* window);
void preparePlatformWindowBackdropSurface(QWidget* window,
    const QColor& opaqueColor,
    fluent::windowing::BackdropSurfaceMode mode);

} // namespace compatibility::detail
