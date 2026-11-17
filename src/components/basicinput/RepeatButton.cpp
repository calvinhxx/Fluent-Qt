#include "RepeatButton.h"

namespace fluent::basicinput {

RepeatButton::RepeatButton(const QString& text, QWidget* parent) : Button(text, parent)
{
    setAutoRepeat(true);
    // Default repeat timing matching WinUI 3.
    setAutoRepeatDelay(500);
    setAutoRepeatInterval(50);
}

RepeatButton::RepeatButton(QWidget* parent) : RepeatButton("", parent) {}

void RepeatButton::setDelay(int d)
{
    if (autoRepeatDelay() != d) {
        setAutoRepeatDelay(d);
        emit delayChanged();
    }
}

void RepeatButton::setInterval(int i)
{
    if (autoRepeatInterval() != i) {
        setAutoRepeatInterval(i);
        emit intervalChanged();
    }
}

} // namespace fluent::basicinput
