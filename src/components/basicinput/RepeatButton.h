#ifndef REPEATBUTTON_H
#define REPEATBUTTON_H

#include "Button.h"

namespace fluent::basicinput {

/**
 * @brief Button that emits repeated click signals while pressed.
 * zh_CN: 按住时持续触发点击信号的按钮。
 */
class RepeatButton : public Button {
    Q_OBJECT
    /**
     * @brief Initial delay before repeat clicks start.
     * zh_CN: 重复点击开始前的初始延迟。
     */
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)
    /**
     * @brief Interval between repeated click emissions.
     * zh_CN: 重复点击信号之间的时间间隔。
     */
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)

public:
    explicit RepeatButton(const QString& text = "", QWidget* parent = nullptr);
    explicit RepeatButton(QWidget* parent = nullptr);

    int delay() const { return autoRepeatDelay(); }
    void setDelay(int d);

    int interval() const { return autoRepeatInterval(); }
    void setInterval(int i);

signals:
    void delayChanged();
    void intervalChanged();
};

} // namespace fluent::basicinput

#endif // REPEATBUTTON_H
