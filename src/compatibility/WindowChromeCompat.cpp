#include "WindowChromeCompat.h"
#include "private/WindowChromeBackend_p.h"

#include <QWindow>
#include <QColor>
#include <QPalette>
#include <QPointer>
#include <QVariant>

#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "compatibility/private/WindowBackdropTransition_p.h"

namespace compatibility {

namespace detail {

namespace {
constexpr char kPaintedNativeSurfaceProperty[] = "_fluentBackdropPaintedNativeId";
constexpr char kOpaqueNativeColorProperty[] = "_fluentBackdropOpaqueBaseColor";
constexpr char kPreparationRevisionProperty[] = "_fluentBackdropPreparationRevision";
} // namespace

bool windowBackdropSurfaceWasPainted(const QWidget* window)
{
    return window && window->internalWinId()
        && window->property(kPaintedNativeSurfaceProperty).toULongLong()
        == static_cast<qulonglong>(window->internalWinId());
}

void markWindowBackdropSurfacePainted(QWidget* window)
{
    if (window && window->internalWinId() && !windowBackdropSurfaceWasPainted(window))
        window->setProperty(kPaintedNativeSurfaceProperty,
            QVariant::fromValue(static_cast<qulonglong>(window->internalWinId())));
}

void resetWindowBackdropSurfacePaint(QWidget* window)
{
    if (window)
        window->setProperty(kPaintedNativeSurfaceProperty, QVariant());
}

QColor windowBackdropSurfaceColor(const QWidget* window)
{
    if (!window)
        return QColor();
    const QColor prepared = window->property(kOpaqueNativeColorProperty).value<QColor>();
    return prepared.isValid() ? prepared : window->palette().color(QPalette::Window);
}

void prepareWindowBackdropSurface(QWidget* window,
    const QColor& activeColor,
    const QColor& inactiveColor,
    bool active,
    fluent::windowing::BackdropSurfaceMode mode)
{
    if (!window || !activeColor.isValid() || !inactiveColor.isValid())
        return;
    QPointer<QWidget> guard(window);
    const qulonglong revision = window->property(kPreparationRevisionProperty).toULongLong() + 1;
    const auto current = [&] {
        return guard && guard->property(kPreparationRevisionProperty).toULongLong() == revision;
    };
    window->setProperty(kPreparationRevisionProperty, QVariant::fromValue(revision));
    if (!current())
        return;
    QColor enabled = activeColor;
    QColor inactive = inactiveColor;
    enabled.setAlpha(255);
    inactive.setAlpha(255);
    const QColor color = active ? enabled : inactive;
    QPalette palette = window->palette();
    palette.setColor(QPalette::Active, QPalette::Window, enabled);
    palette.setColor(QPalette::Inactive, QPalette::Window, inactive);
    palette.setColor(QPalette::Disabled, QPalette::Window, inactive);
    // Pick from the prepared active/inactive window colors without re-broadcasting a new palette to children.
    // zh_CN: 从预设的激活/非激活底色中选择，避免向子控件广播 palette 变更；同时与 Disabled 调色板保持独立。
    if (windowBackdropSurfaceColor(window) != color || !window->property(kOpaqueNativeColorProperty).isValid())
        window->setProperty(kOpaqueNativeColorProperty, color);
    if (!current())
        return;
    if (window->palette() != palette)
        window->setPalette(palette);
    // PaletteChange is synchronous and can supersede this effect request.
    // zh_CN: PaletteChange 同步发送，其回调可能已用新效果请求替代本次准备。
    if (!current())
        return;
    // internalWinId() must not create a native handle before the alpha format is selected.
    // zh_CN: 不得在 alpha 格式确定之前，因准备底色而提前创建原生句柄。
    if (window->internalWinId())
        preparePlatformWindowBackdropSurface(window, color, mode);
}

bool requiresOpaqueBackdropCommit(const QWidget* window,
    const fluent::windowing::BackdropState& previous,
    fluent::windowing::BackdropSurfaceMode next)
{
    return window && previous.surfaceMode == fluent::windowing::BackdropSurfaceMode::CompositedTransparent
        && next != fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
}

bool flushWindowBackdropSurface(QWidget* window)
{
    if (!window || !window->isVisible() || window->isMinimized() || !window->updatesEnabled() || !window->windowHandle()
        || !window->windowHandle()->isExposed())
        return false;
    return flushPlatformWindowBackdropSurface(window);
}
} // namespace detail

namespace {

// Guard against starting OS move/resize before the native window handle and flags are ready.
// zh_CN: 原生窗口句柄和边框标志就绪前不发起系统移动或缩放。
bool canBeginSystemOperation(QWidget* window,
    const WindowChromeOptions& options,
    bool resize,
    Qt::Edges edges = Qt::Edges())
{
    if (!window || !window->isWindow() || !window->isVisible())
        return false;

    if (resize && edges == Qt::Edges())
        return false;

    if (!window->windowHandle())
        return false;

    if (WindowChromeCompat::currentPlatform() == WindowChromeCompat::Platform::Windows) {
        if (!options.useCustomWindowChrome)
            return false;
        if (WindowChromeCompat::expandedClientAreaHintsAvailable()
            && !WindowChromeCompat::windowHasExpandedClientAreaHint(window)) {
            return false;
        }
    }

    return true;
}

} // namespace

WindowChromeCompat::WindowChromeCompat(QWidget* window) : m_window(window)
{
    m_options.useCustomWindowChrome = platformPrefersCustomWindowChrome();
    m_options.preferNativeMacControls = (currentPlatform() == Platform::MacOS);
}

void WindowChromeCompat::configure(const WindowChromeOptions& options)
{
    m_options = options;
    detail::syncPlatformTitleBarGeometry(m_window, m_options);
}

void WindowChromeCompat::applyPlatformWindowFlags()
{
    detail::applyPlatformWindowFlags(m_window, m_options);
}

bool WindowChromeCompat::systemBackdropSupported() const
{
    const BackdropCapabilities capabilities = backdropCapabilities();
    return capabilities.nativeMica || capabilities.nativeAcrylic || capabilities.compositorBlur;
}

BackdropCapabilities WindowChromeCompat::backdropCapabilities() const
{
    return detail::platformBackdropCapabilities();
}

bool WindowChromeCompat::applySystemBackdrop(BackdropEffect effect, bool dark, bool forceRecomposite)
{
    return applySystemBackdropDetailed(effect, dark, forceRecomposite).applied;
}

BackdropApplyResult
WindowChromeCompat::applySystemBackdropDetailed(BackdropEffect effect, bool dark, bool forceRecomposite)
{
    return detail::applyPlatformSystemBackdrop(m_window, effect, dark, forceRecomposite);
}

bool WindowChromeCompat::handleNativeEvent(const QByteArray& eventType, void* message, FluentNativeEventResult* result)
{
    return detail::handlePlatformNativeEvent(m_window, m_options, eventType, message, result);
}

bool WindowChromeCompat::beginSystemMove(const QPoint& globalPos)
{
    if (!canBeginSystemOperation(m_window, m_options, false))
        return false;

    if (m_window->windowHandle()->startSystemMove())
        return true;

    return detail::beginPlatformSystemMove(m_window, globalPos);
}

bool WindowChromeCompat::beginSystemResize(Qt::Edges edges, const QPoint& globalPos)
{
    if (!canBeginSystemOperation(m_window, m_options, true, edges))
        return false;

    if (m_window->windowHandle()->startSystemResize(edges))
        return true;

    return detail::beginPlatformSystemResize(m_window, edges, globalPos);
}

bool WindowChromeCompat::performTitleBarDoubleClick()
{
    return detail::performPlatformTitleBarDoubleClick(m_window, m_options);
}

bool WindowChromeCompat::showSystemMenu(const QPoint& globalPos)
{
    return detail::showPlatformSystemMenu(m_window, globalPos);
}

bool WindowChromeCompat::requestForegroundActivation()
{
    return detail::requestPlatformForegroundActivation(m_window);
}

bool WindowChromeCompat::usesCustomWindowChrome() const
{
    const Platform platform = currentPlatform();
    return m_options.useCustomWindowChrome
        && (platform == Platform::Windows || platform == Platform::Linux || platform == Platform::Other);
}

bool WindowChromeCompat::prefersNativeMacControls() const
{
    return m_options.preferNativeMacControls && currentPlatform() == Platform::MacOS;
}

bool WindowChromeCompat::manualMoveResizeFallbackAllowed() const
{
    return detail::manualMoveResizeFallbackAllowed(m_window, m_options);
}

int WindowChromeCompat::nativeTitleBarLeadingInset() const
{
    return prefersNativeMacControls() ? detail::nativeTitleBarLeadingInset(m_window) : 0;
}

int WindowChromeCompat::nativeTitleBarTrailingInset() const
{
    return 0;
}

int WindowChromeCompat::clientSideFrameMargin() const
{
    return detail::clientSideFrameMargin(m_window, m_options);
}

WindowChromeCompat::HitTest WindowChromeCompat::hitTestLocal(const QPoint& localPos) const
{
    const QSize size = m_window ? m_window->size() : QSize();
    return classifyHitTest(m_options, size, localPos);
}

WindowChromeCompat::Platform WindowChromeCompat::currentPlatform()
{
#ifdef Q_OS_WIN
    return Platform::Windows;
#elif defined(Q_OS_MAC)
    return Platform::MacOS;
#elif defined(Q_OS_LINUX)
    return Platform::Linux;
#else
    return Platform::Other;
#endif
}

bool WindowChromeCompat::platformPrefersCustomWindowChrome()
{
    return currentPlatform() == Platform::Windows || detail::runtimePlatformCapabilities().customWindowChromePreferred;
}

bool WindowChromeCompat::expandedClientAreaHintsAvailable()
{
    return QT_VERSION >= QT_VERSION_CHECK(6, 9, 0);
}

bool WindowChromeCompat::windowHasExpandedClientAreaHint(const QWidget* window)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return window && window->windowFlags().testFlag(Qt::ExpandedClientAreaHint);
#else
    Q_UNUSED(window);
    return false;
#endif
}

