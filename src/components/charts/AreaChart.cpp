#include "AreaChart.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class AreaRenderer final : public ChartRenderer {
public:
    bool includesZero() const override { return true; }
    Projection project(const Context& c, const ChartModel& m) const override
    {
        return projectPolyline(c, m);
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        auto fill = c.color(c.series);
        fill.setAlphaF(c.highContrast ? .2 : .1);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        const qreal baseline = c.map(c.xMin, 0).y();
        // Disjoint constant-size trapezoids keep dense fill rasterization bounded.
        for (const auto& line : data.lines) {
            const QPointF polygon[] = {
                line.p1(), line.p2(), {line.x2(), baseline}, {line.x1(), baseline}};
            p.drawPolygon(polygon, 4);
        }
        paintPolyline(p, c, data);
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeAreaRenderer()
{
    return std::make_unique<AreaRenderer>();
}
} // namespace detail
AreaChart::AreaChart(QWidget* parent) : ChartView(ChartView::Area, parent) {}
} // namespace fluent::charts
