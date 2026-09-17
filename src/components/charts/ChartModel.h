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
 * @brief An implicitly shared, indexed XY snapshot; safe to prepare away from the GUI thread.
 * zh_CN: 隐式共享并带索引的 XY 数据快照，可在 GUI 线程之外预构建。
 * X must be finite and nondecreasing; a NaN Y marks a gap. Labels are optional.
 * Construction is O(n). Copies share storage; projections never copy the full data set.
 * zh_CN: X 必须有限且非递减；NaN Y 表示断点。标签可选。构建 O(n)，复制共享存储。
 */
class ChartData {
public:
    ChartData();
    ChartData(const ChartData& other);
    ChartData& operator=(const ChartData& other);
    ~ChartData();

    static ChartData fromPoints(const QVector<QPointF>& points,
                                const QStringList& labels = QStringList());
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
     * @brief Returns at most budget source rows, preserving bucket endpoints and extrema.
     * zh_CN: 返回最多 budget 个源数据行，保留桶边界和极值；-1 表示断点。
     * The half-open range is clamped. -1 separates gaps. Work is bounded by the budget
     * and index depth, not total history. Budgets below sixteen produce an empty result.
     * zh_CN: 范围为左闭右开；查询量受预算和索引深度约束，不随完整历史逐点扫描。
     */
    QVector<int> sampledRows(int first, int last, int budget) const;
    /** @brief Mean of finite values in a half-open range. zh_CN: 左闭右开范围内有限值的均值。 */
    double mean(int first, int last) const;
    /** @brief Positive contribution to the complete series, in [0, 1]. zh_CN: 占全序列正值总量的比例。 */
    double positiveFraction(int first, int last) const;

private:
    friend class ChartModel;
    QSharedDataPointer<ChartDataPrivate> d;
};

/**
 * @brief Caller-owned XY table model with indexed range queries and batched appends.
 * zh_CN: 由调用方持有的 XY 表格模型，支持索引查询和批量追加。
 * Views borrow this model. Column 0 is X, column 1 is Y; vertical headers use labels.
 * QObject operations belong on the model's thread. Prepare ChartData on a worker,
 * then publish it with setDataSnapshot() on the model's thread.
 * zh_CN: 视图借用模型；第 0 列是 X，第 1 列是 Y。工作线程预构建快照，在模型线程发布。
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
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QString name() const;
    void setName(const QString& name);
    /** @brief Publishes an indexed snapshot without rescanning it. zh_CN: 发布已索引快照，不重扫数据。 */
    bool setDataSnapshot(const ChartData& snapshot);
    ChartData dataSnapshot() const;
    /** @brief Synchronous convenience for small data; use a prepared snapshot for large data.
     * zh_CN: 小数据的同步便捷入口；大数据请预构建快照。 */
    bool setPoints(const QVector<QPointF>& points, const QStringList& labels = QStringList());
    /**
     * @brief Appends a sorted batch with one rowsInserted signal and incremental index updates.
     * zh_CN: 追加有序批次，仅发一次 rowsInserted，并增量更新索引。
     * Existing retained snapshots detach once; avoid retaining snapshots during streaming.
     * Retention belongs to the caller; publish a bounded window when history is unbounded.
     * zh_CN: 保留旧快照时首次写入会分离存储；流式追加避免持有快照，历史保留策略由调用方决定。
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
