#ifndef COACHMARK_H
#define COACHMARK_H

#include <QPointer>
#include <QSize>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPaintEvent;
class QEvent;
class QPropertyAnimation;
class QResizeEvent;

namespace fluent::dialogs_flyouts {

/**
 * @brief A teaching tip rendered in its OWN top-level window, with a tail pointing at a target.
 * zh_CN: 渲染在自己「独立顶层窗口」里的操作引导提示,带指向目标的尾巴。
 *
 * The same-window TeachingTip (a Popup) cannot reliably sit above a dimmed translucent Mica window:
 * a semi-transparent dim scrim and a same-window card composite unreliably (flicker) when they are
 * siblings. CoachMark sidesteps that by living in a separate top-level window — the same approach that
 * keeps Dialog solid over its dim. It hosts caller content via contentHost(), paints a Fluent card +
 * shadow + a tail aimed at the target, fades via window opacity, and glides between targets when
 * setTarget() changes while open.
 * zh_CN: 同窗口的 TeachingTip(Popup)无法稳定浮在压暗的半透明 Mica 窗口之上:半透明遮罩与同窗口卡片作为同级时合成不稳定
 *(闪烁)。CoachMark 通过独立顶层窗口绕开这点——与 Dialog 稳压其压暗同理。它用 contentHost() 承载内容,绘制 Fluent
 * 卡片+阴影+指向目标的尾巴,用窗口透明度淡入淡出,并在打开状态下 setTarget() 变化时在目标间滑动。
 */
class CoachMark : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT
    /**
     * @brief Visible card size, excluding shadow and tail. zh_CN: 可见卡片尺寸,不含阴影和尾巴。
     */
    Q_PROPERTY(QSize cardSize READ cardSize WRITE setCardSize)
    /**
     * @brief Target widget the tail points at; null centers the card with no tail.
     * zh_CN: 尾巴指向的目标控件;为空则居中且无尾巴。
     */
    Q_PROPERTY(QWidget* target READ target WRITE setTarget)
    /**
     * @brief Preferred placement of the card around the target. zh_CN: 卡片相对目标的首选位置。
     */
    Q_PROPERTY(Placement placement READ placement WRITE setPlacement)
    /**
     * @brief Whether the coach mark is open (shown). zh_CN: 操作引导是否处于打开(显示)状态。
     */
    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)

public:
    enum Placement {
        Auto,    ///< Resolves to Bottom. zh_CN: 解析为 Bottom。
        Top,     ///< Card above the target, tail down.
        Bottom,  ///< Card below the target, tail up.
        Left,    ///< Card left of the target, tail right.
        Right,   ///< Card right of the target, tail left.
    };
    Q_ENUM(Placement)

    explicit CoachMark(QWidget* owner = nullptr);
    ~CoachMark() override;

    /**
     * @brief Container for caller content (add an AnchorLayout / QVBoxLayout etc.).
     * zh_CN: 调用方内容容器(向其添加 AnchorLayout / QVBoxLayout 等)。
     */
    QWidget* contentHost() const { return m_contentHost; }

    QSize cardSize() const { return m_cardSize; }
    void setCardSize(const QSize& size);

    QWidget* target() const { return m_target.data(); }
    void setTarget(QWidget* target);

    Placement placement() const { return m_placement; }
    void setPlacement(Placement placement);

    bool isOpen() const { return m_open; }

    void onThemeUpdated() override;

public slots:
    /// Positions the card, shows it, and fades it in. zh_CN: 定位卡片、显示并淡入。
    void open();
    /// Fades the card out and hides it. zh_CN: 淡出并隐藏卡片。
    void close();
    void setOpen(bool open);

signals:
    void openChanged(bool open);
    void opened();
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void queueTargetSync();
    void syncToTarget();
    bool syncThemeOverrideFromSource();
    void reposition(bool animated);  // place the window from target + placement
    QRect cardRect() const;          // card rect inside the window (inset for shadow + tail)

    QWidget* m_owner = nullptr;
    QWidget* m_contentHost = nullptr;
    QSize m_cardSize{330, 168};
    QPointer<QWidget> m_target;
    Placement m_placement = Auto;
    bool m_open = false;
    bool m_targetSyncPending = false;
    bool m_tailVisible = false;
    int m_tailEdge = 0;    // 0=none,1=top,2=bottom,3=left,4=right
    int m_tailCenter = 0;  // tail center along that edge (window-local)
    QPropertyAnimation* m_fadeAnim = nullptr;
    QPropertyAnimation* m_moveAnim = nullptr;
};

} // namespace fluent::dialogs_flyouts

#endif // COACHMARK_H
