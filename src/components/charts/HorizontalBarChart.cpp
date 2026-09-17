#include "HorizontalBarChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class HorizontalBarRenderer final : public ChartRenderer {
public:
    Layout layout() const override { return Layout::Horizontal; }
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
            std::max(1, std::min(c.budget, int(c.plot.height() / (28 * c.seriesCount))));
        const int stride = std::max(1, int((qint64(count) + buckets - 1) / buckets));
        const int slotCount = std::max(1, (count + stride - 1) / stride);
        const double cell = c.plot.height() / slotCount;
        const double thickness = std::max(1.0, std::min(16.0, cell * .65 / c.seriesCount - 4));
        for (int row = first, slot = 0; row < last; row += stride, ++slot) {
            const int end = std::min(last, row + stride);
            const double value = model.mean(row, end);
            if (!std::isfinite(value))
                continue;
            const double y = c.plot.top() + (slot + .5) * cell +
                             (c.series - (c.seriesCount - 1) / 2.0) * (thickness + 4);
            const double zero = c.plot.left() + normalized(0, c.yMin, c.yMax) * c.plot.width();
            const double x = c.plot.left() + normalized(value, c.yMin, c.yMax) * c.plot.width();
            QRectF rect(std::min(x, zero), y - thickness / 2, std::abs(x - zero), thickness);
            QRectF track(c.plot.left(), rect.top(), c.plot.width(), thickness);
            out.shapes.append({roundedBar(rect.intersected(c.plot), true, value >= 0), row, end,
                               c.series, value, track});
        }
        return out;
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        paintShapes(p, c, data, false);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeHorizontalBarRenderer()
{
    return std::make_unique<HorizontalBarRenderer>();
}
} // namespace detail
HorizontalBarChart::HorizontalBarChart(QWidget* parent)
    : ChartView(ChartView::HorizontalBar, parent)
{}
} // namespace fluent::charts
