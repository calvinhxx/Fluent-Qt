#include "RepeatButton.h"

namespace fluent::basicinput {

RepeatButton::RepeatButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    // Enable auto-repeat. zh_CN: 启用自动重复功能。
    setAutoRepeat(true);
    // Defaults matching the usual WinUI 3 feel. zh_CN: 设置默认值，匹配 WinUI 3 常见体验。
    setAutoRepeatDelay(500);
    setAutoRepeatInterval(50);
}

RepeatButton::RepeatButton(QWidget* parent)
    : RepeatButton("", parent) {
}

void RepeatButton::setDelay(int d) {
    if (autoRepeatDelay() != d) {
        setAutoRepeatDelay(d);
        emit delayChanged();
    }
}

void RepeatButton::setInterval(int i) {
    if (autoRepeatInterval() != i) {
        setAutoRepeatInterval(i);
        emit intervalChanged();
    }
}

} // namespace fluent::basicinput
