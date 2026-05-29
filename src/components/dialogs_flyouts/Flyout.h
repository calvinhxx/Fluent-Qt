#ifndef FLYOUT_H
#define FLYOUT_H

#include <QPointer>
#include "components/dialogs_flyouts/Popup.h"

namespace fluent::dialogs_flyouts {

/**
 * @brief Popup anchored to a target with placement and window clamping rules.
 * zh_CN: 按目标锚点定位并支持窗口边界约束的 Popup 派生浮层。
 *
 * Flyout extends Popup with placement, anchor offset, and clamp-to-window behavior
 * for controls such as ComboBox, teaching tips, and contextual command surfaces.
 * zh_CN: Flyout 在 Popup 基础上增加位置、锚点偏移和窗口内约束，适合 ComboBox、
 * TeachingTip 和上下文命令表面。
 */
class Flyout : public Popup {
    Q_OBJECT
    /**
     * @brief Preferred flyout placement relative to its anchor.
     * zh_CN: 浮层相对锚点的首选位置。
     */
    Q_PROPERTY(Placement placement READ placement WRITE setPlacement NOTIFY placementChanged)
    /**
     * @brief Offset from the anchor edge in pixels.
     * zh_CN: 相对锚点边缘的像素偏移。
     */
    Q_PROPERTY(int anchorOffset READ anchorOffset WRITE setAnchorOffset)
    /**
     * @brief Whether flyout geometry is clamped to the host window.
     * zh_CN: 浮层几何是否限制在宿主窗口内。
     */
    Q_PROPERTY(bool clampToWindow READ clampToWindow WRITE setClampToWindow)

public:
    enum Placement {
        Top,        ///< 在 anchor 上方
        Bottom,     ///< 在 anchor 下方
        Left,       ///< 在 anchor 左侧
        Right,      ///< 在 anchor 右侧
        Full,       ///< 顶层窗口居中（与基类默认一致）
        Auto,       ///< 优先 Bottom，空间不足时自动反转到 Top
    };
    Q_ENUM(Placement)

    explicit Flyout(QWidget* parent = nullptr);
    ~Flyout() override;

    Placement placement() const          { return m_placement; }
    void      setPlacement(Placement p);

    int       anchorOffset() const       { return m_anchorOffset; }
    void      setAnchorOffset(int px)    { m_anchorOffset = px; }

    bool      clampToWindow() const      { return m_clampToWindow; }
    void      setClampToWindow(bool e)   { m_clampToWindow = e; }

    QWidget*  anchor() const             { return m_anchor.data(); }

    /// 在指定 anchor 控件上弹出 flyout。等价于 setAnchor(anchor) + open()。
    void showAt(QWidget* anchor);

    /// 仅设置 anchor，不立即弹出。
    void setAnchor(QWidget* anchor);

signals:
    void placementChanged(Placement p);

protected:
    /// 重写：根据 anchor 几何 + Placement + 自身尺寸返回 move() 目标坐标
    /// （含 shadow margin 偏移补偿）
    QPoint computePosition() const override;

private:
    /// Auto 模式下根据可用空间决定实际 placement
    Placement resolveAutoPlacement() const;

    /// 把 anchor 的几何映射到 top-level 坐标系
    QRect anchorRectInTopLevel() const;

    /// 在 top-level 矩形内 clamp 卡片位置（基于「卡片左上角」）
    QPoint clampCardPos(const QPoint& cardTopLeft) const;

    QPointer<QWidget> m_anchor;
    Placement m_placement = Bottom;
    int       m_anchorOffset = 8;     // WinUI 标准间距
    bool      m_clampToWindow = true;
};

} // namespace fluent::dialogs_flyouts

#endif // FLYOUT_H
