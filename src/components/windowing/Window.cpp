#include "Window.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QRegion>
#include <QWindow>
#include <QtMath>

#include "TitleBar.h"
#include "WindowBackdropMaterial.h"
#include "WindowChromeFrame.h"
#include "components/windowing/private/WindowBackdrop_p.h"
#include "design/Breakpoints.h"
#include "design/Typography.h"
#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/status_info/ToolTip.h"
#include "utils/Log.h"

namespace fluent::windowing {

namespace {

constexpr int CaptionButtonWidth = 46;
constexpr int CaptionButtonIconSize = 10;
constexpr int ResizeBorderWidth = 8;
constexpr int VisibleFrameResizeBorderWidth = ResizeBorderWidth;

bool requiresAlphaSurface(const BackdropCapabilities& capabilities, int frameMargin)
{
    return capabilities.supportsTransparentMaterial(BackdropEffect::Mica)
        || capabilities.supportsTransparentMaterial(BackdropEffect::Acrylic)
        || frameMargin > 0;
}

void registerBackdropMetaTypes()
{
    static const bool registered = [] {
        qRegisterMetaType<BackdropEffect>("fluent::windowing::BackdropEffect");
        qRegisterMetaType<BackdropState>("fluent::windowing::BackdropState");
        // Qt 5 moc may store unqualified names for namespace-local property and
        // signal parameters; register both spellings for old-style reflection.
        qRegisterMetaType<BackdropEffect>("BackdropEffect");
        qRegisterMetaType<BackdropState>("BackdropState");
        return true;
    }();
    Q_UNUSED(registered);
}

void refreshFluentDescendants(QWidget* root)
{
    if (!root)
        return;

    const auto widgets = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* widget : widgets) {
        if (auto* fluentWidget = dynamic_cast<FluentElement*>(widget))
            fluentWidget->onThemeUpdated();
        refreshFluentDescendants(widget);
    }
}

QPoint pointerLocalPosForWindow(QWidget* source, QWidget* window, QMouseEvent* event)
{
    if (!source || !window || !event)
        return QPoint();
    const QPoint sourcePos = fluentMousePos(event);
    return source == window ? sourcePos : source->mapTo(window, sourcePos);
}

QRegion roundedRectRegion(const QRect& rect, int radius)
{
    if (rect.isEmpty() || radius <= 0)
        return QRegion(rect);

    const int diameter = qMin(radius * 2, qMin(rect.width(), rect.height()));
    const int effectiveRadius = diameter / 2;
    QRegion region(rect.adjusted(effectiveRadius, 0, -effectiveRadius, 0));
    region += QRegion(rect.adjusted(0, effectiveRadius, 0, -effectiveRadius));
    region += QRegion(QRect(rect.topLeft(), QSize(diameter, diameter)), QRegion::Ellipse);
    region += QRegion(QRect(QPoint(rect.right() - diameter + 1, rect.top()),
                            QSize(diameter, diameter)),
                      QRegion::Ellipse);
    region += QRegion(QRect(QPoint(rect.left(), rect.bottom() - diameter + 1),
                            QSize(diameter, diameter)),
                      QRegion::Ellipse);
    region += QRegion(QRect(QPoint(rect.right() - diameter + 1,
                                   rect.bottom() - diameter + 1),
                            QSize(diameter, diameter)),
                      QRegion::Ellipse);
    return region;
}

} // namespace

