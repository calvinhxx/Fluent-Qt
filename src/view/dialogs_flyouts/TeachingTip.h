#ifndef TEACHINGTIP_H
#define TEACHINGTIP_H

#include <QMargins>
#include <QPointer>
#include <QSize>

#include "view/dialogs_flyouts/Popup.h"

class QKeyEvent;
class QPaintEvent;
class QResizeEvent;
class QWidget;

namespace view::dialogs_flyouts {

/**
 * @brief Contextual teaching flyout with target placement and optional tail.
 * zh_CN: 支持目标定位和可选尾巴的上下文教学浮层。
 *
 * TeachingTip adds preferred placement, placement margin, light-dismiss control,
 * and tail visibility on top of Popup/Flyout overlay behavior.
 * zh_CN: TeachingTip 在 Popup/Flyout 浮层行为上增加首选位置、边距、
 * light-dismiss 控制和尾巴可见性。
 */
class TeachingTip : public Popup {
    Q_OBJECT
    /**
     * @brief Target widget used for teaching-tip anchoring.
     * zh_CN: 教学提示用于锚定的目标控件。
     */
    Q_PROPERTY(QWidget* target READ target WRITE setTarget)
    /**
     * @brief Preferred teaching-tip placement around the target.
     * zh_CN: 教学提示围绕目标的首选位置。
     */
    Q_PROPERTY(PreferredPlacement preferredPlacement READ preferredPlacement WRITE setPreferredPlacement)
    /**
     * @brief Margin kept between teaching tip and target.
     * zh_CN: 教学提示与目标之间保留的间距。
     */
    Q_PROPERTY(int placementMargin READ placementMargin WRITE setPlacementMargin)
    /**
     * @brief Whether outside interaction dismisses the teaching tip.
     * zh_CN: 外部交互是否关闭教学提示。
     */
    Q_PROPERTY(bool lightDismissEnabled READ isLightDismissEnabled WRITE setLightDismissEnabled)
    /**
     * @brief Whether the teaching-tip tail is painted.
     * zh_CN: 是否绘制教学提示尾巴。
     */
    Q_PROPERTY(bool tailVisible READ isTailVisible WRITE setTailVisible)

public:
    enum PreferredPlacement {
        Auto,
        Top,
        TopLeft,
        TopRight,
        Bottom,
        BottomLeft,
        BottomRight,
        Left,
        LeftTop,
        LeftBottom,
        Right,
        RightTop,
        RightBottom,
    };
    Q_ENUM(PreferredPlacement)

    enum CloseReason {
        Programmatic,
        ActionButton,
        CloseButton,
        LightDismiss,
        TargetDestroyed,
    };
    Q_ENUM(CloseReason)

    explicit TeachingTip(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    QWidget* target() const { return m_target.data(); }
    void setTarget(QWidget* target);

    PreferredPlacement preferredPlacement() const { return m_preferredPlacement; }
    void setPreferredPlacement(PreferredPlacement placement);

    int placementMargin() const { return m_placementMargin; }
    void setPlacementMargin(int margin);

    bool isLightDismissEnabled() const { return m_lightDismissEnabled; }
    void setLightDismissEnabled(bool enabled);

    bool isTailVisible() const { return m_tailVisible; }
    void setTailVisible(bool visible);

    /// 卡片内容区域容器。向此 widget 添加子控件（AnchorLayout 等）。
    /// 几何随 placement / cardSize 自动更新，无需手动管理。
    QWidget* contentHost() const { return m_contentHost; }

    /// 设置卡片内容区域尺寸（不含阴影和气泡箭头）。默认 360×200。
    void setCardSize(const QSize& size);
    QSize cardSize() const { return m_cardSizeHint; }

    void showAt(QWidget* target);
    void closeWithReason(CloseReason reason);

signals:
    void closing(CloseReason reason);

protected:
    QPoint computePosition() const override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void syncContentHostGeometry();
    void updateWidgetSize();

    QRect cardRect() const;
    QRect targetRectInTopLevel() const;
    PreferredPlacement resolvedPlacement() const;
    PreferredPlacement resolveAutoPlacement(const QSize& cardSize) const;
    QPoint cardTopLeftForPlacement(PreferredPlacement placement, const QRect& targetRect,
                                   const QSize& cardSize) const;
    QPoint clampCardTopLeft(const QPoint& cardTopLeft, const QSize& cardSize) const;
    QPoint widgetTopLeftForCardTopLeft(const QPoint& cardTopLeft,
                                       PreferredPlacement placement) const;
    QMargins tailInsets(PreferredPlacement placement) const;

    void markPendingCloseReason(CloseReason reason);
    void emitClosingReason();

    QWidget* m_contentHost = nullptr;
    QPointer<QWidget> m_target;
    QPointer<QWidget> m_targetTopLevel;
    PreferredPlacement m_preferredPlacement = Auto;
    int m_placementMargin = 4;
    bool m_lightDismissEnabled = false;
    bool m_tailVisible = true;
    QSize m_cardSizeHint = QSize(360, 200);

    CloseReason m_pendingCloseReason = Programmatic;
    bool m_closeReasonExplicit = false;
};

} // namespace view::dialogs_flyouts

#endif // TEACHINGTIP_H
