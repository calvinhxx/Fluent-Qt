#include "WindowChromeCompat.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)

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

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
}

} // namespace detail
} // namespace compatibility

#endif
