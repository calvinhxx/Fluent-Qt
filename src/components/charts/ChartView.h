#ifndef FLUENTQT_CHARTVIEW_H
#define FLUENTQT_CHARTVIEW_H

#include <QWidget>
#include <QImage>
#include <memory>
#include "ChartModel.h"
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

namespace fluent::charts {
class ChartViewPrivate;

/**
 * @brief Chart widget backed by ChartModel supporting line, bar, area, pie/donut, and scatter plots.
 * Automatically samples large series to keep rendering responsive.
 * zh_CN: 基于 ChartModel 的图表控件，支持折线、柱状、面积、饼/环图和散点图，大数据量时自动抽样显示。
 */
class ChartView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(ChartType chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString subtitle READ subtitle WRITE setSubtitle NOTIFY subtitleChanged)
    Q_PROPERTY(QString valueSuffix READ valueSuffix WRITE setValueSuffix NOTIFY valueSuffixChanged)
    Q_PROPERTY(QString centerText READ centerText WRITE setCenterText NOTIFY centerTextChanged)
    Q_PROPERTY(QString centerCaption READ centerCaption WRITE setCenterCaption NOTIFY centerCaptionChanged)
    Q_PROPERTY(bool loading READ isLoading WRITE setLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool autoYRange READ isAutoYRange NOTIFY yRangeChanged)
    Q_PROPERTY(bool legendVisible READ isLegendVisible WRITE setLegendVisible NOTIFY legendVisibleChanged)
    Q_PROPERTY(int maximumPointCount READ maximumPointCount WRITE setMaximumPointCount NOTIFY maximumPointCountChanged)
    Q_PROPERTY(int maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY maximumFrameRateChanged)
    Q_PROPERTY(bool autoRange READ isAutoRange NOTIFY xRangeChanged)
public:
    enum ChartType { Line, Area, Bar, HorizontalBar, Pie, Donut, Scatter, Sparkline };
    Q_ENUM(ChartType)
    explicit ChartView(QWidget* parent = nullptr);
    ~ChartView() override;

    /**
     * @brief Sets a single chart model (still managed by the caller); pass nullptr to clear.
     * zh_CN: 设置单个数据源（模型仍由应用管理）；传 nullptr 清空。
     */
    void setModel(ChartModel* model);
    ChartModel* model() const;
    /**
     * @brief Adds a series model (up to 32, caller-managed); duplicates are ignored.
     * Pie and Donut only render seriesModel(0); prefer setModel() for those types.
     * zh_CN: 添加序列模型（最多 32 条，仍由应用管理），重复添加会忽略；饼图和环图仅显示首条序列，建议用 setModel()。
     */
    bool addSeries(ChartModel* model);
    bool removeSeries(ChartModel* model);
    int seriesCount() const;
    ChartModel* seriesModel(int series) const;
    void clearSeries();

