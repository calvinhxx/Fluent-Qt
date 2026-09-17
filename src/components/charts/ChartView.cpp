#include "ChartView.h"

#include <QAccessibleWidget>
#include <QApplication>
#include <QFocusEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

#include "ChartRenderer_p.h"
#include "ChartReadout_p.h"
#include "design/ChartTokens_p.h"
#include "design/ThemeColors.h"
#include "design/Typography.h"

namespace fluent::charts {
namespace {
double mix(double a, double b, double t)
{
    return a * (1 - t) + b * t;
}
double normalized(double value, double lo, double hi)
{
    const double scale = std::max({std::abs(lo), std::abs(hi), 1.0});
    const double span = hi / scale - lo / scale;
    return span > 0 ? (value / scale - lo / scale) / span : 0.5;
}
void expand(double& lo, double& hi)
{
    if (lo != hi)
        return;
    const double delta = std::max(std::abs(lo) * 0.05, 1.0);
    const double a = lo - delta, b = hi + delta;
    if (std::isfinite(a))
        lo = a;
    if (std::isfinite(b))
        hi = b;
}
QString number(const QWidget* widget, double value)
{
    return widget->locale().toString(value, 'g', 5);
}

double luminance(const QColor& color)
{
    const auto linear = [](double c) {
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
}

QColor dataColor(QColor source, const FluentElement::Colors& colors)
{
    // Keep the token hue, but make data strokes legible on Fluent neutral surfaces.
    const QColor target =
        luminance(colors.bgLayerAlt) > 0.18 ? QColor(Qt::black) : QColor(Qt::white);
    source.setAlpha(255);
    for (int step = 0; step <= 32; ++step) {
        const double t = step / 32.0;
        const auto candidate = QColor::fromRgbF(mix(source.redF(), target.redF(), t),
                                                mix(source.greenF(), target.greenF(), t),
                                                mix(source.blueF(), target.blueF(), t));
        const double foreground = luminance(candidate) + 0.05;
        bool readable = true;
        for (const QColor& background :
             {colors.bgCanvas, colors.bgLayer, colors.bgLayerAlt, colors.bgSolid}) {
            const double surface = luminance(background) + 0.05;
            readable &= std::max(foreground, surface) / std::min(foreground, surface) >= 3.1;
        }
        if (readable)
            return candidate;
    }
    return target;
}

class ChartAccessible final : public QAccessibleWidget {
public:
    explicit ChartAccessible(ChartView* view) : QAccessibleWidget(view, QAccessible::Chart) {}
    QString text(QAccessible::Text type) const override
    {
        const auto* view = static_cast<ChartView*>(widget());
        if (!view)
            return {};
        if (type == QAccessible::Name && view->accessibleName().isEmpty())
            return view->title();
        if (type == QAccessible::Value)
            return view->currentPointText();
        if (type == QAccessible::Description && view->accessibleDescription().isEmpty()) {
            qint64 count = 0;
            for (int i = 0; i < view->seriesCount(); ++i)
                count += view->seriesModel(i)->rowCount();
            return ChartView::tr("%1 series, %2 data points. Left/Right: point; Up/Down: series; "
                                 "Enter: activate.")
                .arg(view->seriesCount())
                .arg(count);
        }
        return QAccessibleWidget::text(type);
    }
    QStringList actionNames() const override
    {
        auto result = QAccessibleWidget::actionNames();
        if (widget() && widget()->isEnabled())
            result << nextPageAction() << previousPageAction();
        return result;
    }
    void doAction(const QString& action) override
    {
        auto* view = static_cast<ChartView*>(widget());
        if (!view || !view->isEnabled())
            return;
        if (action == nextPageAction() || action == previousPageAction()) {
            const int series = std::max(0, view->currentSeries());
            auto* model = view->seriesModel(series);
            if (model && model->rowCount())
                view->setCurrentPoint(
                    series, std::clamp(view->currentRow() + (action == nextPageAction() ? 1 : -1),
                                       0, model->rowCount() - 1));
        } else
            QAccessibleWidget::doAction(action);
    }
};
QAccessibleInterface* chartAccessibleFactory(const QString&, QObject* object)
{
    if (auto* chart = qobject_cast<ChartView*>(object))
        return new ChartAccessible(chart);
    return nullptr;
}
} // namespace

class ChartViewPrivate {
public:
    explicit ChartViewPrivate(ChartView* owner) : q(owner), renderer(detail::makeLineRenderer())
    {
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, q, [this] {
            ready = true;
            q->update();
        });
    }
    ~ChartViewPrivate() { delete readout.data(); }
    struct Series {
        QPointer<ChartModel> model;
        QVector<QMetaObject::Connection> connections;
    };
    ChartView* q;
    QVector<Series> series;
    QVector<detail::Projection> projections;
    QVector<QColor> dataColors;
    std::unique_ptr<detail::ChartRenderer> renderer;
    QPointer<detail::ChartReadout> readout;
    QTimer timer;
    ChartView::ChartType type = ChartView::Line;
    QString title, subtitle, suffix, centerText, centerCaption;
    bool fixedType = false, legend = true, automatic = true, autoY = true;
    bool dirty = true, ready = true, loading = false;
    bool keyboardFocusVisible = false;
    bool currentReadoutVisible = false;
    bool readoutSyncPending = false;
    double xMin = 0, xMax = 1, yMin = 0, yMax = 1;
    double requestedMin = 0, requestedMax = 1, requestedYMin = 0, requestedYMax = 1;
    qreal dpr = 0, headerBottom = 24;
    int maximumPoints = 4096, frameRate = 30;
    int currentSeries = -1, currentRow = -1, hoverSeries = -1, hoverRow = -1, hoverLast = -1;
    int projectedPoints = 0;
    quint64 builds = 0;
    QRectF plot, legendBox;
    QPointF hoverPosition;
    bool circular() const { return renderer->layout() == detail::Layout::Radial; }
    bool bars() const { return renderer->categories(); }
    bool horizontal() const { return renderer->layout() == detail::Layout::Horizontal; }
    bool compact() const { return renderer->layout() == detail::Layout::Compact; }
    void clearHover() { hoverSeries = hoverRow = hoverLast = -1; }
    void dismissReadout()
    {
        clearHover();
        currentReadoutVisible = false;
        if (readout)
            readout->close();
    }
    void queueReadoutSync()
    {
        if (readoutSyncPending)
            return;
        readoutSyncPending = true;
        // Never create or reposition child widgets inside a paint event.
        QTimer::singleShot(0, q, [this] {
            readoutSyncPending = false;
            syncReadout();
        });
    }
    void selectRenderer(ChartView::ChartType value)
    {
        using namespace detail;
        // This is the only type dispatch. Geometry, reduction and paint live in
        // the individual chart translation units, never in the shared host.
        switch (value) {
        case ChartView::Line:
            renderer = makeLineRenderer();
            break;
        case ChartView::Area:
            renderer = makeAreaRenderer();
            break;
        case ChartView::Bar:
            renderer = makeBarRenderer();
            break;
        case ChartView::HorizontalBar:
            renderer = makeHorizontalBarRenderer();
            break;
        case ChartView::Pie:
            renderer = makePieRenderer();
            break;
        case ChartView::Donut:
            renderer = makeDonutRenderer();
            break;
        case ChartView::Scatter:
            renderer = makeScatterRenderer();
            break;
        case ChartView::Sparkline:
            renderer = makeSparklineRenderer();
            break;
        }
        type = value;
    }
    void invalidate(bool immediate = false)
    {
        dirty = true;
        dismissReadout();
        q->updateGeometry();
        if (immediate) {
            ready = true;
            q->update();
        } else if (q->isVisible() && !timer.isActive())
            timer.start(std::max(1, 1000 / frameRate));
    }
    QColor color(int i) const
    {
        return dataColors.isEmpty() ? q->themeColorsRef().accentDefault
                                    : dataColors[i % dataColors.size()];
    }
    QPointF map(double x, double y) const
    {
        return {plot.left() + normalized(x, xMin, xMax) * plot.width(),
                plot.bottom() - normalized(y, yMin, yMax) * plot.height()};
    }
    QPair<double, double> xBounds() const
    {
        if (!automatic)
            return {requestedMin, requestedMax};
        double lo = 0, hi = 1;
        int count = 0;
        for (const auto& entry : series) {
            if (!entry.model || !entry.model->rowCount())
                continue;
            if (!count) {
                lo = entry.model->minimumX();
                hi = entry.model->maximumX();
            } else {
                lo = std::min(lo, entry.model->minimumX());
                hi = std::max(hi, entry.model->maximumX());
            }
            count = std::max(count, entry.model->rowCount());
        }
        if (bars() && count > 1) {
            const double padding = hi / (2.0 * (count - 1)) - lo / (2.0 * (count - 1));
            if (std::isfinite(lo - padding))
                lo -= padding;
            if (std::isfinite(hi + padding))
                hi += padding;
        }
        expand(lo, hi);
        return {lo, hi};
    }
    QPair<double, double> yBounds() const
    {
        if (!autoY)
            return {requestedYMin, requestedYMax};
        double yMin = 0, yMax = 1;
        bool first = true;
        for (const auto& entry : series) {
            if (!entry.model || !entry.model->rowCount())
                continue;
            if (first) {
                yMin = entry.model->minimumY();
                yMax = entry.model->maximumY();
                first = false;
            } else {
                yMin = std::min(yMin, entry.model->minimumY());
                yMax = std::max(yMax, entry.model->maximumY());
            }
        }
        if (renderer->includesZero()) {
            yMin = std::min(0.0, yMin);
            yMax = std::max(0.0, yMax);
        }
        expand(yMin, yMax);
        if (!compact() && !circular()) {
            const double span = yMax - yMin;
            if (std::isfinite(span) && span > 0) {
                const double magnitude = std::pow(10.0, std::floor(std::log10(span / 4)));
                if (magnitude > 0 && std::isfinite(magnitude)) {
                    const double raw = span / (4 * magnitude);
                    const double step = (raw <= 1   ? 1
                                         : raw <= 2 ? 2
                                         : raw <= 5 ? 5
                                                    : 10) *
                                        magnitude;
                    const double lo = std::floor(yMin / step) * step,
                                 hi = std::ceil(yMax / step) * step;
                    if (std::isfinite(lo) && std::isfinite(hi) && hi > lo) {
                        yMin = lo;
                        yMax = hi;
                    }
                }
            }
        }
        return {yMin, yMax};
    }
    void bounds()
    {
        const auto x = xBounds(), y = yBounds();
        xMin = x.first;
        xMax = x.second;
        yMin = y.first;
        yMax = y.second;
    }
    void layout()
    {
        const qreal width = q->width(), height = q->height();
        headerBottom = 24;
        if (!title.isEmpty())
            headerBottom += 28;
        if (!subtitle.isEmpty())
            headerBottom += (title.isEmpty() ? 0 : 4) + 16;
        legendBox = {};
        if (compact()) {
            plot = QRectF(q->rect()).adjusted(4, 4, -4, -4);
            return;
        }
        qreal top = headerBottom + 16;
        if (circular()) {
            const int count = q->model() ? std::min(12, q->model()->rowCount()) : 0;
            const qreal contentHeight = std::max(32.0, height - top - 24);
            if (legend && count && width >= 520) {
                const qreal side = std::max(32.0, std::min({236.0, contentHeight, width * .43}));
                plot = QRectF(40, top + (contentHeight - side) / 2, side, side);
                legendBox = QRectF(plot.right() + 36,
                                   top + std::max(0.0, (contentHeight - (32 + count * 36)) / 2),
                                   std::max(32.0, width - plot.right() - 60),
                                   std::min(contentHeight, 32.0 + count * 36));
            } else if (legend && count) {
                const qreal legendHeight = std::min(32.0 + count * 32, contentHeight * .48);
                const qreal side = std::max(
                    24.0, std::min({236.0, width - 48, contentHeight - legendHeight - 20}));
                plot = QRectF((width - side) / 2, top, side, side);
                legendBox = QRectF(24, top + side + 20, width - 48, legendHeight);
            } else {
                const qreal side = std::max(24.0, std::min(width - 48, contentHeight));
                plot = QRectF((width - side) / 2, top + (contentHeight - side) / 2, side, side);
            }
        } else {
            if (legend && !series.isEmpty() && !horizontal()) {
                const int columns = std::max(1, int((width - 48) / 130));
                const int rows = (series.size() + columns - 1) / columns;
                // Leave a readable plot even for the maximum 32 series. The
                // final legend slot summarizes overflow; all series remain accessible.
                const int visibleRows = std::min(rows, 2);
                legendBox = QRectF(24, top, width - 48, visibleRows * 24 - 4);
                top = legendBox.bottom() + 16;
            }
            plot = horizontal() ? QRectF(120, top, std::max(24.0, width - 200),
                                         std::max(24.0, height - top - 24))
                                : QRectF(64, top + 18, std::max(24.0, width - 88),
                                         std::max(24.0, height - top - 60));
        }
        plot = plot.intersected(QRectF(q->rect()).adjusted(4, 4, -4, -4));
    }
    detail::Context context(int s) const
    {
        const auto& colors = q->themeColorsRef();
        const bool high = q->effectiveTheme() == FluentElement::HighContrast;
        const bool dark = q->effectiveTheme() == FluentElement::Dark;
        const int budget = std::max(16, std::min(maximumPoints / std::max(1, int(series.size())),
                                                 int(plot.width() * dpr * 2)));
        return {plot,
                xMin,
                xMax,
                yMin,
                yMax,
                dpr,
                budget,
                s,
                std::max(1, int(series.size())),
                dataColors,
                colors.bgLayer,
                high ? colors.strokeDivider : tokens::grid(dark),
                high};
    }
    void project()
    {
        bounds();
        layout();
        dpr = q->devicePixelRatioF();
        const auto& colors = q->themeColorsRef();
        dataColors.clear();
        auto palette =
            tokens::series(q->effectiveTheme() == FluentElement::Dark, colors.accentDefault);
        if (q->effectiveTheme() == FluentElement::HighContrast)
            palette = colors.charts.toVector();
        for (const auto& color : palette)
            dataColors.append(dataColor(color, colors));
        for (int i = 4; i < colors.charts.size(); ++i)
            dataColors.append(dataColor(colors.charts[i], colors));
        if (dataColors.isEmpty())
            dataColors.append(dataColor(colors.accentDefault, colors));
        projections.clear();
        projections.resize(series.size());
        projectedPoints = 0;
        if (!loading && plot.width() > 1 && plot.height() > 1) {
            for (int s = 0; s < series.size(); ++s) {
                const auto* model = series[s].model.data();
                if (!model || !model->rowCount())
                    continue;
                projections[s] = renderer->project(context(s), *model);
                projectedPoints += projections[s].size();
            }
        }
        dirty = false;
        ready = false;
        ++builds;
        if (currentReadoutVisible)
            queueReadoutSync();
    }
    QString rangeText(int seriesIndex, int first, int last) const
    {
        const auto* m = q->seriesModel(seriesIndex);
        if (!m || first < 0 || first >= m->rowCount())
            return {};
        if (last - first > 1)
            return circular() ? ChartView::tr("Other (%1 points): %2%")
                                    .arg(last - first)
                                    .arg(number(q, m->positiveFraction(first, last) * 100))
                              : ChartView::tr("%1–%2: mean %3%4")
                                    .arg(number(q, m->pointAt(first).x()),
                                         number(q, m->pointAt(last - 1).x()),
                                         number(q, m->mean(first, last)), suffix);
        const auto point = m->pointAt(first);
        const QString label = m->labelAt(first);
        return (m->name().isEmpty() ? QString() : m->name() + QStringLiteral(" · ")) +
               (label.isEmpty() ? number(q, point.x()) : label) + QStringLiteral(": ") +
               number(q, point.y()) + suffix;
    }
    bool hit(const QPointF& position, int& s, int& first, int& last) const
    {
        if (loading || !plot.contains(position))
            return false;
        detail::Hit best;
        if (renderer->sharedCursor())
            best.distance = std::numeric_limits<double>::max();
        for (int i = 0; i < projections.size(); ++i) {
            auto candidate = best;
            renderer->hit(position, projections[i], candidate);
            if (candidate.distance < best.distance) {
                best = candidate;
                best.series = i;
            }
        }
        s = best.series;
        first = best.first;
        last = best.last;
        return s >= 0;
    }
    QString category(const detail::Shape& shape) const
    {
        const auto* model = q->model();
        if (!model)
            return {};
        if (shape.last - shape.first > 1)
            return bars() ? ChartView::tr("Rows %1–%2").arg(shape.first + 1).arg(shape.last)
                          : ChartView::tr("Other");
        const auto label = model->labelAt(shape.first);
        return label.isEmpty() ? number(q, model->pointAt(shape.first).x()) : label;
    }
    void paintAxes(QPainter& p) const;
    void paintLegend(QPainter& p) const;
    struct Cursor {
        QPointF anchor;
        QPainterPath shape;
        QString heading;
        QVector<detail::ReadoutRow> rows;
        QVector<QPointF> positions;
    };
    Cursor cursor() const;
    void paintCursor(QPainter& p) const;
    void syncReadout();
};

ChartView::ChartView(QWidget* parent) : QWidget(parent), d(new ChartViewPrivate(this))
{
    static const bool installed = [] {
        QAccessible::installFactory(chartAccessibleFactory);
        return true;
    }();
    Q_UNUSED(installed)
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}
ChartView::ChartView(ChartType type, QWidget* parent) : ChartView(parent)
{
    d->selectRenderer(type);
    d->fixedType = true;
}
ChartView::~ChartView()
{
    // QWidget may delete model children after our private state is gone.
    for (const auto& entry : d->series)
        for (const auto& connection : entry.connections)
            disconnect(connection);
}
ChartModel* ChartView::model() const
{
    return seriesModel(0);
}
int ChartView::seriesCount() const
{
    return d->series.size();
}
ChartModel* ChartView::seriesModel(int series) const
{
    return series >= 0 && series < d->series.size() ? d->series[series].model.data() : nullptr;
}
void ChartView::setModel(ChartModel* model)
{
    if (model && model->thread() != thread())
        return;
    if (seriesCount() == (model ? 1 : 0) && this->model() == model)
        return;
    QPointer<ChartView> guard(this);
    QPointer<ChartModel> modelGuard(model);
    clearSeries();
    if (guard && modelGuard)
        addSeries(modelGuard);
}
bool ChartView::addSeries(ChartModel* model)
{
    if (!model || model->thread() != thread())
        return false;
    for (const auto& entry : d->series)
        if (entry.model == model)
            return true;
    if (seriesCount() >= 32)
        return false;
    ChartViewPrivate::Series entry;
    entry.model = model;
    auto changed = [this] { d->invalidate(); };
    entry.connections << connect(model, &QAbstractItemModel::rowsInserted, this, changed)
                      << connect(model, &QAbstractItemModel::modelReset, this,
                                 [this] {
                                     d->invalidate();
                                     setCurrentPoint(-1, -1);
                                 })
                      << connect(model, &QAbstractItemModel::dataChanged, this, changed)
                      << connect(model, &ChartModel::nameChanged, this, changed)
                      << connect(model, &QObject::destroyed, this, [this] {
                             for (int i = d->series.size() - 1; i >= 0; --i)
                                 if (d->series[i].model.isNull())
                                     d->series.removeAt(i);
                             d->projections.clear();
                             d->invalidate(true);
                             QPointer<ChartView> guard(this);
                             setCurrentPoint(-1, -1);
                             if (guard)
                                 emit modelChanged();
                         });
    d->series.append(entry);
    d->invalidate(true);
    emit modelChanged();
    return true;
}
bool ChartView::removeSeries(ChartModel* model)
{
    for (int i = 0; i < d->series.size(); ++i) {
        if (d->series[i].model != model)
            continue;
        for (const auto& connection : d->series[i].connections)
            disconnect(connection);
        d->series.removeAt(i);
        d->projections.clear();
        d->invalidate(true);
        QPointer<ChartView> guard(this);
        setCurrentPoint(-1, -1);
        if (guard)
            emit modelChanged();
        return true;
    }
    return false;
}
void ChartView::clearSeries()
{
    if (d->series.isEmpty())
        return;
    for (const auto& entry : d->series)
        for (const auto& connection : entry.connections)
            disconnect(connection);
    d->series.clear();
    d->projections.clear();
    d->invalidate(true);
    QPointer<ChartView> guard(this);
    setCurrentPoint(-1, -1);
    if (guard)
        emit modelChanged();
}
ChartView::ChartType ChartView::chartType() const
{
    return d->type;
}
void ChartView::setChartType(ChartType type)
{
    if (d->fixedType || type < Line || type > Sparkline || type == d->type)
        return;
    d->selectRenderer(type);
    updateGeometry();
    d->invalidate(true);
    emit chartTypeChanged(type);
}
QString ChartView::title() const
{
    return d->title;
}
void ChartView::setTitle(const QString& title)
{
    if (title == d->title)
        return;
    d->title = title;
    d->invalidate(true);
    emit titleChanged(title);
}
bool ChartView::isLegendVisible() const
{
    return d->legend;
}
void ChartView::setLegendVisible(bool visible)
{
    if (visible == d->legend)
        return;
    d->legend = visible;
    d->invalidate(true);
    emit legendVisibleChanged(visible);
}
int ChartView::maximumPointCount() const
{
    return d->maximumPoints;
}
void ChartView::setMaximumPointCount(int count)
{
    count = std::clamp(count, 512, 65536);
    if (count == d->maximumPoints)
        return;
    d->maximumPoints = count;
    d->invalidate(true);
    emit maximumPointCountChanged(count);
}
int ChartView::maximumFrameRate() const
{
    return d->frameRate;
}
void ChartView::setMaximumFrameRate(int rate)
{
    rate = std::clamp(rate, 1, 60);
    if (rate == d->frameRate)
        return;
    d->frameRate = rate;
    if (d->timer.isActive())
        d->timer.start(1000 / rate);
    emit maximumFrameRateChanged(rate);
}
bool ChartView::isAutoRange() const
{
    return d->automatic;
}
double ChartView::minimumX() const
{
    return d->xBounds().first;
}
double ChartView::maximumX() const
{
    return d->xBounds().second;
}
void ChartView::setXRange(double minimum, double maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum ||
        (!d->automatic && d->requestedMin == minimum && d->requestedMax == maximum))
        return;
    d->requestedMin = minimum;
    d->requestedMax = maximum;
    d->automatic = false;
    d->invalidate(true);
    emit xRangeChanged();
}
void ChartView::resetXRange()
{
    if (d->automatic)
        return;
    d->automatic = true;
    d->invalidate(true);
    emit xRangeChanged();
}
int ChartView::currentSeries() const
{
    return d->currentSeries;
}
int ChartView::currentRow() const
{
    return d->currentRow;
}
void ChartView::setCurrentPoint(int series, int row)
{
    const auto* m = seriesModel(series);
    if (!m || row < 0 || row >= m->rowCount()) {
        series = row = -1;
    }
    d->currentReadoutVisible = row >= 0;
    if (series == d->currentSeries && row == d->currentRow) {
        d->syncReadout();
        return;
    }
    d->currentSeries = series;
    d->currentRow = row;
    d->syncReadout();
    update();
    QPointer<ChartView> guard(this);
    QAccessibleValueChangeEvent event(this, currentPointText());
    QAccessible::updateAccessibility(&event);
    if (guard)
        emit currentPointChanged(series, row);
}
QString ChartView::currentPointText() const
{
    return d->rangeText(d->currentSeries, d->currentRow, d->currentRow + 1);
}
QRectF ChartView::plotRect() const
{
    return d->plot;
}
int ChartView::renderedPointCount() const
{
    return d->projectedPoints;
}
quint64 ChartView::projectionBuildCount() const
{
    return d->builds;
}
QSize ChartView::sizeHint() const
{
    return d->type == Sparkline ? QSize(220, 56) : QSize(676, 420);
}
bool ChartView::hasHeightForWidth() const
{
    return d->circular() && d->legend;
}
int ChartView::heightForWidth(int width) const
{
    if (!hasHeightForWidth() || width >= 520)
        return sizeHint().height();
    const int count = model() ? std::min(12, model()->rowCount()) : 0;
    return std::max(420, 24 + (d->title.isEmpty() ? 0 : 28) + (d->subtitle.isEmpty() ? 0 : 20) +
                             16 + std::min(236, std::max(24, width - 48)) + 20 + 32 + count * 36 +
                             24);
}
QSize ChartView::minimumSizeHint() const
{
    return d->type == Sparkline ? QSize(32, 20) : QSize(240, 220);
}
void ChartView::onThemeUpdated()
{
    if (d)
        d->invalidate(true);
}

