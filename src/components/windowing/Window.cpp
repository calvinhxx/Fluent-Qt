#include "Window.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "TitleBar.h"
#include "design/Breakpoints.h"
#include "design/Typography.h"
#include "components/basicinput/Button.h"

namespace fluent::windowing {

namespace {

constexpr int CaptionButtonWidth = 46;
constexpr int CaptionButtonIconSize = 10;

} // namespace

Window::Window(QWidget* parent)
    : QWidget(parent),
      m_chrome(this) {
    m_chrome.applyPlatformWindowFlags();

    setAutoFillBackground(false);
    setMinimumSize(Breakpoints::MinWindowWidth, Breakpoints::MinWindowHeight);

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
    }
}

void Window::onThemeUpdated() {
    if (m_titleBar)
        m_titleBar->onThemeUpdated();
    if (m_minimizeButton)
        m_minimizeButton->onThemeUpdated();
    if (m_maximizeButton)
        m_maximizeButton->onThemeUpdated();
    if (m_closeButton)
        m_closeButton->onThemeUpdated();
    update();
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
    const auto& colors = themeColors();
    painter.fillRect(rect(), colors.bgCanvas);

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
    syncTitleBarSystemInsets();
    updateChromeOptions();
}

void Window::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange ||
        event->type() == QEvent::SafeAreaMarginsChange) {
        syncTitleBarSystemInsets();
        syncCaptionButtons();
        updateChromeOptions();
    }
}

bool Window::nativeEvent(const QByteArray& eventType,
                         void* message,
                         compatibility::FluentNativeEventResult* result) {
    return m_chrome.handleNativeEvent(eventType, message, result);
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
        button->setToolTip(tooltip);
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

    m_maximizeButton->setToolTip(isMaximized()
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

    if (m_chrome.performTitleBarDoubleClick()) {
        updateChromeOptions();
        return;
    }

    toggleMaximizeRestore();
}

void Window::handleTitleBarContextMenuRequested(const QPoint& globalPos) {
    m_chrome.showSystemMenu(globalPos);
}

} // namespace fluent::windowing
