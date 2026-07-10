#ifndef COACHMARK_H
#define COACHMARK_H

#include <QPointer>
#include <QSize>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPaintEvent;
class QEvent;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QResizeEvent;

namespace fluent::dialogs_flyouts {

/**
 * @brief A teaching tip surface with a tail pointing at a target.
 * zh_CN: 带指向目标尾巴的操作引导提示面。
 *
 * CoachMark hosts caller content via contentHost(), paints a Fluent card + shadow + a tail aimed at
 * the target, fades in/out, and glides between targets when setTarget() changes while open. The default
 * top-level surface keeps component samples independent from the owner widget; the same-window surface
 * is available for startup tours that must scale and move exactly with the application window.
 * zh_CN: CoachMark 通过 contentHost() 承载调用方内容，绘制 Fluent 卡片 + 阴影 + 指向目标的尾巴，支持淡入淡出，
 * 并在打开状态下 setTarget() 变化时在目标间滑动。默认顶层 surface 让组件示例独立于宿主控件；
 * same-window surface 用于必须与应用窗口缩放和移动完全同步的启动引导。
 */
class CoachMark : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT
    /**
     * @brief Internal fade progress used by both top-level and same-window surfaces.
     * zh_CN: 顶层与同窗口 surface 共享的内部淡入淡出进度。
     */
    Q_PROPERTY(double fadeOpacity READ fadeOpacity WRITE setFadeOpacity)
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
    /**
     * @brief Chooses whether the coach mark is hosted as a separate tool window or inside the owner.
     * zh_CN: 选择操作引导提示承载为独立工具窗口，还是宿主窗口内的子控件。
     */
    enum SurfaceMode {
        TopLevelSurface,   ///< Separate tool window hosted above the owner. zh_CN: 宿主上方的独立工具窗口。
        SameWindowSurface  ///< Child widget inside the owner top-level. zh_CN: 宿主顶层窗口内的子控件。
    };
    Q_ENUM(SurfaceMode)

    enum Placement {
        Auto,    ///< Resolves to Bottom. zh_CN: 解析为 Bottom。
        Top,     ///< Card above the target, tail down.
        Bottom,  ///< Card below the target, tail up.
        Left,    ///< Card left of the target, tail right.
        Right,   ///< Card right of the target, tail left.
    };
    Q_ENUM(Placement)

    /**
     * @brief Creates a coach mark owned by the given widget.
     * zh_CN: 创建由指定控件拥有的操作引导提示。
     */
    explicit CoachMark(QWidget* owner = nullptr,
                       SurfaceMode surfaceMode = TopLevelSurface);
    ~CoachMark() override;

    /**
     * @brief Container for caller content (add an AnchorLayout / QVBoxLayout etc.).
     * zh_CN: 调用方内容容器(向其添加 AnchorLayout / QVBoxLayout 等)。
     */
    QWidget* contentHost() const { return m_contentHost; }

    /**
     * @brief Returns the configured hosting surface mode.
     * zh_CN: 返回当前配置的承载 surface 模式。
     */
    SurfaceMode surfaceMode() const { return m_surfaceMode; }

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
    bool usesSameWindowSurfaceBackend() const;
    void attachToOwnerTopLevel();
    void syncNativeTransientParent();
    double fadeOpacity() const;
    void setFadeOpacity(double opacity);
    void reposition(bool animated);  // place the window from target + placement
    QRect cardRect() const;          // card rect inside the window (inset for shadow + tail)

    QWidget* m_owner = nullptr;
    SurfaceMode m_surfaceMode = TopLevelSurface;
    QWidget* m_contentHost = nullptr;
    QSize m_cardSize{330, 168};
    QPointer<QWidget> m_target;
    Placement m_placement = Auto;
    bool m_open = false;
    bool m_targetSyncPending = false;
    bool m_tailVisible = false;
    int m_tailEdge = 0;    // 0=none,1=top,2=bottom,3=left,4=right
    int m_tailCenter = 0;  // tail center along that edge (window-local)
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_fadeAnim = nullptr;
    QPropertyAnimation* m_moveAnim = nullptr;
};

} // namespace fluent::dialogs_flyouts

#endif // COACHMARK_H
