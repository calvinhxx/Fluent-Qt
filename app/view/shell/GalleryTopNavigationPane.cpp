#include "GalleryTopNavigationPane.h"

#include <QAbstractAnimation>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Popup.h"
#include "view/support/GalleryMotion.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/layout/Divider.h"
#include "components/status_info/ToolTip.h"
#include "GalleryCompactFlyout.h"
#include "GalleryNavigationMetrics.h"
#include "GallerySpatialController.h"

namespace fluent::gallery {

namespace {
constexpr int kTopBarHeight = 48;
constexpr int kButtonSize = 36;
constexpr int kButtonSpacing = 4;
constexpr int kBarHorizontalMargin = 8;
constexpr int kFlyoutVerticalOffset = 8;
constexpr int kFlyoutEntranceOffset = 8;
constexpr int kFlyoutWindowMargin = 12;
} // namespace

GalleryTopNavigationPane::GalleryTopNavigationPane(const QVector<GalleryNavigationItem>& items,
                                                   QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumHeight(kTopBarHeight);

    auto* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setContentsMargins(kBarHorizontalMargin, 6, kBarHorizontalMargin, 6);
    layout->setSpacing(kButtonSpacing);

    for (const GalleryNavigationItem& item : items) {
        if (item.kind == GalleryNavigationItem::Kind::ComponentRoute) {
            if (!item.id.isEmpty() && !item.parentId.isEmpty()) {
                m_parentRoutes.insert(item.id, item.parentId);
                m_childItems[item.parentId].append(item);
            }
            continue;
        }
        if (item.kind == GalleryNavigationItem::Kind::SectionHeader) {
            if (!m_groupDivider) {
                m_groupDivider = new fluent::layout::Divider(Qt::Vertical, this);
                m_groupDivider->setObjectName(QStringLiteral("galleryTopNavigationDivider"));
                m_groupDivider->setFixedSize(1, 20);
                layout->addWidget(m_groupDivider, 0, Qt::AlignVCenter);
            }
            continue;
        }
        if (item.id.isEmpty())
            continue;

        auto* button = new fluent::basicinput::Button(this);
        button->setObjectName(QStringLiteral("galleryTopNavigationButton_%1").arg(item.id));
        button->setAccessibleName(item.title);
        fluent::status_info::ToolTip::attach(button, item.title,
                                             fluent::status_info::ToolTip::Above);
        button->setFluentLayout(fluent::basicinput::Button::IconOnly);
        button->setFluentSize(fluent::basicinput::Button::Small);
        button->setFluentStyle(fluent::basicinput::Button::Subtle);
        button->setCheckable(true);
        button->setIconGlyph(item.iconGlyph, Typography::IconSize::Standard);
        button->setFixedSize(kButtonSize, kButtonSize);
        // Checkable buttons expose Toggle to accessibility clients; it does not emit clicked.
        // zh_CN: 可选中按钮的辅助功能 Toggle 操作不发出 clicked，统一从 toggled 激活路由。
        connect(button, &fluent::basicinput::Button::toggled, this,
                [this, button, routeId = item.id]() {
                    if (routeId == QStringLiteral("settings"))
                        startSettingsIconRotation(button);
                    setSelectedRouteId(routeId);
                    emit routeActivated(routeId);
                    if (m_childItems.contains(routeId))
                        showChildFlyout(routeId, button);
                    else
                        closeChildFlyout();
                });
        m_buttons.insert(item.id, button);
        m_items.append(item);
        m_parentRoutes.insert(item.id, item.parentId);
        layout->addWidget(button);
    }
    m_moreButton = new fluent::basicinput::Button(this);
    m_moreButton->setObjectName(QStringLiteral("galleryTopNavigationMore"));
    m_moreButton->setAccessibleName(tr("More categories"));
    fluent::status_info::ToolTip::attach(m_moreButton, tr("More categories"),
                                         fluent::status_info::ToolTip::Above);
    m_moreButton->setFluentStyle(fluent::basicinput::Button::Subtle);
    m_moreButton->setFluentLayout(fluent::basicinput::Button::IconOnly);
    m_moreButton->setIconGlyph(Typography::Icons::More, Typography::IconSize::Standard);
    m_moreButton->setFixedSize(kButtonSize, kButtonSize);
    connect(m_moreButton, &fluent::basicinput::Button::clicked, this,
            [this] { showChildFlyout(QStringLiteral("overflow"), m_moreButton); });
    layout->addWidget(m_moreButton);
    layout->addStretch(1);
    updateButtonStyles();
}

void GalleryTopNavigationPane::setSelectedRouteId(const QString& routeId)
{
    if (m_selectedRouteId == routeId) {
        updateButtonStyles();
        return;
    }
    closeChildFlyout();
    m_selectedRouteId = routeId;
    updateButtonStyles();
    emit selectedRouteIdChanged(m_selectedRouteId);
}

QSize GalleryTopNavigationPane::sizeHint() const
{
    const int count = m_items.size();
    int labelSpace = 0;
    for (const auto& item : m_items) {
        const auto* button = m_buttons.value(item.id);
        labelSpace = qMax(labelSpace, button->fontMetrics().horizontalAdvance(item.title) + 4);
    }
    // Reserve a stable slot for the current category. Selection does not resize the bar.
    // zh_CN: 为当前分类名称保留固定余量，切换选中项时不让整条导航改变尺寸。
    if (count > 1 && m_buttons.contains(QStringLiteral("home")))
        labelSpace += m_buttons.value(QStringLiteral("home"))
                          ->fontMetrics()
                          .horizontalAdvance(m_items.first().title) +
                      4;
    return QSize(2 * kBarHorizontalMargin + count * kButtonSize +
                     qMax(0, count - 1) * kButtonSpacing + labelSpace +
                     (m_groupDivider ? 1 + kButtonSpacing : 0),
                 kTopBarHeight);
}

QSize GalleryTopNavigationPane::minimumSizeHint() const
{
    return QSize(kButtonSize + 2 * kBarHorizontalMargin, kTopBarHeight);
}

QString GalleryTopNavigationPane::visualRouteId() const
{
    QString visualRoute = m_selectedRouteId;
    while (!m_buttons.contains(visualRoute) && m_parentRoutes.contains(visualRoute))
        visualRoute = m_parentRoutes.value(visualRoute);

    return visualRoute;
}

void GalleryTopNavigationPane::updateButtonStyles()
{
    const auto selected = visualRouteId();
    for (auto iterator = m_buttons.begin(); iterator != m_buttons.end(); ++iterator) {
        const QSignalBlocker blocker(iterator.value());
        iterator.value()->setChecked(iterator.key() == selected);
    }
    updateButtonLayout();
}

void GalleryTopNavigationPane::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    closeChildFlyout(false);
    updateButtonLayout();
}

