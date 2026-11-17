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
 * @brief Fluent on/off toggle switch.
 * Renders only the track, knob, and optional state text; place field labels externally (for example with Field).
 * zh_CN: Fluent 开关控件，仅绘制轨道、滑块和开关状态文案；字段标题请通过 Field 等外部组件组合。
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
     * @brief Fluent Typography preset used while following theme fonts.
     * zh_CN: 跟随主题字体时文本使用的预设字号样式。
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
     * @brief Sets the switch visual scale (clamped to [0.5, 10.0], default 1.0 = 40x20 px) without resizing fixed widgets or text.
     * zh_CN: 设置开关图形的缩放倍率（限制在 [0.5, 10.0]，默认 1.0 为 40x20 轨道），不改变已固定的控件尺寸或文字字体。
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
