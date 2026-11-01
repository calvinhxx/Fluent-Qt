#ifndef RANGESLIDER_H
#define RANGESLIDER_H

#include <QSlider>
#include <QWidget>

#include "compatibility/InputEventCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPainter;
class QVariantAnimation;

namespace fluent::status_info {
class ToolTip;
}

namespace fluent::basicinput {

/**
 * @brief Fluent two-thumb slider that selects a lower and an upper value.
 * zh_CN: 选择下限值与上限值的 Fluent 双手柄滑块。
 *
 * RangeSlider paints the same track, thumb, and value tip as Slider. Its range,
 * step, tick, orientation, and tracking properties keep QSlider's meaning; the
 * two values never cross and stay at least minimumRange apart.
 * zh_CN: RangeSlider 与 Slider 使用相同的轨道、手柄和数值提示。范围、步长、刻度、方向与
 * tracking 属性沿用 QSlider 语义；两个值不会交叉，且间距不小于 minimumRange。
 */
class RangeSlider : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Track orientation; vertical sliders place the minimum at the bottom.
     * zh_CN: 轨道方向；竖向时最小值位于底部。
     */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /**
     * @brief Smallest selectable value.
     * zh_CN: 可选择的最小值。
     */
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    /**
     * @brief Largest selectable value.
     * zh_CN: 可选择的最大值。
     */
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    /**
     * @brief Committed lower value.
     * zh_CN: 已提交的下限值。
     */
    Q_PROPERTY(int lowerValue READ lowerValue WRITE setLowerValue NOTIFY lowerValueChanged)
    /**
     * @brief Committed upper value.
     * zh_CN: 已提交的上限值。
     */
    Q_PROPERTY(int upperValue READ upperValue WRITE setUpperValue NOTIFY upperValueChanged)
    /**
     * @brief Smallest allowed distance between the two values; 0 lets the thumbs meet.
     * zh_CN: 两个值之间允许的最小距离；为 0 时两个手柄可以重合。
     */
    Q_PROPERTY(int minimumRange READ minimumRange WRITE setMinimumRange NOTIFY minimumRangeChanged)
    /**
     * @brief Arrow-key step.
     * zh_CN: 方向键步长。
     */
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep NOTIFY singleStepChanged)
    /**
     * @brief Page Up and Page Down step.
     * zh_CN: Page Up 与 Page Down 步长。
     */
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep NOTIFY pageStepChanged)
    /**
     * @brief Tick placement; ticks appear while the pointer is over the control.
     * zh_CN: 刻度位置；指针位于控件上方时显示刻度。
     */
    Q_PROPERTY(QSlider::TickPosition tickPosition READ tickPosition WRITE setTickPosition NOTIFY tickPositionChanged)
    /**
     * @brief Tick spacing in values; 0 derives it from the steps as QSlider does.
     * zh_CN: 以数值计的刻度间距；为 0 时与 QSlider 一样由步长推导。
     */
    Q_PROPERTY(int tickInterval READ tickInterval WRITE setTickInterval NOTIFY tickIntervalChanged)
    /**
     * @brief Whether values commit while dragging; when false they commit on release.
     * zh_CN: 拖动时是否持续提交数值；为 false 时在松开时提交。
     */
    Q_PROPERTY(bool tracking READ hasTracking WRITE setTracking NOTIFY trackingChanged)
    /**
     * @brief Thumb that receives keyboard input and paints above the other.
     * zh_CN: 接收键盘输入并绘制在另一手柄之上的手柄。
     */
    Q_PROPERTY(Thumb activeThumb READ activeThumb WRITE setActiveThumb NOTIFY activeThumbChanged)