Window::Window(QWidget* parent)
    : QWidget(parent),
      m_chrome(this),
      m_resizeSession(std::make_unique<WindowResizeSession>()) {
    registerBackdropMetaTypes();
    m_chrome.applyPlatformWindowFlags();

    setAutoFillBackground(false);
    setMinimumSize(Breakpoints::MinWindowWidth, Breakpoints::MinWindowHeight);

    // Keep top-level translucency as a platform-level decision. Runtime effect
    // changes update paint hints and requested backdrop type, not native flags.
    // zh_CN: 顶层半透明是平台级决策；运行时切换效果只更新绘制提示和请求的系统背景类型。
    refreshBackdropCapabilities();
    m_windowTranslucent = requiresAlphaSurface(
        m_backdropCapabilities, m_chrome.clientSideFrameMargin());
    if (m_windowTranslucent)
        setAttribute(Qt::WA_TranslucentBackground, true);
    setEffectiveBackdropState(paintedFallbackState(
        m_backdropCapabilities.supportsTransparentMaterial(m_backdropEffect)
            ? QStringLiteral("platform-backdrop-pending")
            : QStringLiteral("painted-fallback")));

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // Only Linux needs an inset, rounded client-side frame host. Keeping this
    // wrapper out of the Windows/macOS widget tree preserves their original
    // direct paint path and avoids routing every visible update through an
    // otherwise transparent full-window child.
    // zh_CN: 仅 Linux 需要内缩圆角客户端窗框容器；Windows/macOS 保持原来的直接绘制路径，
    // 避免每次可见更新都经过一个透明的全窗口子控件。
    QWidget* chromeParent = this;
    QVBoxLayout* chromeLayout = m_rootLayout;
    if (compatibility::WindowChromeCompat::currentPlatform()
        == compatibility::WindowChromeCompat::Platform::Linux) {
        m_frameHost = new QWidget(this);
        m_frameHost->setObjectName(QStringLiteral("fluentWindowFrameHost"));
        m_frameHost->setAutoFillBackground(false);
        auto* frameLayout = new QVBoxLayout(m_frameHost);
        frameLayout->setContentsMargins(0, 0, 0, 0);
        frameLayout->setSpacing(0);
        m_rootLayout->addWidget(m_frameHost, 1);
        chromeParent = m_frameHost;
        chromeLayout = frameLayout;
    }

    m_titleBar = new TitleBar(chromeParent);
    m_titleBar->setSystemReservedLeadingWidth(m_chrome.nativeTitleBarLeadingInset());
    m_titleBar->setVisible(m_chrome.usesCustomWindowChrome() || m_chrome.prefersNativeMacControls());
    chromeLayout->addWidget(m_titleBar);
    setupCaptionButtons();

    m_contentHost = new QWidget(chromeParent);
    m_contentHost->setObjectName(QStringLiteral("fluentWindowContentHost"));
    m_contentHost->setAutoFillBackground(false);
    auto* contentLayout = new QVBoxLayout(m_contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    chromeLayout->addWidget(m_contentHost, 1);

    if (m_frameHost) {
        m_frameEdgeOverlay = new ClientSideFrameEdgeOverlay(this);
        m_frameEdgeOverlay->setObjectName(QStringLiteral("fluentWindowFrameEdgeOverlay"));
        m_frameEdgeOverlay->setMouseEventHandler(
            [this](QWidget* source, QMouseEvent* event) {
                return handleResizeBorderMouseEvent(source, event);
            });
        m_frameEdgeOverlay->hide();
    }

    connect(m_titleBar, &TitleBar::chromeGeometryChanged, this, &Window::updateChromeOptions);
    connect(m_titleBar, &TitleBar::dragStarted, this, &Window::handleTitleBarDragStarted);
    connect(m_titleBar, &TitleBar::dragMoved, this, &Window::handleTitleBarDragMoved);
    connect(m_titleBar, &TitleBar::dragFinished, this, &Window::handleTitleBarDragFinished);
    connect(m_titleBar, &TitleBar::doubleClicked, this, &Window::handleTitleBarDoubleClicked);
    connect(m_titleBar, &TitleBar::contextMenuRequested, this, &Window::handleTitleBarContextMenuRequested);
    connect(m_titleBar, &TitleBar::titleBarHeightChanged, this, &Window::syncCaptionButtons);

    onThemeUpdated();
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
}

Window::~Window() {
    if (QWidget::mouseGrabber() == this)
        releaseMouse();
}

void Window::setContentWidget(QWidget* widget) {
    if (m_contentWidget == widget)
        return;

    auto* contentLayout = qobject_cast<QVBoxLayout*>(m_contentHost->layout());
    if (m_contentWidget) {
        contentLayout->removeWidget(m_contentWidget);
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (widget) {
        widget->setParent(m_contentHost);
        contentLayout->addWidget(widget);
        refreshFluentDescendants(m_contentHost);
    }
}

void Window::setCustomWindowChromeEnabled(bool enabled) {
    compatibility::WindowChromeOptions options = m_chrome.options();
    if (options.useCustomWindowChrome == enabled)
        return;

    options.useCustomWindowChrome = enabled;
    m_chrome.configure(options);
    m_chrome.applyPlatformWindowFlags();

    refreshBackdropCapabilities();
    const bool resolvedTranslucency = requiresAlphaSurface(
        m_backdropCapabilities, m_chrome.clientSideFrameMargin());
    // Once an alpha-capable native surface has been requested, keep it sticky.
    // Disabling it on a live QWidget can recreate the platform surface and
    // produce focus churn/black edges; opaque modes simply paint every pixel.
    // zh_CN: 顶层 alpha surface 一旦启用便保持不变；不透明模式通过完整绘制覆盖。
    m_windowTranslucent = m_windowTranslucent || resolvedTranslucency;
    setAttribute(Qt::WA_TranslucentBackground, m_windowTranslucent);
    resolveBackdropState(isVisible());

    setupCaptionButtons();
    syncClientSideResizeInput();
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
    update();
}

bool Window::customWindowChromeEnabled() const {
    return m_chrome.options().useCustomWindowChrome;
}

void Window::refreshBackdropCapabilities()
{
    m_backdropCapabilities = m_chrome.backdropCapabilities();
}

BackdropState Window::paintedFallbackState(const QString& reason) const
{
    BackdropState state;
    state.requestedEffect = m_backdropEffect;
    state.effectiveEffect = m_backdropEffect;
    state.reason = reason;

    if (m_backdropEffect == BackdropEffect::Solid) {
        state.backend = BackdropBackend::Solid;
        state.fidelity = BackdropFidelity::Solid;
        state.surfaceMode = BackdropSurfaceMode::SolidOpaque;
        return state;
    }

    state.backend = BackdropBackend::PaintedMaterial;
    state.fidelity = BackdropFidelity::Emulated;
    state.surfaceMode = BackdropSurfaceMode::PaintedOpaque;
    return state;
}

void Window::setEffectiveBackdropState(const BackdropState& state)
{
    const bool changed = m_backdropState != state;
    m_backdropState = state;

    // Keep the old dynamic properties as compatibility aliases while all new
    // consumers use the typed state published by WindowBackdrop.
    // zh_CN: 保留旧动态属性作为兼容别名；新消费者统一读取 WindowBackdrop 发布的类型化状态。
    setProperty("fluentWindowBackdropEffect", static_cast<int>(state.requestedEffect));
    setProperty("fluentMicaBackdrop",
                state.surfaceMode == BackdropSurfaceMode::CompositedTransparent);
    setProperty("fluentBackdropSurfaceMode", static_cast<int>(state.surfaceMode));
    setProperty("fluentBackdropBackend", static_cast<int>(state.backend));
    publishWindowBackdropState(this, state);

    if (changed) {
        LOG_DEBUG(QStringLiteral(
                      "Window backdrop resolved requested=%1 effective=%2 backend=%3 "
                      "fidelity=%4 surface=%5 platformApplied=%6 provider=%7 reason=%8")
                      .arg(static_cast<int>(state.requestedEffect))
                      .arg(static_cast<int>(state.effectiveEffect))
                      .arg(static_cast<int>(state.backend))
                      .arg(static_cast<int>(state.fidelity))
                      .arg(static_cast<int>(state.surfaceMode))
                      .arg(state.platformApplied)
                      .arg(m_backdropCapabilities.provider, state.reason));
        update();
        const QList<QWidget*> descendants = findChildren<QWidget*>();
        for (QWidget* child : descendants)
            child->update();
        emit backdropStateChanged(m_backdropState);
    }
}

void Window::scheduleBackdropResolution()
{
    if (m_backdropResolutionPending)
        return;
    m_backdropResolutionPending = true;
    QTimer::singleShot(0, this, [this] {
        m_backdropResolutionPending = false;
        refreshBackdropCapabilities();
        updateChromeOptions();
        resolveBackdropState(isVisible(), /*forceRecomposite*/ isVisible());
        syncClientSideFrameShape();
        update();
    });
}

void Window::scheduleNativeChromeRepair()
{
    if (compatibility::WindowChromeCompat::currentPlatform()
            != compatibility::WindowChromeCompat::Platform::Windows
        || m_nativeChromeRepairPending) {
        return;
    }

    m_nativeChromeRepairPending = true;
    QTimer::singleShot(0, this, [this] {
        m_nativeChromeRepairPending = false;
        if (!isVisible())
            return;

        // Qt 5/6.2 can rewrite the Win32 style after a native handle, DPI, or
        // window-state transition. Re-assert custom chrome after that event has
        // fully unwound so WS_THICKFRAME and the client-area hit test remain paired.
        // zh_CN: Qt 5/6.2 可能在原生句柄、DPI 或窗口状态切换后重写 Win32 style；
        // 待事件结束后重新施加自定义 chrome，确保 WS_THICKFRAME 与客户区命中测试保持配对。
        m_chrome.applyPlatformWindowFlags();
        updateChromeOptions();
    });
}

void Window::resolveBackdropState(bool applyPlatform, bool forceRecomposite)
{
    BackdropState next = paintedFallbackState(
        m_backdropEffect == BackdropEffect::Solid
            ? QStringLiteral("solid-requested")
            : QStringLiteral("painted-fallback"));

    const bool canUsePlatform = m_backdropCapabilities.supportsTransparentMaterial(
        m_backdropEffect);
    if (applyPlatform && (m_backdropEffect == BackdropEffect::Solid || canUsePlatform)) {
        const BackdropApplyResult applied = m_chrome.applySystemBackdropDetailed(
            m_backdropEffect,
            effectiveTheme() == Dark,
            forceRecomposite);
        if (applied.applied) {
            next.backend = applied.backend;
            next.fidelity = applied.fidelity;
            next.surfaceMode = applied.surfaceMode;
            next.platformApplied =
                applied.surfaceMode == BackdropSurfaceMode::CompositedTransparent;
            next.reason = applied.reason;
        } else if (!applied.reason.isEmpty()) {
            next.reason = applied.reason;
        }
    } else if (!applyPlatform && canUsePlatform) {
        next.reason = QStringLiteral("platform-backdrop-pending");
    } else if (m_backdropEffect != BackdropEffect::Solid
               && !m_backdropCapabilities.provider.isEmpty()) {
        next.reason = QStringLiteral("%1-fallback").arg(m_backdropCapabilities.provider);
    }

    setEffectiveBackdropState(next);
}

void Window::onThemeUpdated() {
    // Keep the native backdrop tint in step with this window's effective theme.
    // zh_CN: 让原生背景着色跟随窗口的实际主题。
    resolveBackdropState(isVisible());
    if (m_titleBar)
        m_titleBar->onThemeUpdated();
    if (m_minimizeButton)
        m_minimizeButton->onThemeUpdated();
    if (m_maximizeButton)
        m_maximizeButton->onThemeUpdated();
    if (m_closeButton)
        m_closeButton->onThemeUpdated();
    syncClientSideFrameShape();
    // Content descendants are themed by the global FluentThemeManager; walking
    // the whole prewarmed page tree here caused slow theme switches.
    // zh_CN: 内容子级由全局主题管理器刷新；这里不再遍历整棵预热页面树。
    update();
}

void Window::reapplySystemBackdrop() {
    if (!isVisible())
        return;
    refreshBackdropCapabilities();
    updateChromeOptions();                                 // re-extend the sheet-of-glass
    resolveBackdropState(/*applyPlatform*/ true, /*forceRecomposite*/ true);
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    update();
}

void Window::prepareForNativeRestore()
{
    updateChromeOptions();
    m_chrome.applyPlatformWindowFlags();
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
}

void Window::requestForegroundActivation()
{
    if (!isVisible())
        show();
    raise();
    activateWindow();
    if (QWindow* handle = windowHandle())
        handle->requestActivate();
    m_chrome.requestForegroundActivation();
}

void Window::setBackdropEffect(BackdropEffect effect) {
    if (m_backdropEffect == effect)
        return;
    m_backdropEffect = effect;

    // Switching effects updates paint hints and the requested OS backdrop type.
    // Avoid toggling native window flags at runtime to prevent flicker/focus churn.
    // zh_CN: 切换效果只更新绘制提示和请求的系统背景类型，避免运行时重置原生窗口 flag。
    // Deliberate effect switches resolve the actual backend before descendants
    // are allowed to clear the top-level backing store.
    // zh_CN: 用户主动切换效果时先解析实际后端，再允许后代控件清除顶层后备缓冲。
    resolveBackdropState(isVisible(), /*forceRecomposite*/ isVisible());
    emit backdropEffectChanged(m_backdropEffect);

}

void Window::minimizeWindow() {
    emit minimizeRequested();
    showMinimized();
}

void Window::toggleMaximizeRestore() {
    if (isMaximized()) {
        emit restoreRequested();
        showNormal();
    } else {
        emit maximizeRequested();
        showMaximized();
    }
    updateMaximizeButtonIcon();
    updateChromeOptions();
}

void Window::closeWindow() {
    emit closeRequested();
    close();
}

ClientSideFramePaintOptions Window::clientSideFramePaintOptions() const {
    const auto& colors = themeColors();
    QColor fill = chromeBackdropFill(this, isActiveWindow());
    if (!fill.isValid())
        fill = themeBackdrop(isActiveWindow());

    ClientSideFramePaintOptions options;
    options.windowRect = rect();
    options.frameRect = windowFrameRect();
    options.fill = fill;
    options.stroke = colors.strokeDefault;
    options.radius = qMax<qreal>(themeRadius().overlay, themeRadius().control);
    options.shadow = themeShadow(Elevation::VeryHigh);
    options.dark = effectiveTheme() == Dark;
    options.useTranslucentMaterial =
        m_backdropState.surfaceMode == BackdropSurfaceMode::CompositedTransparent;
    options.usePaintedMaterial =
        m_backdropState.surfaceMode == BackdropSurfaceMode::PaintedOpaque;
    options.effect = m_backdropEffect;
    options.material = WindowBackdropMaterialOptions::forTheme(
        effectiveTheme() == Dark,
        colors.bgCanvas,
        colors.accentDefault);
    options.material.effect = m_backdropEffect;
    options.material.active = isActiveWindow();
    options.material.devicePixelRatio = devicePixelRatioF();
    return options;
}

void Window::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    const int frameMargin = activeClientSideFrameMargin();
    if (frameMargin > 0) {
        paintClientSideFrame(painter, clientSideFramePaintOptions());
        return;
    }

    // Transparent top-level widgets keep their backing-store pixels between frames on macOS.
    // Replace them explicitly so page or material switches cannot retain the previous frame.
    // zh_CN: macOS 透明顶层控件会跨帧保留 backing-store 像素，这里显式清空。
    if (m_backdropState.surfaceMode == BackdropSurfaceMode::CompositedTransparent) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        return;
    }

    if (m_backdropState.surfaceMode == BackdropSurfaceMode::PaintedOpaque) {
        const auto& colors = themeColors();
        WindowBackdropMaterialOptions options = WindowBackdropMaterialOptions::forTheme(
            effectiveTheme() == Dark,
            colors.bgCanvas,
            colors.accentDefault);
        options.effect = m_backdropEffect;
        options.active = isActiveWindow();
        options.devicePixelRatio = devicePixelRatioF();
        WindowBackdropMaterial::paint(painter, QRectF(rect()), options);
        return;
    }

    const auto& colors = themeColors();
    // Use the same active/inactive backdrop source as the title bar and nav pane.
    // zh_CN: 使用与标题栏、导航栏一致的激活/非激活背景来源，避免 Normal 模式接缝。
    const QColor backdrop = chromeBackdropFill(this, isActiveWindow());
    painter.fillRect(rect(), backdrop.isValid() ? backdrop : themeBackdrop(isActiveWindow()));

    if (m_chrome.usesCustomWindowChrome()) {
        painter.setPen(colors.strokeDefault);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

void Window::mousePressEvent(QMouseEvent* event) {
    if (handleResizeBorderMouseEvent(this, event))
        return;

    QWidget::mousePressEvent(event);
}

void Window::mouseMoveEvent(QMouseEvent* event) {
    if (handleResizeBorderMouseEvent(this, event))
        return;

    QWidget::mouseMoveEvent(event);
}

void Window::mouseReleaseEvent(QMouseEvent* event) {
    if (handleResizeBorderMouseEvent(this, event))
        return;

    QWidget::mouseReleaseEvent(event);
}

void Window::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
}

void Window::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_chrome.applyPlatformWindowFlags();
    refreshBackdropCapabilities();
    resolveBackdropState(/*applyPlatform*/ true);
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
    scheduleNativeChromeRepair();

    // Re-assert the system backdrop after the first event-loop turn, once the
    // native window is realized and placed.
    // zh_CN: 首次显示后等原生窗口完成创建和定位，再重新施加系统背景。
    if (m_backdropCapabilities.supportsTransparentMaterial(m_backdropEffect)
        && !m_backdropPrimed) {
        m_backdropPrimed = true;
        scheduleBackdropResolution();
    }
}

