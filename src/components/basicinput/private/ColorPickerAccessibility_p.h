#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_COLORPICKERACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_COLORPICKERACCESSIBILITY_P_H

class QWidget;

namespace fluent::basicinput {
class ColorPicker;

namespace detail {

void ensureColorPickerAccessibilityFactory();
void notifyColorPickerValueChanged(ColorPicker* picker);
void notifyColorPickerHueChanged(QWidget* hueBar);
void notifyColorPickerSpectrumChanged(QWidget* spectrum);
void notifyColorPickerStructureChanged(ColorPicker* picker);

} // namespace detail
} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_COLORPICKERACCESSIBILITY_P_H
