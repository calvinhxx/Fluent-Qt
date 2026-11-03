#include "GalleryInterfaceZoomController.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "compatibility/AccessibilityCompat.h"
#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "platform/GalleryPlatform.h"
#include "support/logging/Log.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "viewmodel/GalleryInterfaceZoom.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
namespace {

// Long enough to read the level, short enough to stay out of the way.
// zh_CN: 足够看清档位，又不长时间遮挡内容。
constexpr int kBubbleIdleCloseMs = 2000;
constexpr int kBubbleEdgeMargin = 12;
constexpr int kBubbleTitleBarGap = 4;

QString percentText(int percent)
{
    return QStringLiteral("%1%").arg(percent);
}

// The bubble is a transient command surface: a zoom command shows it without
// moving focus away from the page being zoomed.
// zh_CN: 缩放气泡是瞬态命令表面：缩放命令显示它时不移走被缩放页面的焦点。
class ZoomBubble final : public fluent::dialogs_flyouts::Popup {
public:
    using Popup::Popup;
    using Popup::setFocusOnOpenEnabled;
};

} // namespace

GalleryInterfaceZoomController::GalleryInterfaceZoomController(QWidget* window) : QObject(window), m_window(window)
{
    setObjectName(QStringLiteral("galleryInterfaceZoomController"));
    auto& settings = GallerySettings::instance();
    // The application applies the saved zoom before creating its first window.
    // zh_CN: 应用在创建首个窗口前已应用保存的缩放。
    m_appliedPercent = qRound(platform::appliedInterfaceZoom() * 100);
    m_window->setMinimumSize(minimumSizeAt(m_appliedPercent));

    const auto addCommand =
        [this](const QString& objectName, const QString& text, const QList<QKeySequence>& keys,
            void (GalleryInterfaceZoomController::*command)()) {
            auto* action = new QAction(text, m_window);
            action->setObjectName(objectName);
            action->setShortcuts(keys);
            action->setShortcutContext(Qt::WindowShortcut);
            connect(action, &QAction::triggered, this, command);
            m_commands.append(action);
        };
    // Accept Ctrl/Cmd+= as well as Ctrl/Cmd++, so zooming in needs no Shift.
    // zh_CN: 同时接受 Ctrl/Cmd+= 与 Ctrl/Cmd++，放大时无需按 Shift。
    addCommand(QStringLiteral("galleryZoomInAction"), QStringLiteral("Zoom In"),
        {QKeySequence(Qt::CTRL | Qt::Key_Equal), QKeySequence(QKeySequence::ZoomIn)},
        &GalleryInterfaceZoomController::zoomIn);
    addCommand(QStringLiteral("galleryZoomOutAction"), QStringLiteral("Zoom Out"),
        {QKeySequence(QKeySequence::ZoomOut)}, &GalleryInterfaceZoomController::zoomOut);
    addCommand(QStringLiteral("galleryActualSizeAction"), QStringLiteral("Actual Size"),
        {QKeySequence(Qt::CTRL | Qt::Key_0)}, &GalleryInterfaceZoomController::resetZoom);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(kBubbleIdleCloseMs);
    connect(m_idleTimer, &QTimer::timeout, this, &GalleryInterfaceZoomController::closeBubbleWhenIdle);
    // Keep the bubble at the trailing edge while the logical window size changes.
    // zh_CN: 逻辑窗口尺寸变化时，气泡保持在尾部边缘。
    m_window->installEventFilter(this);
    connect(&settings, &GallerySettings::interfaceZoomPercentChanged, this,
        &GalleryInterfaceZoomController::applySavedZoom);
}

QList<QAction*> GalleryInterfaceZoomController::commands() const
{
    return m_commands;
}

void GalleryInterfaceZoomController::zoomIn()
{
    changeZoom(interfacezoom::zoomedIn(GallerySettings::instance().interfaceZoomPercent()));
}

void GalleryInterfaceZoomController::zoomOut()
{
    changeZoom(interfacezoom::zoomedOut(GallerySettings::instance().interfaceZoomPercent()));
}

