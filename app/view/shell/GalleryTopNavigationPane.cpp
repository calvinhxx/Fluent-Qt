#include "GalleryTopNavigationPane.h"

#include <QAbstractAnimation>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/status_info/ToolTip.h"
#include "GalleryCompactFlyout.h"
#include "GalleryNavigationMetrics.h"

namespace fluent::gallery {

namespace {
constexpr int kTopBarHeight = 48;
constexpr int kButtonSize = 32;
constexpr int kButtonSpacing = 4;
constexpr int kBarHorizontalMargin = 8;
constexpr int kFlyoutVerticalOffset = 8;
constexpr int kFlyoutEntranceOffset = 8;
constexpr int kFlyoutWindowMargin = 12;
}

GalleryTopNavigationPane::GalleryTopNavigationPane(
    const QVector<GalleryNavigationItem>& items,
    QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumHeight(kTopBarHeight);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kBarHorizontalMargin, 8, kBarHorizontalMargin, 8);
    layout->setSpacing(kButtonSpacing);

    for (const GalleryNavigationItem& item : items) {
        if (item.kind == GalleryNavigationItem::Kind::ComponentRoute) {
            if (!item.id.isEmpty() && !item.parentId.isEmpty()) {
                m_parentRoutes.insert(item.id, item.parentId);
                m_childItems[item.parentId].append(item);
            }
            continue;
        }
        if (item.kind == GalleryNavigationItem::Kind::SectionHeader || item.id.isEmpty())
            continue;

        auto* button = new fluent::basicinput::Button(this);
        button->setObjectName(QStringLiteral("galleryTopNavigationButton_%1").arg(item.id));
        button->setAccessibleName(item.title);
        fluent::status_info::ToolTip::attach(button, item.title,
                                              fluent::status_info::ToolTip::Above);
        button->setFluentLayout(fluent::basicinput::Button::IconOnly);
        button->setFluentSize(fluent::basicinput::Button::Small);
        button->setFluentStyle(fluent::basicinput::Button::Subtle);
        button->setIconGlyph(item.iconGlyph, Typography::IconSize::Standard);
        button->setFixedSize(kButtonSize, kButtonSize);
        connect(button, &fluent::basicinput::Button::clicked,
                this, [this, button, routeId = item.id]() {
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
        m_parentRoutes.insert(item.id, item.parentId);
        layout->addWidget(button);
    }
}

void GalleryTopNavigationPane::setSelectedRouteId(const QString& routeId)
{
    if (m_selectedRouteId == routeId)
        return;
    closeChildFlyout();
    m_selectedRouteId = routeId;
    updateButtonStyles();
    emit selectedRouteIdChanged(m_selectedRouteId);
}

QSize GalleryTopNavigationPane::sizeHint() const
{
    const int count = m_buttons.size();
    return QSize(2 * kBarHorizontalMargin + count * kButtonSize
                     + qMax(0, count - 1) * kButtonSpacing,
                 kTopBarHeight);
}

void GalleryTopNavigationPane::updateButtonStyles()
{
    QString visualRoute = m_selectedRouteId;
    while (!m_buttons.contains(visualRoute) && m_parentRoutes.contains(visualRoute))
        visualRoute = m_parentRoutes.value(visualRoute);

    for (auto iterator = m_buttons.begin(); iterator != m_buttons.end(); ++iterator) {
        iterator.value()->setFluentStyle(iterator.key() == visualRoute
                                             ? fluent::basicinput::Button::Standard
                                             : fluent::basicinput::Button::Subtle);
    }
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
        fluent::dialogs_flyouts::Popup::CloseOnPressOutside
        | fluent::dialogs_flyouts::Popup::CloseOnEscape));
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
        auto* row = new CompactFlyoutRow(child.id,
                                         child.title,
                                         child.id == m_selectedRouteId,
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
    const QSize cardSize(contentSize.width() + kCompactFlyoutContentMargins.left()
                             + kCompactFlyoutContentMargins.right(),
                         contentSize.height() + kCompactFlyoutContentMargins.top()
                             + kCompactFlyoutContentMargins.bottom());
    const QPoint anchorTopLeft = anchor->mapTo(host, QPoint(0, 0));
    QPoint cardTopLeft(anchorTopLeft.x(),
                       anchorTopLeft.y() + anchor->height() + kFlyoutVerticalOffset);
    cardTopLeft = fluent::overlay::clampCardTopLeft(cardTopLeft,
                                                    cardSize,
                                                    fluent::overlay::overlaySurfaceRect(host),
                                                    kFlyoutWindowMargin);

    const QRect panelRect = fluent::overlay::visibleCardRect(
                               QRect(QPoint(0, 0),
                                     fluent::overlay::outerSizeForVisibleCard(cardSize)))
                               .marginsRemoved(kCompactFlyoutContentMargins);
    m_childFlyout->resize(fluent::overlay::outerSizeForVisibleCard(cardSize));
    m_childFlyoutPanel->setGeometry(panelRect);
    m_childFlyoutPanel->show();
    m_childFlyout->setPosition(host, cardTopLeft);
    m_childFlyout->open();

    const QPoint endPosition = m_childFlyout->pos();
    auto* entrance = new QPropertyAnimation(m_childFlyout, "pos", m_childFlyout);
    entrance->setObjectName(QStringLiteral("galleryTopNavigationFlyoutEntranceAnimation"));
    entrance->setDuration(themeAnimation().fast);
    entrance->setEasingCurve(themeAnimation().decelerate);
    entrance->setStartValue(endPosition - QPoint(0, kFlyoutEntranceOffset));
    entrance->setEndValue(endPosition);
    m_childFlyout->move(entrance->startValue().toPoint());
    entrance->start(QAbstractAnimation::DeleteWhenStopped);
}

void GalleryTopNavigationPane::closeChildFlyout(bool animated)
{
    if (!m_childFlyout)
        return;
    auto* popup = m_childFlyout;
    m_childFlyout = nullptr;
    m_childFlyoutPanel = nullptr;
    if (animated && popup->isVisible()) {
        connect(popup, &fluent::dialogs_flyouts::Popup::closed,
                popup, &QObject::deleteLater);
        popup->close();
    } else {
        popup->hide();
        popup->deleteLater();
    }
}

void GalleryTopNavigationPane::startSettingsIconRotation(
    fluent::basicinput::Button* button)
{
    if (!button)
        return;
    auto* animation = button->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTopSettingsIconRotationAnimation"),
        Qt::FindDirectChildrenOnly);
    if (!animation) {
        animation = new QPropertyAnimation(button, "iconRotation", button);
        animation->setObjectName(QStringLiteral("galleryTopSettingsIconRotationAnimation"));
        connect(animation, &QPropertyAnimation::finished,
                button, [button]() { button->setIconRotation(0.0); });
    }
    animation->stop();
    animation->setDuration(themeAnimation().slow);
    animation->setEasingCurve(themeAnimation().decelerate);
    animation->setStartValue(button->iconRotation());
    animation->setEndValue(button->iconRotation() + kSettingsIconRotationDegrees - 0.01);
    animation->start();
}

} // namespace fluent::gallery
