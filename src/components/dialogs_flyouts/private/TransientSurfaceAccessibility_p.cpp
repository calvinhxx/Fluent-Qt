#include "TransientSurfaceAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>

#include "components/dialogs_flyouts/CoachMark.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/dialogs_flyouts/TeachingTip.h"

namespace fluent::dialogs_flyouts::detail {

#if QT_CONFIG(accessibility)

namespace {

const QString& dismissAction()
{
    static const QString action = QStringLiteral("dismiss");
    return action;
}

QString transientText(const char* source)
{
    return QCoreApplication::translate(
        "TransientSurfaceAccessibility", source);
}

bool popupCanDismiss(const Popup* popup)
{
    return popup && popup->isOpen() && popup->isEnabled()
        && popup->closePolicy().testFlag(Popup::CloseOnEscape);
}

QWidget* relationTargetFor(QWidget* surface,
                           QAccessible::Relation* relation)
{
    if (auto* teachingTip = qobject_cast<TeachingTip*>(surface)) {
        *relation = QAccessible::DescriptionFor;
        return teachingTip->target();
    }
    if (auto* flyout = qobject_cast<Flyout*>(surface)) {
        *relation = QAccessible::Controlled;
        return flyout->anchor();
    }
    if (auto* coachMark = qobject_cast<CoachMark*>(surface)) {
        *relation = QAccessible::DescriptionFor;
        return coachMark->target();
    }
    return nullptr;
}

QList<std::pair<QAccessibleInterface*, QAccessible::Relation>>
surfaceRelations(
    QWidget* surface,
    QList<std::pair<QAccessibleInterface*, QAccessible::Relation>> inherited,
    QAccessible::Relation match)
{
    QAccessible::Relation relation = QAccessible::AllRelations;
    QWidget* target = relationTargetFor(surface, &relation);
    if (!target || !(match & relation))
        return inherited;

    QAccessibleInterface* targetInterface =
        QAccessible::queryAccessibleInterface(target);
    if (!targetInterface)
        return inherited;

    for (const auto& existing : inherited) {
        if (existing.first == targetInterface && existing.second == relation)
            return inherited;
    }
    inherited.append({targetInterface, relation});
    return inherited;
}

class PopupAccessible final : public QAccessibleWidget {
public:
    explicit PopupAccessible(Popup* popup)
        : QAccessibleWidget(popup, QAccessible::Pane)
    {
    }

    QAccessible::Role role() const override
    {
        return qobject_cast<TeachingTip*>(popup())
            ? QAccessible::HelpBalloon
            : QAccessible::Pane;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        const Popup* surface = popup();
        if (!surface)
            return result;

        const bool open = surface->isOpen();
        result.active = open;
        result.invisible = !open;
        result.offscreen = !open || result.offscreen;
        result.modal = open && surface->isModal();
        result.focusable = open && surface->isEnabled()
            && surface->focusPolicy() != Qt::NoFocus;
        result.focused = open && surface->hasFocus();
        return result;
    }

    QList<std::pair<QAccessibleInterface*, QAccessible::Relation>>
    relations(QAccessible::Relation match) const override
    {
        return surfaceRelations(
            popup(), QAccessibleWidget::relations(match), match);
    }

    QStringList actionNames() const override
    {
        Popup* surface = popup();
        if (!surface || !surface->isOpen() || !surface->isEnabled())
            return {};
        QStringList result = QAccessibleWidget::actionNames();
        if (popupCanDismiss(surface) && !result.contains(dismissAction()))
            result.append(dismissAction());
        return result;
    }

    QString localizedActionName(const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? transientText("Dismiss")
            : QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? transientText("Closes the transient surface")
            : QAccessibleWidget::localizedActionDescription(actionName);
    }

    void doAction(const QString& actionName) override
    {
        Popup* surface = popup();
        if (actionName != dismissAction()) {
            QAccessibleWidget::doAction(actionName);
            return;
        }
        if (!popupCanDismiss(surface))
            return;

        if (auto* teachingTip = qobject_cast<TeachingTip*>(surface)) {
            teachingTip->closeWithReason(TeachingTip::LightDismiss);
            return;
        }
        surface->closeWithReason(Popup::Escape);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == dismissAction() && popupCanDismiss(popup()))
            return {QStringLiteral("Escape")};
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

private:
    Popup* popup() const
    {
        return static_cast<Popup*>(widget());
    }
};

class CoachMarkAccessible final : public QAccessibleWidget {
public:
    explicit CoachMarkAccessible(CoachMark* coachMark)
        : QAccessibleWidget(coachMark, QAccessible::HelpBalloon)
    {
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        const CoachMark* surface = coachMark();
        if (!surface)
            return result;

        const bool open = surface->isOpen();
        result.active = open;
        result.invisible = !open;
        result.offscreen = !open || result.offscreen;
        result.focusable = false;
        result.focused = false;
        return result;
    }