void Window::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);

    if (fluentIsWindowInsetChangeEvent(event)) {
        syncTitleBarSystemInsets();
        syncCaptionButtons();
        updateChromeOptions();
    }

    // Repaint the deepest backdrop on activation changes so it stays in sync
    // with title/nav focus tint.
    // zh_CN: 窗口激活状态变化时重绘底层背景，使其与标题栏/导航栏焦点色同步。
    if (event->type() == QEvent::ActivationChange)
        update();
    if (event->type() == QEvent::WindowStateChange) {
        syncClientSideFrameMargins();
        syncClientSideFrameShape();
        syncTitleBarSystemInsets();
        updateChromeOptions();
        scheduleNativeChromeRepair();
    }
}

bool Window::event(QEvent* event)
{
    if (isWindowBackdropReevaluationEvent(event)) {
        scheduleBackdropResolution();
        return true;
    }

    const QEvent::Type type = event ? event->type() : QEvent::None;
    const bool handled = QWidget::event(event);
    bool nativeSurfaceMayHaveChanged = type == QEvent::WinIdChange
        || type == QEvent::ScreenChangeInternal
        || fluentIsDevicePixelRatioChangeEvent(event);
    if (nativeSurfaceMayHaveChanged && isVisible()) {
        scheduleBackdropResolution();
        scheduleNativeChromeRepair();
    }
    return handled;
}

