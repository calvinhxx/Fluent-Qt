#include "BarChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class BarRenderer final : public ChartRenderer {
public:
    bool includesZero() const override { return true; }
    bool categories() const override { return true; }
    bool sharedCursor() const override { return false; }
    LegendMarker legendMarker() const override { return LegendMarker::Block; }
    Projection project(const Context& c, const ChartModel& model) const override
    {
        Projection out;
        const int first = model.lowerBound(c.xMin), last = model.upperBound(c.xMax),
                  count = last - first;
        const int buckets =
            std::max(1, std::min(c.budget, int(c.plot.width() / (8 * c.seriesCount))));
        const int stride = std::max(1, int((qint64(count) + buckets - 1) / buckets));
        const double cell = c.plot.width() / std::max(1, std::min(count, buckets));
        const double width = std::max(1.0, std::min(18.0, cell * .72 / c.seriesCount - 5));
        const double gap = std::min(5.0, width / 3);
        const double group = (width + gap) * c.seriesCount - gap;
        for (int row = first; row < last; row += stride) {
            const int end = std::min(last, row + stride);
            const double value = model.mean(row, end);
            if (!std::isfinite(value))
                continue;
            const double x = mix(model.pointAt(row).x(), model.pointAt(end - 1).x(), .5);
            const auto position = c.map(x, value), zero = c.map(x, 0);
            QRectF rect(position.x() - group / 2 + c.series * (width + gap),
                        std::min(position.y(), zero.y()), width, std::abs(position.y() - zero.y()));
            out.shapes.append({roundedBar(rect.intersected(c.plot), false, value >= 0),
                               row,
                               end,
                               c.series,
                               value,
                               {}});
        }
        return out;
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        paintShapes(p, c, data, false);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeBarRenderer()
{
    return std::make_unique<BarRenderer>();
}
} // namespace detail
BarChart::BarChart(QWidget* parent) : ChartView(ChartView::Bar, parent) {}
} // namespace fluent::charts