    ChartType chartType() const;
    /**
     * @brief Changes the chart type on a generic ChartView; dedicated subclasses keep a fixed type.
     * zh_CN: 修改通用 ChartView 的图表类型；专用图表子类的类型固定不变。
     */
    void setChartType(ChartType type);
    QString title() const;
    void setTitle(const QString& title);
    QString subtitle() const;
    void setSubtitle(const QString& subtitle);
    /**
     * @brief Unit suffix appended to value readouts (e.g. " ms" or "%").
     * zh_CN: 数值提示的单位后缀（如 " ms" 或 "%"），不改变数据刻度。
     */
    QString valueSuffix() const;
    void setValueSuffix(const QString& suffix);
    QString centerText() const;
    void setCenterText(const QString& text);
    QString centerCaption() const;
    void setCenterCaption(const QString& caption);
    /**
     * @brief Shows a loading state while data is prepared externally.
     * zh_CN: 是否显示加载占位状态（由应用在准备数据时自行控制）。
     */
    bool isLoading() const;
    void setLoading(bool loading);
    bool isAutoYRange() const;
    double minimumY() const;
    double maximumY() const;
    /**
     * @brief Sets a finite increasing Y range (e.g. [0, 100]); invalid ranges are ignored.
     * zh_CN: 设置递增的 Y 轴范围（如 [0, 100]）；无效范围会被忽略。
     */
    void setYRange(double minimum, double maximum);
    void resetYRange();
    bool isLegendVisible() const;
    void setLegendVisible(bool visible);
    int maximumPointCount() const;
    /**
     * @brief Sets the sampled point budget in [512, 65536], capped by viewport width.
     * zh_CN: 设置采样点数上限（范围 [512, 65536]），实际绘制还会受视口宽度限制。
     */
    void setMaximumPointCount(int count);
    int maximumFrameRate() const;
    /**
     * @brief Throttles model updates to [1, 60] Hz; hidden charts pause updates.
     * zh_CN: 将高频数据更新限流至 [1, 60] Hz；控件隐藏时暂停重算。
     */
    void setMaximumFrameRate(int rate);
    bool isAutoRange() const;
    double minimumX() const;
    double maximumX() const;
    /**
     * @brief Sets a finite increasing X range; invalid ranges are ignored.
     * zh_CN: 设置递增的 X 轴范围；无效范围会被忽略。
     */
    void setXRange(double minimum, double maximum);
    void resetXRange();

    int currentSeries() const;
    int currentRow() const;
    /** @brief Moves the data cursor to a series row; pass -1 to clear. zh_CN: 将数据游标移至指定序列和行号；传 -1 清空。 */
    void setCurrentPoint(int series, int row);
    QString currentPointText() const;
    QRectF plotRect() const;
    QRectF legendRect() const;
    /** @brief Sampled point count from the most recent paint pass. zh_CN: 最近一次绘制的采样点数量。 */
    int renderedPointCount() const;
    quint64 projectionBuildCount() const;
    /**
     * @brief Exports the chart image at the given scale (without hover cursor or focus outline); call on the GUI thread.
     * zh_CN: 按指定缩放倍率导出图表图片（不含游标和焦点框），须在 GUI 线程调用。
     */
    QImage exportImage(double scale = 1.0);
    /**
     * @brief Encodes exportImage() as PNG bytes, returning empty on failure.
     * zh_CN: 将图表导出为 PNG 字节流，失败时返回空。
     */
    QByteArray exportPng(double scale = 1.0);
    /**
     * @brief Exports raw model rows as UTF-8 CSV (columns: series, name, x, y, label, unit); call on the GUI thread.
     * Pass visibleRangeOnly = true to restrict Cartesian charts to the current X range.
     * zh_CN: 将原始数据导出为 UTF-8 CSV，须在 GUI 线程调用。
     */
    QByteArray exportCsv(bool visibleRangeOnly = false) const;
    /**
     * @brief Copies the chart PNG to the clipboard and emits imageCopied().
     * zh_CN: 将图表 PNG 图片复制到剪贴板，完成后发出 imageCopied()。
     */
    Q_SLOT void copyImage(double scale = 1.0);
    /**
     * @brief Copies exported CSV rows to the clipboard and emits dataCopied().
     * Must be called on the GUI thread (off-thread calls emit dataCopied(false)).
     * zh_CN: 将导出的 CSV 数据复制到剪贴板，须在 GUI 线程调用，完成后发出 dataCopied()。
     */
    Q_SLOT void copyData(bool visibleRangeOnly = false);
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
    /**
     * @brief Emitted when a displayed point or bucket [first, last) is activated.
     * zh_CN: 点击或激活图表点或聚合区间 [first, last) 时发出。
     */
    void rangeActivated(int series, int first, int last);
    void imageCopied(bool success);
    void dataCopied(bool success);

protected:
    /**
     * @brief Constructs a chart view locked to a fixed ChartType for subclasses.
     * zh_CN: 供专用图表子类构造固定类型的图表视图。
     */
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
    void paintChart(QPainter& painter, bool interactive);
    std::unique_ptr<ChartViewPrivate> d;
};
} // namespace fluent::charts

#endif
