#include "ChartModel.h"

#include <QSharedData>
#include <QPointer>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fluent::charts {
namespace {
constexpr int BlockSize = 64;
double missing()
{
    return std::numeric_limits<double>::quiet_NaN();
}

struct Summary {
    int minimum = -1;
    int maximum = -1;
    int count = 0;
    bool gap = false;
    double scale = 0;
    double sum = 0;
    double positive = 0;
};

QString validate(const QVector<QPointF>& points, const QStringList& labels,
                 double previous = -std::numeric_limits<double>::infinity())
{
    if (points.size() > std::numeric_limits<int>::max())
        return QStringLiteral("Point count exceeds the Qt model row limit.");
    if (!labels.isEmpty() && labels.size() != points.size())
        return QStringLiteral("Labels must be empty or match the point count.");
    for (const auto& point : points) {
        if (!std::isfinite(point.x()) || point.x() < previous || std::isinf(point.y()))
            return QStringLiteral("X must be finite and sorted; Y must be finite or NaN.");
        previous = point.x();
    }
    return {};
}
} // namespace

class ChartDataPrivate : public QSharedData {
public:
    QVector<QPointF> points;
    QStringList labels;
    QVector<QVector<Summary>> levels;
    QString error;

    Summary merge(const Summary& a, const Summary& b) const
    {
        Summary out;
        out.count = a.count + b.count;
        out.gap = a.gap || b.gap;
        out.minimum = a.minimum;
        out.maximum = a.maximum;
        if (b.minimum >= 0 && (out.minimum < 0 || points[b.minimum].y() < points[out.minimum].y()))
            out.minimum = b.minimum;
        if (b.maximum >= 0 && (out.maximum < 0 || points[b.maximum].y() > points[out.maximum].y()))
            out.maximum = b.maximum;
        out.scale = std::max(a.scale, b.scale);
        if (out.scale > 0) {
            out.sum = a.sum * (a.scale / out.scale) + b.sum * (b.scale / out.scale);
            out.positive = a.positive * (a.scale / out.scale) + b.positive * (b.scale / out.scale);
        }
        return out;
    }

    Summary scan(int first, int last) const
    {
        Summary out;
        for (int row = first; row < last; ++row) {
            Summary next;
            const double y = points[row].y();
            next.gap = std::isnan(y);
            if (!next.gap) {
                next.minimum = next.maximum = row;
                next.count = 1;
                next.scale = std::abs(y);
                next.sum = y > 0 ? 1 : (y < 0 ? -1 : 0);
                next.positive = y > 0 ? 1 : 0;
            }
            out = merge(out, next);
        }
        return out;
    }

    void rebuildTail(int first)
    {
        const int blocks = (points.size() + BlockSize - 1) / BlockSize;
        if (!blocks) {
            levels.clear();
            return;
        }
        if (levels.isEmpty())
            levels.append(QVector<Summary>());
        levels[0].resize(blocks);
        int start = first / BlockSize;
        for (int block = start; block < blocks; ++block)
            levels[0][block] =
                scan(block * BlockSize, std::min((block + 1) * BlockSize, int(points.size())));
        int level = 1;
        while (levels[level - 1].size() > 1) {
            if (levels.size() <= level)
                levels.append(QVector<Summary>());
            const int count = (levels[level - 1].size() + 1) / 2;
            const int oldCount = levels[level].size();
            levels[level].resize(count);
            start = std::min(start / 2, oldCount);
            for (int i = start; i < count; ++i) {
                const auto& children = levels[level - 1];
                levels[level][i] = merge(
                    children[i * 2], i * 2 + 1 < children.size() ? children[i * 2 + 1] : Summary());
            }
            ++level;
        }
        levels.resize(level);
    }

    Summary range(int first, int last) const
    {
        first = std::clamp(first, 0, int(points.size()));
        last = std::clamp(last, first, int(points.size()));
        Summary out;
        const int aligned = std::min(last, ((first + BlockSize - 1) / BlockSize) * BlockSize);
        out = scan(first, aligned);
        first = aligned;
        while (last - first >= BlockSize) {
            int level = 0;
            qint64 span = BlockSize;
            while (level + 1 < levels.size() && first % (span * 2) == 0 &&
                   span * 2 <= last - first) {
                ++level;
                span *= 2;
            }
            out = merge(out, levels[level][int(first / span)]);
            first += int(span);
        }
        return merge(out, scan(first, last));
    }

