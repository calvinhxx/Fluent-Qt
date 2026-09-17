#include "PieChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class PieRenderer final : public ChartRenderer {
public:
    Layout layout() const override { return Layout::Radial; }
    bool sharedCursor() const override { return false; }
    Projection project(const Context& c, const ChartModel& m) const override
    {
        return projectSectors(c, m, 0);
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        paintShapes(p, c, data, true);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makePieRenderer()
{
    return std::make_unique<PieRenderer>();
}
} // namespace detail
PieChart::PieChart(QWidget* parent) : ChartView(ChartView::Pie, parent) {}
} // namespace fluent::charts