void GalleryInterfaceZoomController::resetZoom()
{
    changeZoom(interfacezoom::DefaultPercent);
}

bool GalleryInterfaceZoomController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window && event->type() == QEvent::Resize && m_bubble && m_bubble->isOpen())
        placeBubble();
    return QObject::eventFilter(watched, event);
}

void GalleryInterfaceZoomController::changeZoom(int percent)
{
    GallerySettings::instance().setInterfaceZoomPercent(percent);
    showBubble();
    QString announcement = QStringLiteral("Zoom %1").arg(m_levelLabel->text());
    if (!m_restartHint->isHidden())
        announcement += QStringLiteral(". ") + m_restartHint->text();
    fluentSendAccessibleAnnouncement(m_bubble, announcement, FluentAccessibleAnnouncementPoliteness::Polite);
}

void GalleryInterfaceZoomController::applySavedZoom(int percent)
{
    if (percent != m_appliedPercent && platform::supportsLiveInterfaceZoom()) {
        // Relax the logical minimum before rescaling and tighten it afterwards, so
        // neither scale can enlarge the native window.
        // zh_CN: 缩放前先放宽逻辑最小尺寸、缩放后再收紧，避免任一倍率撑大原生窗口。
        const QSize currentMinimum = m_window->minimumSize();
        m_window->setMinimumSize(currentMinimum.boundedTo(metrics::AppWindow::minimumSize(QSize(), percent)));
        if (!platform::applyInterfaceZoom(percent / 100.0)) {
            m_window->setMinimumSize(currentMinimum);
            LOG_WARN(QStringLiteral("GalleryInterfaceZoom rejected percent=%1").arg(percent));
            return;
        }
        m_appliedPercent = percent;
        m_window->setMinimumSize(minimumSizeAt(percent));
        LOG_INFO(QStringLiteral("GalleryInterfaceZoom applied percent=%1 logicalSize=%2x%3")
                .arg(percent)
                .arg(m_window->width())
                .arg(m_window->height()));
    }
    if (m_bubble) {
        syncBubble();
        if (m_bubble->isOpen())
            placeBubble();
    }
}

