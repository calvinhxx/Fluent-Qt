#ifndef FLYOUT_H
#define FLYOUT_H

#include <QPointer>
#include "Popup.h"

namespace fluent::dialogs_flyouts {

/**
 * @brief Popup anchored to a target widget with placement options and window clamping.
 * zh_CN: 锚定到目标控件的浮层，支持方位策略与窗口边界约束。
 *
 * Extends Popup with anchor offset and edge clamping for ComboBox dropdowns,
 * teaching tips, and contextual command bars.
 * zh_CN: 在 Popup 基础上增加锚点偏移和窗口内约束，适用于 ComboBox 下拉、
 * TeachingTip 与上下文命令栏。
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
    Q_PROPERTY(int anchorOffset READ anchorOffset WRITE setAnchorOffset NOTIFY anchorOffsetChanged)
    /**
     * @brief Whether flyout geometry is clamped to the host window.
     * zh_CN: 浮层几何是否限制在宿主窗口内。
     */
    Q_PROPERTY(bool clampToWindow READ clampToWindow WRITE setClampToWindow NOTIFY clampToWindowChanged)

public:
    enum Placement {
        Top,    ///< Above the anchor. zh_CN: 在 anchor 上方。
        Bottom, ///< Below the anchor. zh_CN: 在 anchor 下方。
        Left,   ///< Left of the anchor. zh_CN: 在 anchor 左侧。
        Right,  ///< Right of the anchor. zh_CN: 在 anchor 右侧。
        Full,   ///< Centered in the top-level window (base-class default). zh_CN: 顶层窗口居中。
        Auto,   ///< Prefer Bottom, flipping to Top when space runs out. zh_CN: 优先 Bottom，空间不足时反转到 Top。
    };
    Q_ENUM(Placement)

    explicit Flyout(QWidget* parent = nullptr);
    ~Flyout() override;

    Placement placement() const { return m_placement; }
    void setPlacement(Placement p);

    int anchorOffset() const { return m_anchorOffset; }
    void setAnchorOffset(int px);

    bool clampToWindow() const { return m_clampToWindow; }
    void setClampToWindow(bool e);

    QWidget* anchor() const { return m_anchor.data(); }

    /** @brief Opens the flyout anchored to anchor (equivalent to setAnchor(anchor) + open()).
     * zh_CN: 在指定锚点控件旁弹出浮层，等价于 setAnchor(anchor) + open()。 */
    void showAt(QWidget* anchor);

    /** @brief Sets the anchor widget without opening the flyout.
     * zh_CN: 仅设置锚点控件，不立即弹出。 */
    void setAnchor(QWidget* anchor);

signals:
    void placementChanged(Placement p);
    void anchorOffsetChanged(int px);
    void clampToWindowChanged(bool enabled);

protected:
    /** @brief Computes the move() position from anchor geometry, placement, and shadow margin.
     * zh_CN: 按锚点几何、Placement 及阴影边距计算窗口内 move() 目标坐标。 */
    QPoint computePosition() const override;
    QWidget* automaticPositionAnchor() const override { return m_anchor.data(); }

private:
    Placement resolveAutoPlacement() const;
    QRect anchorRectInTopLevel() const;
    QPoint clampCardPos(const QPoint& cardTopLeft) const;

    QPointer<QWidget> m_anchor;
    Placement m_placement = Bottom;
    int m_anchorOffset = 8; // Standard WinUI gap. zh_CN: WinUI 标准间距。
    bool m_clampToWindow = true;
};

} // namespace fluent::dialogs_flyouts

#endif // FLYOUT_H