    Summary all() const { return levels.isEmpty() ? Summary() : levels.last().first(); }
};

ChartData::ChartData() : d(new ChartDataPrivate) {}
ChartData::ChartData(const ChartData&) = default;
ChartData& ChartData::operator=(const ChartData&) = default;
ChartData::~ChartData() = default;

ChartData ChartData::fromPoints(const QVector<QPointF>& points, const QStringList& labels)
{
    ChartData result;
    result.d->error = validate(points, labels);
    if (result.d->error.isEmpty()) {
        result.d->points = points;
        result.d->labels = labels;
        result.d->rebuildTail(0);
    }
    return result;
}
bool ChartData::isValid() const
{
    return d->error.isEmpty();
}
QString ChartData::errorString() const
{
    return d->error;
}
int ChartData::size() const
{
    return d->points.size();
}
QPointF ChartData::pointAt(int row) const
{
    return row >= 0 && row < size() ? d->points[row] : QPointF(missing(), missing());
}
QString ChartData::labelAt(int row) const
{
    return row >= 0 && row < d->labels.size() ? d->labels[row] : QString();
}
double ChartData::minimumX() const
{
    return size() ? d->points.first().x() : 0;
}
double ChartData::maximumX() const
{
    return size() ? d->points.last().x() : 0;
}
double ChartData::minimumY() const
{
    const int row = d->all().minimum;
    return row >= 0 ? d->points[row].y() : 0;
}
double ChartData::maximumY() const
{
    const int row = d->all().maximum;
    return row >= 0 ? d->points[row].y() : 0;
}
int ChartData::lowerBound(double x) const
{
    return int(std::lower_bound(d->points.cbegin(), d->points.cend(), x,
                                [](const QPointF& p, double value) { return p.x() < value; }) -
               d->points.cbegin());
}
int ChartData::upperBound(double x) const
{
    return int(std::upper_bound(d->points.cbegin(), d->points.cend(), x,
                                [](double value, const QPointF& p) { return value < p.x(); }) -
               d->points.cbegin());
}

QVector<int> ChartData::sampledRows(int first, int last, int budget) const
{
    QVector<int> rows;
    first = std::clamp(first, 0, size());
    last = std::clamp(last, first, size());
    if (budget < 16 || first == last)
        return rows;
    if (last - first <= budget) {
        rows.reserve(last - first);
        for (int i = first; i < last; ++i)
            rows.append(std::isfinite(d->points[i].y()) ? i : -1);
        return rows;
    }
    // Reserve separators around all four representatives; never bridge missing data.
    const int buckets = std::max(1, budget / 10);
    qint64 span = BlockSize;
    while ((last - 1) / span - first / span + 1 > buckets)
        span *= 2;
    rows.reserve(buckets * 10);
    for (qint64 begin = (first / span) * span; begin < last; begin += span) {
        const int lo = int(std::max<qint64>(begin, first));
        const int hi = int(std::min<qint64>(begin + span, last));
        const Summary summary = d->range(lo, hi);
        QVector<int> candidates{lo, summary.minimum, summary.maximum, hi - 1};
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        for (int row : candidates) {
            if (row < 0 || !std::isfinite(d->points[row].y()))
                continue;
            if (summary.gap)
                rows.append(-1);
            rows.append(row);
        }
        if (summary.gap && (rows.isEmpty() || rows.last() != -1))
            rows.append(-1);
    }
    return rows;
}
double ChartData::mean(int first, int last) const
{
    const Summary s = d->range(first, last);
    return s.count ? (s.sum / s.count) * s.scale : missing();
}
double ChartData::positiveFraction(int first, int last) const
{
    const Summary total = d->all();
    if (total.positive <= 0)
        return 0;
    const Summary part = d->range(first, last);
    return (part.scale / total.scale) * (part.positive / total.positive);
}