void GalleryTopNavigationPane::onThemeUpdated()
{
    updateButtonLayout();
    updateGeometry();
}

void GalleryTopNavigationPane::updateButtonLayout()
{
    if (!m_moreButton)
        return;
    const QString selected = visualRouteId();
    QVector<int> widths;
    QVector<bool> visible(m_items.size(), true);
    for (const auto& item : m_items) {
        auto* button = m_buttons.value(item.id);
        const bool label =
            item.id == selected || item.id == QStringLiteral("home") || m_items.size() == 1;
        button->setText(label ? item.title : QString());
        button->setFluentLayout(label ? fluent::basicinput::Button::IconBefore
                                      : fluent::basicinput::Button::IconOnly);
        widths.append(label ? qMax(kButtonSize, button->sizeHint().width()) : kButtonSize);
        button->setFixedSize(widths.last(), kButtonSize);
    }
    const int available = qMax(0, width() - 2 * kBarHorizontalMargin);
    const auto occupied = [&](bool more) {
        int total = more ? kButtonSize + kButtonSpacing : 0;
        for (int i = 0; i < widths.size(); ++i) {
            if (visible[i])
                total += widths[i] + kButtonSpacing;
        }
        return total - kButtonSpacing + (m_groupDivider ? 1 + kButtonSpacing : 0);
    };
    bool overflow = occupied(false) > available;
    // Keep Home and the active category in sight; other categories remain reachable in More.
    // zh_CN: 保留 Home 和当前分类，其余超出空间的分类仍可通过“更多”访问。
    for (int i = m_items.size() - 1; overflow && occupied(true) > available && i >= 0; --i) {
        if (m_items[i].id != selected && m_items[i].id != QStringLiteral("home"))
            visible[i] = false;
    }
    QVector<GalleryNavigationItem> hidden;
    bool hasControls = false;
    for (int i = 0; i < m_items.size(); ++i) {
        m_buttons.value(m_items[i].id)->setVisible(visible[i]);
        if (!visible[i])
            hidden.append(m_items[i]);
        else if (m_items[i].group == QStringLiteral("Controls"))
            hasControls = true;
    }
    m_childItems[QStringLiteral("overflow")] = hidden;
    m_moreButton->setVisible(!hidden.isEmpty());
    if (m_groupDivider) {
        const auto* foundation = m_buttons.value(QStringLiteral("foundation"));
        m_groupDivider->setVisible(hasControls && foundation && !foundation->isHidden());
    }
    // Popup anchors use the geometry of the newly expanded selected button.
    // zh_CN: 弹层锚点使用选中按钮展开名称后的几何位置。
    layout()->activate();
}

