#include "ValueAccessibility_p.h"

#include <QWidget>

namespace fluent::accessibility::detail {

#if QT_CONFIG(accessibility)

ValueAccessibleAdapter::ValueAccessibleAdapter(
    QWidget* widget, QAccessible::Role role)
    : QAccessibleWidget(widget, role)
{
}

void* ValueAccessibleAdapter::interface_cast(
    QAccessible::InterfaceType type)
{
    if (type == QAccessible::ValueInterface)
        return static_cast<QAccessibleValueInterface*>(this);
    if (type == QAccessible::ActionInterface)
        return static_cast<QAccessibleActionInterface*>(this);
    return QAccessibleWidget::interface_cast(type);
}

QAccessible::State ValueAccessibleAdapter::state() const
{
    QAccessible::State result = QAccessibleWidget::state();
    const bool readOnly = accessibleValueReadOnly();
    result.readOnly = readOnly;
    result.editable = accessibleValueEditable() && !readOnly
        && !result.disabled;
    result.busy = result.busy || accessibleValueBusy();
    result.animated = result.animated || accessibleValueAnimated();
    result.invalid = result.invalid || accessibleValueInvalid();
    return result;
}

QString ValueAccessibleAdapter::text(QAccessible::Text type) const
{
    if (type == QAccessible::Value)
        return accessibleValueText();
    return QAccessibleWidget::text(type);
}

QStringList ValueAccessibleAdapter::actionNames() const
{
    QStringList result = QAccessibleWidget::actionNames();
    if (canIncreaseAccessibleValue()) {
        const QString action = QAccessibleActionInterface::increaseAction();
        if (!result.contains(action))
            result.append(action);
    }
    if (canDecreaseAccessibleValue()) {
        const QString action = QAccessibleActionInterface::decreaseAction();
        if (!result.contains(action))
            result.append(action);
    }
    return result;
}

void ValueAccessibleAdapter::doAction(const QString& actionName)
{
    if (actionName == QAccessibleActionInterface::increaseAction()
        && canIncreaseAccessibleValue()) {
        changeAccessibleValueByStep(1);
        return;
    }
    if (actionName == QAccessibleActionInterface::decreaseAction()
        && canDecreaseAccessibleValue()) {
        changeAccessibleValueByStep(-1);
        return;
    }
    QAccessibleWidget::doAction(actionName);
}

QStringList ValueAccessibleAdapter::keyBindingsForAction(
    const QString& actionName) const
{
    if (actionName == QAccessibleActionInterface::increaseAction())
        return {QStringLiteral("Up")};
    if (actionName == QAccessibleActionInterface::decreaseAction())
        return {QStringLiteral("Down")};
    return QAccessibleWidget::keyBindingsForAction(actionName);
}

QString ValueAccessibleAdapter::accessibleValueText() const
{
    return currentValue().toString();
}

void notifyValueAccessibilityValue(
    QWidget* widget, const QVariant& value)
{
    if (!widget)
        return;
    QAccessibleValueChangeEvent event(widget, value);
    QAccessible::updateAccessibility(&event);
}

void notifyValueAccessibilityState(
    QWidget* widget, const QAccessible::State& changed)
{
    if (!widget)
        return;
    QAccessibleStateChangeEvent event(widget, changed);
    QAccessible::updateAccessibility(&event);
}

void notifyValueAccessibilityText(QWidget* widget, QAccessible::Event type)
{
    if (!widget)
        return;
    QAccessibleEvent event(widget, type);
    QAccessible::updateAccessibility(&event);
}

#else

void notifyValueAccessibilityValue(QWidget*, const QVariant&) {}
void notifyValueAccessibilityState(QWidget*, const QAccessible::State&) {}
void notifyValueAccessibilityText(QWidget*, QAccessible::Event) {}

#endif // QT_CONFIG(accessibility)

} // namespace fluent::accessibility::detail