bool Window::nativeEvent(const QByteArray& eventType,
                         void* message,
                         compatibility::FluentNativeEventResult* result) {
    return m_chrome.handleNativeEvent(eventType, message, result);
}

void Window::setChromeInteractive(bool interactive) {
    if (m_chromeInteractive == interactive)
        return;
    m_chromeInteractive = interactive;
    if (m_captionButtonHost)
        m_captionButtonHost->setEnabled(interactive);
    syncClientSideResizeInput();
    updateChromeOptions();
    if (interactive)
        scheduleNativeChromeRepair();
}

void Window::updateChromeOptions() {
    if (!m_titleBar)
        return;

    compatibility::WindowChromeOptions options = m_chrome.options();
    options.titleBarRect = m_titleBar->isVisible()
        ? QRect(m_titleBar->mapTo(this, QPoint(0, 0)), m_titleBar->size())
        : QRect();
    options.dragExclusionRects.clear();
    if (m_titleBar->isVisible()) {
        for (const QRect& rect : m_titleBar->dragExclusionRects()) {
            options.dragExclusionRects << QRect(m_titleBar->mapTo(this, rect.topLeft()), rect.size());
        }
    }
    options.resizeBorderWidth = ResizeBorderWidth;
    options.chromeInteractive = m_chromeInteractive;
    m_chrome.configure(options);

    const int reservedResizeBorder = m_chrome.usesCustomWindowChrome()
            && m_chromeInteractive && !isMaximized() && !isFullScreen()
        ? ResizeBorderWidth
        : 0;
    const char* resizeBorderProperty =
        ::fluent::overlay::windowResizeBorderWidthPropertyName();
    if (property(resizeBorderProperty).toInt() != reservedResizeBorder)
        setProperty(resizeBorderProperty, reservedResizeBorder);
}

