#include "PickerAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>
#include <QWidget>

#include "components/date_time/DatePicker.h"
#include "components/date_time/TimePicker.h"
#include "components/foundation/private/ValueAccessibility_p.h"

namespace fluent::date_time::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::ValueAccessibleAdapter;

QString pickerText(const char* source)
{
    return QCoreApplication::translate("PickerAccessibility", source);
}

QString dateValueText(const DatePicker* picker)
{
    if (!picker || !picker->selectedDate().isValid())
        return pickerText("No date selected");

    QStringList fields;
    if (picker->monthVisible())
        fields.append(picker->fieldDisplayText(DatePicker::DateField::Month));
    if (picker->dayVisible())
        fields.append(picker->fieldDisplayText(DatePicker::DateField::Day));
    if (picker->yearVisible())
        fields.append(picker->fieldDisplayText(DatePicker::DateField::Year));
    return fields.join(QStringLiteral(" "));
}

QString timeValueText(const TimePicker* picker)
{
    if (!picker || !picker->selectedTime().isValid())
        return pickerText("No time selected");

    QStringList fields;
    fields.append(picker->fieldDisplayText(TimePicker::TimeField::Hour));
    fields.append(picker->fieldDisplayText(TimePicker::TimeField::Minute));
    if (picker->clockIdentifier()
        == TimePicker::ClockIdentifier::TwelveHourClock) {
        fields.append(picker->fieldDisplayText(TimePicker::TimeField::Period));
    }
    return fields.join(QStringLiteral(" "));
}

class PickerRootAccessible final : public QAccessibleWidget {
public:
    explicit PickerRootAccessible(QWidget* picker)
        : QAccessibleWidget(picker, QAccessible::ButtonMenu)
    {
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        const bool open = isOpen();
        result.hasPopup = true;
        result.expandable = true;
        result.expanded = open;
        result.collapsed = !open;
        return result;
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        if (!inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name) {
            return datePicker() ? pickerText("Date picker")
                                : pickerText("Time picker");
        }
        if (type == QAccessible::Value)
            return valueText();
        return inherited;
    }

    QList<std::pair<QAccessibleInterface*, QAccessible::Relation>>
    relations(QAccessible::Relation match) const override
    {
        auto result = QAccessibleWidget::relations(match);
        if (!(match & QAccessible::Controller) || !widget())
            return result;

        const QString objectName = datePicker()
            ? QStringLiteral("DatePickerFlyout")
            : QStringLiteral("TimePickerFlyout");
        QWidget* flyout = widget()->findChild<QWidget*>(objectName);
        QAccessibleInterface* target = flyout
            ? QAccessible::queryAccessibleInterface(flyout) : nullptr;
        if (target)
            result.append({target, QAccessible::Controller});
        return result;
    }

    QStringList actionNames() const override
    {
        QStringList result = QAccessibleWidget::actionNames();
        if (!widget() || !widget()->isEnabled())
            return result;
        const QString press = QAccessibleActionInterface::pressAction();
        const QString showMenu = QAccessibleActionInterface::showMenuAction();
        if (!result.contains(press))
            result.append(press);
        if (!result.contains(showMenu))
            result.append(showMenu);
        return result;
    }

    void doAction(const QString& actionName) override
    {
        if (widget() && widget()->isEnabled()
            && (actionName == QAccessibleActionInterface::pressAction()
                || actionName
                    == QAccessibleActionInterface::showMenuAction())) {
            if (DatePicker* picker = datePicker())
                picker->openPicker();
            else if (TimePicker* picker = timePicker())
                picker->openPicker();
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::pressAction())
            return {QStringLiteral("Space"), QStringLiteral("Enter")};
        if (actionName == QAccessibleActionInterface::showMenuAction())
            return {QStringLiteral("Alt+Down"), QStringLiteral("F4")};
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

private:
    DatePicker* datePicker() const
    {
        return dynamic_cast<DatePicker*>(widget());
    }

    TimePicker* timePicker() const
    {
        return dynamic_cast<TimePicker*>(widget());
    }

    bool isOpen() const
    {
        if (const DatePicker* picker = datePicker())
            return picker->isDropDownOpen();
        if (const TimePicker* picker = timePicker())
            return picker->isDropDownOpen();
        return false;
    }

    QString valueText() const
    {
        if (const DatePicker* picker = datePicker())
            return dateValueText(picker);
        return timeValueText(timePicker());
    }
};

class PickerColumnAccessible final : public ValueAccessibleAdapter {
public:
    explicit PickerColumnAccessible(PickerColumnAccessibilityHost* host)
        : ValueAccessibleAdapter(host->pickerColumnWidget(),
                                 QAccessible::SpinBox)
        , m_host(host)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = ValueAccessibleAdapter::text(type);
        if (type == QAccessible::Name && inherited.isEmpty() && m_host)
            return m_host->pickerColumnName();
        return inherited;
    }