public:
    /**
     * @brief Identifies one of the two thumbs.
     * zh_CN: 标识两个手柄之一。
     */
    enum class Thumb { Lower, Upper };
    Q_ENUM(Thumb)

    explicit RangeSlider(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
    explicit RangeSlider(QWidget* parent);
    ~RangeSlider() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override { update(); }

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    /**
     * @brief Sets both bounds; a maximum below the minimum becomes the minimum.
     * zh_CN: 同时设置上下界；小于最小值的最大值会被设为最小值。
     */
    void setRange(int minimum, int maximum);

    int lowerValue() const { return m_lower; }
    void setLowerValue(int value);
    int upperValue() const { return m_upper; }
    void setUpperValue(int value);
    /**
     * @brief Sets both values at once, ordering them and applying minimumRange.
     * zh_CN: 同时设置两个值，自动排序并应用 minimumRange。
     */
    void setValues(int lower, int upper);

    /**
     * @brief Visual lower position; differs from lowerValue only while dragging without tracking.
     * zh_CN: 下限手柄的视觉位置；仅在关闭 tracking 拖动时与 lowerValue 不同。
     */
    int lowerPosition() const { return m_lowerPosition; }
    /**
     * @brief Visual upper position; differs from upperValue only while dragging without tracking.
     * zh_CN: 上限手柄的视觉位置；仅在关闭 tracking 拖动时与 upperValue 不同。
     */
    int upperPosition() const { return m_upperPosition; }

    int minimumRange() const { return m_minimumRange; }
    void setMinimumRange(int range);

    int singleStep() const { return m_singleStep; }
    void setSingleStep(int step);
    int pageStep() const { return m_pageStep; }
    void setPageStep(int step);

    QSlider::TickPosition tickPosition() const { return m_tickPosition; }
    void setTickPosition(QSlider::TickPosition position);
    int tickInterval() const { return m_tickInterval; }
    void setTickInterval(int interval);

    bool hasTracking() const { return m_tracking; }
    void setTracking(bool enabled);

    Thumb activeThumb() const { return m_activeThumb; }
    void setActiveThumb(Thumb thumb);

    /**
     * @brief Whether a thumb is being dragged.
     * zh_CN: 是否正在拖动手柄。
     */
    bool isSliderDown() const { return m_dragging; }

    /**
     * @brief Thumb center in widget coordinates, for layout and accessibility.
     * zh_CN: 手柄中心的控件坐标，用于布局与无障碍。
     */
    QPoint thumbCenter(Thumb thumb) const;

signals:
    void orientationChanged(Qt::Orientation orientation);
    void rangeChanged(int minimum, int maximum);
    void lowerValueChanged(int value);
    void upperValueChanged(int value);
    /**
     * @brief Emitted once after either committed value changes.
     * zh_CN: 任一已提交值变化后发送一次。
     */
    void valuesChanged(int lower, int upper);
    void minimumRangeChanged(int range);
    void singleStepChanged(int step);
    void pageStepChanged(int step);
    void tickPositionChanged(QSlider::TickPosition position);
    void tickIntervalChanged(int interval);
    void trackingChanged(bool enabled);
    void activeThumbChanged(Thumb thumb);
    /**
     * @brief Emitted when a pointer press starts a drag.
     * zh_CN: 指针按下开始拖动时发送。
     */
    void sliderPressed();
    /**
     * @brief Emitted when a dragged position moves, whether or not tracking is enabled.
     * zh_CN: 拖动位置变化时发送，与 tracking 是否开启无关。
     */
    void sliderMoved(int lower, int upper);
    /**
     * @brief Emitted once when the drag ends, after the final values commit.
     * zh_CN: 拖动结束、最终值提交后发送一次。
     */
    void sliderReleased();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct ThumbMotion {
        qreal hoverRatio = 0.0;
        qreal pressRatio = 0.0;
        QVariantAnimation* hoverAnim = nullptr;
        QVariantAnimation* pressAnim = nullptr;
    };

    int padding() const;
    int trackLength() const;
    bool isUpsideDown() const;
    int valueToPixel(int value) const;
    int pixelToValue(int pixel) const;
    int pointerAxis(const QPoint& point) const;

    int position(Thumb thumb) const { return thumb == Thumb::Lower ? m_lowerPosition : m_upperPosition; }
    qint64 effectiveMinimumRange() const;
    void commitValues(int lower, int upper);
    void moveThumbTo(Thumb thumb, int value);
    void stepThumb(Thumb thumb, qint64 delta);
    Thumb thumbForPointer(const QPoint& point, bool* grabbed) const;
    bool hitsThumb(Thumb thumb, const QPoint& point) const;
    void finishDrag();

    ThumbMotion& motion(Thumb thumb) { return thumb == Thumb::Lower ? m_lowerMotion : m_upperMotion; }
    QVariantAnimation* createAnimation(qreal* ratio);
    void animate(QVariantAnimation* animation, qreal from, qreal to);
    void setHoveredThumb(bool hasThumb, Thumb thumb);
    void updateHover(const QPoint& point);

    void showToolTip();
    void updateToolTip();
    void hideToolTip();

    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_minimum = 0;
    int m_maximum = 99;
    int m_lower = 0;
    int m_upper = 99;
    int m_lowerPosition = 0;
    int m_upperPosition = 99;
    int m_minimumRange = 0;
    int m_singleStep = 1;
    int m_pageStep = 10;
    QSlider::TickPosition m_tickPosition = QSlider::NoTicks;
    int m_tickInterval = 0;
    bool m_tracking = true;
    Thumb m_activeThumb = Thumb::Lower;

    bool m_dragging = false;
    bool m_directionPending = false;
    int m_dragOffset = 0;
    bool m_hasHoveredThumb = false;
    Thumb m_hoveredThumb = Thumb::Lower;
    bool m_keyboardFocusVisible = false;

    int m_handleSize = 20;  // Matches Slider. zh_CN: 与 Slider 一致。
    int m_trackHeight = 4;  // Matches Slider. zh_CN: 与 Slider 一致。
    int m_visualMargin = 2; // Keeps antialiasing inside the widget. zh_CN: 避免抗锯齿被裁切。
    int m_defaultLength = 160;

    qreal m_widgetHoverRatio = 0.0;
    QVariantAnimation* m_widgetHoverAnim = nullptr;
    ThumbMotion m_lowerMotion;
    ThumbMotion m_upperMotion;

    fluent::status_info::ToolTip* m_toolTip = nullptr;
};

} // namespace fluent::basicinput

#endif // RANGESLIDER_H
