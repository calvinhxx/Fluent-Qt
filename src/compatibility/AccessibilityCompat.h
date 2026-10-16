#pragma once

// Accessibility API and event adaptation.

#include "QtFeatures.h"
#include <QAccessible>
#include <QAccessibleInterface>
#include <utility>

using FluentAccessibleRelationList = decltype(std::declval<const QAccessibleInterface&>().relations(
    QAccessible::AllRelations));

enum class FluentAccessibleAnnouncementPoliteness { Unspecified = -1, Polite, Assertive };

/**
 * @brief Returns the accessibility event type used for live announcements.
 * zh_CN: 返回当前 Qt 版本用于实时播报的无障碍事件类型。
 */
inline QAccessible::Event fluentAccessibleAnnouncementEventType()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    return QAccessible::Announcement;
#else
    return QAccessible::Alert;
#endif
}

/**
 * @brief Reports whether accessibility announcements expose text and politeness.
 * zh_CN: 返回当前 Qt 版本的无障碍播报是否提供文本和礼貌级别。
 */
constexpr bool fluentAccessibleAnnouncementSupportsDetails()
{
    return QT_VERSION >= QT_VERSION_CHECK(6, 8, 0);
}

/**
 * @brief Extracts announcement text when the active Qt API provides it.
 * zh_CN: 当当前 Qt API 支持时提取无障碍播报文本。
 */
inline QString fluentAccessibleAnnouncementMessage(const QAccessibleEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (!event || event->type() != QAccessible::Announcement)
        return {};
    return static_cast<const QAccessibleAnnouncementEvent*>(event)->message();
#else
    Q_UNUSED(event);
    return {};
#endif
}

/**
 * @brief Extracts announcement politeness when the active Qt API provides it.
 * zh_CN: 当当前 Qt API 支持时提取无障碍播报礼貌级别。
 */
inline FluentAccessibleAnnouncementPoliteness fluentAccessibleAnnouncementPoliteness(const QAccessibleEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (!event || event->type() != QAccessible::Announcement)
        return FluentAccessibleAnnouncementPoliteness::Unspecified;
    const auto politeness = static_cast<const QAccessibleAnnouncementEvent*>(event)->politeness();
    return politeness == QAccessible::AnnouncementPoliteness::Assertive
        ? FluentAccessibleAnnouncementPoliteness::Assertive
        : FluentAccessibleAnnouncementPoliteness::Polite;
#else
    Q_UNUSED(event);
    return FluentAccessibleAnnouncementPoliteness::Unspecified;
#endif
}

/**
 * @brief Sends an accessibility announcement through the best available Qt API.
 * zh_CN: 通过当前 Qt 版本可用的最佳 API 发送无障碍播报。
 */
inline void fluentSendAccessibleAnnouncement(QObject* object,
    const QString& message,
    FluentAccessibleAnnouncementPoliteness politeness)
{
#if QT_CONFIG(accessibility)
    if (!object || message.isEmpty())
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QAccessibleAnnouncementEvent event(object, message);
    event.setPoliteness(politeness == FluentAccessibleAnnouncementPoliteness::Assertive
            ? QAccessible::AnnouncementPoliteness::Assertive
            : QAccessible::AnnouncementPoliteness::Polite);
#else
    Q_UNUSED(politeness);
    QAccessibleEvent event(object, QAccessible::Alert);
#endif
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(object);
    Q_UNUSED(message);
    Q_UNUSED(politeness);
#endif
}

/**
 * @brief Replaces an accessibility interface cached before a widget finished
 * constructing when it does not expose the expected role.
 * zh_CN: 当控件构造完成前缓存的无障碍接口角色不正确时，重新创建该接口。
 */
inline void fluentRefreshAccessibleInterfaceAfterConstruction(QObject* object, QAccessible::Role expectedRole)
{
#if QT_CONFIG(accessibility)
    if (!object)
        return;
    QAccessibleInterface* accessibleInterface = QAccessible::queryAccessibleInterface(object);
    if (!accessibleInterface || accessibleInterface->role() == expectedRole)
        return;

    const QAccessible::Id id = QAccessible::uniqueId(accessibleInterface);
    if (id)
        QAccessible::deleteAccessibleInterface(id);
    QAccessible::queryAccessibleInterface(object);
#else
    Q_UNUSED(object);
    Q_UNUSED(expectedRole);
#endif
}