void Window::syncTitleBarSystemInsets() {
    if (!m_titleBar)
        return;

    m_titleBar->setSystemReservedLeadingWidth(m_chrome.nativeTitleBarLeadingInset());
    syncCaptionButtons();
    m_titleBar->setSystemReservedTrailingWidth(captionButtonReservedWidth());
    m_titleBar->setVisible(m_chrome.usesCustomWindowChrome() || m_chrome.prefersNativeMacControls());
}

void Window::setupCaptionButtons() {
    if (!m_titleBar || !m_chrome.usesCustomWindowChrome() || m_captionButtonHost)
        return;

    m_captionButtonHost = new QWidget(m_titleBar);
    m_captionButtonHost->setObjectName(QStringLiteral("fluentWindowCaptionButtonHost"));
    m_captionButtonHost->setAttribute(Qt::WA_StyledBackground, false);
    m_captionButtonHost->setAutoFillBackground(false);

    auto* buttonLayout = new QHBoxLayout(m_captionButtonHost);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    auto createCaptionButton = [this](const QString& objectName,
                                      const QString& glyph,
                                      const QString& tooltip) {
        auto* button = new fluent::basicinput::Button(m_captionButtonHost);
        button->setObjectName(objectName);
        fluent::status_info::ToolTip::attach(button, tooltip);
        button->setFluentStyle(fluent::basicinput::Button::Subtle);
        button->setFluentLayout(fluent::basicinput::Button::IconOnly);
        button->setFluentSize(fluent::basicinput::Button::Small);
        button->setIconGlyph(glyph, CaptionButtonIconSize);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    };

    m_minimizeButton = createCaptionButton(QStringLiteral("fluentWindowMinimizeButton"),
                                           Typography::Icons::ChromeMinimize,
                                           QStringLiteral("Minimize"));
    m_maximizeButton = createCaptionButton(QStringLiteral("fluentWindowMaximizeButton"),
                                           Typography::Icons::ChromeMaximize,
                                           QStringLiteral("Maximize"));
    m_closeButton = createCaptionButton(QStringLiteral("fluentWindowCloseButton"),
                                        Typography::Icons::ChromeClose,
                                        QStringLiteral("Close"));
    m_closeButton->setCriticalOnHover(true);

    // Linux leaves the caption surface square and lets the top-level frame
    // overlay clip the complete client surface once. Windows and macOS retain
    // their established per-button corner radius.
    // zh_CN: Linux 保持标题按钮表面为直角，由顶层窗框 overlay 对完整客户区统一裁剪；
    // Windows/macOS 继续保留既有的按钮圆角。
    const int windowCornerRadius = m_frameHost ? 0 : themeRadius().control;
    m_minimizeButton->setCornerRadii(QMargins(0, 0, 0, 0));
    m_maximizeButton->setCornerRadii(QMargins(0, 0, 0, 0));
    m_closeButton->setCornerRadii(QMargins(0, windowCornerRadius, 0, 0));

    buttonLayout->addWidget(m_minimizeButton);
    buttonLayout->addWidget(m_maximizeButton);
    buttonLayout->addWidget(m_closeButton);

    connect(m_minimizeButton, &QPushButton::clicked, this, &Window::minimizeWindow);
    connect(m_maximizeButton, &QPushButton::clicked, this, &Window::toggleMaximizeRestore);
    connect(m_closeButton, &QPushButton::clicked, this, &Window::closeWindow);

    if (auto* anchorLayout = qobject_cast<fluent::AnchorLayout*>(m_titleBar->layout())) {
        fluent::AnchorLayout::Anchors anchors;
        anchors.right = {m_titleBar, fluent::AnchorLayout::Edge::Right, 0};
        anchors.verticalCenter = {m_titleBar, fluent::AnchorLayout::Edge::VCenter, 0};
        anchorLayout->addAnchoredWidget(m_captionButtonHost, anchors);
    }

    syncCaptionButtons();
}

