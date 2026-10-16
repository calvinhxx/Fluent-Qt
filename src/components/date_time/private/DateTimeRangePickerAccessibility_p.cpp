#include "DateTimeRangePickerAccessibility_p.h"
#include "DateTimeRangePickerText_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QPointer>

#include "components/date_time/DateTimeRangePicker.h"

namespace fluent::date_time::detail {
#if QT_CONFIG(accessibility)
namespace {

class DateTimeRangeAccessible final : public QAccessibleWidget {
public:
    explicit DateTimeRangeAccessible(DateTimeRangePicker* picker) : QAccessibleWidget(picker, QAccessible::ComboBox) {}

    QAccessible::State state() const override
    {
        auto result = QAccessibleWidget::state();
        result.hasPopup = true;
        result.expandable = true;
        result.expanded = picker()->isPickerOpen();
        result.collapsed = !result.expanded;
        return result;
    }

    QStringList actionNames() const override
    {
        auto actions = QAccessibleWidget::actionNames();
        if (picker()->isEnabled()) {
            actions.append(QAccessibleActionInterface::pressAction());
            actions.append(QAccessibleActionInterface::showMenuAction());
        }
        return actions;
    }

    void doAction(const QString& action) override
    {
        if (action == QAccessibleActionInterface::pressAction()
            || action == QAccessibleActionInterface::showMenuAction()) {
            picker()->openPicker();
            return;
        }
        QAccessibleWidget::doAction(action);
    }

    QString text(QAccessible::Text type) const override
    {
        if (type == QAccessible::Description) {
            const QString authored = QAccessibleWidget::text(type);
            return authored.isEmpty() && !picker()->isRangeValid() ? picker()->invalidRangeText() : authored;
        }
        if (type != QAccessible::Value)
            return QAccessibleWidget::text(type);
        const auto valueText = [this](const QDateTime& value) {
            if (!value.isValid())
                return dateTimeRangeText(picker()->locale(),
                    QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Unbounded"));
            return picker()->isTimeEnabled()
                ? picker()->locale().toString(value, QLocale::ShortFormat)
                : picker()->locale().toString(value.date(), QLocale::ShortFormat);
        };
        return dateTimeRangeText(picker()->locale(),
            QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "%1: %2; %3: %4"))
            .arg(picker()->startLabelText().isEmpty()
                    ? dateTimeRangeText(picker()->locale(),
                          QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Start"))
                    : picker()->startLabelText(),
                valueText(picker()->startDateTime()),
                picker()->endLabelText().isEmpty()
                    ? dateTimeRangeText(picker()->locale(),
                          QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "End"))
                    : picker()->endLabelText(),
                valueText(picker()->endDateTime()));
    }

private:
    DateTimeRangePicker* picker() const { return static_cast<DateTimeRangePicker*>(widget()); }
};

QAccessibleInterface* rangeFactory(const QString&, QObject* object)
{
    if (auto* picker = qobject_cast<DateTimeRangePicker*>(object))
        return new DateTimeRangeAccessible(picker);
    return nullptr;
}

} // namespace

void ensureDateTimeRangePickerAccessibilityFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(rangeFactory);
        return true;
    }();
    Q_UNUSED(installed);
}

void notifyDateTimeRangePickerDescriptionChanged(DateTimeRangePicker* picker)
{
    if (!picker->accessibleDescription().isEmpty())
        return;
    QAccessibleEvent event(picker, QAccessible::DescriptionChanged);
    QAccessible::updateAccessibility(&event);
}

void notifyDateTimeRangePickerOpenChanged(DateTimeRangePicker* picker)
{
    QAccessible::State changed;
    changed.expanded = true;
    changed.collapsed = true;
    QAccessibleStateChangeEvent event(picker, changed);
    QAccessible::updateAccessibility(&event);
}

void notifyDateTimeRangePickerValueChanged(DateTimeRangePicker* picker, bool validityChanged)
{
    auto* interface = QAccessible::queryAccessibleInterface(picker);
    if (!interface)
        return;
    QAccessibleValueChangeEvent value(picker, interface->text(QAccessible::Value));
    const QPointer<DateTimeRangePicker> guard(picker);
    QAccessible::updateAccessibility(&value);
    if (guard && validityChanged)
        notifyDateTimeRangePickerDescriptionChanged(picker);
}
#else
void ensureDateTimeRangePickerAccessibilityFactory() {}
void notifyDateTimeRangePickerValueChanged(DateTimeRangePicker*, bool) {}
void notifyDateTimeRangePickerDescriptionChanged(DateTimeRangePicker*) {}
void notifyDateTimeRangePickerOpenChanged(DateTimeRangePicker*) {}
#endif
} // namespace fluent::date_time::detail
