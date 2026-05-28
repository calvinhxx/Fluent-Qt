#ifndef PROGRESSRING_H
#define PROGRESSRING_H

#include <QBasicTimer>
#include <QColor>
#include <QWidget>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

namespace view::status_info {

/**
 * @brief Fluent circular progress indicator for active or indeterminate work.
 * zh_CN: 用于活动或不确定任务的 Fluent 环形进度指示器。
 *
 * ProgressRing exposes active state, value range, ring size, stroke width, status,
 * and optional background rendering for compact progress feedback.
 * zh_CN: ProgressRing 暴露活动态、值范围、环尺寸、描边宽度、状态和可选背景绘制，
 * 用于紧凑进度反馈。
 */
class ProgressRing : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether progress animation is active.
     * zh_CN: 进度动画是否处于活动状态。
     */
    Q_PROPERTY(bool isActive READ isActive WRITE setIsActive NOTIFY isActiveChanged)
    /**
     * @brief Whether the progress indicator uses indeterminate motion.
     * zh_CN: 进度指示器是否使用不确定动画。
     */
    Q_PROPERTY(bool isIndeterminate READ isIndeterminate WRITE setIsIndeterminate NOTIFY isIndeterminateChanged)
    /**
     * @brief Lower bound used when calculating determinate ring progress.
     * zh_CN: 计算确定环形进度时使用的下边界。
     */
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
    /**
     * @brief Upper bound used when calculating determinate ring progress.
     * zh_CN: 计算确定环形进度时使用的上边界。
     */
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    /**
     * @brief Current determinate ring value between minimum and maximum.
     * zh_CN: minimum 与 maximum 之间的当前确定环形进度值。
     */
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    /**
     * @brief Outer size of the progress ring.
     * zh_CN: 进度环外部尺寸。
     */
    Q_PROPERTY(ProgressRingSize ringSize READ ringSize WRITE setRingSize NOTIFY ringSizeChanged)
    /**
     * @brief Stroke width used by the progress ring.
     * zh_CN: 进度环使用的线宽。
     */
    Q_PROPERTY(qreal strokeWidth READ strokeWidth WRITE setStrokeWidth NOTIFY strokeWidthChanged)
    /**
     * @brief Semantic status used to choose visual treatment.
     * zh_CN: 用于选择视觉表现的语义状态。
     */
    Q_PROPERTY(ProgressRingStatus status READ status WRITE setStatus NOTIFY statusChanged)
    /**
     * @brief Whether the inactive ring track is painted behind progress.
     * zh_CN: 是否在进度后方绘制未激活环形轨道。
     */
    Q_PROPERTY(bool backgroundVisible READ backgroundVisible WRITE setBackgroundVisible NOTIFY backgroundVisibleChanged)

public:
    enum class ProgressRingSize { Small, Medium, Large };
    Q_ENUM(ProgressRingSize)

    enum class ProgressRingStatus { Running, Paused, Error };
    Q_ENUM(ProgressRingStatus)

    explicit ProgressRing(QWidget* parent = nullptr);
    ~ProgressRing() override;

    bool isActive() const { return m_isActive; }
    void setIsActive(bool active);

    bool isIndeterminate() const { return m_isIndeterminate; }
    void setIsIndeterminate(bool indeterminate);

    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);

    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);

    void setRange(int minimum, int maximum);

    int value() const { return m_value; }
    void setValue(int value);

    ProgressRingSize ringSize() const { return m_ringSize; }
    void setRingSize(ProgressRingSize size);

    qreal strokeWidth() const { return m_strokeWidth; }
    void setStrokeWidth(qreal width);

    ProgressRingStatus status() const { return m_status; }
    void setStatus(ProgressRingStatus status);

    bool backgroundVisible() const { return m_backgroundVisible; }
    bool isBackgroundVisible() const { return backgroundVisible(); }
    void setBackgroundVisible(bool visible);

    double progressRatio() const;
    bool isAnimationRunning() const { return m_animationTimer.isActive(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void isActiveChanged(bool active);
    void isIndeterminateChanged(bool indeterminate);
    void minimumChanged(int minimum);
    void maximumChanged(int maximum);
    void valueChanged(int value);
    void ringSizeChanged(ProgressRingSize size);
    void strokeWidthChanged(qreal width);
    void statusChanged(ProgressRingStatus status);
    void backgroundVisibleChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    int diameterForSize() const;
    QRectF ringRect(qreal effectiveStrokeWidth) const;
    QColor indicatorColor() const;
    void updateThemeColors();
    void updateAnimationState();
    bool shouldAnimate() const;

    bool m_isActive = false;
    bool m_isIndeterminate = true;
    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = 0;
    ProgressRingSize m_ringSize = ProgressRingSize::Medium;
    qreal m_strokeWidth = 3.0;
    ProgressRingStatus m_status = ProgressRingStatus::Running;
    bool m_backgroundVisible = false;

    QBasicTimer m_animationTimer;
    qreal m_animationPhase = 0.0;

    QColor m_runningColor;
    QColor m_pausedColor;
    QColor m_errorColor;
    QColor m_disabledColor;
    QColor m_trackColor;
};

} // namespace view::status_info

#endif // PROGRESSRING_H
