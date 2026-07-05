#include "Window.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "TitleBar.h"
#include "design/Breakpoints.h"
#include "design/Typography.h"
#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/status_info/ToolTip.h"

namespace fluent::windowing {

namespace {

constexpr int CaptionButtonWidth = 46;
constexpr int CaptionButtonIconSize = 10;

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

} // namespace

Window::Window(QWidget* parent)
    : QWidget(parent),
      m_chrome(this) {
    m_chrome.applyPlatformWindowFlags();

    setAutoFillBackground(false);
    setMinimumSize(Breakpoints::MinWindowWidth, Breakpoints::MinWindowHeight);

    // Window translucency is a FIXED, platform-level decision: on platforms that support a system
    // backdrop the window is translucent for its whole lifetime, so switching effects at runtime
    // never restyles the native surface (which would flicker + steal focus). "fluentMicaBackdrop"
    // is the separate paint-hint: true means surfaces paint transparent to reveal the OS backdrop
    // (Mica/Acrylic); false (Normal, or unsupported platforms) means they paint opaque themselves.
    // The DWM/vibrancy attribute itself is applied in showEvent once the native handle exists.
    // zh_CN: 窗口半透明性是固定的平台级决策：支持系统背景的平台上窗口整生命周期半透明，故运行时切换效果绝不重塑
    // 原生表面（否则会闪烁 + 抢焦点）。"fluentMicaBackdrop" 是另一回事——绘制提示：true 表示表面画透明以透出系统
    // 背景（Mica/Acrylic），false（Normal 或不支持的平台）表示表面自绘不透明。DWM/vibrancy 属性本身在 showEvent
    // 拿到原生句柄后施加。
    m_windowTranslucent = m_chrome.systemBackdropSupported();
    m_micaBackdrop = m_windowTranslucent
                     && m_backdropEffect != compatibility::BackdropEffect::Solid;
    if (m_windowTranslucent)
        setAttribute(Qt::WA_TranslucentBackground, true);
    setProperty("fluentWindowBackdropEffect", static_cast<int>(m_backdropEffect));
    setProperty("fluentMicaBackdrop", m_micaBackdrop);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_titleBar = new TitleBar(this);
    m_titleBar->setSystemReservedLeadingWidth(m_chrome.nativeTitleBarLeadingInset());
    m_titleBar->setVisible(m_chrome.usesCustomWindowChrome() || m_chrome.prefersNativeMacControls());
    rootLayout->addWidget(m_titleBar);
    setupCaptionButtons();

    m_contentHost = new QWidget(this);
    m_contentHost->setObjectName(QStringLiteral("fluentWindowContentHost"));
    m_contentHost->setAutoFillBackground(false);
    auto* contentLayout = new QVBoxLayout(m_contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    rootLayout->addWidget(m_contentHost, 1);

    connect(m_titleBar, &TitleBar::chromeGeometryChanged, this, &Window::updateChromeOptions);
    connect(m_titleBar, &TitleBar::dragStarted, this, &Window::handleTitleBarDragStarted);
    connect(m_titleBar, &TitleBar::dragMoved, this, &Window::handleTitleBarDragMoved);
    connect(m_titleBar, &TitleBar::dragFinished, this, &Window::handleTitleBarDragFinished);
    connect(m_titleBar, &TitleBar::doubleClicked, this, &Window::handleTitleBarDoubleClicked);
    connect(m_titleBar, &TitleBar::contextMenuRequested, this, &Window::handleTitleBarContextMenuRequested);
    connect(m_titleBar, &TitleBar::titleBarHeightChanged, this, &Window::syncCaptionButtons);

    onThemeUpdated();
    syncTitleBarSystemInsets();
    updateChromeOptions();
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

void Window::onThemeUpdated() {
    // Keep the DWM dark-mode bit (and thus the backdrop tint) in step with this window's effective
    // theme. Without a local override this is still the global app theme.
    // zh_CN: 让 DWM 暗色模式位（进而背景着色）跟随该窗口的实际主题；没有局部覆盖时仍等同于全局应用主题。
    if (m_windowTranslucent)
        m_chrome.applySystemBackdrop(m_backdropEffect, effectiveTheme() == Dark);
    if (m_titleBar)
        m_titleBar->onThemeUpdated();
    if (m_minimizeButton)
        m_minimizeButton->onThemeUpdated();
    if (m_maximizeButton)
        m_maximizeButton->onThemeUpdated();
    if (m_closeButton)
        m_closeButton->onThemeUpdated();
    // Content descendants are themed by the global FluentThemeManager (visible ones synchronously, the
    // rest lazily/on-show), so this no longer walks the whole content tree — that synchronous walk over
    // every prewarmed page was the multi-hundred-ms theme-switch freeze. zh_CN: 内容子级由全局
    // FluentThemeManager 刷新（可见的同步，其余延后/显示时刷新），故此处不再遍历整棵内容树——对每个预热页的
    // 同步遍历正是切换主题数百毫秒卡顿的根源。
    update();
}

void Window::reapplySystemBackdrop() {
    if (!m_windowTranslucent || !isVisible())
        return;
    updateChromeOptions();                                 // re-extend the sheet-of-glass
    m_chrome.applySystemBackdrop(m_backdropEffect, effectiveTheme() == Dark,
                                 /*forceRecomposite*/ true);  // re-assert + force a frame refresh
}

void Window::setBackdropEffect(compatibility::BackdropEffect effect) {
    if (m_backdropEffect == effect)
        return;
    m_backdropEffect = effect;

    // The window surface stays as it was created (see ctor): switching effects only updates the
    // paint-hint + the requested OS backdrop type, then repaints. No WA_TranslucentBackground toggle,
    // no native flag re-assertion — so no flicker and no focus loss. zh_CN: 窗口表面保持构造时的状态
    //（见 ctor）：切换效果只更新绘制提示 + 请求的系统背景类型，然后重绘。不切 WA_TranslucentBackground、不重置
    // 原生标志——故无闪烁、无焦点丢失。
    m_micaBackdrop = m_windowTranslucent
                     && effect != compatibility::BackdropEffect::Solid;
    setProperty("fluentWindowBackdropEffect", static_cast<int>(m_backdropEffect));
    setProperty("fluentMicaBackdrop", m_micaBackdrop);

    // forceRecomposite: a deliberate, infrequent effect switch must composite on the spot. Acrylic
    // (DWMSBT_TRANSIENTWINDOW) in particular often stays a flat default glass until the next
    // (de)activation — which is exactly why the user had to switch to another app and back before it
    // looked right. The NCACTIVATE round-trip reproduces that without changing real focus. (Theme
    // changes still pass forceRecomposite=false, since those are already composited and frequent.)
    // zh_CN: forceRecomposite：用户主动且不频繁的效果切换必须当场合成。Acrylic（DWMSBT_TRANSIENTWINDOW）尤其常
    // 停在扁平默认玻璃上，要等下次激活/失活才生效——这正是用户必须切到别的 app 再切回来才正常的原因。NCACTIVATE
    // 往返复现该动作且不改变真实焦点。（切主题仍传 forceRecomposite=false，因其已合成且频繁。）
    if (m_windowTranslucent && isVisible())
        m_chrome.applySystemBackdrop(effect, effectiveTheme() == Dark,
                                     /*forceRecomposite*/ true);  // Solid maps to DWMSBT_AUTO

    // Repaint chrome + content so every surface re-reads the new paint-hint (transparent<->opaque, or
    // a different translucent material). zh_CN: 重绘 chrome 与内容，使各表面重新读取新绘制提示。
    if (isVisible()) {
        const QList<QWidget*> descendants = findChildren<QWidget*>();
        for (QWidget* child : descendants)
            child->update();
        update();
    }
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

void Window::paintEvent(QPaintEvent*) {
    QPainter painter(this);

    // Transparent top-level widgets keep their backing-store pixels between frames on macOS.
    // Replace them explicitly so page or material switches cannot retain the previous frame.
    // zh_CN: macOS 的透明顶层控件会跨帧保留后备缓冲像素，需显式替换，避免页面或材质切换残留上一帧。
    if (m_micaBackdrop) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        return;
    }

    const auto& colors = themeColors();
    // Fill the deepest backdrop with the SAME chromeBackdropFill() the title bar and nav pane use,
    // not a flat bgCanvas. Under a translucent top-level the chrome's own fill is cleared where a
    // transparent child (the nav pane) sits, so the pane reveals this window backing — if it stayed a
    // flat bgCanvas while the title bar washed toward bgLayer on focus loss, the title bar and nav
    // pane would drift apart (the Normal-mode "titlebar ≠ nav" seam). Sharing one source keeps them
    // identical in both the active and inactive states.
    // zh_CN: 用与标题栏/导航栏完全相同的 themeBackdrop(active) 填充最底层背景，而非扁平 bgCanvas。半透明顶层下，
    // chrome 自身的填充在透明子控件（导航窗格）所在处会被清除，故窗格透出此窗口底；若它保持扁平 bgCanvas 而标题栏
    // 在失焦时洗向 bgLayer，标题栏与导航窗格就会分叉（Normal 模式「标题栏≠导航栏」的缝）。共用同一来源使二者在激活
    // 与非激活态都一致。
    const QColor backdrop = chromeBackdropFill(this, isActiveWindow());
    painter.fillRect(rect(), backdrop.isValid() ? backdrop : themeBackdrop(isActiveWindow()));

    if (m_chrome.usesCustomWindowChrome()) {
        painter.setPen(colors.strokeDefault);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

void Window::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncTitleBarSystemInsets();
    updateChromeOptions();
}

void Window::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_chrome.applyPlatformWindowFlags();
    if (m_windowTranslucent)
        m_chrome.applySystemBackdrop(m_backdropEffect, effectiveTheme() == Dark);
    syncTitleBarSystemInsets();
    updateChromeOptions();

    // On first show DWM may not composite the Mica backdrop until the window is next
    // (de)activated — and the window is move()d into place right after show(), before Qt's
    // translucent composition has settled. Re-assert the backdrop + sheet-of-glass once the event
    // loop is running (window fully realized and placed); applySystemBackdrop forces a frame
    // refresh so Mica appears immediately, without the user switching away and back.
    // zh_CN: 首次显示时 DWM 可能要等窗口下次激活/失活才合成 Mica；而窗口在 show() 后立刻被 move() 定位，
    // 此时 Qt 的半透明合成尚未稳定。待事件循环启动（窗口完全就绪并定位）后，重新施加背景 + 整窗玻璃；
    // applySystemBackdrop 会触发 frame 刷新，使 Mica 立即显示，无需切走再切回。
    if (m_windowTranslucent && !m_micaBackdropPrimed) {
        m_micaBackdropPrimed = true;
        QTimer::singleShot(0, this, [this] {
            if (!m_windowTranslucent || !isVisible())
                return;
            updateChromeOptions();                                 // re-extend the sheet-of-glass
            m_chrome.applySystemBackdrop(m_backdropEffect, effectiveTheme() == Dark,
                                         /*forceRecomposite*/ true);  // re-assert + force frame refresh
        });
    }
}

void Window::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);

    if (fluentIsWindowInsetChangeEvent(event)) {
        syncTitleBarSystemInsets();
        syncCaptionButtons();
        updateChromeOptions();
    }

    // Repaint the backdrop when the window activates/deactivates so it tracks the focus wash in
    // lock-step with the title bar and nav pane (both already repaint on WindowActivate/Deactivate).
    // Without this the deepest backdrop kept its last-painted focus state and the chrome drifted.
    // zh_CN: 窗口激活/失活时重绘背景，使其与标题栏、导航窗格（二者已在 WindowActivate/Deactivate 时重绘）同步跟随
    // 焦点洗色。否则最底层背景会停在上次绘制的焦点状态，与 chrome 分叉。
    if (event->type() == QEvent::ActivationChange)
        update();
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
    updateChromeOptions();
}

void Window::updateChromeOptions() {
    if (!m_titleBar)
        return;

    compatibility::WindowChromeOptions options = m_chrome.options();
    options.titleBarRect = m_titleBar->isVisible() ? m_titleBar->geometry() : QRect();
    options.dragExclusionRects.clear();
    if (m_titleBar->isVisible()) {
        for (const QRect& rect : m_titleBar->dragExclusionRects()) {
            options.dragExclusionRects << QRect(m_titleBar->mapTo(this, rect.topLeft()), rect.size());
        }
    }
    options.resizeBorderWidth = 8;
    options.chromeInteractive = m_chromeInteractive;
    m_chrome.configure(options);
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
    if (!m_titleBar || !m_chrome.usesCustomWindowChrome())
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

    const int windowCornerRadius = themeRadius().control;
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

    // Always use our own Fluent caption buttons (even under Mica): they're anchored
    // verticalCenter + right:0 to the full-width title bar, so they sit centered and flush to the
    // edge — unlike the DWM glass buttons, which render top-aligned at the system caption height
    // and inset by the resize frame. The native glass buttons are suppressed in the chrome layer
    // (no WS_CAPTION), so there's no duplication. zh_CN: 始终使用自绘 Fluent 标题栏按钮（Mica 下也是）：
    // 它们以 verticalCenter + right:0 锚定到满宽标题栏，故垂直居中且贴右边缘——不像 DWM 玻璃按钮那样顶部
    // 对齐于系统 caption 高度、且被缩放边框内缩。原生玻璃按钮已在 chrome 层抑制（去掉 WS_CAPTION），不会重复。
    const bool showCaptionButtons = m_chrome.usesCustomWindowChrome();
    const int buttonHeight = m_titleBar->titleBarHeight();
    const int buttonCount = 3;
    m_captionButtonHost->setFixedSize(CaptionButtonWidth * buttonCount, buttonHeight);
    for (auto* button : {m_minimizeButton, m_maximizeButton, m_closeButton}) {
        if (button)
            button->setFixedSize(CaptionButtonWidth, buttonHeight);
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

void Window::handleTitleBarDragStarted(const QPoint& globalPos) {
    if (!m_chromeInteractive)  // modal: no dragging (covers the Qt fallback path too)
        return;
    updateChromeOptions();
    m_fallbackDragging = false;
    if (m_chrome.beginSystemMove(globalPos))
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

} // namespace fluent::windowing