    QVariant currentValue() const override
    {
        return m_host ? m_host->pickerColumnCurrentValue() : QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        if (m_host && widget() && widget()->isEnabled())
            m_host->pickerColumnSetValue(value);
    }

    QVariant maximumValue() const override
    {
        return m_host ? m_host->pickerColumnMaximumValue() : QVariant();
    }

    QVariant minimumValue() const override
    {
        return m_host ? m_host->pickerColumnMinimumValue() : QVariant();
    }

    QVariant minimumStepSize() const override
    {
        return m_host ? m_host->pickerColumnStepSize() : QVariant();
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return {QStringLiteral("Down")};
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return {QStringLiteral("Up")};
        return ValueAccessibleAdapter::keyBindingsForAction(actionName);
    }

protected:
    bool accessibleValueReadOnly() const override { return false; }

    bool canIncreaseAccessibleValue() const override
    {
        return m_host && widget() && widget()->isEnabled()
            && m_host->pickerColumnCanShift(1);
    }

    bool canDecreaseAccessibleValue() const override
    {
        return m_host && widget() && widget()->isEnabled()
            && m_host->pickerColumnCanShift(-1);
    }

    void changeAccessibleValueByStep(int direction) override
    {
        if (m_host)
            m_host->pickerColumnShift(direction);
    }

    QString accessibleValueText() const override
    {
        return m_host ? m_host->pickerColumnValueText() : QString();
    }

private:
    PickerColumnAccessibilityHost* m_host = nullptr;
};

QAccessibleInterface* pickerAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* datePicker = dynamic_cast<DatePicker*>(object))
        return new PickerRootAccessible(datePicker);
    if (auto* timePicker = dynamic_cast<TimePicker*>(object))
        return new PickerRootAccessible(timePicker);
    if (auto* host = dynamic_cast<PickerColumnAccessibilityHost*>(object))
        return new PickerColumnAccessible(host);
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensurePickerAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(pickerAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyPickerRootValueChanged(QWidget* picker)
{
#if QT_CONFIG(accessibility)
    if (!picker)
        return;
    QString value;
    if (auto* datePicker = dynamic_cast<DatePicker*>(picker))
        value = dateValueText(datePicker);
    else if (auto* timePicker = dynamic_cast<TimePicker*>(picker))
        value = timeValueText(timePicker);
    QAccessibleValueChangeEvent event(picker, value);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(picker)
#endif
}

void notifyPickerRootPopupChanged(QWidget* picker)
{
#if QT_CONFIG(accessibility)
    if (!picker)
        return;
    QAccessible::State changed;
    changed.expanded = true;
    changed.collapsed = true;
    QAccessibleStateChangeEvent stateEvent(picker, changed);
    QAccessible::updateAccessibility(&stateEvent);
    QAccessibleEvent actionEvent(picker, QAccessible::ActionChanged);
    QAccessible::updateAccessibility(&actionEvent);
#else
    Q_UNUSED(picker)
#endif
}

void notifyPickerColumnValueChanged(QWidget* column)
{
#if QT_CONFIG(accessibility)
    auto* host = dynamic_cast<PickerColumnAccessibilityHost*>(column);
    if (!host)
        return;
    QAccessibleValueChangeEvent event(
        column, host->pickerColumnCurrentValue());
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(column)
#endif
}

} // namespace fluent::date_time::detail
