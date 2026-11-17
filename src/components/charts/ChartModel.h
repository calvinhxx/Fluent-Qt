#ifndef FLUENTQT_CHARTMODEL_H
#define FLUENTQT_CHARTMODEL_H

#include <QAbstractTableModel>
#include <QPointF>
#include <QSharedDataPointer>
#include <QStringList>
#include <QVector>

namespace fluent::charts {

class ChartDataPrivate;

/**
 * @brief Implicitly shared, indexed XY snapshot that can be built off the GUI thread.
 * X must be finite and nondecreasing, NaN Y marks a gap, and labels are optional.
 * Construction is O(n) and copies share storage.
 * zh_CN: 隐式共享的带索引 XY 数据快照，可在工作线程预构建。
 * X 须为有限非递减值，Y 为 NaN 表示断点，标签可选；构建复杂度 O(n)，拷贝共享底层存储。
 */
class ChartData {
public:
    ChartData();
    ChartData(const ChartData& other);
    ChartData& operator=(const ChartData& other);
    ~ChartData();

    static ChartData fromPoints(const QVector<QPointF>& points, const QStringList& labels = QStringList());
    bool isValid() const;
    QString errorString() const;
    int size() const;
    QPointF pointAt(int row) const;
    QString labelAt(int row) const;
    double minimumX() const;
    double maximumX() const;
    double minimumY() const;
    double maximumY() const;
    int lowerBound(double x) const;
    int upperBound(double x) const;

    /**
     * @brief Samples up to budget row indices in [first, last), keeping bucket endpoints and extrema.
     * Bounds are clamped, -1 marks gaps, and budgets under 16 return empty.
     * Query cost depends on budget and index depth rather than total row count.
     * zh_CN: 在左闭右开区间 [first, last) 内按上限 budget 采样行号，保留分桶端点和极值（-1 表示断点）。
     * 越界范围会自动裁剪，budget 小于 16 时返回空；耗时取决于预算和索引深度，无需全量扫描。
     */
    QVector<int> sampledRows(int first, int last, int budget) const;
    /** @brief Mean of finite Y values in [first, last). zh_CN: 左闭右开区间 [first, last) 内有限 Y 值的均值。 */
    double mean(int first, int last) const;
    /**
     * @brief Share of positive Y values in [first, last) relative to the full series, in [0, 1].
     * zh_CN: 区间 [first, last) 内正值占全序列正值总量的比例，范围 [0, 1]。
     */
    double positiveFraction(int first, int last) const;

private:
    friend class ChartModel;
    QSharedDataPointer<ChartDataPrivate> d;
};

/**
 * @brief External XY table model with indexed range queries and batched appends.
 * Column 0 holds X, column 1 holds Y, and vertical headers hold point labels.
 * Build ChartData on a worker thread if needed, then call QObject methods such as
 * setDataSnapshot() on the model's own thread.
 * zh_CN: 外部管理的 XY 表格模型，支持索引范围查询和批量追加（视图仅引用而不接管所有权）。
 * 第 0 列为 X，第 1 列为 Y，垂直表头为点标签；可在工作线程构建 ChartData，再回到模型所在线程调用 setDataSnapshot() 等接口。
 */
class ChartModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
public:
    explicit ChartModel(QObject* parent = nullptr);
    ~ChartModel() override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QString name() const;
    void setName(const QString& name);
    /** @brief Replaces the model with a pre-indexed snapshot without rescanning. zh_CN: 发布已索引的数据快照，无需重新扫描。 */
    bool setDataSnapshot(const ChartData& snapshot);
    ChartData dataSnapshot() const;
    /**
     * @brief Synchronous helper for small datasets; build a ChartData snapshot off-thread for large ones.
     * zh_CN: 适用于小数据量的同步便捷方法；大数据量请先在后台构建 ChartData 快照。
     */
    bool setPoints(const QVector<QPointF>& points, const QStringList& labels = QStringList());
    /**
     * @brief Appends sorted points in one batch, emitting a single rowsInserted and updating the index incrementally.
     * Copy-on-write detaches if external snapshots are still held, so avoid holding snapshots while streaming;
     * trim unbounded history by publishing a bounded window from the application.
     * zh_CN: 批量追加有序数据点，仅触发一次 rowsInserted 并增量更新索引。
     * 若外部仍持有旧快照，写入会触发写时分离；流式追加时应避免长期持有快照，无限历史请由外部按滑动窗口裁剪后发布。
     */
    bool appendPoints(const QVector<QPointF>& points, const QStringList& labels = QStringList());
    void clear();

    virtual QPointF pointAt(int row) const;
    QString labelAt(int row) const;
    virtual QVector<int> sampledRows(int first, int last, int budget) const;
    int lowerBound(double x) const;
    int upperBound(double x) const;
    double minimumX() const;
    double maximumX() const;
    double minimumY() const;
    double maximumY() const;
    double mean(int first, int last) const;
    double positiveFraction(int first, int last) const;

signals:
    void nameChanged(const QString& name);

private:
    ChartData m_data;
    QString m_name;
    bool m_mutating = false;
};

} // namespace fluent::charts

Q_DECLARE_METATYPE(fluent::charts::ChartData)

#endif
