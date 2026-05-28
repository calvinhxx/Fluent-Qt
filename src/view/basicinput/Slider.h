#ifndef SLIDER_H
#define SLIDER_H

#include <QSlider>
#include <QPropertyAnimation>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"
#include "design/Spacing.h"

class QPainter;
class QStyleOptionSlider;

namespace view::textfields { class Label; }
namespace view::status_info { class ToolTip; }

namespace view::basicinput {

/**
 * @brief Fluent slider with custom track, handle, and animated feedback metrics.
 * zh_CN: 自定义轨道、滑块和动画反馈参数的 Fluent 滑块。
 *
 * Slider builds on QSlider semantics while exposing handle size, track height,
 * hover ratio, and press ratio for polished visual-state transitions.
 * zh_CN: Slider 保留 QSlider 语义，并暴露滑块尺寸、轨道高度、hover 比例和 press 比例，
 * 用于更细致的视觉状态过渡。
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
    // 动画属性
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

    void onThemeUpdated() override { update(); }

    int handleSize() const { return m_handleSize; }
    void setHandleSize(int size) { m_handleSize = size; update(); }

    int trackHeight() const { return m_trackHeight; }
    void setTrackHeight(int height) { m_trackHeight = height; update(); }

    qreal hoverRatio() const { return m_hoverRatio; }
    void setHoverRatio(qreal ratio) { m_hoverRatio = ratio; update(); }

    qreal pressRatio() const { return m_pressRatio; }
    void setPressRatio(qreal ratio) { m_pressRatio = ratio; update(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawHorizontal(QPainter& p, const QStyleOptionSlider& opt);
    void drawVertical(QPainter& p, const QStyleOptionSlider& opt);

    int pixelPosToRangeValue(int pos) const;
    int valueToPixelPos(int value) const;
    void showToolTip();
    void updateToolTipPos();
    void hideToolTip();

    int m_handleSize = 20; // WinUI 3 Standard Thumb (5 * BaseUnit)
    int m_trackHeight = ::Spacing::XSmall; // 默认 4px
    int m_visualMargin = 2; // 避免抗锯齿被裁切的边距
    int m_defaultLength = 160; // 默认长度
    bool m_isPressed = false;

    // 动画状态
    qreal m_hoverRatio = 0.0;
    qreal m_pressRatio = 0.0;
    QPropertyAnimation* m_hoverAnim = nullptr;
    QPropertyAnimation* m_pressAnim = nullptr;

    view::status_info::ToolTip* m_toolTip = nullptr;
};

} // namespace view::basicinput

#endif // SLIDER_H