bool WindowChromeCompat::windowHasNoTitleBarBackgroundHint(const QWidget* window)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return window && window->windowFlags().testFlag(Qt::NoTitleBarBackgroundHint);
#else
    Q_UNUSED(window);
    return false;
#endif
}

WindowChromeCompat::HitTest
WindowChromeCompat::classifyHitTest(const WindowChromeOptions& options, const QSize& windowSize, const QPoint& localPos)
{
    if (!QRect(QPoint(0, 0), windowSize).contains(localPos))
        return HitTest::Client;

    // Modal lock: treat the entire window as non-draggable client area.
    // zh_CN: 模态锁定时禁用拖拽与缩放，整个窗口按客户区处理。
    if (!options.chromeInteractive)
        return HitTest::Client;

    const int border = qMax(0, options.resizeBorderWidth);
    if (border > 0) {
        const bool left = localPos.x() < border;
        const bool right = localPos.x() >= windowSize.width() - border;
        const bool top = localPos.y() < border;
        const bool bottom = localPos.y() >= windowSize.height() - border;

        if (top && left)
            return HitTest::TopLeft;
        if (top && right)
            return HitTest::TopRight;
        if (bottom && left)
            return HitTest::BottomLeft;
        if (bottom && right)
            return HitTest::BottomRight;
        if (left)
            return HitTest::Left;
        if (right)
            return HitTest::Right;
        if (top)
            return HitTest::Top;
        if (bottom)
            return HitTest::Bottom;
    }

    if (options.titleBarRect.contains(localPos)) {
        // Interactive controls inside the title bar remain clickable client areas.
        // zh_CN: 标题栏内的子控件区域保持普通客户区行为。
        for (const QRect& r : options.dragExclusionRects) {
            if (r.contains(localPos))
                return HitTest::Client;
        }
        return HitTest::Caption;
    }

    return HitTest::Client;
}

} // namespace compatibility