void GalleryTopNavigationPane::showChildFlyout(const QString& routeId,
                                               fluent::basicinput::Button* anchor)
{
    const QVector<GalleryNavigationItem> children = m_childItems.value(routeId);
    if (!anchor || children.isEmpty() || !window())
        return;

    closeChildFlyout(false);
    m_childFlyout = new fluent::dialogs_flyouts::Popup(this);
    m_childFlyout->setObjectName(QStringLiteral("galleryTopNavigationFlyout"));
    m_childFlyout->setAnimationEnabled(true);
    // Dismiss instantly without an exit animation: the shared fade-out misbehaves over a native
    // vibrancy backdrop on macOS, so the flyout just hides on close while keeping its slide-up entrance.
    // zh_CN: 关闭时不播放退场动画,直接隐藏:共享的淡出在 macOS 原生 vibrancy 背景上表现异常;入场的上滑动画保留。
    m_childFlyout->setExitAnimationEnabled(false);
    m_childFlyout->setClosePolicy(fluent::dialogs_flyouts::Popup::ClosePolicy(
        fluent::dialogs_flyouts::Popup::CloseOnPressOutside |
        fluent::dialogs_flyouts::Popup::CloseOnEscape));
    // Light-dismiss consumes the outside press: clicking another top nav item first closes the
    // current flyout, and a second click is required to activate/open the target item.
    // zh_CN: 轻关闭会吞掉这次外部按下：点击另一个顶部导航项时先关闭当前浮窗，需要第二次点击才激活/打开目标项。
    m_childFlyout->setLightDismissConsumesPress(true);
    // Guard the destroyed handler against a STALE flyout nulling the CURRENT one. closeChildFlyout()
    // hands the outgoing popup to deleteLater() and immediately lets a new flyout be assigned to
    // m_childFlyout; the outgoing popup's destroyed signal then fires a turn later. Without the
    // identity check it would clear m_childFlyout even though it now points at the freshly opened,
    // visible flyout — leaving m_childFlyout null so the next row click's closeChildFlyout() early-returns
    // and the flyout never collapses. zh_CN: 防止「旧浮窗」的析构把「当前浮窗」置空。closeChildFlyout() 把上一个
    // 浮窗交给 deleteLater() 后会立刻把新浮窗赋给 m_childFlyout;旧浮窗的 destroyed 信号在下一轮才触发。若不做身份校验,
    // 它会把已指向新（可见）浮窗的 m_childFlyout 清空——导致下次点击行时 closeChildFlyout() 提前返回,浮窗无法收起。
    auto* createdFlyout = m_childFlyout;
    connect(createdFlyout, &QObject::destroyed, this, [this, createdFlyout]() {
        if (m_childFlyout != createdFlyout)
            return;
        m_childFlyout = nullptr;
        m_childFlyoutPanel = nullptr;
    });

    m_childFlyoutPanel = new CompactFlyoutPanel(m_childFlyout);
    m_childFlyoutPanel->setObjectName(QStringLiteral("galleryTopNavigationFlyoutPanel"));
    auto* layout = new QVBoxLayout(m_childFlyoutPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    for (const GalleryNavigationItem& child : children) {
        auto* row = new CompactFlyoutRow(child.id, child.title, child.id == m_selectedRouteId,
                                         m_childFlyoutPanel);
        row->onActivated = [this](const QString& childRouteId) {
            closeChildFlyout(false);
            setSelectedRouteId(childRouteId);
            emit routeActivated(childRouteId);
        };
        layout->addWidget(row);
    }

    QWidget* host = window();
    const QSize contentSize = m_childFlyoutPanel->sizeHint();
    const QSize cardSize(contentSize.width() + kCompactFlyoutContentMargins.left() +
                             kCompactFlyoutContentMargins.right(),
                         contentSize.height() + kCompactFlyoutContentMargins.top() +
                             kCompactFlyoutContentMargins.bottom());
    const auto* spatial = host->findChild<GallerySpatialController*>();
    const QPoint anchorBottom =
        spatial ? spatial->projectedPosition(anchor, QPoint(0, anchor->height()))
                : anchor->mapTo(host, QPoint(0, anchor->height()));
    QPoint cardTopLeft = anchorBottom + QPoint(0, kFlyoutVerticalOffset);
    cardTopLeft = fluent::overlay::clampCardTopLeft(
        cardTopLeft, cardSize, fluent::overlay::overlaySurfaceRect(host), kFlyoutWindowMargin);

    const QRect panelRect =
        fluent::overlay::visibleCardRect(
            QRect(QPoint(0, 0), fluent::overlay::outerSizeForVisibleCard(cardSize)))
            .marginsRemoved(kCompactFlyoutContentMargins);
    m_childFlyout->resize(fluent::overlay::outerSizeForVisibleCard(cardSize));
    m_childFlyoutPanel->setGeometry(panelRect);
    m_childFlyoutPanel->show();
    m_childFlyout->setPosition(host, cardTopLeft);
    m_childFlyout->open();

    const QPoint endPosition = m_childFlyout->pos();
    auto* entrance = new QPropertyAnimation(m_childFlyout, "pos", m_childFlyout);
    entrance->setObjectName(QStringLiteral("galleryTopNavigationFlyoutEntranceAnimation"));
    const auto animation = themeAnimation();
    entrance->setDuration(animation.fast);
    entrance->setEasingCurve(animation.decelerate);
    entrance->setStartValue(endPosition - QPoint(0, kFlyoutEntranceOffset));
    entrance->setEndValue(endPosition);
    m_childFlyout->move(entrance->startValue().toPoint());
    ::fluent::gallery::motion::startFiniteTransition(entrance, animation.fast, true,
                                                     QAbstractAnimation::DeleteWhenStopped);
}

void GalleryTopNavigationPane::closeChildFlyout(bool animated)
{
    if (!m_childFlyout)
        return;
    auto* popup = m_childFlyout;
    m_childFlyout = nullptr;
    m_childFlyoutPanel = nullptr;
    if (!animated)
        popup->setExitAnimationEnabled(false);
    connect(popup, &fluent::dialogs_flyouts::Popup::closed, popup, &QObject::deleteLater);
    if (popup->isOpen() || popup->isVisible())
        popup->close();
    else
        popup->deleteLater();
}

void GalleryTopNavigationPane::startSettingsIconRotation(fluent::basicinput::Button* button)
{
    if (!button)
        return;
    auto* animation = button->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTopSettingsIconRotationAnimation"), Qt::FindDirectChildrenOnly);
    if (!animation) {
        animation = new QPropertyAnimation(button, "iconRotation", button);
        animation->setObjectName(QStringLiteral("galleryTopSettingsIconRotationAnimation"));
        connect(animation, &QPropertyAnimation::finished, button,
                [button]() { button->setIconRotation(0.0); });
    }
    animation->stop();
    const auto motion = themeAnimation();
    animation->setDuration(motion.slow);
    animation->setEasingCurve(motion.decelerate);
    animation->setStartValue(button->iconRotation());
    animation->setEndValue(button->iconRotation() + kSettingsIconRotationDegrees - 0.01);
    ::fluent::gallery::motion::startFiniteTransition(animation, motion.slow);
}

} // namespace fluent::gallery
