#include "ToggleSplitButton.h"
namespace fluent::basicinput {

ToggleSplitButton::ToggleSplitButton(const QString& text, QWidget* parent)
    : SplitButton(text, parent) {
    setCheckable(true);
}

} // namespace fluent::basicinput
