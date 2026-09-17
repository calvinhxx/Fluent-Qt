#pragma once

#include <QPainter>
#include <QPainterPath>
#include <memory>
#include "ChartModel.h"

namespace fluent::charts::detail {
enum class Layout { Cartesian, Horizontal, Radial, Compact };
enum class LegendMarker { Line, Block, Point };
struct Vertex {
    QPointF position;
    int row;
    QPointF value;
};
struct Shape {
    QPainterPath path;
    int first, last, color;
    double value;
    QRectF track;
};
struct Projection {
    QVector<QLineF> lines;
    QVector<QPointF> isolated;
    QVector<Vertex> points;
    QVector<Shape> shapes;
    int size() const { return points.size() + shapes.size(); }
};
struct Context {
    QRectF plot;
    double xMin, xMax, yMin, yMax;
    qreal dpr;
    int budget, series, seriesCount;
    QVector<QColor> colors;
    QColor surface, track;
    bool highContrast;
    QColor color(int i) const { return colors[i % colors.size()]; }
    QPointF map(double x, double y) const;
};
struct Hit {
    int series = -1, first = -1, last = -1;
    double distance = 144;
};

// Each concrete renderer owns its projection and paint policy. The view owns
// model lifetime, scheduling, shared chrome and the total projection budget.
class ChartRenderer {
public:
    virtual ~ChartRenderer() = default;
    virtual Layout layout() const { return Layout::Cartesian; }
    virtual bool includesZero() const { return false; }
    virtual bool categories() const { return false; }
    virtual bool sharedCursor() const { return true; }
    virtual LegendMarker legendMarker() const { return LegendMarker::Line; }
    virtual qreal innerRadius() const { return 0; }
    virtual Projection project(const Context& c, const ChartModel& model) const = 0;
    virtual void paint(QPainter& p, const Context& c, const Projection& data) const = 0;
    virtual void hit(const QPointF& position, const Projection& data, Hit& hit) const;
};

double normalized(double value, double lo, double hi);
double mix(double a, double b, double t);
Projection projectPolyline(const Context& c, const ChartModel& model);
void paintPolyline(QPainter& p, const Context& c, const Projection& data);
Projection projectSectors(const Context& c, const ChartModel& model, qreal innerRadius);
void paintShapes(QPainter& p, const Context& c, const Projection& data, bool radial);
QPainterPath roundedBar(const QRectF& rect, bool horizontal, bool positive);
std::unique_ptr<ChartRenderer> makeLineRenderer();
std::unique_ptr<ChartRenderer> makeAreaRenderer();
std::unique_ptr<ChartRenderer> makeBarRenderer();
std::unique_ptr<ChartRenderer> makeHorizontalBarRenderer();
std::unique_ptr<ChartRenderer> makePieRenderer();
std::unique_ptr<ChartRenderer> makeDonutRenderer();
std::unique_ptr<ChartRenderer> makeScatterRenderer();
std::unique_ptr<ChartRenderer> makeSparklineRenderer();
} // namespace fluent::charts::detail
