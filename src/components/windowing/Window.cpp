#include "compatibility/ApplicationCompat.h"
#include "compatibility/PaintCompat.h"
#include "compatibility/InputEventCompat.h"
#include "Window.h"
#include "compatibility/private/WindowPlatformCompat_p.h"

#include <QApplication>
#include <QBackingStore>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEngine>
#include <QPointer>
#include <QPixmap>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QRegion>
#include <QWindow>
#include <QtMath>

#include "TitleBar.h"
#include "WindowBackdropMaterial.h"
#include "WindowChromeFrame.h"
#include "compatibility/private/WindowBackdropEvents_p.h"
#include "compatibility/private/WindowBackdropTransition_p.h"
#include "compatibility/private/NativePaintDeviceCompat_p.h"
#include "compatibility/private/WindowChromeBackend_p.h"
#include "design/Breakpoints.h"
#include "design/Typography.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "components/basicinput/Button.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/status_info/ToolTip.h"
#include "utils/private/FluentQtLogging_p.h"

namespace fluent::windowing {

namespace {

constexpr int CaptionButtonWidth = 46;
constexpr int CaptionButtonIconSize = Typography::IconSize::Standard;
constexpr int ResizeBorderWidth = 8;
constexpr int VisibleFrameResizeBorderWidth = ResizeBorderWidth;

bool requiresAlphaSurface(const BackdropCapabilities& capabilities, int frameMargin)
{
    return capabilities.supportsTransparentMaterial(BackdropEffect::Mica)
        || capabilities.supportsTransparentMaterial(BackdropEffect::Acrylic) || frameMargin > 0;
}

bool usesFluentCaptionButtons(const compatibility::WindowChromeCompat& chrome)
{
    return chrome.usesCustomWindowChrome() && !chrome.prefersNativeMacControls();
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

} // namespace

Window::Window(QWidget* parent)
    : QWidget(parent), m_chrome(this), m_resizeSession(std::make_unique<WindowResizeSession>())
{
    registerBackdropMetaTypes();

    // Resolve alpha-surface requirements before any platform call triggers winId(): some backends
    // latch native surface opacity on handle creation and ignore later WA_TranslucentBackground changes.
    // zh_CN: 必须在触发 winId() 创建原生句柄前确定是否需要透明表面，部分后端会在句柄创建时锁定不透明度。
    refreshBackdropCapabilities();
    m_windowTranslucent = requiresAlphaSurface(m_backdropCapabilities, m_chrome.clientSideFrameMargin());
    if (m_windowTranslucent)
        setAttribute(Qt::WA_TranslucentBackground, true);
    prepareBackdropSurface();
    m_chrome.applyPlatformWindowFlags();

    setAutoFillBackground(false);
    setMinimumSize(Breakpoints::MinWindowWidth, Breakpoints::MinWindowHeight);

    // Keep top-level translucency fixed; runtime effect changes only update paint state and OS backdrop attributes.
    // zh_CN: 顶层半透明在窗口期保持固定；运行时切换效果只更新绘制状态与系统背景属性。
    setEffectiveBackdropState(paintedFallbackState(m_backdropCapabilities.supportsTransparentMaterial(m_backdropEffect)
            ? QStringLiteral("platform-backdrop-pending")
            : QStringLiteral("painted-fallback")));

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    QWidget* chromeParent = this;
    QVBoxLayout* chromeLayout = m_rootLayout;
    const bool needsClientSideFrameHost = compatibility::detail::windowPlatformCapabilities().keepClientSideFrameHost
        || m_chrome.clientSideFrameMargin() > 0;
    if (needsClientSideFrameHost) {
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
        m_frameEdgeOverlay->setMouseEventHandler([this](QWidget* source, QMouseEvent* event) {
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
    connect(m_titleBar, &TitleBar::windowActiveChanged, this, &Window::syncCaptionButtonActivation);

    onThemeUpdated();
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
}

Window::~Window()
{
    // Disconnect before ~QWidget tears down children, which can emit WindowDeactivate back into this Window.
    // zh_CN: 提前断开连接，避免 ~QWidget 析构子控件时向半析构的 Window 派发 WindowDeactivate。
    if (m_titleBar)
        disconnect(m_titleBar, nullptr, this, nullptr);
    if (QWidget::mouseGrabber() == this)
        releaseMouse();
}

void Window::setContentWidget(QWidget* widget)
{
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

void Window::setCustomWindowChromeEnabled(bool enabled)
{
    compatibility::WindowChromeOptions options = m_chrome.options();
    if (options.useCustomWindowChrome == enabled)
        return;

    options.useCustomWindowChrome = enabled;
    m_chrome.configure(options);
    m_chrome.applyPlatformWindowFlags();

    refreshBackdropCapabilities();
    const bool resolvedTranslucency = requiresAlphaSurface(m_backdropCapabilities, m_chrome.clientSideFrameMargin());
    // Keep WA_TranslucentBackground sticky once enabled to avoid recreating the native surface and causing focus churn.
    // zh_CN: 启用半透明后保持不变，避免运行期关闭导致重建原生句柄和焦点抖动。
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

bool Window::customWindowChromeEnabled() const
{
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
    QPointer<Window> guard(this);
    const quint64 revision = ++m_backdropStateRevision;
    const auto current = [&] { return guard && guard->m_backdropStateRevision == revision; };
    const bool changed = m_backdropState != state;
    m_backdropState = state;
    prepareBackdropSurface();
    if (!current())
        return;

    // Keep legacy dynamic properties synchronized for callers not yet using WindowBackdrop.
    // zh_CN: 同步保留旧动态属性，方便尚未迁移到 WindowBackdrop 的调用方读取。
    setProperty("fluentWindowBackdropEffect", static_cast<int>(state.requestedEffect));
    if (!current())
        return;
    setProperty("fluentMicaBackdrop", state.surfaceMode == BackdropSurfaceMode::CompositedTransparent);
    if (!current())
        return;
    setProperty("fluentBackdropSurfaceMode", static_cast<int>(state.surfaceMode));
    if (!current())
        return;
    setProperty("fluentBackdropBackend", static_cast<int>(state.backend));
    if (!current())
        return;
    publishWindowBackdropState(this, state);
    if (!current())
        return;

    if (changed) {
        invalidatePaintedSurfaceCache();
        qCDebug(fluent::logging::windowingCategory).noquote()
            << QStringLiteral("Window backdrop resolved requested=%1 effective=%2 backend=%3 "
                              "fidelity=%4 surface=%5 platformApplied=%6 provider=%7 reason=%8")
                   .arg(static_cast<int>(state.requestedEffect))
                   .arg(static_cast<int>(state.effectiveEffect))
                   .arg(static_cast<int>(state.backend))
                   .arg(static_cast<int>(state.fidelity))
                   .arg(static_cast<int>(state.surfaceMode))
                   .arg(state.platformApplied)
                   .arg(m_backdropCapabilities.provider, state.reason);
        update();
        const QList<QWidget*> descendants = findChildren<QWidget*>();
        for (QWidget* child : descendants)
            child->update();
        emit backdropStateChanged(m_backdropState);
    }
}

void Window::prepareBackdropSurface()
{
    // Keep the active native backdrop until the opaque replacement frame is painted; cold windows seed with theme color.
    // zh_CN: 旧原生材质保留到不透明替代帧提交后；冷启动窗口则先以主题色打底。
    const auto mode =
        m_backdropDisablePending ? BackdropSurfaceMode::CompositedTransparent : m_backdropState.surfaceMode;
    const auto baseColor = [this](bool active) {
        return m_backdropEffect == BackdropEffect::Solid
            ? themeBackdrop(active)
            : WindowBackdropMaterial::opaqueBaseColor(paintedSurfaceMaterialOptions(active));
    };
    compatibility::detail::prepareWindowBackdropSurface(this, baseColor(true), baseColor(false), isEffectivelyActive(),
        mode);
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
    const auto repair = compatibility::detail::windowPlatformCapabilities().chromeRepair;
    if (repair == compatibility::detail::NativeChromeRepair::None || m_nativeChromeRepairPending) {
        return;
    }

    m_nativeChromeRepairPending = true;
    QTimer::singleShot(0, this, [this, repair] {
        m_nativeChromeRepairPending = false;
        if (!isVisible())
            return;

        if (repair == compatibility::detail::NativeChromeRepair::WindowFlags)
            m_chrome.applyPlatformWindowFlags();
        else
            syncTitleBarSystemInsets();
        updateChromeOptions();
    });
}

void Window::resolveBackdropState(bool applyPlatform, bool forceRecomposite)
{
    BackdropState next = paintedFallbackState(m_backdropEffect == BackdropEffect::Solid
            ? QStringLiteral("solid-requested")
            : QStringLiteral("painted-fallback"));

    const bool canUsePlatform = m_backdropCapabilities.supportsTransparentMaterial(m_backdropEffect);
    // Commit an opaque frame before tearing down an active transparent backdrop.
    // zh_CN: 先完成不透明帧提交，再拆除当前的透明背景。
    if (!canUsePlatform)
        beginOpaqueBackdropCommit(next);
    if (applyPlatform && !m_backdropDisablePending && (m_backdropEffect == BackdropEffect::Solid || canUsePlatform)) {
        const BackdropApplyResult applied = m_chrome.applySystemBackdropDetailed(m_backdropEffect,
            effectiveThemeUsesDarkAppearance(), forceRecomposite);
        if (applied.applied) {
            next.backend = applied.backend;
            next.fidelity = applied.fidelity;
            next.surfaceMode = applied.surfaceMode;
            next.platformApplied = applied.surfaceMode == BackdropSurfaceMode::CompositedTransparent;
            next.reason = applied.reason;
        } else if (!applied.reason.isEmpty()) {
            next.reason = applied.reason;
        }
    } else if (!applyPlatform && canUsePlatform) {
        next.reason = QStringLiteral("platform-backdrop-pending");
    } else if (m_backdropEffect != BackdropEffect::Solid && !m_backdropCapabilities.provider.isEmpty()) {
        next.reason = QStringLiteral("%1-fallback").arg(m_backdropCapabilities.provider);
    }

    beginOpaqueBackdropCommit(next);
    setEffectiveBackdropState(next);
}

void Window::beginOpaqueBackdropCommit(const BackdropState& next)
{
    if (m_backdropDisablePending
        || !compatibility::detail::requiresOpaqueBackdropCommit(this, m_backdropState, next.surfaceMode))
        return;
    m_backdropDisablePending = true;
    m_backdropOpaqueFramePainted = false;
    m_backdropCommitWinId = internalWinId();
}

void Window::onThemeUpdated()
{
    QPointer<Window> guard(this);
    invalidatePaintedSurfaceCache();
    // Keep the native backdrop tint in step with this window's effective theme.
    // zh_CN: 让原生背景着色跟随窗口的实际主题。
    resolveBackdropState(isVisible());
    if (!guard)
        return;
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

void Window::reapplySystemBackdrop()
{
    if (!isVisible())
        return;
    refreshBackdropCapabilities();
    updateChromeOptions(); // re-extend the sheet-of-glass
    resolveBackdropState(/*applyPlatform*/ true, /*forceRecomposite*/ true);
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    update();
}

void Window::prepareForNativeRestore()
{
    // Reset hover/pressed state before remapping, since compositors may hide windows without leave/release events.
    // zh_CN: 重新显示前清除悬停和按下状态，防止窗口隐藏时未收到离开/松开事件导致关闭按钮残留高亮。
    for (auto* button : {m_minimizeButton, m_maximizeButton, m_closeButton}) {
        if (!button)
            continue;
        button->setDown(false);
        button->setInteractionState(fluent::basicinput::Button::Rest);
        button->setAttribute(Qt::WA_UnderMouse, false);
        button->update();
    }

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

    compatibility::detail::requestQtForegroundActivation(this);
    m_chrome.requestForegroundActivation();
}

void Window::setBackdropEffect(BackdropEffect effect)
{
    if (m_backdropEffect == effect)
        return;
    QPointer<Window> guard(this);
    m_backdropEffect = effect;
    m_backdropOpaqueFramePainted = false;
    if (m_backdropCapabilities.supportsTransparentMaterial(effect)) {
        m_backdropDisablePending = false;
        m_backdropCommitWinId = 0;
    }

    // Refresh the effective material using the backend's recomposition policy.
    // zh_CN: 按后端重组策略刷新实际材质。
    const bool forceRecomposite = isVisible()
        && compatibility::detail::windowPlatformCapabilities().recomposeOnEffectChange;
    resolveBackdropState(isVisible(), forceRecomposite);
    if (!guard || m_backdropEffect != effect)
        return;
    emit backdropEffectChanged(m_backdropEffect);
}

void Window::finishPendingBackdropChange()
{
    if (!m_backdropDisablePending || !m_backdropOpaqueFramePainted || m_backdropCommitWinId != internalWinId()
        || !compatibility::detail::flushWindowBackdropSurface(this))
        return;

    m_backdropDisablePending = false;
    m_backdropCommitWinId = 0;
    // Seed the native opaque base before removing the compositor material.
    // zh_CN: 移除材质前先设置好原生不透明底色。
    prepareBackdropSurface();
    if (m_backdropEffect != BackdropEffect::Solid)
        m_chrome.applySystemBackdropDetailed(BackdropEffect::Solid, effectiveThemeUsesDarkAppearance());
    resolveBackdropState(isVisible());
}

void Window::minimizeWindow()
{
    QPointer<Window> guard(this);
    emit minimizeRequested();
    if (!guard)
        return;
    if (usesHostedWindowSurface())
        guard->hide();
    else
        guard->showMinimized();
}

void Window::toggleMaximizeRestore()
{
    QPointer<Window> guard(this);
    if (isEffectivelyMaximized()) {
        emit restoreRequested();
        if (!guard)
            return;
        if (usesHostedWindowSurface()) {
            QRect restore = m_hostedRestoreGeometry;
            if (!restore.isValid() && parentWidget()) {
                const QSize size(qMax(minimumWidth(), parentWidget()->width() * 3 / 4),
                    qMax(minimumHeight(), parentWidget()->height() * 3 / 4));
                restore = QRect(QPoint((parentWidget()->width() - size.width()) / 2,
                                    (parentWidget()->height() - size.height()) / 2),
                    size);
            }
            guard->setGeometry(restore);
        } else {
            guard->showNormal();
        }
    } else {
        emit maximizeRequested();
        if (!guard)
            return;
        if (usesHostedWindowSurface() && parentWidget()) {
            m_hostedRestoreGeometry = geometry();
            guard->setGeometry(parentWidget()->rect());
        } else {
            guard->showMaximized();
        }
    }
    if (!guard)
        return;
    guard->updateMaximizeButtonIcon();
    guard->updateChromeOptions();
}

void Window::closeWindow()
{
    QPointer<Window> guard(this);
    emit closeRequested();
    if (guard)
        guard->close();
}

ClientSideFramePaintOptions Window::clientSideFramePaintOptions() const
{
    const auto& colors = themeColorsRef();
    const bool active = isEffectivelyActive();
    QColor fill = windowChromeBackdropFill(*this, this, active);
    if (!fill.isValid())
        fill = themeBackdrop(active);

    ClientSideFramePaintOptions options;
    options.windowRect = rect();
    options.frameRect = windowFrameRect();
    options.fill = fill;
    options.stroke = colors.strokeDefault;
    options.radius = qMax<qreal>(themeRadius().overlay, themeRadius().control);
    options.shadow = themeShadow(Elevation::VeryHigh);
    options.dark = effectiveThemeUsesDarkAppearance();
    options.useTranslucentMaterial = m_backdropState.surfaceMode == BackdropSurfaceMode::CompositedTransparent;
    options.usePaintedMaterial = m_backdropState.surfaceMode == BackdropSurfaceMode::PaintedOpaque;
    options.effect = m_backdropEffect;
    options.material = paintedSurfaceMaterialOptions(active);
    return options;
}

WindowBackdropMaterialOptions Window::paintedSurfaceMaterialOptions(bool active) const
{
    const auto& colors = themeColorsRef();
    auto options = WindowBackdropMaterialOptions::forTheme(effectiveThemeUsesDarkAppearance(), colors.bgCanvas,
        colors.accentDefault);
    options.effect = m_backdropEffect;
    options.active = active;
    options.devicePixelRatio = devicePixelRatioF();
    return options;
}

void Window::invalidatePaintedSurfaceCache()
{
    m_paintedSurfaceCache.reset();
    m_paintedSurfaceCachePixelSize = QSize();
    m_paintedSurfaceCacheDpr = 0.0;
}

void Window::paintPaintedSurface(QPainter& painter, bool includeClientFrame)
{
    const ClientSideFramePaintOptions options = clientSideFramePaintOptions();
    const auto renderSurface = [&](QPainter& target) {
        if (includeClientFrame) {
            paintClientSideFrame(target, options);
            return;
        }
        WindowBackdropMaterial::paint(target, QRectF(rect()), options.material);
    };

    const bool cacheSurface = compatibility::detail::runtimePlatformCapabilities().cachePaintedWindowSurfaces;
    if (!cacheSurface) {
        renderSurface(painter);
        return;
    }

    const qreal dpr = qMax<qreal>(0.5, devicePixelRatioF());
    const QSize pixelSize(qMax(1, qCeil(width() * dpr)), qMax(1, qCeil(height() * dpr)));
    const bool cacheMatches = m_paintedSurfaceCache && m_paintedSurfaceCachePixelSize == pixelSize
        && qFuzzyCompare(m_paintedSurfaceCacheDpr, dpr) && m_paintedSurfaceCacheIncludesFrame == includeClientFrame;
    if (!cacheMatches) {
        auto cache = std::make_unique<QPixmap>(pixelSize);
        cache->setDevicePixelRatio(dpr);
        cache->fill(Qt::transparent);
        FluentRasterCachePainter cachePainter(cache.get());
        renderSurface(cachePainter);
        cachePainter.end();

        m_paintedSurfaceCache = std::move(cache);
        m_paintedSurfaceCachePixelSize = pixelSize;
        m_paintedSurfaceCacheDpr = dpr;
        m_paintedSurfaceCacheIncludesFrame = includeClientFrame;
        setProperty("fluentPaintedSurfaceCacheGeneration", property("fluentPaintedSurfaceCacheGeneration").toInt() + 1);
    }

    fluentDrawPixmapAt(painter, QPointF(), *m_paintedSurfaceCache);
}

void Window::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    QBackingStore* const store = backingStore();
    // Native handles can predate backing-store allocation; ignore cache/offscreen paints when tracking real surface commits.
    // zh_CN: 原生句柄可能早于绘制缓冲分配；离屏缓存绘制不计为窗口真实表面绘制。
    const bool paintsNativeSurface = !QWidget::sharedPainter() && store && !store->size().isEmpty()
        && painter.paintEngine() && painter.paintEngine()->paintDevice() == store->paintDevice();
    if (m_backdropDisablePending || !compatibility::detail::windowBackdropSurfaceWasPainted(this)) {
        const auto painted =
            [this, nativeId = internalWinId(),
                opaque = m_backdropState.surfaceMode != BackdropSurfaceMode::CompositedTransparent] {
                if (nativeId != internalWinId())
                    return;
                compatibility::detail::markWindowBackdropSurfacePainted(this);
                if (opaque && m_backdropDisablePending && !m_backdropOpaqueFramePainted
                    && m_backdropState.surfaceMode != BackdropSurfaceMode::CompositedTransparent
                    && m_backdropCommitWinId == nativeId) {
                    m_backdropOpaqueFramePainted = true;
                    QTimer::singleShot(0, this, &Window::finishPendingBackdropChange);
                }
            };
        if (paintsNativeSurface) {
            painted();
        } else if (painter.paintEngine()) {
            // Low-DPR tiles count only after committing to the backing store; render()/grab() captures do not.
            // zh_CN: 低 DPR 图块写回绘制缓冲后才标记为已绘制；render()/grab() 截图不计入。
            compatibility::detail::afterNativeRasterCommit(painter.paintEngine()->paintDevice(), this, painted);
        }
    }

    const int frameMargin = activeClientSideFrameMargin();
    if (frameMargin > 0) {
        if (m_backdropState.surfaceMode == BackdropSurfaceMode::PaintedOpaque)
            paintPaintedSurface(painter, true);
        else
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
        paintPaintedSurface(painter, false);
        return;
    }

    const auto& colors = themeColorsRef();
    // Use the same active/inactive backdrop source as the title bar and nav pane.
    // zh_CN: 使用与标题栏、导航栏一致的激活/非激活背景来源，避免 Normal 模式接缝。
    const bool active = isEffectivelyActive();
    const QColor backdrop = windowChromeBackdropFill(*this, this, active);
    painter.fillRect(rect(), backdrop.isValid() ? backdrop : themeBackdrop(active));

    if (m_chrome.usesCustomWindowChrome()) {
        fluent::painting::RoundedSurfacePaint surface;
        surface.border = colors.strokeDefault;
        fluent::painting::paintRoundedSurface(painter, QRectF(rect()), surface);
    }
}

void Window::mousePressEvent(QMouseEvent* event)
{
    if (handleResizeBorderMouseEvent(this, event))
        return;

    QWidget::mousePressEvent(event);
}

void Window::mouseMoveEvent(QMouseEvent* event)
{
    if (handleResizeBorderMouseEvent(this, event))
        return;

    QWidget::mouseMoveEvent(event);
}

void Window::mouseReleaseEvent(QMouseEvent* event)
{
    if (handleResizeBorderMouseEvent(this, event))
        return;

    QWidget::mouseReleaseEvent(event);
}

void Window::resizeEvent(QResizeEvent* event)
{
    invalidatePaintedSurfaceCache();
    QWidget::resizeEvent(event);
    if (usesHostedWindowSurface() && parentWidget() && geometry() != parentWidget()->rect()) {
        m_hostedRestoreGeometry = geometry();
    }
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
}

void Window::setVisible(bool visible)
{
    const Qt::WindowStates state = windowState();
    const bool showingMaximized = visible && isWindow() && !isVisible() && state.testFlag(Qt::WindowMaximized)
        && !(state & (Qt::WindowMinimized | Qt::WindowFullScreen));
    QWidget::setVisible(visible);
    if (showingMaximized && isVisible())
        compatibility::detail::restorePlatformMaximizedFrame(this);
}

void Window::showEvent(QShowEvent* event)
{
    prepareBackdropSurface();
    QWidget::showEvent(event);
    m_chrome.applyPlatformWindowFlags();
    refreshBackdropCapabilities();
    resolveBackdropState(/*applyPlatform*/ true);
    syncClientSideFrameMargins();
    syncClientSideFrameShape();
    syncTitleBarSystemInsets();
    updateChromeOptions();
    scheduleNativeChromeRepair();

    // Re-apply the system backdrop after the first event-loop turn once the native window is mapped.
    // zh_CN: 首次显示后等原生窗口创建并定位完成，再重新应用系统背景。
    if (m_backdropCapabilities.supportsTransparentMaterial(m_backdropEffect) && !m_backdropPrimed) {
        m_backdropPrimed = true;
        scheduleBackdropResolution();
    }
}

void Window::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (fluentIsWindowInsetChangeEvent(event)) {
        syncTitleBarSystemInsets();
        syncCaptionButtons();
        updateChromeOptions();
    }

    // Repaint the deepest backdrop on activation changes so it stays in sync
    // with title/nav focus tint.
    // zh_CN: 窗口激活状态变化时重绘底层背景，使其与标题栏/导航栏焦点色同步。
    if (event->type() == QEvent::ActivationChange) {
        prepareBackdropSurface();
        invalidatePaintedSurfaceCache();
        update();
    }
    if (event->type() == QEvent::WindowStateChange) {
        syncClientSideFrameMargins();
        syncClientSideFrameShape();
        syncTitleBarSystemInsets();
        updateChromeOptions();
        if (compatibility::detail::windowPlatformCapabilities().materialHostChangesWithWindowState
            && m_backdropCapabilities.supportsTransparentMaterial(m_backdropEffect))
            scheduleBackdropResolution();
        scheduleNativeChromeRepair();
    }
}

bool Window::event(QEvent* event)
{
    if (compatibility::detail::isWindowBackdropReevaluationEvent(event)) {
        scheduleBackdropResolution();
        return true;
    }

    const QEvent::Type type = event ? event->type() : QEvent::None;
    const bool handled = QWidget::event(event);
    if (type == QEvent::UpdateRequest)
        finishPendingBackdropChange();
    bool nativeSurfaceMayHaveChanged = type == QEvent::WinIdChange || fluentIsDisplayScaleChangeEvent(event);
    if (type == QEvent::WinIdChange)
        compatibility::detail::resetWindowBackdropSurfacePaint(this);
    if (type == QEvent::WinIdChange
        && (m_backdropDisablePending || m_backdropState.surfaceMode == BackdropSurfaceMode::CompositedTransparent)) {
        // Native surface changed: discard pending opaque-commit state and paint history from the destroyed handle.
        // zh_CN: 原生窗口句柄已重建，丢弃旧表面的待提交状态与已绘制标记。
        m_backdropDisablePending = false;
        m_backdropOpaqueFramePainted = false;
        m_backdropCommitWinId = 0;
        QPointer<Window> guard(this);
        setEffectiveBackdropState(paintedFallbackState(QStringLiteral("native-surface-pending")));
        if (!guard)
            return handled;
    }
    if (nativeSurfaceMayHaveChanged)
        prepareBackdropSurface();
    if (nativeSurfaceMayHaveChanged && isVisible()) {
        invalidatePaintedSurfaceCache();
        scheduleBackdropResolution();
        scheduleNativeChromeRepair();
    }
    if (type == QEvent::WindowActivate && isVisible()
        && compatibility::detail::windowPlatformCapabilities().titleBarLayoutChangesOnActivation)
        scheduleNativeChromeRepair();
    return handled;
}

bool Window::nativeEvent(const QByteArray& eventType, void* message, compatibility::FluentNativeEventResult* result)
{
    return m_chrome.handleNativeEvent(eventType, message, result);
}

void Window::setChromeInteractive(bool interactive)
{
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

void Window::setCaptionButtonToolTips(const QString& minimizeTooltip,
    const QString& maximizeTooltip,
    const QString& closeTooltip,
    const QString& restoreTooltip)
{
    m_minimizeTooltip = minimizeTooltip;
    m_maximizeTooltip = maximizeTooltip;
    m_closeTooltip = closeTooltip;
    m_restoreTooltip = restoreTooltip.isEmpty() ? maximizeTooltip : restoreTooltip;

    if (m_minimizeButton)
        fluent::status_info::ToolTip::attach(m_minimizeButton, m_minimizeTooltip);
    if (m_closeButton)
        fluent::status_info::ToolTip::attach(m_closeButton, m_closeTooltip);
    updateMaximizeButtonIcon();
}

void Window::setCaptionButtonAccessibleNames(const QString& minimizeName,
    const QString& maximizeName,
    const QString& closeName,
    const QString& restoreName)
{
    m_minimizeAccessibleName = minimizeName;
    m_maximizeAccessibleName = maximizeName;
    m_closeAccessibleName = closeName;
    m_restoreAccessibleName = restoreName;

    if (m_minimizeButton)
        m_minimizeButton->setAccessibleName(m_minimizeAccessibleName);
    if (m_closeButton)
        m_closeButton->setAccessibleName(m_closeAccessibleName);
    updateMaximizeButtonIcon();
}

void Window::updateChromeOptions()
{
    if (!m_titleBar)
        return;

    compatibility::WindowChromeOptions options = m_chrome.options();
    options.titleBarRect =
        m_titleBar->isVisible() ? QRect(m_titleBar->mapTo(this, QPoint(0, 0)), m_titleBar->size()) : QRect();
    options.dragExclusionRects.clear();
    if (m_titleBar->isVisible()) {
        for (const QRect& rect : m_titleBar->dragExclusionRects()) {
            options.dragExclusionRects << QRect(m_titleBar->mapTo(this, rect.topLeft()), rect.size());
        }
    }
    options.resizeBorderWidth = ResizeBorderWidth;
    options.chromeInteractive = m_chromeInteractive;
    options.maximizeButton = m_maximizeButton;
    m_chrome.configure(options);

    const int reservedResizeBorder =
        m_chrome.usesCustomWindowChrome() && m_chromeInteractive && !isEffectivelyMaximized() && !isFullScreen()
        ? ResizeBorderWidth
        : 0;
    const char* resizeBorderProperty = ::fluent::overlay::windowResizeBorderWidthPropertyName();
    if (property(resizeBorderProperty).toInt() != reservedResizeBorder)
        setProperty(resizeBorderProperty, reservedResizeBorder);
}

void Window::syncTitleBarSystemInsets()
{
    if (!m_titleBar)
        return;

    m_titleBar->setSystemReservedLeadingWidth(m_chrome.nativeTitleBarLeadingInset());
    syncCaptionButtons();
    m_titleBar->setSystemReservedTrailingWidth(captionButtonReservedWidth());
    m_titleBar->setVisible(m_chrome.usesCustomWindowChrome() || m_chrome.prefersNativeMacControls());
}

void Window::setupCaptionButtons()
{
    if (!m_titleBar || !usesFluentCaptionButtons(m_chrome) || m_captionButtonHost)
        return;

    m_captionButtonHost = new QWidget(m_titleBar);
    m_captionButtonHost->setObjectName(QStringLiteral("fluentWindowCaptionButtonHost"));
    m_captionButtonHost->setAttribute(Qt::WA_StyledBackground, false);
    m_captionButtonHost->setAutoFillBackground(false);

    auto* buttonLayout = new QHBoxLayout(m_captionButtonHost);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    auto createCaptionButton =
        [this](const QString& objectName, const QString& glyph, const QString& tooltip, const QString& accessibleName) {
            auto* button = new fluent::basicinput::Button(m_captionButtonHost);
            button->setObjectName(objectName);
            fluent::status_info::ToolTip::attach(button, tooltip);
            button->setAccessibleName(accessibleName);
            button->setFluentStyle(fluent::basicinput::Button::Subtle);
            button->setFluentLayout(fluent::basicinput::Button::IconOnly);
            button->setFluentSize(fluent::basicinput::Button::Small);
            button->setIconGlyph(glyph, CaptionButtonIconSize);
            button->setFocusPolicy(Qt::NoFocus);
            return button;
        };

    m_minimizeButton = createCaptionButton(QStringLiteral("fluentWindowMinimizeButton"),
        Typography::Icons::ChromeMinimize, m_minimizeTooltip, m_minimizeAccessibleName);
    m_maximizeButton = createCaptionButton(QStringLiteral("fluentWindowMaximizeButton"),
        Typography::Icons::ChromeMaximize, m_maximizeTooltip, m_maximizeAccessibleName);
    m_closeButton = createCaptionButton(QStringLiteral("fluentWindowCloseButton"), Typography::Icons::ChromeClose,
        m_closeTooltip, m_closeAccessibleName);
    m_closeButton->setCriticalOnHover(true);

    // Linux clips the entire client surface once in the frame overlay; Windows/macOS round the close button directly.
    // zh_CN: Linux 保持标题按钮直角，由顶层边框遮罩统一裁剪；Windows/macOS 保留按钮自身圆角。
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
    syncCaptionButtonActivation(m_titleBar->isWindowActive());
}

void Window::syncCaptionButtons()
{
    if (!m_captionButtonHost || !m_titleBar)
        return;

    // Show Fluent caption buttons only when custom window chrome is enabled.
    // zh_CN: 仅在启用自定义窗口标题栏时显示自绘控制按钮。
    const bool showCaptionButtons = usesFluentCaptionButtons(m_chrome);
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

void Window::syncCaptionButtonActivation(bool active)
{
    // WinUI keeps caption-button geometry unchanged and softens only its foreground when the
    // window loses activation. Retaining the full hit target also avoids native hit-test churn.
    // zh_CN: WinUI 在窗口失活时保持标题按钮几何不变，仅弱化前景；完整命中区域也可避免原生命中测试抖动。
    constexpr qreal InactiveCaptionOpacity = 0.55;
    const qreal opacity = active ? 1.0 : InactiveCaptionOpacity;
    for (auto* button : {m_minimizeButton, m_maximizeButton, m_closeButton}) {
        if (button)
            button->setContentOpacity(opacity);
    }
}

void Window::updateMaximizeButtonIcon()
{
    if (!m_maximizeButton)
        return;

    fluent::status_info::ToolTip::attach(m_maximizeButton,
        isEffectivelyMaximized() ? m_restoreTooltip : m_maximizeTooltip);
    m_maximizeButton->setAccessibleName(isEffectivelyMaximized() ? m_restoreAccessibleName : m_maximizeAccessibleName);
    m_maximizeButton->setIconGlyph(
        isEffectivelyMaximized() ? Typography::Icons::ChromeRestore : Typography::Icons::ChromeMaximize,
        CaptionButtonIconSize);
}

bool Window::usesHostedWindowSurface() const
{
    return compatibility::detail::runtimePlatformCapabilities().hostsApplicationWindowsInDesktopSurface
        && parentWidget() && !isWindow();
}

bool Window::isEffectivelyActive() const
{
    return isActiveWindow() || usesHostedWindowSurface();
}

bool Window::isEffectivelyMaximized() const
{
    return isMaximized() || (usesHostedWindowSurface() && parentWidget() && geometry() == parentWidget()->rect());
}

int Window::captionButtonReservedWidth() const
{
    if (m_captionButtonHost && usesFluentCaptionButtons(m_chrome))
        return m_captionButtonHost->width() > 0 ? m_captionButtonHost->width() : CaptionButtonWidth * 3;

    return m_chrome.nativeTitleBarTrailingInset();
}

int Window::activeClientSideFrameMargin() const
{
    if (isEffectivelyMaximized() || isFullScreen())
        return 0;
    return m_chrome.clientSideFrameMargin();
}

QRect Window::windowFrameRect() const
{
    return chromeFrameRect();
}

QRect Window::chromeFrameRect() const
{
    if (m_frameHost)
        return m_frameHost->geometry();

    const int margin = activeClientSideFrameMargin();
    return rect().marginsRemoved(QMargins(margin, margin, margin, margin));
}

void Window::syncClientSideFrameMargins()
{
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

void Window::syncClientSideFrameShape()
{
    const int frameMargin = activeClientSideFrameMargin();
    const QRect surface = (frameMargin > 0 ? chromeFrameRect() : rect()).intersected(rect());
    const QRect effectiveSurface = surface.isEmpty() ? rect() : surface;
    const char* surfaceProperty = ::fluent::overlay::overlaySurfaceRectPropertyName();
    if (property(surfaceProperty).toRect() != effectiveSurface)
        setProperty(surfaceProperty, effectiveSurface);

    // Windows and macOS have no client-side frame host; skip Linux mask and overlay updates.
    // zh_CN: Windows/macOS 不使用自绘边框宿主，直接跳过 Linux 的圆角遮罩与命中区更新。
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
        const QRegion frameMask = ::fluent::overlay::roundedRectRegion(m_frameHost->rect(), qCeil(radius));
        if (m_frameHost->mask() != frameMask)
            m_frameHost->setMask(frameMask);
    } else if (!m_frameHost->mask().isEmpty()) {
        m_frameHost->clearMask();
    }
    if (m_frameEdgeOverlay) {
        m_frameEdgeOverlay->setGeometry(rect());
        m_frameEdgeOverlay->setFrameVisualRect(chromeFrameRect());
        m_frameEdgeOverlay->setFrameRadius(radius);
        m_frameEdgeOverlay->setFrameStroke(themeColorsRef().strokeDefault);
        m_frameEdgeOverlay->setVisible(radius > 0.0);
        const bool acceptsResizeInput = usesClientSideResizeInput() && m_chromeInteractive && radius > 0.0;
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

void Window::syncClientSideResizeInput()
{
    if (!usesClientSideResizeInput() && QWidget::mouseGrabber() == this)
        releaseMouse();
    syncClientSideFrameShape();
}

bool Window::usesClientSideResizeInput() const
{
    // Linux client-side chrome always needs edge hit zones; X11 falls back to manual resize if the WM declines startSystemResize().
    // zh_CN: Linux 自绘窗框在 Wayland/X11 下均需边缘输入区；先尝试 startSystemResize()，X11 窗管不支持时再回退手动缩放。
    return m_chrome.usesCustomWindowChrome() && m_chrome.clientSideFrameMargin() > 0;
}

void Window::handleTitleBarDragStarted(const QPoint& globalPos)
{
    if (!m_chromeInteractive) // modal: no dragging (covers the Qt fallback path too)
        return;
    if (isEffectivelyMaximized())
        return;
    updateChromeOptions();
    m_fallbackDragging = false;
    if (m_chrome.beginSystemMove(globalPos))
        return;
    if (!m_chrome.manualMoveResizeFallbackAllowed())
        return;

    m_fallbackDragging = true;
    m_fallbackDragOffset = globalPos - mapToGlobal(QPoint(0, 0));
}

void Window::handleTitleBarDragMoved(const QPoint& globalPos)
{
    if (!m_fallbackDragging)
        return;

    const QPoint globalTopLeft = globalPos - m_fallbackDragOffset;
    if (usesHostedWindowSurface() && parentWidget())
        move(parentWidget()->mapFromGlobal(globalTopLeft));
    else
        move(globalTopLeft);
}

void Window::handleTitleBarDragFinished()
{
    m_fallbackDragging = false;
}

void Window::handleTitleBarDoubleClicked(const QPoint& globalPos)
{
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

void Window::handleTitleBarContextMenuRequested(const QPoint& globalPos)
{
    if (!m_chromeInteractive)
        return;
    m_chrome.showSystemMenu(globalPos);
}

Qt::Edges Window::resizeEdgesAtLocalPos(const QPoint& localPos) const
{
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

    const int frameBorder = qMin(VisibleFrameResizeBorderWidth, qMax(1, qMin(frame.width(), frame.height()) / 2));
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

bool Window::handleResizeBorderMouseEvent(QWidget* source, QMouseEvent* event)
{
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

    const bool overCaptionButton = m_captionButtonHost
        && (source == m_captionButtonHost || m_captionButtonHost->isAncestorOf(source));
    if (overCaptionButton) {
        return false;
    }

    if (!m_chromeInteractive || isEffectivelyMaximized()) {
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

void Window::resizeFromGlobalPoint(const QPoint& globalPos)
{
    setGeometry(m_resizeSession->geometryForGlobalPoint(globalPos, minimumSize()));
}

} // namespace fluent::windowing