void Window::syncCaptionButtons() {
    if (!m_captionButtonHost || !m_titleBar)
        return;

    // Fluent caption buttons are used only when this window owns custom chrome.
    // zh_CN: 仅在窗口拥有自定义 chrome 时显示 Fluent 标题栏按钮。
    const bool showCaptionButtons = m_chrome.usesCustomWindowChrome();
    const int buttonHeight = m_titleBar->titleBarHeight();
    const int buttonCount = 3;
    m_captionButtonHost->setFixedSize(CaptionButtonWidth * buttonCount, buttonHeight);
    for (auto* button : {m_minimizeButton, m_maximizeButton, m_closeButton}) {
        if (button)
            button->setFixedSize(CaptionButtonWidth, buttonHeight);
    }
    if (m_closeButton) {
        const int windowCornerRadius = m_frameHost ? 0 : themeRadius().control;
        m_closeButton->setCornerRadii(QMargins(0, windowCornerRadius, 0, 0));
    }
    m_captionButtonHost->setVisible(showCaptionButtons);
    updateMaximizeButtonIcon();
}

void Window::updateMaximizeButtonIcon() {
    if (!m_maximizeButton)
        return;

    fluent::status_info::ToolTip::attach(m_maximizeButton,
                                         isMaximized()
                                             ? QStringLiteral("Restore")
                                             : QStringLiteral("Maximize"));
    m_maximizeButton->setIconGlyph(isMaximized()
                                       ? Typography::Icons::ChromeRestore
                                       : Typography::Icons::ChromeMaximize,
                                   CaptionButtonIconSize);
}

int Window::captionButtonReservedWidth() const {
    if (m_captionButtonHost && m_chrome.usesCustomWindowChrome())
        return m_captionButtonHost->width() > 0 ? m_captionButtonHost->width() : CaptionButtonWidth * 3;

    return m_chrome.nativeTitleBarTrailingInset();
}

