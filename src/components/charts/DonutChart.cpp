#include "DonutChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class DonutRenderer final : public ChartRenderer {
public:
    Layout layout() const override { return Layout::Radial; }
    bool sharedCursor() const override { return false; }
    qreal innerRadius() const override { return .72; }
    Projection project(const Context& c, const ChartModel& m) const override
    {
        return projectSectors(c, m, innerRadius());
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        paintShapes(p, c, data, true);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeDonutRenderer()
{
    return std::make_unique<DonutRenderer>();
}
} // namespace detail
DonutChart::DonutChart(QWidget* parent) : ChartView(ChartView::Donut, parent) {}
} // namespace fluent::charts
