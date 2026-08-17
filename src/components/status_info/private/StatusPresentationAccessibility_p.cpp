#include "StatusPresentationAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QCoreApplication>
#include <QVector>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/status_info/InfoBar.h"
#include "components/status_info/Shimmer.h"

namespace fluent::status_info::detail {

namespace {

const QString& dismissAction()
{
    static const QString action = QStringLiteral("dismiss");
    return action;
}

QString statusText(const char* source)
{
    return QCoreApplication::translate(
        "StatusPresentationAccessibility", source);
}

QString severityText(InfoBar::InfoBarSeverity severity)
{
    switch (severity) {
    case InfoBar::Success:
        return statusText("Success");
    case InfoBar::Warning:
        return statusText("Warning");
    case InfoBar::Error:
        return statusText("Error");
    case InfoBar::Informational:
        return statusText("Informational");
    }
    return {};
}

QString infoBarContentText(const InfoBar* bar)
{
    if (!bar)
        return {};
    if (bar->title().isEmpty())
        return bar->message();
    if (bar->message().isEmpty())
        return bar->title();
    return bar->title() + QStringLiteral(": ") + bar->message();
}

QString infoBarAnnouncementText(const InfoBar* bar)
{
    if (!bar)
        return {};
    const QString severity = severityText(bar->severity());
    const QString content = infoBarContentText(bar);
    return content.isEmpty()
        ? severity
        : severity + QStringLiteral(": ") + content;
}

bool canDismiss(const InfoBar* bar)
{
    return bar && bar->isOpen() && bar->isEnabled()
        && bar->isClosable();
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

void announceInfoBar(InfoBar* bar)
{
    if (!bar || !bar->isOpen() || !bar->isVisible())
        return;
    fluentSendAccessibleAnnouncement(
        bar,
        infoBarAnnouncementText(bar),
        bar->severity() == InfoBar::Error
            ? FluentAccessibleAnnouncementPoliteness::Assertive
            : FluentAccessibleAnnouncementPoliteness::Polite);
}

#if QT_CONFIG(accessibility)

void sendInfoBarStateChanged(InfoBar* bar)
{
    if (!bar)
        return;
    QAccessible::State changed;
    changed.active = true;
    changed.invisible = true;
    changed.offscreen = true;
    QAccessibleStateChangeEvent event(bar, changed);
    QAccessible::updateAccessibility(&event);
}

void sendShimmerStateChanged(
    Shimmer* shimmer, bool busy, bool animated, bool invisible)
{
    if (!shimmer)
        return;
    QAccessible::State changed;
    changed.busy = busy;
    changed.animated = animated;
    changed.invisible = invisible;
    QAccessibleStateChangeEvent event(shimmer, changed);
    QAccessible::updateAccessibility(&event);
}

#endif

} // namespace

#if QT_CONFIG(accessibility)

class InfoBarAccessible final : public QAccessibleWidget {
public:
    explicit InfoBarAccessible(InfoBar* bar)
        : QAccessibleWidget(bar, QAccessible::Notification)
    {
    }

    QAccessibleInterface* childAt(int x, int y) const override
    {
        for (int index = childCount() - 1; index >= 0; --index) {
            QAccessibleInterface* candidate = child(index);
            if (candidate && candidate->rect().contains(x, y))
                return candidate;
        }
        return nullptr;
    }

    QAccessibleInterface* focusChild() const override
    {
        QWidget* focused = QApplication::focusWidget();
        if (!focused)
            return nullptr;
        for (QWidget* candidate : semanticChildren()) {
            if (candidate == focused
                || (candidate && candidate->isAncestorOf(focused))) {
                return QAccessible::queryAccessibleInterface(candidate);
            }
        }
        return nullptr;
    }

    int childCount() const override
    {
        return semanticChildren().size();
    }

    int indexOfChild(
        const QAccessibleInterface* childInterface) const override
    {
        if (!childInterface)
            return -1;
        const QVector<QWidget*> children = semanticChildren();
        return children.indexOf(
            qobject_cast<QWidget*>(childInterface->object()));
    }

    QAccessibleInterface* child(int index) const override
    {
        const QVector<QWidget*> children = semanticChildren();
        QWidget* candidate = index >= 0 && index < children.size()
            ? children.at(index) : nullptr;
        return candidate
            ? QAccessible::queryAccessibleInterface(candidate)
            : nullptr;
    }

    QString text(QAccessible::Text type) const override
    {
        const QString authored = QAccessibleWidget::text(type);
        InfoBar* current = bar();
        if (!authored.isEmpty() || !current)
            return authored;
        if (type == QAccessible::Name)
            return infoBarContentText(current);
        if (type == QAccessible::Description)
            return severityText(current->severity());
        return authored;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        InfoBar* current = bar();
        if (!current)
            return result;
        result.active = current->isOpen();
        result.invisible = !current->isOpen() || result.invisible;
        result.offscreen = !current->isOpen() || result.offscreen;
        result.focusable = false;
        result.focused = false;
        return result;
    }

    QStringList actionNames() const override
    {
        return canDismiss(bar())
            ? QStringList{dismissAction()}
            : QStringList{};
    }

