#include "DrawerViewAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>

#include "components/collections/DrawerView.h"

namespace fluent::collections::detail {

#if QT_CONFIG(accessibility)

namespace {

const QString& dismissAction()
{
    static const QString action = QStringLiteral("dismiss");
    return action;
}

QString drawerText(const char* source)
{
    return QCoreApplication::translate(
        "DrawerViewAccessibility", source);
}

bool canDismiss(const DrawerView* drawer)
{
    return drawer && drawer->isOpen() && drawer->isEnabled()
        && drawer->closePolicy().testFlag(DrawerView::CloseOnEscape);
}

class DrawerViewAccessible final : public QAccessibleWidget {
public:
    explicit DrawerViewAccessible(DrawerView* drawer)
        : QAccessibleWidget(drawer, QAccessible::Pane)
    {
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        const DrawerView* surface = drawer();
        if (!surface)
            return result;

        const bool open = surface->isOpen();
        result.active = open;
        result.invisible = !open;
        result.offscreen = !open || result.offscreen;
        result.modal = open && surface->isModal();
        result.expandable = true;
        result.expanded = open;
        result.collapsed = !open;
        result.focusable = open && surface->isEnabled()
            && surface->focusPolicy() != Qt::NoFocus;
        result.focused = open && surface->hasFocus();
        return result;
    }

    QStringList actionNames() const override
    {
        return canDismiss(drawer())
            ? QStringList{dismissAction()}
            : QStringList{};
    }

    QString localizedActionName(const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? drawerText("Dismiss")
            : QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        return actionName == dismissAction()
            ? drawerText("Closes the drawer")
            : QAccessibleWidget::localizedActionDescription(actionName);
    }

    void doAction(const QString& actionName) override
    {
        DrawerView* surface = drawer();
        if (actionName == dismissAction() && canDismiss(surface)) {
            surface->close();
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == dismissAction() && canDismiss(drawer()))
            return {QStringLiteral("Escape")};
        return {};
    }

private:
    DrawerView* drawer() const
    {
        return static_cast<DrawerView*>(widget());
    }
};

QAccessibleInterface* drawerViewAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* drawer = qobject_cast<DrawerView*>(object);
    return drawer ? new DrawerViewAccessible(drawer) : nullptr;
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureDrawerViewAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(drawerViewAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyDrawerViewAccessibilityOpenChanged(DrawerView* drawer)
{
#if QT_CONFIG(accessibility)
    if (!drawer)
        return;
    QAccessible::State changed;
    changed.active = true;
    changed.invisible = true;
    changed.modal = true;
    changed.expanded = true;
    changed.collapsed = true;
    QAccessibleStateChangeEvent stateEvent(drawer, changed);
    QAccessible::updateAccessibility(&stateEvent);
    sendEvent(drawer, QAccessible::ActionChanged);
#else
    Q_UNUSED(drawer)
#endif
}

void notifyDrawerViewAccessibilityModalChanged(DrawerView* drawer)
{
#if QT_CONFIG(accessibility)
    if (!drawer || !drawer->isOpen())
        return;
    QAccessible::State changed;
    changed.modal = true;
    QAccessibleStateChangeEvent event(drawer, changed);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(drawer)
#endif
}

void notifyDrawerViewAccessibilityActionsChanged(DrawerView* drawer)
{
#if QT_CONFIG(accessibility)
    if (drawer && drawer->isOpen())
        sendEvent(drawer, QAccessible::ActionChanged);
#else
    Q_UNUSED(drawer)
#endif
}

void notifyDrawerViewAccessibilityContentChanged(DrawerView* drawer)
{
#if QT_CONFIG(accessibility)
    sendEvent(drawer, QAccessible::ObjectReorder);
#else
    Q_UNUSED(drawer)
#endif
}

} // namespace fluent::collections::detail
