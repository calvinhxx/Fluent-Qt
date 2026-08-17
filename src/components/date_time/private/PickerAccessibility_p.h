#ifndef FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERACCESSIBILITY_P_H

#include <QString>
#include <QVariant>

class QWidget;

namespace fluent::date_time::detail {

/** Private bridge implemented by the custom-painted Date/Time picker columns. */
class PickerColumnAccessibilityHost {
public:
    virtual ~PickerColumnAccessibilityHost() = default;

    virtual QWidget* pickerColumnWidget() = 0;
    virtual QString pickerColumnName() const = 0;
    virtual QString pickerColumnValueText() const = 0;
    virtual QVariant pickerColumnCurrentValue() const = 0;
    virtual QVariant pickerColumnMinimumValue() const = 0;
    virtual QVariant pickerColumnMaximumValue() const = 0;
    virtual QVariant pickerColumnStepSize() const = 0;
    virtual bool pickerColumnCanShift(int direction) const = 0;
    virtual void pickerColumnShift(int direction) = 0;
    virtual void pickerColumnSetValue(const QVariant& value) = 0;
};

void ensurePickerAccessibilityFactory();
void notifyPickerRootValueChanged(QWidget* picker);
void notifyPickerRootPopupChanged(QWidget* picker);
void notifyPickerColumnValueChanged(QWidget* column);

} // namespace fluent::date_time::detail

#endif // FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERACCESSIBILITY_P_H
