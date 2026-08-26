#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_MULTISELECTCOMBOBOXACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_MULTISELECTCOMBOBOXACCESSIBILITY_P_H

namespace fluent::basicinput {
class MultiSelectComboBox;

namespace detail {

void ensureMultiSelectComboBoxAccessibilityFactory();
void notifyMultiSelectComboBoxOpenChanged(MultiSelectComboBox *box);
void notifyMultiSelectComboBoxSelectionChanged(MultiSelectComboBox *box,
                                               bool countChanged);

} // namespace detail
} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_MULTISELECTCOMBOBOXACCESSIBILITY_P_H
