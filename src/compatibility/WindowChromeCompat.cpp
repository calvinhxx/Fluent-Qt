#include "WindowChromeCompat.h"

#include <QWindow>

namespace compatibility {

namespace detail {
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
void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options);
int nativeTitleBarLeadingInset(QWidget* window);
bool platformSupportsSystemBackdrop();
bool applyPlatformSystemBackdrop(QWidget* window, bool dark);
}

namespace {

// Guard system move/resize entry points so widgets do not start platform
// operations before the native window handle and chrome policy are ready.
// zh_CN: 在原生窗口句柄和 chrome 策略就绪前，避免 widget 启动平台系统移动/缩放。
bool canBeginSystemOperation(QWidget* window,
                             const WindowChromeOptions& options,
                             bool resize,
                             Qt::Edges edges = Qt::Edges()) {
    if (!window || !window->isWindow() || !window->isVisible())
        return false;

    if (resize && edges == Qt::Edges())
        return false;

    if (!window->windowHandle())
        return false;

    if (WindowChromeCompat::currentPlatform() == WindowChromeCompat::Platform::Windows) {
        if (!options.useCustomWindowChrome)
            return false;
#ifdef Q_OS_WIN
        if (!window->windowFlags().testFlag(Qt::ExpandedClientAreaHint))
            return false;
#endif
    }

    return true;
}

}

WindowChromeCompat::WindowChromeCompat(QWidget* window)
    : m_window(window) {
    m_options.useCustomWindowChrome = platformPrefersCustomWindowChrome();
    m_options.preferNativeMacControls = (currentPlatform() == Platform::MacOS);
}

void WindowChromeCompat::configure(const WindowChromeOptions& options) {
    m_options = options;
    detail::syncPlatformTitleBarGeometry(m_window, m_options);
}

void WindowChromeCompat::applyPlatformWindowFlags() {
    detail::applyPlatformWindowFlags(m_window, m_options);
}

bool WindowChromeCompat::systemBackdropSupported() const {
    return detail::platformSupportsSystemBackdrop();
}

bool WindowChromeCompat::applySystemBackdrop(bool dark) {
    return detail::applyPlatformSystemBackdrop(m_window, dark);
}

bool WindowChromeCompat::handleNativeEvent(const QByteArray& eventType,
                                           void* message,
                                           FluentNativeEventResult* result) {
    return detail::handlePlatformNativeEvent(m_window, m_options, eventType, message, result);
}

bool WindowChromeCompat::beginSystemMove(const QPoint& globalPos) {
    if (!canBeginSystemOperation(m_window, m_options, false))
        return false;

    if (m_window->windowHandle()->startSystemMove())
        return true;

    return detail::beginPlatformSystemMove(m_window, globalPos);
}

bool WindowChromeCompat::beginSystemResize(Qt::Edges edges, const QPoint& globalPos) {
    if (!canBeginSystemOperation(m_window, m_options, true, edges))
        return false;

    if (m_window->windowHandle()->startSystemResize(edges))
        return true;

    return detail::beginPlatformSystemResize(m_window, edges, globalPos);
}

bool WindowChromeCompat::performTitleBarDoubleClick() {
    return detail::performPlatformTitleBarDoubleClick(m_window, m_options);
}

bool WindowChromeCompat::showSystemMenu(const QPoint& globalPos) {
    return detail::showPlatformSystemMenu(m_window, globalPos);
}

bool WindowChromeCompat::usesCustomWindowChrome() const {
    return m_options.useCustomWindowChrome && currentPlatform() == Platform::Windows;
}

bool WindowChromeCompat::prefersNativeMacControls() const {
    return m_options.preferNativeMacControls && currentPlatform() == Platform::MacOS;
}

int WindowChromeCompat::nativeTitleBarLeadingInset() const {
    return prefersNativeMacControls() ? detail::nativeTitleBarLeadingInset(m_window) : 0;
}

int WindowChromeCompat::nativeTitleBarTrailingInset() const {
    return 0;
}

WindowChromeCompat::HitTest WindowChromeCompat::hitTestLocal(const QPoint& localPos) const {
    const QSize size = m_window ? m_window->size() : QSize();
    return classifyHitTest(m_options, size, localPos);
}

WindowChromeCompat::Platform WindowChromeCompat::currentPlatform() {
#ifdef Q_OS_WIN
    return Platform::Windows;
#elif defined(Q_OS_MAC)
    return Platform::MacOS;
#else
    return Platform::Other;
#endif
}

bool WindowChromeCompat::platformPrefersCustomWindowChrome() {
    return currentPlatform() == Platform::Windows;
}

WindowChromeCompat::HitTest WindowChromeCompat::classifyHitTest(
    const WindowChromeOptions& options,
    const QSize& windowSize,
    const QPoint& localPos) {
    if (!QRect(QPoint(0, 0), windowSize).contains(localPos))
        return HitTest::Client;

    // Chrome locked (modal): no move, no resize — the whole window behaves as client.
    // zh_CN: chrome 锁定(模态):不可移动、不可缩放——整个窗口按 client 处理。
    if (!options.chromeInteractive)
        return HitTest::Client;

    const int border = qMax(0, options.resizeBorderWidth);
    if (border > 0) {
        const bool left = localPos.x() < border;
        const bool right = localPos.x() >= windowSize.width() - border;
        const bool top = localPos.y() < border;
        const bool bottom = localPos.y() >= windowSize.height() - border;

        if (top && left) return HitTest::TopLeft;
        if (top && right) return HitTest::TopRight;
        if (bottom && left) return HitTest::BottomLeft;
        if (bottom && right) return HitTest::BottomRight;
        if (left) return HitTest::Left;
        if (right) return HitTest::Right;
        if (top) return HitTest::Top;
        if (bottom) return HitTest::Bottom;
    }

    if (options.titleBarRect.contains(localPos)) {
        // Child controls hosted inside the title bar remain normal client areas.
        // zh_CN: 标题栏内承载的子控件仍然保持普通 client 区域语义。
        for (const QRect& r : options.dragExclusionRects) {
            if (r.contains(localPos))
                return HitTest::Client;
        }
        return HitTest::Caption;
    }

    return HitTest::Client;
}

} // namespace compatibility
