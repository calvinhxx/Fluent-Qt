#include "WindowChromeCompat.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)

namespace compatibility {
namespace detail {

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(options);
    if (window)
        window->setWindowFlag(Qt::Window, true);
}

bool handlePlatformNativeEvent(QWidget* window,
                               const WindowChromeOptions& options,
                               const QByteArray& eventType,
                               void* message,
                               FluentNativeEventResult* result) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

bool beginPlatformSystemMove(QWidget* window, const QPoint& globalPos) {
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

bool beginPlatformSystemResize(QWidget* window, Qt::Edges edges, const QPoint& globalPos) {
    Q_UNUSED(window);
    Q_UNUSED(edges);
    Q_UNUSED(globalPos);
    return false;
}

bool performPlatformTitleBarDoubleClick(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos) {
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
}

int nativeTitleBarLeadingInset(QWidget* window) {
    Q_UNUSED(window);
    return 0;
}

BackdropCapabilities platformBackdropCapabilities() {
    BackdropCapabilities capabilities;
    capabilities.provider = QStringLiteral("painted-material");
    return capabilities;
}

bool platformSupportsSystemBackdrop() {
    return false;
}

BackdropApplyResult applyPlatformSystemBackdrop(QWidget* window,
                                                BackdropEffect effect,
                                                bool dark,
                                                bool forceRecomposite) {
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

int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return 0;
}

bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

} // namespace detail
} // namespace compatibility

#endif
