#ifndef TOOLTIP_H
#define TOOLTIP_H

#include <QPointer>
#include <QWidget>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPropertyAnimation;

namespace fluent::textfields { class Label; }

namespace fluent::status_info {

/**
 * @brief Fluent tooltip bubble with margin and animation controls.
 * zh_CN: 支持边距和动画控制的 Fluent 提示气泡。
 *
 * ToolTip provides a lightweight status surface that can be styled consistently
 * with overlay radius, spacing, and theme animation decisions.
 * zh_CN: ToolTip 提供轻量提示表面，可与浮层圆角、间距和主题动效保持一致。
 */
class ToolTip : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Margins around tooltip content.
     * zh_CN: 工具提示内容周围的边距。
     */
    Q_PROPERTY(QMargins margins READ margins WRITE setMargins NOTIFY marginsChanged)
    /**
     * @brief Whether tooltip show/hide transitions are animated.
     * zh_CN: 提示气泡显示/隐藏过程是否播放过渡动画。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
public:
    /**
     * @brief Preferred side of the target used by an attached tooltip.
     * zh_CN: 附加工具提示相对于目标控件的首选方向。
     */
    enum Placement { Above, Below, Left, Right };
    Q_ENUM(Placement)

    explicit ToolTip(QWidget* parent = nullptr);

    /**
     * @brief Attaches a Fluent tooltip to a widget and reuses an existing attachment.
     * zh_CN: 将 Fluent 工具提示附加到控件，并复用该控件已有的附加提示。
     */
    static ToolTip* attach(QWidget* target,
                           const QString& text,
                           Placement placement = Above);
    
    void setText(const QString& text);
    QString text() const;

    QMargins margins() const { return m_margins; }
    void setMargins(const QMargins& margins);

    // Transparent border (px) reserved on every side for the drop shadow; callers
    // that position the visible bubble precisely should offset by this.
    // zh_CN: 为投影在四周预留的透明边距（像素）；需要精确定位可见气泡的调用方应据此偏移。
    int shadowMargin() const;

    // Shadows QWidget::setFont so it reaches the inner label. zh_CN: 影子 QWidget::setFont 以应用到内部标签。
    void setFont(const QFont& font);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    void setVisible(bool visible) override;

    void onThemeUpdated() override;

signals:
    void marginsChanged();
    void animationEnabledChanged(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setTarget(QWidget* target, Placement placement);
    void positionForTarget();
    void applyLayoutMargins();
    void ensureOpacityAnimation();
    void startShowAnimation();
    void startHideAnimation();
    void finishHideAnimation();

    fluent::textfields::Label* m_textBlock;
    QMargins m_margins;

    QColor m_bgColor;
    QColor m_borderColor;
    QColor m_textColor;

    bool m_animationEnabled = true;
    bool m_hideOnAnimationFinished = false;
    QPropertyAnimation* m_opacityAnimation = nullptr;
    QPointer<QWidget> m_target;
    Placement m_placement = Above;
};

} // namespace fluent::status_info

#endif // TOOLTIP_H
