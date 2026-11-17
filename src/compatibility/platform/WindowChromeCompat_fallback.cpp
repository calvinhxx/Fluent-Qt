#include "compatibility/private/WindowChromeBackend_p.h"

#include "compatibility/private/RuntimePlatformCapabilities_p.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)

namespace compatibility {
namespace detail {

void preparePlatformWindowBackdropSurface(QWidget* window,
    const QColor& opaqueColor,
    fluent::windowing::BackdropSurfaceMode mode)
{
    Q_UNUSED(window);
    Q_UNUSED(opaqueColor);
    Q_UNUSED(mode);
}

bool flushPlatformWindowBackdropSurface(QWidget* window)
{
    Q_UNUSED(window);
    return true;
}

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options)
{
    if (!window)
        return;

    // Desktop-hosted (e.g. WASM) windows stay child widgets; re-applying Qt::Window in showEvent() detaches them.
    // zh_CN: 宿主桌面表面内的应用窗口须保持普通子控件；若在 showEvent() 重设 Qt::Window 会脱离宿主容器。
    if (runtimePlatformCapabilities().hostsApplicationWindowsInDesktopSurface)
        return;

    window->setWindowFlag(Qt::Window, true);
    window->setWindowFlag(Qt::FramelessWindowHint, options.useCustomWindowChrome);
}

bool handlePlatformNativeEvent(QWidget* window,
    const WindowChromeOptions& options,
    const QByteArray& eventType,
    void* message,
    FluentNativeEventResult* result)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

bool beginPlatformSystemMove(QWidget* window, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

bool beginPlatformSystemResize(QWidget* window, Qt::Edges edges, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(edges);
    Q_UNUSED(globalPos);
    return false;
}

bool performPlatformTitleBarDoubleClick(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
}

int nativeTitleBarLeadingInset(QWidget* window)
{
    Q_UNUSED(window);
    return 0;
}

BackdropCapabilities platformBackdropCapabilities()
{
    BackdropCapabilities capabilities;
    capabilities.provider = QStringLiteral("painted-material");
    return capabilities;
}

bool requestPlatformForegroundActivation(QWidget* window)
{
    Q_UNUSED(window);
    return false;
}

bool restorePlatformMaximizedFrame(QWidget* window)
{
    Q_UNUSED(window);
    return false;
}

bool platformSupportsSystemBackdrop()
{
    return false;
}

BackdropApplyResult
applyPlatformSystemBackdrop(QWidget* window, BackdropEffect effect, bool dark, bool forceRecomposite)
{
    Q_UNUSED(window);
    Q_UNUSED(dark);
    Q_UNUSED(forceRecomposite);
    BackdropApplyResult result;
    if (effect == BackdropEffect::Solid) {
        result.applied = true;
        result.backend = fluent::windowing::BackdropBackend::Solid;
        result.fidelity = fluent::windowing::BackdropFidelity::Solid;
        result.surfaceMode = fluent::windowing::BackdropSurfaceMode::SolidOpaque;
        result.reason = QStringLiteral("solid-requested");
    } else {
        result.reason = QStringLiteral("native-backdrop-unavailable");
    }
    return result;
}

int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    return options.useCustomWindowChrome ? qMax(0, runtimePlatformCapabilities().clientSideFrameMargin) : 0;
}

bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    return options.useCustomWindowChrome && options.chromeInteractive
        && runtimePlatformCapabilities().manualMoveResizeFallback;
}

} // namespace detail
} // namespace compatibility

#endif