int Window::activeClientSideFrameMargin() const {
    if (isMaximized() || isFullScreen())
        return 0;
    return m_chrome.clientSideFrameMargin();
}

QRect Window::windowFrameRect() const {
    return chromeFrameRect();
}

QRect Window::chromeFrameRect() const {
    if (m_frameHost)
        return m_frameHost->geometry();

    const int margin = activeClientSideFrameMargin();
    return rect().marginsRemoved(QMargins(margin, margin, margin, margin));
}

void Window::syncClientSideFrameMargins() {
    if (!m_rootLayout)
        return;

    const int margin = activeClientSideFrameMargin();
    const QMargins margins(margin, margin, margin, margin);
    if (m_rootLayout->contentsMargins() == margins)
        return;

    m_rootLayout->setContentsMargins(margins);
    updateGeometry();
    syncClientSideFrameShape();
    update();
}

void Window::syncClientSideFrameShape() {
    const int frameMargin = activeClientSideFrameMargin();
    const QRect surface = (frameMargin > 0 ? chromeFrameRect() : rect()).intersected(rect());
    const QRect effectiveSurface = surface.isEmpty() ? rect() : surface;
    const char* surfaceProperty = ::fluent::overlay::overlaySurfaceRectPropertyName();
    if (property(surfaceProperty).toRect() != effectiveSurface)
        setProperty(surfaceProperty, effectiveSurface);

    // Windows and macOS do not own a client-side frame host. Stop here so
    // Linux-only hit zones and rounded-frame repaints never enter their
    // visible-window update path.
    // zh_CN: Windows/macOS 没有客户端窗框容器；在此直接返回，使 Linux 专属遮罩、
    // 命中区和圆角重绘完全不进入其可见窗口更新路径。
    if (!m_frameHost) {
        const char* radiusProperty = ::fluent::overlay::clientSideFrameRadiusPropertyName();
        if (!qFuzzyIsNull(property(radiusProperty).toDouble()))
            setProperty(radiusProperty, 0.0);
        return;
    }

    const qreal radius = (frameMargin > 0 && !m_frameHost->rect().isEmpty())
        ? qMax<qreal>(themeRadius().overlay, themeRadius().control)
        : 0.0;
    const char* radiusProperty = ::fluent::overlay::clientSideFrameRadiusPropertyName();
    if (!qFuzzyCompare(property(radiusProperty).toDouble() + 1.0, radius + 1.0))
        setProperty(radiusProperty, radius);
    if (!qFuzzyCompare(m_frameHost->property(radiusProperty).toDouble() + 1.0, radius + 1.0))
        m_frameHost->setProperty(radiusProperty, radius);
    if (radius > 0.0) {
        const QRegion frameMask = roundedRectRegion(m_frameHost->rect(), qCeil(radius));
        if (m_frameHost->mask() != frameMask)
            m_frameHost->setMask(frameMask);
    } else if (!m_frameHost->mask().isEmpty()) {
        m_frameHost->clearMask();
    }
    if (m_frameEdgeOverlay) {
        m_frameEdgeOverlay->setGeometry(rect());
        m_frameEdgeOverlay->setFrameVisualRect(chromeFrameRect());
        m_frameEdgeOverlay->setFrameRadius(radius);
        m_frameEdgeOverlay->setFrameStroke(themeColors().strokeDefault);
        m_frameEdgeOverlay->setVisible(radius > 0.0);
        const bool acceptsResizeInput = usesClientSideResizeInput()
            && m_chromeInteractive && radius > 0.0;
        if (acceptsResizeInput) {
            const QRect edgeRect = m_frameEdgeOverlay->rect();
            const int border = qMin(VisibleFrameResizeBorderWidth,
                                    qMax(1, qMin(edgeRect.width(), edgeRect.height()) / 2));
            m_frameEdgeOverlay->setResizeInputEnabled(true, border);
        } else {
            m_frameEdgeOverlay->setResizeInputEnabled(false, 1);
        }
        m_frameEdgeOverlay->raise();
        m_frameEdgeOverlay->update();
    }
    if (m_titleBar)
        m_titleBar->update();
    syncCaptionButtons();
}

void Window::syncClientSideResizeInput() {
    if (!usesClientSideResizeInput() && QWidget::mouseGrabber() == this)
        releaseMouse();
    syncClientSideFrameShape();
}

bool Window::usesClientSideResizeInput() const {
    // Linux client-side chrome needs an application-owned edge surface on both
    // Wayland and X11. The press is offered to QWindow::startSystemResize()
    // first; only X11 falls back to manual geometry changes when the window
    // manager declines that request.
    // zh_CN: Linux 客户端窗框在 Wayland 和 X11 下都需要由应用提供边缘输入面。
    // 首先调用 QWindow::startSystemResize()；仅当 X11 窗管拒绝时才回退到手动改几何。
    return m_chrome.usesCustomWindowChrome() && m_chrome.clientSideFrameMargin() > 0;
}

