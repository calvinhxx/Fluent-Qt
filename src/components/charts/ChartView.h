#ifndef FLUENTQT_CHARTVIEW_H
#define FLUENTQT_CHARTVIEW_H

#include <QWidget>
#include <memory>
#include "ChartModel.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::charts {
class ChartViewPrivate;

/**
 * @brief A bounded, native chart projection of caller-owned models.
 * zh_CN: 调用方模型的原生图表视图，绘制数据量受预算约束。
 * One model is one series. Dense lines retain extrema, dense bars show bucket means,
 * and pie/donut charts show positive values from the first eleven rows plus an aggregate remainder.
 * Scatter uses representative points, not a density estimate. No per-point widgets exist.
 * zh_CN: 一模型一序列；密集曲线保留极值，密集柱展示桶均值，饼环图合并剩余项。
 */
class ChartView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(ChartType chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY subtitleChanged)
    Q_PROPERTY(QString valueSuffix READ valueSuffix WRITE setValueSuffix NOTIFY valueSuffixChanged)
    Q_PROPERTY(QString centerText READ centerText WRITE setCenterText NOTIFY centerTextChanged)
    Q_PROPERTY(
        QString centerCaption READ centerCaption WRITE setCenterCaption NOTIFY centerCaptionChanged)
    Q_PROPERTY(bool loading READ isLoading WRITE setLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool autoYRange READ isAutoYRange NOTIFY yRangeChanged)
    Q_PROPERTY(
        bool legendVisible READ isLegendVisible WRITE setLegendVisible NOTIFY legendVisibleChanged)
    Q_PROPERTY(int maximumPointCount READ maximumPointCount WRITE setMaximumPointCount NOTIFY
                   maximumPointCountChanged)
    Q_PROPERTY(int maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY
                   maximumFrameRateChanged)
    Q_PROPERTY(bool autoRange READ isAutoRange NOTIFY xRangeChanged)
public:
    enum ChartType { Line, Area, Bar, HorizontalBar, Pie, Donut, Scatter, Sparkline };
    Q_ENUM(ChartType)
    explicit ChartView(QWidget* parent = nullptr);
    ~ChartView() override;

    /** @brief Replaces series with one borrowed model; nullptr clears. zh_CN: 设置借用模型；空指针清空。 */
    void setModel(ChartModel* model);
    ChartModel* model() const;
    /**
     * @brief Adds a borrowed series, up to 32; duplicates are no-ops.
     * zh_CN: 添加借用序列，最多 32 个；重复添加不改变状态。
     * Pie and Donut retain additional models but render only seriesModel(0).
     * Prefer setModel() for these single-series presentations.
     * zh_CN: 饼图和环图保留额外模型，但仅绘制 seriesModel(0)；建议使用 setModel()。
     */
    bool addSeries(ChartModel* model);
    bool removeSeries(ChartModel* model);
    int seriesCount() const;
    ChartModel* seriesModel(int series) const;
    void clearSeries();

    ChartType chartType() const;
    /** @brief Changes a generic ChartView; dedicated chart components keep their fixed type.
     * zh_CN: 更改通用 ChartView 类型；独立图表组件始终保持其固定类型。 */
    void setChartType(ChartType type);
    QString title() const;
    void setTitle(const QString& title);
    QString subtitle() const;
    void setSubtitle(const QString& subtitle);
    /** @brief Suffix for value readouts, for example " ms" or "%"; does not rescale data.
     * zh_CN: 数值读数后缀，例如 " ms" 或 "%"；不改变数据比例。 */
    QString valueSuffix() const;
    void setValueSuffix(const QString& suffix);
    QString centerText() const;
    void setCenterText(const QString& text);
    QString centerCaption() const;
    void setCenterCaption(const QString& caption);
    /** @brief Host-controlled presentation while preparing a snapshot; owns no worker task.
     * zh_CN: 由宿主控制的快照准备状态；不创建工作线程任务。 */
    bool isLoading() const;
    void setLoading(bool loading);
    bool isAutoYRange() const;
    double minimumY() const;
    double maximumY() const;
    /** @brief Sets a finite increasing value range; percentage bars should use [0, 100].
     * zh_CN: 设置有限递增的数值范围；百分比条应使用 [0, 100]。 */
    void setYRange(double minimum, double maximum);
    void resetYRange();
    bool isLegendVisible() const;
    void setLegendVisible(bool visible);
    int maximumPointCount() const;
    /** @brief Total projection budget, [512, 65536], additionally limited by viewport width.
     * zh_CN: 总投影点预算，范围 512 到 65536，并进一步受视口宽度限制。 */
    void setMaximumPointCount(int count);
    int maximumFrameRate() const;
    /** @brief Coalesces model update bursts at [1, 60] Hz; hidden views do no projection work.
     * zh_CN: 将模型更新合并到每秒 1 到 60 帧；隐藏视图不计算投影。 */
    void setMaximumFrameRate(int rate);
    bool isAutoRange() const;
    double minimumX() const;
    double maximumX() const;
    /** @brief Sets a finite increasing X viewport; invalid ranges are ignored.
     * zh_CN: 设置有限且递增的 X 视口范围，忽略无效范围。 */
    void setXRange(double minimum, double maximum);
    void resetXRange();

    int currentSeries() const;
    int currentRow() const;
    /** @brief Moves the data cursor without copying model data; -1 clears it.
     * zh_CN: 移动数据游标，不复制模型数据；-1 清空。 */
    void setCurrentPoint(int series, int row);
    QString currentPointText() const;
    QRectF plotRect() const;
    QRectF legendRect() const;
    /** @brief Number of cached representative points, after the last paint.
     * zh_CN: 最近一次绘制缓存的代表点数量。 */
    int renderedPointCount() const;
    quint64 projectionBuildCount() const;
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void modelChanged();
    void chartTypeChanged(ChartType type);
    void titleChanged(const QString& title);
    void subtitleChanged(const QString& subtitle);
    void valueSuffixChanged(const QString& suffix);
    void centerTextChanged(const QString& text);
    void centerCaptionChanged(const QString& caption);
    void loadingChanged(bool loading);
    void yRangeChanged();
    void legendVisibleChanged(bool visible);
    void maximumPointCountChanged(int count);
    void maximumFrameRateChanged(int rate);
    void xRangeChanged();
    void currentPointChanged(int series, int row);
    /** @brief Activates the displayed half-open source range; aggregate bars/slices keep their range.
     * zh_CN: 激活显示的左闭右开源数据范围；聚合柱或扇区保留完整范围。 */
    void rangeActivated(int series, int first, int last);

protected:
    /** @brief Constructs a fixed-presentation chart for a dedicated component.
     * zh_CN: 为独立图表组件构造固定类型的视图。 */
    ChartView(ChartType type, QWidget* parent);
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    std::unique_ptr<ChartViewPrivate> d;
};
} // namespace fluent::charts

#endif
