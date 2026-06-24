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
        Top,        ///< Above the anchor. zh_CN: 在 anchor 上方。
        Bottom,     ///< Below the anchor. zh_CN: 在 anchor 下方。
        Left,       ///< Left of the anchor. zh_CN: 在 anchor 左侧。
        Right,      ///< Right of the anchor. zh_CN: 在 anchor 右侧。
        Full,       ///< Centered in the top-level window (base-class default). zh_CN: 顶层窗口居中。
        Auto,       ///< Prefer Bottom, flipping to Top when space runs out. zh_CN: 优先 Bottom，空间不足时反转到 Top。
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

    /// Opens the flyout at the anchor; equals setAnchor(anchor) + open().
    /// zh_CN: 在指定 anchor 控件上弹出 flyout，等价于 setAnchor(anchor) + open()。
    void showAt(QWidget* anchor);

    /// Sets the anchor without opening. zh_CN: 仅设置 anchor，不立即弹出。
    void setAnchor(QWidget* anchor);

signals:
    void placementChanged(Placement p);

protected:
    /// Override: computes the move() target from the anchor geometry, the
    /// placement, and the flyout size (including shadow-margin compensation).
    /// zh_CN: 重写——按 anchor 几何 + Placement + 自身尺寸返回 move() 目标坐标
    /// （含 shadow margin 偏移补偿）。
    QPoint computePosition() const override;
    QWidget* automaticPositionAnchor() const override { return m_anchor.data(); }

private:
    /// Resolves the effective placement from available space in Auto mode.
    /// zh_CN: Auto 模式下根据可用空间决定实际 placement。
    Placement resolveAutoPlacement() const;

    /// Maps the anchor geometry into top-level coordinates. zh_CN: 把 anchor 几何映射到 top-level 坐标系。
    QRect anchorRectInTopLevel() const;

    /// Clamps the card position (by its top-left) inside the top-level rect.
    /// zh_CN: 在 top-level 矩形内 clamp 卡片位置（基于卡片左上角）。
    QPoint clampCardPos(const QPoint& cardTopLeft) const;

    QPointer<QWidget> m_anchor;
    Placement m_placement = Bottom;
    int       m_anchorOffset = 8;     // Standard WinUI gap. zh_CN: WinUI 标准间距。
    bool      m_clampToWindow = true;
};

} // namespace fluent::dialogs_flyouts

#endif // FLYOUT_H