QRectF ChartView::legendRect() const
{
    return d->legendBox;
}
QString ChartView::subtitle() const
{
    return d->subtitle;
}
void ChartView::setSubtitle(const QString& value)
{
    if (d->subtitle == value)
        return;
    d->subtitle = value;
    d->invalidate(true);
    emit subtitleChanged(value);
}
QString ChartView::valueSuffix() const
{
    return d->suffix;
}
void ChartView::setValueSuffix(const QString& value)
{
    if (d->suffix == value)
        return;
    d->suffix = value;
    d->invalidate(true);
    emit valueSuffixChanged(value);
}
QString ChartView::centerText() const
{
    return d->centerText;
}
void ChartView::setCenterText(const QString& value)
{
    if (d->centerText == value)
        return;
    d->centerText = value;
    d->invalidate(true);
    emit centerTextChanged(value);
}
QString ChartView::centerCaption() const
{
    return d->centerCaption;
}
void ChartView::setCenterCaption(const QString& value)
{
    if (d->centerCaption == value)
        return;
    d->centerCaption = value;
    d->invalidate(true);
    emit centerCaptionChanged(value);
}
bool ChartView::isLoading() const
{
    return d->loading;
}
void ChartView::setLoading(bool loading)
{
    if (d->loading == loading)
        return;
    d->loading = loading;
    d->invalidate(true);
    emit loadingChanged(loading);
}
bool ChartView::isAutoYRange() const
{
    return d->autoY;
}
double ChartView::minimumY() const
{
    return d->yBounds().first;
}
double ChartView::maximumY() const
{
    return d->yBounds().second;
}
void ChartView::setYRange(double lo, double hi)
{
    if (!std::isfinite(lo) || !std::isfinite(hi) || lo >= hi ||
        (!d->autoY && lo == d->requestedYMin && hi == d->requestedYMax))
        return;
    d->autoY = false;
    d->requestedYMin = lo;
    d->requestedYMax = hi;
    d->invalidate(true);
    emit yRangeChanged();
}
void ChartView::resetYRange()
{
    if (d->autoY)
        return;
    d->autoY = true;
    d->invalidate(true);
    emit yRangeChanged();
}