void Window::handleTitleBarDragStarted(const QPoint& globalPos) {
    if (!m_chromeInteractive)  // modal: no dragging (covers the Qt fallback path too)
        return;
    if (isMaximized())
        return;
    updateChromeOptions();
    m_fallbackDragging = false;
    if (m_chrome.beginSystemMove(globalPos))
        return;
    if (!m_chrome.manualMoveResizeFallbackAllowed())
        return;

    m_fallbackDragging = true;
    m_fallbackDragOffset = globalPos - frameGeometry().topLeft();
}

void Window::handleTitleBarDragMoved(const QPoint& globalPos) {
    if (!m_fallbackDragging)
        return;

    move(globalPos - m_fallbackDragOffset);
}

void Window::handleTitleBarDragFinished() {
    m_fallbackDragging = false;
}

void Window::handleTitleBarDoubleClicked(const QPoint& globalPos) {
    Q_UNUSED(globalPos);
    m_fallbackDragging = false;
    if (!m_chromeInteractive)
        return;

    if (m_chrome.performTitleBarDoubleClick()) {
        updateChromeOptions();
        return;
    }

    toggleMaximizeRestore();
}

void Window::handleTitleBarContextMenuRequested(const QPoint& globalPos) {
    if (!m_chromeInteractive)
        return;
    m_chrome.showSystemMenu(globalPos);
}

Qt::Edges Window::resizeEdgesAtLocalPos(const QPoint& localPos) const {
    Qt::Edges edges = resizeEdgesForHitTest(m_chrome.hitTestLocal(localPos));
    if (edges != Qt::Edges() || activeClientSideFrameMargin() <= 0)
        return edges;

    const QRect outer = rect();
    if (outer.contains(localPos)) {
        const int outerBorder = qMax(ResizeBorderWidth, activeClientSideFrameMargin());
        const bool left = localPos.x() < outer.left() + outerBorder;
        const bool right = localPos.x() >= outer.right() - outerBorder + 1;
        const bool top = localPos.y() < outer.top() + outerBorder;
        const bool bottom = localPos.y() >= outer.bottom() - outerBorder + 1;

        if (left)
            edges |= Qt::LeftEdge;
        if (right)
            edges |= Qt::RightEdge;
        if (top)
            edges |= Qt::TopEdge;
        if (bottom)
            edges |= Qt::BottomEdge;
        if (edges != Qt::Edges())
            return edges;
    }

    const QRect frame = chromeFrameRect();
    if (!frame.contains(localPos))
        return Qt::Edges();

    const int frameBorder = qMin(VisibleFrameResizeBorderWidth,
                                 qMax(1, qMin(frame.width(), frame.height()) / 2));
    const bool left = localPos.x() < frame.left() + frameBorder;
    const bool right = localPos.x() >= frame.right() - frameBorder + 1;
    const bool top = localPos.y() < frame.top() + frameBorder;
    const bool bottom = localPos.y() >= frame.bottom() - frameBorder + 1;

    if (left)
        edges |= Qt::LeftEdge;
    if (right)
        edges |= Qt::RightEdge;
    if (top)
        edges |= Qt::TopEdge;
    if (bottom)
        edges |= Qt::BottomEdge;
    return edges;
}

bool Window::handleResizeBorderMouseEvent(QWidget* source, QMouseEvent* event) {
    if (!source || !event || !usesClientSideResizeInput())
        return false;

    const QEvent::Type type = event->type();
    if (m_resizeSession->isActive()) {
        if (type == QEvent::MouseMove) {
            resizeFromGlobalPoint(fluentMouseGlobalPos(event));
            event->accept();
            return true;
        }
        if (type == QEvent::MouseButtonRelease && event->button() == Qt::LeftButton) {
            m_resizeSession->finish();
            if (QWidget::mouseGrabber() == this)
                releaseMouse();
            unsetCursor();
            updateChromeOptions();
            event->accept();
            return true;
        }
    }

    const bool overCaptionButton =
        m_captionButtonHost
        && (source == m_captionButtonHost || m_captionButtonHost->isAncestorOf(source));
    if (overCaptionButton) {
        return false;
    }

    if (!m_chromeInteractive || isMaximized()) {
        return false;
    }

    const QPoint localPos = pointerLocalPosForWindow(source, this, event);

    if (type == QEvent::MouseMove) {
        return false;
    }

    if (type != QEvent::MouseButtonPress || event->button() != Qt::LeftButton)
        return false;

    updateChromeOptions();
    const Qt::Edges edges = resizeEdgesAtLocalPos(localPos);
    if (edges == Qt::Edges())
        return false;

    const QPoint globalPos = fluentMouseGlobalPos(event);
    if (m_chrome.beginSystemResize(edges, globalPos)) {
        event->accept();
        return true;
    }
    if (!m_chrome.manualMoveResizeFallbackAllowed())
        return false;

    m_resizeSession->start(edges, globalPos, geometry());
    setCursor(cursorShapeForResizeEdges(edges));
    if (!QWidget::mouseGrabber())
        grabMouse();
    event->accept();
    return true;
}

void Window::resizeFromGlobalPoint(const QPoint& globalPos) {
    setGeometry(m_resizeSession->geometryForGlobalPoint(globalPos, minimumSize()));
}

} // namespace fluent::windowing
