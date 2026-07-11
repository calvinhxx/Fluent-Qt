#ifndef FLUENTWINDOWCHROMEFRAME_H
#define FLUENTWINDOWCHROMEFRAME_H

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>
#include <Qt>

#include <array>
#include <functional>

#include "compatibility/WindowChromeCompat.h"
#include "components/windowing/WindowBackdropMaterial.h"
#include "design/Elevation.h"

class QEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;

namespace fluent::windowing {

struct ClientSideFramePaintOptions {
    QRect windowRect;
    QRect frameRect;
    QColor fill;
    QColor stroke;
    qreal radius = 0.0;
    Elevation::ShadowParams shadow;
    bool dark = false;
    bool useTranslucentMaterial = false;
    bool usePaintedMaterial = false;
    compatibility::BackdropEffect effect = compatibility::BackdropEffect::Solid;
    WindowBackdropMaterialOptions material;
};

void paintClientSideFrame(QPainter& painter, const ClientSideFramePaintOptions& options);

Qt::Edges resizeEdgesForHitTest(compatibility::WindowChromeCompat::HitTest hit);
Qt::CursorShape cursorShapeForResizeEdges(Qt::Edges edges);
QRect resizedGeometryForEdges(const QRect& startGeometry,
                              Qt::Edges edges,
                              const QPoint& delta,
                              const QSize& minimumSize);

class WindowResizeSession {
public:
    bool isActive() const { return m_edges != Qt::Edges(); }
    Qt::Edges edges() const { return m_edges; }

    void start(Qt::Edges edges, const QPoint& globalPos, const QRect& geometry);
    void finish();
    QRect geometryForGlobalPoint(const QPoint& globalPos, const QSize& minimumSize) const;

private:
    QPoint m_startGlobal;
    QRect m_startGeometry;
    Qt::Edges m_edges = Qt::Edges();
};

class ClientSideFrameEdgeOverlay : public QWidget {
public:
    using MouseEventHandler = std::function<bool(QWidget*, QMouseEvent*)>;

    explicit ClientSideFrameEdgeOverlay(QWidget* parent = nullptr);

    void setFrameVisualRect(const QRect& rect);
    void setFrameRadius(qreal radius);
    void setFrameStroke(const QColor& stroke);
    void setResizeInputEnabled(bool enabled, int hitWidth);
    void setMouseEventHandler(MouseEventHandler handler);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void syncHitZones();

    QRect m_frameVisualRect;
    QColor m_frameStroke;
    qreal m_frameRadius = 0.0;
    int m_resizeHitWidth = 1;
    bool m_resizeInputEnabled = false;
    bool m_hitZonesAreSiblings = false;
    std::array<QWidget*, 8> m_hitZones{};
    MouseEventHandler m_mouseEventHandler;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOWCHROMEFRAME_H
