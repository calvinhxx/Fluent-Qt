#include "ToggleSplitButton.h"
#include <QMouseEvent>

namespace fluent::basicinput {

ToggleSplitButton::ToggleSplitButton(const QString& text, QWidget* parent)
    : SplitButton(text, parent) {
    setCheckable(true);
}

void ToggleSplitButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        SplitPart releasePart = getPartAt(event->pos());
        
        if (releasePart == Secondary && m_pressPart == Secondary) {
            // Drop-down zone clicked: open the menu without toggling. zh_CN: 点击下拉区——弹出菜单，但不触发切换。
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
    
    // Primary-zone clicks fall through to SplitButton (and QPushButton) for click/toggle handling.
// zh_CN: 点击左侧或其它情况，交由 SplitButton（进而 QPushButton）处理点击/切换。
    SplitButton::mouseReleaseEvent(event);
}

} // namespace fluent::basicinput
