#include "ToggleSplitButton.h"
#include <QMouseEvent>

namespace view::basicinput {

ToggleSplitButton::ToggleSplitButton(const QString& text, QWidget* parent)
    : SplitButton(text, parent) {
    setCheckable(true);
}

void ToggleSplitButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        SplitPart releasePart = getPartAt(event->pos());
        
        if (releasePart == Secondary && m_pressPart == Secondary) {
            // 点击的是下拉区：弹出菜单，但不触发切换状态
            if (menu()) {
                QPoint popupPos = mapToGlobal(rect().bottomLeft());
                menu()->exec(popupPos);
            }
            m_pressPart = None;
            update();
            event->accept();
            return; // 拦截，不执行基类逻辑（防止切换状态）
        }
    }
    
    // 如果是点击左侧或其它情况，执行 SplitButton (进而执行 QPushButton) 逻辑处理点击/切换
    SplitButton::mouseReleaseEvent(event);
}

} // namespace view::basicinput