ChartModel::ChartModel(QObject* parent) : QAbstractTableModel(parent) {}
ChartModel::~ChartModel() = default;
int ChartModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_data.size();
}
int ChartModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 2;
}
QVariant ChartModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || index.row() >= rowCount() ||
        index.column() >= 2 || (role != Qt::DisplayRole && role != Qt::EditRole))
        return {};
    const auto point = pointAt(index.row());
    return index.column() == 0 ? point.x() : point.y();
}
QVariant ChartModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Horizontal)
        return section == 0 ? QStringLiteral("X") : (section == 1 ? m_name : QString());
    return labelAt(section);
}
QString ChartModel::name() const
{
    return m_name;
}
void ChartModel::setName(const QString& name)
{
    if (QThread::currentThread() != thread() || name == m_name)
        return;
    m_name = name;
    QPointer<ChartModel> guard(this);
    emit headerDataChanged(Qt::Horizontal, 1, 1);
    if (!guard)
        return;
    emit nameChanged(name);
}
bool ChartModel::setDataSnapshot(const ChartData& snapshot)
{
    if (QThread::currentThread() != thread() || m_mutating || !snapshot.isValid())
        return false;
    if (m_data.d.constData() == snapshot.d.constData())
        return true;
    m_mutating = true;
    QPointer<ChartModel> guard(this);
    beginResetModel();
    if (!guard)
        return false;
    m_data = snapshot;
    endResetModel();
    if (guard)
        m_mutating = false;
    return true;
}
ChartData ChartModel::dataSnapshot() const
{
    return m_data;
}
bool ChartModel::setPoints(const QVector<QPointF>& points, const QStringList& labels)
{
    if (QThread::currentThread() != thread())
        return false;
    return setDataSnapshot(ChartData::fromPoints(points, labels));
}
bool ChartModel::appendPoints(const QVector<QPointF>& points, const QStringList& labels)
{
    if (QThread::currentThread() != thread() || m_mutating)
        return false;
    if (points.isEmpty())
        return labels.isEmpty();
    if (qint64(rowCount()) + points.size() > std::numeric_limits<int>::max() ||
        !validate(points, labels,
                  rowCount() ? maximumX() : -std::numeric_limits<double>::infinity())
             .isEmpty())
        return false;
    const int first = rowCount();
    m_mutating = true;
    QPointer<ChartModel> guard(this);
    beginInsertRows(QModelIndex(), first, first + points.size() - 1);
    if (!guard)
        return false;
    auto* storage = m_data.d.data();
    storage->points += points;
    if (!storage->labels.isEmpty() || !labels.isEmpty()) {
        while (storage->labels.size() < first)
            storage->labels.append(QString());
        if (labels.isEmpty()) {
            for (int i = 0; i < points.size(); ++i)
                storage->labels.append(QString());
        } else {
            storage->labels += labels;
        }
    }
    storage->rebuildTail(first);
    endInsertRows();
    if (guard)
        m_mutating = false;
    return true;
}
void ChartModel::clear()
{
    if (rowCount())
        setDataSnapshot(ChartData());
}
QPointF ChartModel::pointAt(int row) const
{
    return m_data.pointAt(row);
}
QString ChartModel::labelAt(int row) const
{
    return m_data.labelAt(row);
}
QVector<int> ChartModel::sampledRows(int first, int last, int budget) const
{
    return m_data.sampledRows(first, last, budget);
}
int ChartModel::lowerBound(double x) const
{
    return m_data.lowerBound(x);
}
int ChartModel::upperBound(double x) const
{
    return m_data.upperBound(x);
}
double ChartModel::minimumX() const
{
    return m_data.minimumX();
}
double ChartModel::maximumX() const
{
    return m_data.maximumX();
}
double ChartModel::minimumY() const
{
    return m_data.minimumY();
}
double ChartModel::maximumY() const
{
    return m_data.maximumY();
}
double ChartModel::mean(int first, int last) const
{
    return m_data.mean(first, last);
}
double ChartModel::positiveFraction(int first, int last) const
{
    return m_data.positiveFraction(first, last);
}

} // namespace fluent::charts
