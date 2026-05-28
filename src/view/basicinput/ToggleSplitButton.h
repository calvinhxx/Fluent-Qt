#ifndef TOGGLESPLITBUTTON_H
#define TOGGLESPLITBUTTON_H

#include "SplitButton.h"

namespace view::basicinput {

/**
 * @brief Split button whose primary region toggles checked state.
 * zh_CN: 主操作区可切换选中态的拆分按钮。
 *
 * ToggleSplitButton combines toggle-button selection semantics with the same
 * secondary menu region used by SplitButton.
 * zh_CN: ToggleSplitButton 将 ToggleButton 的选中语义与 SplitButton 的二级菜单区域组合起来。
 */
class ToggleSplitButton : public SplitButton {
    Q_OBJECT
public:
    explicit ToggleSplitButton(const QString& text = "", QWidget* parent = nullptr);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
};

} // namespace view::basicinput

#endif // TOGGLESPLITBUTTON_H
