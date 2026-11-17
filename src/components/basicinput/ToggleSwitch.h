#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include "compatibility/InputEventCompat.h"
#include <QFont>
#include <QWidget>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPropertyAnimation;
class QFocusEvent;

namespace fluent::basicinput {

/**
 * @brief Fluent binary on/off toggle switch.
 * zh_CN: Fluent 二元开关控件。
 *
 * Renders only the track, knob, and optional on/off state text; compose field
 * labels externally (for example with Field).
 * zh_CN: 仅绘制轨道、滑块和开关状态文案；字段标题请通过 Field 等外部组件组合。
 */
class ToggleSwitch : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Current on/off state of the switch.
     * zh_CN: 开关当前的开启/关闭状态。
     */
    Q_PROPERTY(bool isOn READ isOn WRITE setIsOn NOTIFY toggled)
    /**
     * @brief Text shown for the on state.
     * zh_CN: 开启状态显示的文本。
     */
    Q_PROPERTY(QString onContent READ onContent WRITE setOnContent NOTIFY onContentChanged)
    /**
     * @brief Text shown for the off state.
     * zh_CN: 关闭状态显示的文本。
     */
    Q_PROPERTY(QString offContent READ offContent WRITE setOffContent NOTIFY offContentChanged)
    /**
     * @brief Fluent typography role used while the switch follows theme fonts.
     * zh_CN: 开关跟随主题字体时用于文本绘制的 Fluent 排版角色。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Scale of the switch graphics and content gap, independent of widget size and font.
     * zh_CN: 开关图形及文字间距的缩放比例，独立于控件大小和字体；默认为 1.0。
     */
    Q_PROPERTY(qreal visualScale READ visualScale WRITE setVisualScale NOTIFY visualScaleChanged)
    /**
     * @brief Animated switch knob position, normalized from off to on.
     * zh_CN: 开关滑块位置动画值，从关闭到开启归一化。
     */
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    void onThemeUpdated() override;

    bool isOn() const { return m_isOn; }
    void setIsOn(bool on);

    QString onContent() const { return m_onContent; }
    void setOnContent(const QString& content);

    QString offContent() const { return m_offContent; }
    void setOffContent(const QString& content);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);

    qreal visualScale() const { return m_visualScale; }
    /**
     * @brief Sets the visual scale and updates layout hints without resizing a fixed widget.
     * zh_CN: 设置视觉缩放并更新布局建议尺寸，不改变已固定的控件大小。
     *
     * Finite values are clamped to [0.5, 10.0]; non-finite values are ignored.
     * The default 1.0 draws a 40x20 logical-pixel track with strokes >= 1 px.
     * Text keeps its font; use setFontRole() or setFont() to change it.
     * zh_CN: 有限值限制在 [0.5, 10.0]，忽略 NaN 和无穷值；1.0 对应 40x20 逻辑像素轨道，
     * 描边至少 1 px；文字字体不变，可通过字体 API 单独调整。
     */
    void setVisualScale(qreal scale);

    /**
     * @brief Sets an explicit font that remains unchanged by later theme refreshes.
     * zh_CN: 设置显式字体；后续主题刷新不会覆盖该字体。
     */
    void setFont(const QFont& font);

    qreal knobPosition() const { return m_knobPosition; }
    void setKnobPosition(qreal pos);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void toggled(bool isOn);
    void onContentChanged(const QString& content);
    void offContentChanged(const QString& content);
    void fontRoleChanged();
    void visualScaleChanged(qreal scale);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void applyFontRole();
    void toggle();
    void animateKnob(bool toOn);
    void updateAccessibleText();
    QRectF trackRect() const;
    QRectF knobRect() const;
    int contentAreaX() const;

    bool m_isOn = false;
    QString m_onContent;
    QString m_offContent;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    qreal m_visualScale = 1.0;
    bool m_hasExplicitFont = false;
    bool m_applyingFontRole = false;

    qreal m_knobPosition = 0.0; // 0.0 = Off, 1.0 = On
    bool m_isHovered = false;
    bool m_isPressed = false;
    bool m_keyboardFocusVisible = false;

    QPropertyAnimation* m_knobAnimation = nullptr;
    QString m_autoAccessibleDescription;
};

} // namespace fluent::basicinput

#endif // TOGGLESWITCH_H
