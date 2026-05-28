#include "RepeatButton.h"

namespace view::basicinput {

RepeatButton::RepeatButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    // 启用自动重复功能
    setAutoRepeat(true);
    // 设置默认值，匹配 WinUI 3 常见体验
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

} // namespace view::basicinput