void GalleryInterfaceZoomController::buildBubble()
{
    using fluent::basicinput::Button;

    auto* bubble = new ZoomBubble(m_window);
    m_bubble = bubble;
    bubble->setObjectName(QStringLiteral("galleryInterfaceZoomBubble"));
    bubble->setAccessibleName(QStringLiteral("Zoom"));
    bubble->setFocusOnOpenEnabled(false);
    connect(bubble, &fluent::dialogs_flyouts::Popup::closed, m_idleTimer, &QTimer::stop);

    auto* column = new QVBoxLayout(bubble);
    column->setContentsMargins(8, 6, 8, 6);
    column->setSpacing(4);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    column->addLayout(row);

    const auto addStepButton =
        [this, bubble, row](const QString& objectName, const QString& glyph, const QString& name,
            void (GalleryInterfaceZoomController::*command)()) {
            auto* button = new Button(bubble);
            button->setObjectName(objectName);
            button->setFluentStyle(Button::Subtle);
            button->setFluentLayout(Button::IconOnly);
            button->setFluentSize(Button::Small);
            button->setIconGlyph(glyph, Typography::IconSize::Standard);
            button->setAccessibleName(name);
            // Clicking keeps focus on the page, so only hover or keyboard focus holds the bubble open.
            // zh_CN: 点击不抢走页面焦点，只有悬停或键盘焦点才让气泡保持打开。
            button->setFocusPolicy(Qt::TabFocus);
            connect(button, &Button::clicked, this, command);
            row->addWidget(button);
            return button;
        };

    m_zoomOutButton = addStepButton(QStringLiteral("galleryZoomOutButton"), Typography::Icons::ZoomOut,
        QStringLiteral("Zoom out"), &GalleryInterfaceZoomController::zoomOut);
    m_levelLabel = new fluent::textfields::Label(percentText(m_appliedPercent), bubble);
    m_levelLabel->setObjectName(QStringLiteral("galleryZoomLevelLabel"));
    m_levelLabel->setFluentTypography(Typography::FontRole::BodyStrong);
    m_levelLabel->setAlignment(Qt::AlignCenter);
    // Reserve the widest level so the bubble never shifts while stepping.
    // zh_CN: 预留最宽档位的宽度，逐档调整时气泡不跳动。
    m_levelLabel->setMinimumWidth(m_levelLabel->fontMetrics().horizontalAdvance(QStringLiteral("200%")) + 8);
    row->addWidget(m_levelLabel);
    m_zoomInButton = addStepButton(QStringLiteral("galleryZoomInButton"), Typography::Icons::ZoomIn,
        QStringLiteral("Zoom in"), &GalleryInterfaceZoomController::zoomIn);
    row->addSpacing(8);

    m_resetButton = new Button(QStringLiteral("Reset"), bubble);
    m_resetButton->setObjectName(QStringLiteral("galleryZoomResetButton"));
    m_resetButton->setFluentStyle(Button::Standard);
    m_resetButton->setFluentSize(Button::Small);
    m_resetButton->setAccessibleName(QStringLiteral("Reset zoom"));
    m_resetButton->setFocusPolicy(Qt::TabFocus);
    connect(m_resetButton, &Button::clicked, this, &GalleryInterfaceZoomController::resetZoom);
    row->addWidget(m_resetButton);

    // Builds without live zoom keep the current scale until the next launch.
    // zh_CN: 不支持实时缩放的构建会保持当前倍率，直到下次启动。
    m_restartHint = new fluent::textfields::Label(QStringLiteral("Applies after restarting Gallery"), bubble);
    m_restartHint->setObjectName(QStringLiteral("galleryZoomRestartHint"));
    m_restartHint->setFluentTypography(Typography::FontRole::Caption);
    m_restartHint->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    m_restartHint->setVisible(false);
    column->addWidget(m_restartHint);
}

void GalleryInterfaceZoomController::showBubble()
{
    if (!m_bubble)
        buildBubble();
    syncBubble();
    placeBubble();
    if (!m_bubble->isOpen())
        m_bubble->open();
    m_idleTimer->start();
}

void GalleryInterfaceZoomController::placeBubble()
{
    m_bubble->adjustSize();
    const QSize card = ::fluent::overlay::visibleCardSize(m_bubble->size());
    const int x = m_window->layoutDirection() == Qt::RightToLeft
        ? kBubbleEdgeMargin
        : m_window->width() - card.width() - kBubbleEdgeMargin;
    m_bubble->setPosition(m_window, QPoint(x, metrics::TitleBar::Height + kBubbleTitleBarGap));
}

void GalleryInterfaceZoomController::syncBubble()
{
    const int percent = GallerySettings::instance().interfaceZoomPercent();
    m_levelLabel->setText(percentText(percent));
    m_zoomOutButton->setEnabled(interfacezoom::zoomedOut(percent) != percent);
    m_zoomInButton->setEnabled(interfacezoom::zoomedIn(percent) != percent);
    m_resetButton->setEnabled(percent != interfacezoom::DefaultPercent);
    m_restartHint->setVisible(percent != m_appliedPercent);
}

void GalleryInterfaceZoomController::closeBubbleWhenIdle()
{
    if (!m_bubble || !m_bubble->isOpen())
        return;
    // A hovered or focused bubble stays open until the pointer or focus leaves it.
    // zh_CN: 悬停或获得焦点时保持打开，直到指针或焦点离开。
    if (m_bubble->underMouse() || m_bubble->isAncestorOf(QApplication::focusWidget())) {
        m_idleTimer->start();
        return;
    }
    m_bubble->close();
}

QSize GalleryInterfaceZoomController::minimumSizeAt(int percent) const
{
    const QScreen* screen = m_window->screen();
    return metrics::AppWindow::minimumSize(screen ? screen->availableGeometry().size() : QSize(), percent);
}

} // namespace fluent::gallery
