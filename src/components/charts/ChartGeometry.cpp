#include "ChartRenderer_p.h"
#include <algorithm>
#include <cmath>

namespace fluent::charts::detail {
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
QPointF Context::map(double x, double y) const
{
    return {plot.left() + normalized(x, xMin, xMax) * plot.width(),
            plot.bottom() - normalized(y, yMin, yMax) * plot.height()};
}
Projection projectPolyline(const Context& c, const ChartModel& model)
{
    Projection out;
    const int first = model.lowerBound(c.xMin), last = model.upperBound(c.xMax);
    const auto rows =
        model.sampledRows(std::max(0, first - 1), std::min(model.rowCount(), last + 1), c.budget);
    bool started = false, connected = false;
    QPointF end;
    const auto finish = [&] {
        if (started && !connected)
            out.isolated.append(end);
        started = connected = false;
    };
    for (int row : rows) {
        if (row < 0) {
            finish();
            continue;
        }
        const auto value = model.pointAt(row);
        const auto position = c.map(value.x(), value.y());
        if (!std::isfinite(position.x()) || !std::isfinite(position.y())) {
            finish();
            continue;
        }
        if (started) {
            out.lines.append(QLineF(end, position));
            connected = true;
        }
        started = true;
        end = position;
        out.points.append({position, row, value});
    }
    finish();
    return out;
}
void paintPolyline(QPainter& p, const Context& c, const Projection& data)
{
    const bool dense = data.points.size() > c.plot.width() * c.dpr / 4;
    p.setRenderHint(QPainter::Antialiasing, !dense);
    QPen pen(c.color(c.series), 2);
    pen.setCapStyle(dense ? Qt::FlatCap : Qt::RoundCap);
    if (c.series % 3 == 1)
        pen.setStyle(Qt::DashLine);
    if (c.series % 3 == 2)
        pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // Keep segments independent: a combined Qt raster path can be quadratic
    // for dense extrema. Do not replace this with drawPath or drawLines.
    for (const auto& line : data.lines)
        p.drawLine(line);
    p.setBrush(c.color(c.series));
    for (const auto& point : data.isolated)
        p.drawEllipse(point, 3, 3);
}
Projection projectSectors(const Context& c, const ChartModel& model, qreal innerRadius)
{
    Projection out;
    if (c.series != 0)
        return out;
    const double side = std::min(c.plot.width(), c.plot.height());
    QRectF circle(0, 0, side, side);
    circle.moveCenter(c.plot.center());
    double angle = 90;
    for (int i = 0; i < std::min(12, model.rowCount()); ++i) {
        const int end = i == 11 ? model.rowCount() : i + 1;
        const double fraction = model.positiveFraction(i, end);
        if (fraction <= 0)
            continue;
        // Bound the angular gap so small positive slices remain visible.
        const double gap = model.rowCount() == 1 ? 0 : std::min(1.0, fraction * 90);
        QPainterPath path;
        path.moveTo(circle.center());
        path.arcTo(circle, angle - gap / 2, -fraction * 360 + gap);
        path.closeSubpath();
        if (innerRadius > 0) {
            QPainterPath hole;
            QRectF inner(0, 0, side * innerRadius, side * innerRadius);
            inner.moveCenter(circle.center());
            hole.addEllipse(inner);
            path = path.subtracted(hole);
        }
        out.shapes.append({path, i, end, i, fraction, {}});
        angle -= fraction * 360;
    }
    return out;
}
void paintShapes(QPainter& p, const Context& c, const Projection& data, bool radial)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    for (const auto& shape : data.shapes) {
        if (!shape.track.isEmpty()) {
            p.setBrush(c.track);
            p.drawRoundedRect(shape.track, 4, 4);
        }
        p.setBrush(c.color(shape.color));
        p.drawPath(shape.path);
    }
    Q_UNUSED(radial)
}
QPainterPath roundedBar(const QRectF& r, bool horizontal, bool positive)
{
    QPainterPath path;
    const qreal radius = std::min({4.0, r.width() / 2, r.height() / 2});
    path.setFillRule(Qt::WindingFill);
    path.addRoundedRect(r, radius, radius);
    // Flatten only the baseline edge; rounding a baseline distorts short bars.
    if (horizontal)
        path.addRect(QRectF(positive ? r.left() : r.right() - radius, r.top(), radius, r.height()));
    else
        path.addRect(QRectF(r.left(), positive ? r.bottom() - radius : r.top(), r.width(), radius));
    return path.simplified();
}
void ChartRenderer::hit(const QPointF& position, const Projection& data, Hit& hit) const
{
    for (const auto& shape : data.shapes)
        if (shape.path.contains(position)) {
            hit.first = shape.first;
            hit.last = shape.last;
            hit.distance = 0;
            return;
        }
    for (const auto& point : data.points) {
        const auto delta = point.position - position;
        const double distance =
            sharedCursor() ? delta.x() * delta.x() : delta.x() * delta.x() + delta.y() * delta.y();
        if (distance < hit.distance) {
            hit.first = point.row;
            hit.last = point.row + 1;
            hit.distance = distance;
        }
    }
}
} // namespace fluent::charts::detail
