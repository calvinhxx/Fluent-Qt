#ifndef SLIDER_H
#define SLIDER_H

#include "../../compatibility/InputEventCompat.h"
#include <QSlider>
#include <QPropertyAnimation>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"
#include "../../design/Spacing.h"

class QPainter;
class QStyleOptionSlider;

namespace fluent::textfields {
class Label;
}
namespace fluent::status_info {
class ToolTip;
}

namespace fluent::basicinput {

/**
 * @brief Fluent slider with custom track, thumb, and drag value tooltip.
 * zh_CN: 自定义轨道、滑块及拖动数值提示的 Fluent 滑块。
 */
class Slider : public QSlider, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Slider handle size in pixels.
     * zh_CN: 滑块手柄尺寸，单位为像素。
     */
    Q_PROPERTY(int handleSize READ handleSize WRITE setHandleSize)
    /**
     * @brief Slider track height in pixels.
     * zh_CN: 滑块轨道高度，单位为像素。
     */
    Q_PROPERTY(int trackHeight READ trackHeight WRITE setTrackHeight)
    /**
     * @brief Hover animation ratio for slider visuals.
     * zh_CN: 滑块视觉使用的悬停动画比例。
     */
    Q_PROPERTY(qreal hoverRatio READ hoverRatio WRITE setHoverRatio)
    /**
     * @brief Pressed animation ratio for slider visuals.
     * zh_CN: 滑块视觉使用的按压动画比例。
     */
    Q_PROPERTY(qreal pressRatio READ pressRatio WRITE setPressRatio)

public:
    explicit Slider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    explicit Slider(QWidget* parent);
    ~Slider();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override { update(); }

    int handleSize() const { return m_handleSize; }
    void setHandleSize(int size)
    {
        m_handleSize = size;
        updateGeometry();
        update();
    }

    int trackHeight() const { return m_trackHeight; }
    void setTrackHeight(int height)
    {
        m_trackHeight = height;
        updateGeometry();
        update();
    }

    qreal hoverRatio() const { return m_hoverRatio; }
    void setHoverRatio(qreal ratio)
    {
        m_hoverRatio = ratio;
        update();
    }

    qreal pressRatio() const { return m_pressRatio; }
    void setPressRatio(qreal ratio)
    {
        m_pressRatio = ratio;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void drawHorizontal(QPainter& p, const QStyleOptionSlider& opt);
    void drawVertical(QPainter& p, const QStyleOptionSlider& opt);

    int pixelPosToRangeValue(int pos) const;
    int valueToPixelPos(int value) const;
    void showToolTip();
    void updateToolTipPos();
    void hideToolTip();

    int m_handleSize = 20; // WinUI 3 Standard Thumb (5 * BaseUnit)
    int m_trackHeight = ::Spacing::XSmall;
    int m_visualMargin = 2; // Extra margin so antialiasing is never clipped.
    int m_defaultLength = 160;
    bool m_isPressed = false;
    bool m_keyboardFocusVisible = false;

    qreal m_hoverRatio = 0.0;
    qreal m_pressRatio = 0.0;
    QPropertyAnimation* m_hoverAnim = nullptr;
    QPropertyAnimation* m_pressAnim = nullptr;

    fluent::status_info::ToolTip* m_toolTip = nullptr;
};

} // namespace fluent::basicinput

#endif // SLIDER_H
