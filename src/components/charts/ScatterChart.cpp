#include "ScatterChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class ScatterRenderer final : public ChartRenderer {
public:
    bool sharedCursor() const override { return false; }
    LegendMarker legendMarker() const override { return LegendMarker::Point; }
    Projection project(const Context& c, const ChartModel& m) const override
    {
        auto data = projectPolyline(c, m);
        data.lines.clear();
        data.isolated.clear();
        return data;
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(c.color(c.series));
        for (const auto& point : data.points)
            p.drawEllipse(point.position, 3.5, 3.5);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeScatterRenderer()
{
    return std::make_unique<ScatterRenderer>();
}
} // namespace detail
ScatterChart::ScatterChart(QWidget* parent) : ChartView(ChartView::Scatter, parent) {}
} // namespace fluent::charts
