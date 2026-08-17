#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_MENUBUTTONACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_MENUBUTTONACCESSIBILITY_P_H

namespace fluent::basicinput {

class DropDownButton;
class SplitButton;

namespace detail {

void ensureMenuButtonAccessibilityFactory();

void showMenuButtonMenu(DropDownButton* button);
void showMenuButtonMenu(SplitButton* button);

void notifyMenuButtonMenuAccessibility(
    DropDownButton* button, bool availabilityChanged);
void notifyMenuButtonMenuAccessibility(
    SplitButton* button, bool availabilityChanged);

void notifyMenuButtonOpenAccessibility(DropDownButton* button);
void notifyMenuButtonOpenAccessibility(SplitButton* button);

} // namespace detail
} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_MENUBUTTONACCESSIBILITY_P_H
