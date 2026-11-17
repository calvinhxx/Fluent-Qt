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
 * @brief Implicitly shared XY data snapshot that can be built off the GUI thread.
 * X must be finite and nondecreasing, and NaN Y marks a gap.
 * zh_CN: 可在工作线程构建的共享 XY 数据快照。
 * X 须为有限非递减值，Y 为 NaN 表示断点，拷贝共享底层数据。
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
     * @brief Samples up to budget row indices in [first, last), keeping endpoints and extrema.
     * Returns empty for budgets under 16; -1 marks a gap.
     * zh_CN: 在区间 [first, last) 内按上限 budget 采样行号，保留端点与极值（-1 表示断点，budget 小于 16 时返回空）。
     */
    QVector<int> sampledRows(int first, int last, int budget) const;
    /** @brief Mean of finite Y values in [first, last). zh_CN: 区间 [first, last) 内有限 Y 值的均值。 */
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
 * @brief Indexed XY table model with range queries and batched appends.
 * Column 0 holds X, column 1 holds Y, and vertical headers hold labels; build ChartData off-thread and call setDataSnapshot() on the model's thread.
 * zh_CN: 支持索引查询和批量追加的 XY 表格模型。
 * 第 0 列为 X、第 1 列为 Y、垂直表头为点标签；大数据量可在工作线程构建 ChartData，再回模型线程调用 setDataSnapshot()。
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
     * @brief Appends sorted points in one batch and emits a single rowsInserted.
     * Avoid holding old ChartData snapshots while streaming so storage updates in place.
     * zh_CN: 批量追加有序数据点并触发一次 rowsInserted，流式场景避免长期保留旧快照。
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
