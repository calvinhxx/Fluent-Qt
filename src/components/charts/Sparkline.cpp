#include "Sparkline.h"
#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts {
namespace detail {
namespace {
class SparklineRenderer final : public ChartRenderer {
public:
    Layout layout() const override { return Layout::Compact; }
    Projection project(const Context& c, const ChartModel& m) const override
    {
        return projectPolyline(c, m);
    }
    void paint(QPainter& p, const Context& c, const Projection& data) const override
    {
        paintPolyline(p, c, data);
        if (!data.points.isEmpty()) {
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(c.color(c.series));
            p.drawEllipse(data.points.last().position, 3, 3);
        }
    }
};
} // namespace
std::unique_ptr<ChartRenderer> makeSparklineRenderer()
{
    return std::make_unique<SparklineRenderer>();
}
} // namespace detail
Sparkline::Sparkline(QWidget* parent) : ChartView(ChartView::Sparkline, parent) {}
} // namespace fluent::charts
