#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QPropertyAnimation;

namespace view::basicinput {

/**
 * @brief Fluent switch for binary on/off settings.
 * zh_CN: 用于二元开关设置的 Fluent 切换控件。
 *
 * ToggleSwitch owns its track, knob, header, and state text rendering. It keeps
 * keyboard and pointer toggling explicit while exposing knob progress for animation.
 * zh_CN: ToggleSwitch 自绘轨道、滑块、标题和状态文案，并显式处理键盘/指针切换；
 * knobPosition 用于驱动滑块动画。
 */
class ToggleSwitch : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Current on/off state of the switch.
     * zh_CN: 开关当前的开启/关闭状态。
     */
    Q_PROPERTY(bool isOn READ isOn WRITE setIsOn NOTIFY toggled)
    /**
     * @brief Optional label text displayed above the switch content.
     * zh_CN: 显示在开关内容上方的可选标签文本。
     */
    Q_PROPERTY(QString header READ header WRITE setHeader NOTIFY headerChanged)
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
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 排版角色。
     */
    Q_PROPERTY(QString fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
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

    QString header() const { return m_header; }
    void setHeader(const QString& header);

    QString onContent() const { return m_onContent; }
    void setOnContent(const QString& content);

    QString offContent() const { return m_offContent; }
    void setOffContent(const QString& content);

    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

    qreal knobPosition() const { return m_knobPosition; }
    void setKnobPosition(qreal pos);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void toggled(bool isOn);
    void headerChanged(const QString& header);
    void onContentChanged(const QString& content);
    void offContentChanged(const QString& content);
    void fontRoleChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void toggle();
    void animateKnob(bool toOn);
    QRectF trackRect() const;
    QRectF knobRect() const;
    int trackWidth() const { return 40; }
    int trackHeight() const { return 20; }
    int contentAreaX() const;

    bool m_isOn = false;
    QString m_header;
    QString m_onContent = "On";
    QString m_offContent = "Off";
    QString m_fontRole = "Body";

    qreal m_knobPosition = 0.0;  // 0.0 = Off, 1.0 = On
    bool m_isHovered = false;
    bool m_isPressed = false;

    QPropertyAnimation* m_knobAnimation = nullptr;
};

} // namespace view::basicinput

#endif // TOGGLESWITCH_H
