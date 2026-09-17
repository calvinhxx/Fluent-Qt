#include "LineChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class LineRenderer final : public ChartRenderer {
public:
    Projection project(const Context& c, const ChartModel& m) const override
    {
        return projectPolyline(c, m);
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        paintPolyline(p, c, data);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeLineRenderer()
{
    return std::make_unique<LineRenderer>();
}
} // namespace detail
LineChart::LineChart(QWidget* parent) : ChartView(ChartView::Line, parent) {}
} // namespace fluent::charts
