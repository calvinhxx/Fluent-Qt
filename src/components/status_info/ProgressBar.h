#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <QBasicTimer>
#include <QColor>
#include <QString>
#include <QWidget>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::status_info {

/**
 * @brief Fluent linear progress indicator for determinate and indeterminate work.
 * zh_CN: 用于确定和不确定进度的 Fluent 线性进度指示器。
 *
 * ProgressBar exposes value bounds, paused/error status, rail visibility, bar
 * thickness, and indeterminate animation as explicit visual state.
 * zh_CN: ProgressBar 将值范围、暂停/错误状态、轨道可见性、条厚度和不确定动画暴露为明确视觉状态。
 */
class ProgressBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether the progress indicator uses indeterminate motion.
     * zh_CN: 进度指示器是否使用不确定动画。
     */
    Q_PROPERTY(bool isIndeterminate READ isIndeterminate WRITE setIsIndeterminate NOTIFY isIndeterminateChanged)
    /**
     * @brief Lower bound used when calculating determinate progress.
     * zh_CN: 计算确定进度时使用的下边界。
     */
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
    /**
     * @brief Upper bound used when calculating determinate progress.
     * zh_CN: 计算确定进度时使用的上边界。
     */
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    /**
     * @brief Current determinate progress value between minimum and maximum.
     * zh_CN: minimum 与 maximum 之间的当前确定进度值。
     */
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    /**
     * @brief Whether paused-state visual treatment is shown.
     * zh_CN: 是否显示暂停状态视觉。
     */
    Q_PROPERTY(bool showPaused READ showPaused WRITE setShowPaused NOTIFY showPausedChanged)
    /**
     * @brief Whether error-state visual treatment is shown.
     * zh_CN: 是否显示错误状态视觉。
     */
    Q_PROPERTY(bool showError READ showError WRITE setShowError NOTIFY showErrorChanged)
    /**
     * @brief Width of the painted progress bar segment.
     * zh_CN: 进度条片段的绘制宽度。
     */
    Q_PROPERTY(int barWidth READ barWidth WRITE setBarWidth NOTIFY barWidthChanged)
    /**
     * @brief Thickness of the progress track.
     * zh_CN: 进度轨道厚度。
     */
    Q_PROPERTY(qreal trackThickness READ trackThickness WRITE setTrackThickness NOTIFY trackThicknessChanged)
    /**
     * @brief Whether the progress rail is painted.
     * zh_CN: 是否绘制进度背景轨道。
     */
    Q_PROPERTY(bool railVisible READ railVisible WRITE setRailVisible NOTIFY railVisibleChanged)

public:
    explicit ProgressBar(QWidget* parent = nullptr);
    ~ProgressBar() override;

    bool isIndeterminate() const { return m_isIndeterminate; }
    void setIsIndeterminate(bool indeterminate);

    double minimum() const { return m_minimum; }
    void setMinimum(double minimum);

    double maximum() const { return m_maximum; }
    void setMaximum(double maximum);

    void setRange(double minimum, double maximum);

    double value() const { return m_value; }
    void setValue(double value);

    bool showPaused() const { return m_showPaused; }
    void setShowPaused(bool paused);

    bool showError() const { return m_showError; }
    void setShowError(bool error);

    int barWidth() const { return m_barWidth; }
    void setBarWidth(int width);

    qreal trackThickness() const { return m_trackThickness; }
    void setTrackThickness(qreal thickness);

    bool railVisible() const { return m_railVisible; }
    void setRailVisible(bool visible);

    double progressRatio() const;
    QString progressText() const;
    bool isAnimationRunning() const { return m_animationTimer.isActive(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void isIndeterminateChanged(bool indeterminate);
    void minimumChanged(double minimum);
    void maximumChanged(double maximum);
    void valueChanged(double value);
    void showPausedChanged(bool paused);
    void showErrorChanged(bool error);
    void barWidthChanged(int width);
    void trackThicknessChanged(qreal thickness);
    void railVisibleChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QRectF barRect() const;
    QColor indicatorColor() const;
    void updateThemeColors();
    void updateAnimationState();
    bool shouldAnimate() const;
    bool isRunningState() const;
    double normalizedValue(double value) const;

    bool m_isIndeterminate = false;
    double m_minimum = 0.0;
    double m_maximum = 100.0;
    double m_value = 0.0;
    bool m_showPaused = false;
    bool m_showError = false;
    int m_barWidth = 220;
    qreal m_trackThickness = 3.0;
    bool m_railVisible = true;

    QBasicTimer m_animationTimer;
    qreal m_animationPhase = 0.0;

    QColor m_runningColor;
    QColor m_pausedColor;
    QColor m_errorColor;
    QColor m_disabledColor;
    QColor m_railColor;
};

} // namespace fluent::status_info

#endif // PROGRESSBAR_H
