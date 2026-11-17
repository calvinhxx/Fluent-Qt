#ifndef TOGGLESPLITBUTTON_H
#define TOGGLESPLITBUTTON_H

#include "SplitButton.h"

namespace fluent::basicinput {

/**
 * @brief SplitButton whose primary region toggles checked state.
 * zh_CN: 主操作区可切换选中状态的拆分按钮。
 */
class ToggleSplitButton : public SplitButton {
    Q_OBJECT
public:
    explicit ToggleSplitButton(const QString& text = "", QWidget* parent = nullptr);
};

} // namespace fluent::basicinput

#endif // TOGGLESPLITBUTTON_H