void ChartViewPrivate::paintAxes(QPainter& p) const
{
    if (circular() || compact() || plot.isEmpty())
        return;
    const auto& colors = q->themeColorsRef();
    p.setFont(q->themeFont(Typography::FontRole::Caption).toQFont());
    p.setPen(colors.textSecondary);
    if (horizontal()) {
        if (projections.isEmpty())
            return;
        for (const auto& shape : projections.first().shapes) {
            const auto y = shape.track.center().y();
            QString label = category(shape);
            if (shape.last - shape.first > 1)
                label = ChartView::tr("%1–%2").arg(shape.first + 1).arg(shape.last);
            p.drawText(QRectF(24, y - 10, 80, 20), Qt::AlignLeading | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(label, Qt::ElideRight, 80));
        }
        for (const auto& projection : projections)
            for (const auto& shape : projection.shapes)
                p.drawText(QRectF(plot.right() + 12, shape.track.center().y() - 10, 56, 20),
                           Qt::AlignRight | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(number(q, shape.value) + suffix,
                                                      Qt::ElideRight, 56));
        return;
    }
    const bool dark = q->effectiveTheme() == FluentElement::Dark;
    const bool high = q->effectiveTheme() == FluentElement::HighContrast;
    for (int i = 0; i <= 4; ++i) {
        const double t = i / 4.0, y = plot.bottom() - t * plot.height();
        p.setPen(QPen(
            high ? colors.strokeDivider : (i ? tokens::grid(dark) : tokens::baseline(dark)), 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(colors.textSecondary);
        p.drawText(QRectF(8, y - 10, 44, 20), Qt::AlignRight | Qt::AlignVCenter,
                   number(q, mix(yMin, yMax, t)));
    }
    if (bars() && !projections.isEmpty()) {
        const auto& shapes = projections.first().shapes;
        const int slotCount = std::max(1, int(plot.width() / 64));
        const int stride = std::max(1, int((shapes.size() + slotCount - 1) / slotCount));
        for (int i = 0; i < shapes.size(); i += stride) {
            const auto& shape = shapes[i];
            // Center the axis label on the complete group, not its first bar.
            qreal x = shape.path.boundingRect().center().x();
            if (projections.size() > 1 && i < projections.last().shapes.size())
                x = (x + projections.last().shapes[i].path.boundingRect().center().x()) / 2;
            p.drawText(QRectF(x - 30, plot.bottom() + 10, 60, 20), Qt::AlignCenter,
                       p.fontMetrics().elidedText(category(shape), Qt::ElideRight, 60));
        }
        return;
    }
    const int ticks = plot.width() >= 480 ? 6 : std::max(1, std::min(3, int(plot.width() / 80)));
    for (int i = 0; i <= ticks; ++i) {
        const double t = double(i) / ticks, value = mix(xMin, xMax, t);
        QString text = number(q, value);
        if (const auto* model = q->model()) {
            const int row = std::min(model->rowCount() - 1, model->lowerBound(value));
            const auto label = model->labelAt(row);
            if (!label.isEmpty())
                text = label;
        }
        p.drawText(QRectF(plot.left() + t * plot.width() - 32, plot.bottom() + 10, 64, 20),
                   Qt::AlignCenter, p.fontMetrics().elidedText(text, Qt::ElideRight, 64));
    }
}

void ChartViewPrivate::paintLegend(QPainter& p) const
{
    if (legendBox.isEmpty())
        return;
    const auto& colors = q->themeColorsRef();
    p.setFont(q->themeFont(Typography::FontRole::Caption).toQFont());
    if (circular()) {
        if (projections.isEmpty())
            return;
        const auto& shapes = projections.first().shapes;
        p.setPen(colors.textSecondary);
        p.drawText(legendBox.adjusted(0, 0, 0, -legendBox.height() + 20),
                   Qt::AlignLeading | Qt::AlignVCenter, ChartView::tr("Source"));
        p.drawText(legendBox.adjusted(0, 0, 0, -legendBox.height() + 20),
                   Qt::AlignRight | Qt::AlignVCenter, ChartView::tr("Share"));
        const qreal cell = std::min(
            36.0, std::max(16.0, (legendBox.height() - 28) / std::max(1, int(shapes.size()))));
        for (int i = 0; i < shapes.size(); ++i) {
            const auto& shape = shapes[i];
            const qreal y = legendBox.top() + 28 + i * cell;
            p.setPen(Qt::NoPen);
            p.setBrush(color(shape.color));
            p.drawRoundedRect(QRectF(legendBox.left(), y + 6, 8, 8), 2, 2);
            p.setPen(colors.textPrimary);
            const int nameWidth = std::max(0, int(legendBox.width() - 80));
            p.drawText(QRectF(legendBox.left() + 20, y, nameWidth, 20),
                       Qt::AlignLeading | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(category(shape), Qt::ElideRight, nameWidth));
            p.setFont(q->themeFont(Typography::FontRole::BodyStrong).toQFont());
            p.drawText(QRectF(legendBox.right() - 60, y, 60, 20), Qt::AlignRight | Qt::AlignVCenter,
                       q->locale().toString(shape.value * 100, 'f', 1) + QLatin1Char('%'));
            p.setFont(q->themeFont(Typography::FontRole::Caption).toQFont());
        }
        return;
    }
    const int columns = std::max(1, int(legendBox.width() / 130));
    const int slotCount = columns * (legendBox.height() > 24 ? 2 : 1);
    for (int i = 0; i < std::min(slotCount, int(series.size())); ++i) {
        const bool overflow = i == slotCount - 1 && series.size() > slotCount;
        const int column =
            q->layoutDirection() == Qt::RightToLeft ? columns - 1 - i % columns : i % columns;
        const qreal x = legendBox.left() + column * 130, y = legendBox.top() + (i / columns) * 24;
        if (!overflow) {
            p.setPen(Qt::NoPen);
            p.setBrush(color(i));
            switch (renderer->legendMarker()) {
            case detail::LegendMarker::Block:
                p.drawRoundedRect(QRectF(x + 2, y + 5, 12, 10), 2, 2);
                break;
            case detail::LegendMarker::Point:
                p.drawEllipse(QPointF(x + 8, y + 10), 3.5, 3.5);
                break;
            case detail::LegendMarker::Line: {
                QPen pen(color(i), 2);
                if (i % 3 == 1)
                    pen.setStyle(Qt::DashLine);
                if (i % 3 == 2)
                    pen.setStyle(Qt::DotLine);
                p.setPen(pen);
                p.drawLine(QPointF(x, y + 10), QPointF(x + 16, y + 10));
                break;
            }
            }
        }
        p.setPen(colors.textSecondary);
        const QString label = overflow ? ChartView::tr("+%1 series").arg(series.size() - i)
                                       : q->seriesModel(i)->name();
        p.drawText(QRectF(x + 24, y, 98, 20), Qt::AlignLeading | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(label, Qt::ElideRight, 98));
    }
}

ChartViewPrivate::Cursor ChartViewPrivate::cursor() const
{
    Cursor result;
    if (loading || !q->isEnabled() || dirty)
        return result;
    const int s = hoverSeries >= 0 ? hoverSeries : currentSeries;
    const int row = hoverSeries >= 0 ? hoverRow : currentRow;
    if (s < 0 || s >= projections.size() || row < 0)
        return result;
    auto& readouts = result.rows;
    auto& anchor = result.anchor;
    auto& heading = result.heading;
    if (circular() || bars()) {
        const auto& shapes = projections[s].shapes;
        const auto it =
            std::find_if(shapes.begin(), shapes.end(), [row](const detail::Shape& shape) {
                return shape.first <= row && row < shape.last;
            });
        if (it == shapes.end())
            return result;
        anchor = hoverSeries >= 0 ? hoverPosition : it->path.boundingRect().center();
        heading = category(*it);
        if (circular() && it->last - it->first > 1)
            heading = ChartView::tr("Other (%1 points)").arg(it->last - it->first);
        readouts.append(
            {circular()
                 ? ChartView::tr("Share")
                 : (it->last - it->first > 1 ? ChartView::tr("Mean") : q->seriesModel(s)->name()),
             circular() ? q->locale().toString(it->value * 100, 'f', 1) + QLatin1Char('%')
                        : number(q, it->value) + suffix,
             color(it->color), true});
        result.shape = it->path;
    } else {
        const auto& points = projections[s].points;
        const auto it =
            std::find_if(points.begin(), points.end(),
                         [row](const detail::Vertex& point) { return point.row == row; });
        QPointF value;
        if (it != points.end()) {
            value = it->value;
            anchor = it->position;
        } else {
            const auto* model = q->seriesModel(s);
            if (!model || row >= model->rowCount())
                return result;
            value = model->pointAt(row);
            anchor = map(value.x(), value.y());
        }
        if (!std::isfinite(anchor.y()) || !plot.contains(anchor))
            return result;
        heading = q->seriesModel(s)->labelAt(row);
        if (heading.isEmpty())
            heading = number(q, value.x());
        for (int offset = 0; offset < projections.size(); ++offset) {
            const int i = (s + offset) % projections.size();
            if (!renderer->sharedCursor() && i != s)
                continue;
            const auto& samples = projections[i].points;
            const auto aligned =
                std::find_if(samples.begin(), samples.end(), [&](const detail::Vertex& point) {
                    return point.value.x() == value.x();
                });
            // A shared heading must never imply that a nearby sample has the
            // same timestamp. Compare only exact X matches in the bounded cache.
            if (i != s && aligned == samples.end())
                continue;
            // For an explicitly selected raw row, preserve the exact value.
            const auto position = i == s ? anchor : aligned->position;
            const double y = i == s ? value.y() : aligned->value.y();
            readouts.append({q->seriesModel(i)->name(), number(q, y) + suffix, color(i), i == s});
            result.positions.append(position);
        }
    }
    return result;
}

void ChartViewPrivate::paintCursor(QPainter& p) const
{
    const auto selected = cursor();
    if (selected.rows.isEmpty())
        return;
    const auto& colors = q->themeColorsRef();
    p.save();
    p.setClipRect(plot);
    if (!selected.shape.isEmpty()) {
        p.setPen(QPen(colors.textPrimary, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(selected.shape);
    } else {
        if (renderer->sharedCursor() && !compact()) {
            p.setPen(QPen(colors.textTertiary, 1, Qt::DashLine));
            p.drawLine(QPointF(selected.anchor.x(), plot.top()),
                       QPointF(selected.anchor.x(), plot.bottom()));
        }
        for (int i = 0; i < selected.positions.size(); ++i) {
            p.setPen(QPen(colors.bgLayer, 2));
            p.setBrush(selected.rows[i].color);
            p.drawEllipse(selected.positions[i], 4.5, 4.5);
        }
    }
    p.restore();
}

void ChartViewPrivate::syncReadout()
{
    const auto selected =
        q->isVisible() && !compact() && (hoverSeries >= 0 || currentReadoutVisible) ? cursor()
                                                                                    : Cursor{};
    if (selected.rows.isEmpty()) {
        if (readout)
            readout->close();
        return;
    }
    if (!readout) {
        readout = new detail::ChartReadout(q);
        QObject::connect(readout, &dialogs_flyouts::Popup::closed, q, [this] {
            clearHover();
            currentReadoutVisible = false;
            q->update();
        });
    }
    readout->present(selected.heading, selected.rows, selected.anchor);
}

void ChartView::paintEvent(QPaintEvent*)
{
    if ((d->dirty && d->ready) || d->dpr != devicePixelRatioF())
        d->project();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto& colors = themeColorsRef();
    const bool dark = effectiveTheme() == Dark, high = effectiveTheme() == HighContrast;
    if (!d->compact()) {
        painter.setBrush(colors.bgLayer);
        painter.setPen(QPen(high ? colors.strokeCard : tokens::border(dark), 1));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), themeRadius().overlay,
                                themeRadius().overlay);
    }
    if (!isEnabled())
        painter.setOpacity(.45);
    if (!d->compact()) {
        qreal y = 24;
        if (!d->title.isEmpty()) {
            painter.setFont(themeFont(Typography::FontRole::Subtitle).toQFont());
            painter.setPen(colors.textPrimary);
            painter.drawText(QRectF(24, y, width() - 48, 28), Qt::AlignLeading | Qt::AlignVCenter,
                             painter.fontMetrics().elidedText(d->title, Qt::ElideRight,
                                                              std::max(0, width() - 48)));
            y += 32;
        }
        if (!d->subtitle.isEmpty()) {
            painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
            painter.setPen(colors.textSecondary);
            painter.drawText(QRectF(24, y, width() - 48, 16), Qt::AlignLeading | Qt::AlignVCenter,
                             painter.fontMetrics().elidedText(d->subtitle, Qt::ElideRight,
                                                              std::max(0, width() - 48)));
        }
        d->paintAxes(painter);
        d->paintLegend(painter);
    }
    painter.save();
    painter.setClipRect(d->bars() || d->circular() ? d->plot : d->plot.adjusted(-4, -4, 4, 4));
    for (int s = 0; s < d->projections.size(); ++s)
        d->renderer->paint(painter, d->context(s), d->projections[s]);
    painter.restore();
    if (d->renderer->innerRadius() > 0 && !d->loading && d->projectedPoints) {
        const qreal width = d->plot.width() * d->renderer->innerRadius() - 20;
        const qreal x = d->plot.center().x() - width / 2;
        painter.setFont(themeFont(Typography::FontRole::Title).toQFont());
        painter.setPen(colors.textPrimary);
        painter.drawText(QRectF(x, d->plot.center().y() - 30, width, 36), Qt::AlignCenter,
                         painter.fontMetrics().elidedText(d->centerText, Qt::ElideRight,
                                                          std::max(0, int(width))));
        painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
        painter.setPen(colors.textSecondary);
        painter.drawText(QRectF(x, d->plot.center().y() + 8, width, 20), Qt::AlignCenter,
                         painter.fontMetrics().elidedText(d->centerCaption, Qt::ElideRight,
                                                          std::max(0, int(width))));
    }
    if ((!d->projectedPoints || d->loading) && !d->compact()) {
        painter.setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());
        painter.setPen(colors.textPrimary);
        painter.drawText(d->plot.adjusted(0, -14, 0, -14), Qt::AlignCenter,
                         d->loading ? tr("Preparing data") : tr("No data yet"));
        painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
        painter.setPen(colors.textSecondary);
        painter.drawText(d->plot.adjusted(0, 14, 0, 14), Qt::AlignCenter,
                         d->loading ? tr("The chart will appear when ready.")
                                    : tr("Add a series to see it here."));
    }
    d->paintCursor(painter);
    if (hasFocus() && isEnabled() && d->keyboardFocusVisible) {
        painter.setOpacity(1);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.accentDefault, 2));
        painter.drawRoundedRect(QRectF(rect()).adjusted(2, 2, -2, -2), themeRadius().overlay,
                                themeRadius().overlay);
    }
}
void ChartView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    d->invalidate(true);
}
void ChartView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    d->invalidate(true);
}
void ChartView::hideEvent(QHideEvent* event)
{
    d->timer.stop();
    d->dismissReadout();
    QWidget::hideEvent(event);
}
void ChartView::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason)
        d->keyboardFocusVisible = false;
    else if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
             event->reason() == Qt::ShortcutFocusReason)
        d->keyboardFocusVisible = true;
    update();
}
void ChartView::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    d->dismissReadout();
    update();
}
void ChartView::mouseMoveEvent(QMouseEvent* event)
{
    d->currentReadoutVisible = false;
    int series = -1, first = -1, last = -1;
    if (!d->dirty && isEnabled())
        d->hit(event->pos(), series, first, last);
    d->hoverSeries = series;
    d->hoverRow = first;
    d->hoverLast = last;
    d->hoverPosition = event->pos();
    d->syncReadout();
    update();
    QWidget::mouseMoveEvent(event);
}
void ChartView::mousePressEvent(QMouseEvent* event)
{
    d->keyboardFocusVisible = false;
    update();
    if (event->button() == Qt::LeftButton) {
        int s = -1, first = -1, last = -1;
        if (isEnabled() && !d->dirty && d->hit(event->pos(), s, first, last)) {
            d->hoverSeries = s;
            d->hoverRow = first;
            d->hoverLast = last;
            d->hoverPosition = event->pos();
            QPointer<ChartView> guard(this);
            setCurrentPoint(s, first);
            if (guard)
                emit rangeActivated(s, first, last);
            return;
        }
    }
    d->dismissReadout();
    QWidget::mousePressEvent(event);
}
void ChartView::leaveEvent(QEvent* event)
{
    d->dismissReadout();
    update();
    QWidget::leaveEvent(event);
}
void ChartView::keyPressEvent(QKeyEvent* event)
{
    d->keyboardFocusVisible = true;
    update();
    int series = std::max(0, currentSeries());
    int row = std::max(0, currentRow());
    if (event->key() == Qt::Key_Up)
        series = std::max(0, series - 1);
    else if (event->key() == Qt::Key_Down)
        series = std::min(seriesCount() - 1, series + 1);
    else if (event->key() == Qt::Key_Left)
        row = std::max(0, row - 1);
    else if (event->key() == Qt::Key_Right)
        row = currentRow() < 0 ? 0 : row + 1;
    else if (event->key() == Qt::Key_Home)
        row = 0;
    else if (event->key() == Qt::Key_End)
        row = std::numeric_limits<int>::max();
    else if (event->key() == Qt::Key_Escape) {
        d->dismissReadout();
        if (!d->circular() && !isAutoRange()) {
            event->accept();
            resetXRange();
        } else {
            QWidget::keyPressEvent(event);
        }
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) {
        d->clearHover();
        d->currentReadoutVisible = currentRow() >= 0;
        d->syncReadout();
        if (currentRow() >= 0)
            emit rangeActivated(currentSeries(), currentRow(), currentRow() + 1);
        return;
    } else {
        QWidget::keyPressEvent(event);
        return;
    }
    d->clearHover();
    if (auto* m = seriesModel(series))
        setCurrentPoint(series, std::min(row, m->rowCount() - 1));
    event->accept();
}
void ChartView::wheelEvent(QWheelEvent* event)
{
    if (!(event->modifiers() & Qt::ControlModifier) || d->circular()) {
        event->ignore();
        return;
    }
    const double lo = minimumX(), hi = maximumX();
    const double factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    setXRange(mix(lo, hi, (1 - factor) / 2), mix(lo, hi, (1 + factor) / 2));
    event->accept();
}
void ChartView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange && !isEnabled())
        d->dismissReadout();
    if (event->type() == QEvent::LocaleChange || event->type() == QEvent::FontChange ||
        event->type() == QEvent::LayoutDirectionChange)
        d->invalidate(true);
}

} // namespace fluent::charts