    QList<std::pair<QAccessibleInterface*, QAccessible::Relation>>
    relations(QAccessible::Relation match) const override
    {
        return surfaceRelations(
            coachMark(), QAccessibleWidget::relations(match), match);
    }

    QStringList actionNames() const override
    {
        CoachMark* surface = coachMark();
        if (!surface || !surface->isOpen() || !surface->isEnabled())
            return {};
        QStringList result = QAccessibleWidget::actionNames();
        if (!result.contains(dismissAction()))
            result.append(dismissAction());
        return result;
    }

    QString localizedActionName(const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? transientText("Dismiss")
            : QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? transientText("Closes the help surface")
            : QAccessibleWidget::localizedActionDescription(actionName);
    }

    void doAction(const QString& actionName) override
    {
        CoachMark* surface = coachMark();
        if (actionName == dismissAction() && surface
            && surface->isOpen() && surface->isEnabled()) {
            surface->close();
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        CoachMark* surface = coachMark();
        if (actionName == dismissAction() && surface
            && surface->isOpen() && surface->isEnabled()) {
            return {QStringLiteral("Escape")};
        }
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

private:
    CoachMark* coachMark() const
    {
        return static_cast<CoachMark*>(widget());
    }
};

QAccessibleInterface* transientSurfaceAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* coachMark = qobject_cast<CoachMark*>(object))
        return new CoachMarkAccessible(coachMark);
    if (auto* popup = qobject_cast<Popup*>(object))
        return new PopupAccessible(popup);
    return nullptr;
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

void sendOpenStateEvent(QWidget* surface, bool modal)
{
    if (!surface)
        return;
    QAccessible::State changed;
    changed.active = true;
    changed.invisible = true;
    changed.modal = modal;
    QAccessibleStateChangeEvent event(surface, changed);
    QAccessible::updateAccessibility(&event);
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureTransientSurfaceAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(transientSurfaceAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyPopupAccessibilityOpenChanged(Popup* popup, bool open)
{
#if QT_CONFIG(accessibility)
    if (!popup)
        return;
    sendOpenStateEvent(popup, popup->isModal());
    sendEvent(popup, QAccessible::ActionChanged);
    if (qobject_cast<TeachingTip*>(popup)) {
        sendEvent(popup, open ? QAccessible::ContextHelpStart
                              : QAccessible::ContextHelpEnd);
    }
#else
    Q_UNUSED(popup)
    Q_UNUSED(open)
#endif
}

void notifyPopupAccessibilityModalChanged(Popup* popup)
{
#if QT_CONFIG(accessibility)
    if (!popup || !popup->isOpen())
        return;
    QAccessible::State changed;
    changed.modal = true;
    QAccessibleStateChangeEvent event(popup, changed);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(popup)
#endif
}

void notifyPopupAccessibilityActionsChanged(Popup* popup)
{
#if QT_CONFIG(accessibility)
    if (popup && popup->isOpen())
        sendEvent(popup, QAccessible::ActionChanged);
#else
    Q_UNUSED(popup)
#endif
}

void notifyTransientSurfaceAccessibilityRelationChanged(QWidget* surface)
{
#if QT_CONFIG(accessibility)
    sendEvent(surface, QAccessible::ObjectReorder);
#else
    Q_UNUSED(surface)
#endif
}

void notifyCoachMarkAccessibilityOpenChanged(
    CoachMark* coachMark, bool open)
{
#if QT_CONFIG(accessibility)
    if (!coachMark)
        return;
    sendOpenStateEvent(coachMark, false);
    sendEvent(coachMark, QAccessible::ActionChanged);
    sendEvent(coachMark, open ? QAccessible::ContextHelpStart
                              : QAccessible::ContextHelpEnd);
    if (open)
        sendEvent(coachMark, QAccessible::Alert);
#else
    Q_UNUSED(coachMark)
    Q_UNUSED(open)
#endif
}

} // namespace fluent::dialogs_flyouts::detail