    QString localizedActionName(
        const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? statusText("Dismiss")
            : QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? statusText("Dismisses the notification")
            : QAccessibleWidget::localizedActionDescription(
                  actionName);
    }

    void doAction(const QString& actionName) override
    {
        InfoBar* current = bar();
        if (actionName == dismissAction() && canDismiss(current)
            && current->m_closeButton) {
            current->m_closeButton->click();
        }
    }

private:
    QVector<QWidget*> semanticChildren() const
    {
        QVector<QWidget*> result;
        InfoBar* current = bar();
        if (!current || !current->isOpen())
            return result;
        if (current->m_actionWidget
            && current->m_actionWidget->isVisible()) {
            result.append(current->m_actionWidget);
        }
        if (current->m_closeButton
            && current->m_closeButton->isVisible()) {
            result.append(current->m_closeButton);
        }
        return result;
    }

    InfoBar* bar() const
    {
        return static_cast<InfoBar*>(widget());
    }
};

class ShimmerAccessible final : public QAccessibleWidget {
public:
    explicit ShimmerAccessible(Shimmer* shimmer)
        : QAccessibleWidget(shimmer, QAccessible::Animation)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString authored = QAccessibleWidget::text(type);
        if (!authored.isEmpty())
            return authored;
        if (type == QAccessible::Name)
            return statusText("Loading");
        if (type == QAccessible::Description)
            return statusText("Content is loading");
        return authored;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        Shimmer* current = shimmer();
        if (!current)
            return result;
        result.busy = current->isActive() && current->isEnabled();
        result.animated = current->isAnimationRunning();
        result.invisible = !current->isActive() || result.invisible;
        result.focusable = false;
        result.focused = false;
        result.readOnly = true;
        return result;
    }

    QStringList actionNames() const override { return {}; }

private:
    Shimmer* shimmer() const
    {
        return static_cast<Shimmer*>(widget());
    }
};

namespace {

QAccessibleInterface* statusPresentationAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* bar = qobject_cast<InfoBar*>(object))
        return new InfoBarAccessible(bar);
    if (auto* shimmer = qobject_cast<Shimmer*>(object))
        return new ShimmerAccessible(shimmer);
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureStatusPresentationAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(
            statusPresentationAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

QString infoBarDismissAccessibleName()
{
    return statusText("Dismiss notification");
}

void notifyInfoBarAccessibilityContentChanged(InfoBar* bar)
{
#if QT_CONFIG(accessibility)
    if (bar && bar->accessibleName().isEmpty())
        sendEvent(bar, QAccessible::NameChanged);
    announceInfoBar(bar);
#else
    Q_UNUSED(bar)
#endif
}

void notifyInfoBarAccessibilitySeverityChanged(InfoBar* bar)
{
#if QT_CONFIG(accessibility)
    if (bar && bar->accessibleDescription().isEmpty())
        sendEvent(bar, QAccessible::DescriptionChanged);
    announceInfoBar(bar);
#else
    Q_UNUSED(bar)
#endif
}

void notifyInfoBarAccessibilityOpenChanged(InfoBar* bar)
{
#if QT_CONFIG(accessibility)
    sendInfoBarStateChanged(bar);
    sendEvent(bar, QAccessible::ActionChanged);
    sendEvent(bar, QAccessible::ObjectReorder);
    announceInfoBar(bar);
#else
    Q_UNUSED(bar)
#endif
}

void notifyInfoBarAccessibilityDismissChanged(InfoBar* bar)
{
#if QT_CONFIG(accessibility)
    if (bar && bar->isOpen()) {
        sendEvent(bar, QAccessible::ActionChanged);
        sendEvent(bar, QAccessible::ObjectReorder);
    }
#else
    Q_UNUSED(bar)
#endif
}

void notifyInfoBarAccessibilityStructureChanged(InfoBar* bar)
{
#if QT_CONFIG(accessibility)
    if (bar && bar->isOpen())
        sendEvent(bar, QAccessible::ObjectReorder);
#else
    Q_UNUSED(bar)
#endif
}

void notifyInfoBarAccessibilityEnabledChanged(InfoBar* bar)
{
#if QT_CONFIG(accessibility)
    if (!bar)
        return;
    QAccessible::State changed;
    changed.disabled = true;
    QAccessibleStateChangeEvent event(bar, changed);
    QAccessible::updateAccessibility(&event);
    if (bar->isOpen())
        sendEvent(bar, QAccessible::ActionChanged);
#else
    Q_UNUSED(bar)
#endif
}

void notifyShimmerAccessibilityActiveChanged(Shimmer* shimmer)
{
#if QT_CONFIG(accessibility)
    sendShimmerStateChanged(shimmer, true, true, true);
#else
    Q_UNUSED(shimmer)
#endif
}

void notifyShimmerAccessibilityAnimationChanged(Shimmer* shimmer)
{
#if QT_CONFIG(accessibility)
    sendShimmerStateChanged(shimmer, false, true, false);
#else
    Q_UNUSED(shimmer)
#endif
}

void notifyShimmerAccessibilityEnabledChanged(Shimmer* shimmer)
{
#if QT_CONFIG(accessibility)
    sendShimmerStateChanged(shimmer, true, true, false);
#else
    Q_UNUSED(shimmer)
#endif
}

} // namespace fluent::status_info::detail
