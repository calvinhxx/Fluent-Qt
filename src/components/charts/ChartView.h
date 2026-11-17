#ifndef FLUENTQT_CHARTVIEW_H
#define FLUENTQT_CHARTVIEW_H

#include <QWidget>
#include <QImage>
#include <memory>
#include "ChartModel.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::charts {
class ChartViewPrivate;

/**
 * @brief Native chart widget that renders external ChartModel series within a point budget.
 * Each model provides one series without transferring ownership. Dense lines preserve extrema,
 * dense bars show bucket means, pie/donut charts show the first 11 positive rows plus an "Other"
 * slice, and scatter plots sample representative points without allocating per-point widgets.
 * zh_CN: 基于外部 ChartModel 的原生图表控件（不接管模型所有权），绘制点数受预算控制。
 * 每个模型对应一条序列：密集折线保留极值，密集柱图显示分桶均值，饼/环图显示前 11 个正值项并汇总其余项，散点图按代表点采样绘制。
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
     * @brief Binds a single external model (without taking ownership); nullptr clears all series.
     * zh_CN: 关联单个外部模型（不接管所有权）；传入 nullptr 清空序列。
     */
    void setModel(ChartModel* model);
    ChartModel* model() const;
    /**
     * @brief Adds an external series model (up to 32, without taking ownership); duplicates are ignored.
     * Pie and Donut store extra series but only draw seriesModel(0); prefer setModel() for them.
     * zh_CN: 添加外部序列模型（最多 32 条，不接管所有权），重复添加会忽略。
     * 饼图和环图会保留额外模型但仅绘制 seriesModel(0)，这类单序列图表建议使用 setModel()。
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
     * @brief Unit suffix appended to value readouts (e.g. " ms" or "%") without rescaling data.
     * zh_CN: 数值提示的单位后缀（如 " ms" 或 "%"），不改变数据刻度。
     */
    QString valueSuffix() const;
    void setValueSuffix(const QString& suffix);
    QString centerText() const;
    void setCenterText(const QString& text);
    QString centerCaption() const;
    void setCenterCaption(const QString& caption);
    /**
     * @brief Shows a loading state while data is prepared externally; does not manage background jobs.
     * zh_CN: 显示加载占位状态（由外部在准备数据时控制，视图自身不启动后台任务）。
     */
    bool isLoading() const;
    void setLoading(bool loading);
    bool isAutoYRange() const;
    double minimumY() const;
    double maximumY() const;
    /**
     * @brief Sets a finite increasing Y range (for example [0, 100] for percentage bars); invalid ranges are ignored.
     * zh_CN: 设置有限递增的 Y 轴范围（如百分比柱图可用 [0, 100]）；无效范围会被忽略。
     */
    void setYRange(double minimum, double maximum);
    void resetYRange();
    bool isLegendVisible() const;
    void setLegendVisible(bool visible);
    int maximumPointCount() const;
    /**
     * @brief Sets the total sampled point budget in [512, 65536], further capped by viewport width.
     * zh_CN: 设置总采样点预算（范围 [512, 65536]），实际绘制还会受视口宽度限制。
     */
    void setMaximumPointCount(int count);
    int maximumFrameRate() const;
    /**
     * @brief Throttles burst model updates to [1, 60] Hz; hidden charts skip projection work.
     * zh_CN: 将高频模型更新节流至 [1, 60] Hz；控件隐藏时不执行投影计算。
     */
    void setMaximumFrameRate(int rate);
    bool isAutoRange() const;
    double minimumX() const;
    double maximumX() const;
    /**
     * @brief Sets a finite increasing X viewport range; invalid ranges are ignored.
     * zh_CN: 设置有限递增的 X 轴视口范围；无效范围会被忽略。
     */
    void setXRange(double minimum, double maximum);
    void resetXRange();

    int currentSeries() const;
    int currentRow() const;
    /** @brief Moves the data cursor to a series row; pass -1 to clear. zh_CN: 将数据游标移动到指定序列和行号；传 -1 清空。 */
    void setCurrentPoint(int series, int row);
    QString currentPointText() const;
    QRectF plotRect() const;
    QRectF legendRect() const;
    /** @brief Cached sample point count from the most recent paint pass. zh_CN: 最近一次绘制缓存的采样点数量。 */
    int renderedPointCount() const;
    quint64 projectionBuildCount() const;
    /**
     * @brief Renders the chart at the given pixel scale without hover cursor or focus outlines.
     * Must be called on the GUI thread; invalid scales or outputs exceeding 16 million pixels return a null QImage.
     * zh_CN: 按指定像素倍率导出图表图像（不含游标和焦点框）。
     * 须在 GUI 线程调用；无效缩放比例或总像素超过 1600 万时返回空图像。
     */
    QImage exportImage(double scale = 1.0);
    /**
     * @brief Encodes exportImage() as PNG bytes, returning empty on failure.
     * zh_CN: 将 exportImage() 编码为 PNG 字节流，失败时返回空。
     */
    QByteArray exportPng(double scale = 1.0);
    /**
     * @brief Exports raw model rows as UTF-8 CSV (columns: series, name, x, y, label, unit).
     * Exports original rows rather than sampled points, leaving NaN Y cells blank. When visibleRangeOnly
     * is true, X bounds are inclusive; radial charts always export the full first series. Call on the GUI thread.
     * zh_CN: 将原始模型数据导出为 UTF-8 CSV（列：序列索引、名称、X、Y、标签、单位）。
     * 直接导出原始行而非采样/聚合值，缺失 Y 值留空；visibleRangeOnly 为 true 时包含 X 边界，饼环图始终导出完整首序列。须在 GUI 线程调用。
     */
    QByteArray exportCsv(bool visibleRangeOnly = false) const;
    /**
     * @brief Copies the rendered PNG to the system clipboard and emits imageCopied().
     * zh_CN: 将图表 PNG 图像复制到系统剪贴板，完成后发出 imageCopied()。
     */
    Q_SLOT void copyImage(double scale = 1.0);
    /**
     * @brief Copies exported CSV rows to the system clipboard and emits dataCopied().
     * Off-GUI-thread calls emit dataCopied(false) without touching the clipboard.
     * zh_CN: 将导出的 CSV 数据复制到系统剪贴板，完成后发出 dataCopied()。
     * 在非 GUI 线程调用会直接发出 dataCopied(false)，不修改剪贴板。
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
     * @brief Emitted when a displayed point or bucket [first, last) is clicked or activated.
     * zh_CN: 点击或激活图表点/聚合区间 [first, last) 时发出。
     */
    void rangeActivated(int series, int first, int last);
    void imageCopied(bool success);
    void dataCopied(bool success);

protected:
    /**
     * @brief Constructs a chart view locked to a fixed ChartType for dedicated subclasses.
     * zh_CN: 为专用图表子类构造固定类型的视图。
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
