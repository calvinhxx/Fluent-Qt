#include "WindowChromeFrame.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QtGlobal>

#include <utility>

#include "components/foundation/overlay/OverlayShadow.h"

namespace fluent::windowing {

ClientSideFrameEdgeOverlay::ClientSideFrameEdgeOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    m_hitZonesAreSiblings = parent != nullptr;
    if (m_hitZonesAreSiblings)
        setAttribute(Qt::WA_TransparentForMouseEvents, true);

    const std::array<Qt::CursorShape, 8> cursors{
        Qt::SizeVerCursor,
        Qt::SizeVerCursor,
        Qt::SizeHorCursor,
        Qt::SizeHorCursor,
        Qt::SizeFDiagCursor,
        Qt::SizeBDiagCursor,
        Qt::SizeBDiagCursor,
        Qt::SizeFDiagCursor,
    };

    for (int i = 0; i < static_cast<int>(m_hitZones.size()); ++i) {
        // In production the zones are siblings of the visual overlay, so the
        // full-size overlay can remain transparent to pointer input. Visual
        // corner clipping is owned separately by the Linux frame host.
        // zh_CN: 生产环境中命中区与视觉 overlay 为同级控件，因此全尺寸视觉层可以始终
        // 穿透鼠标；圆角视觉裁剪由 Linux frame host 单独负责。
        auto* zone = new QWidget(m_hitZonesAreSiblings ? parent : this);
        zone->setObjectName(QStringLiteral("fluentWindowResizeHitZone%1").arg(i));
        zone->setAttribute(Qt::WA_NoSystemBackground, true);
        zone->setAttribute(Qt::WA_TranslucentBackground, true);
        zone->setMouseTracking(true);
        zone->setAttribute(Qt::WA_Hover, true);
        zone->setCursor(cursors.at(i));
        zone->installEventFilter(this);
        m_hitZones.at(i) = zone;
    }
}

void ClientSideFrameEdgeOverlay::setFrameVisualRect(const QRect& rect) {
    if (m_frameVisualRect == rect)
        return;
    m_frameVisualRect = rect;
    syncHitZones();
    update();
}

void ClientSideFrameEdgeOverlay::setFrameRadius(qreal radius) {
    if (qFuzzyCompare(m_frameRadius, radius))
        return;
    m_frameRadius = radius;
    update();
}

void ClientSideFrameEdgeOverlay::setFrameStroke(const QColor& stroke) {
    if (m_frameStroke == stroke)
        return;
    m_frameStroke = stroke;
    update();
}

void ClientSideFrameEdgeOverlay::setResizeInputEnabled(bool enabled, int hitWidth) {
    const int clampedWidth = qMax(1, hitWidth);
    if (m_resizeInputEnabled == enabled && m_resizeHitWidth == clampedWidth)
        return;

    m_resizeInputEnabled = enabled;
    m_resizeHitWidth = clampedWidth;
    setAttribute(Qt::WA_TransparentForMouseEvents,
                 m_hitZonesAreSiblings ? true : !enabled);
    setMouseTracking(enabled);
    setAttribute(Qt::WA_Hover, enabled);
    if (!enabled)
        unsetCursor();
    syncHitZones();
}

void ClientSideFrameEdgeOverlay::setMouseEventHandler(MouseEventHandler handler) {
    m_mouseEventHandler = std::move(handler);
}

bool ClientSideFrameEdgeOverlay::event(QEvent* event) {
    if (event && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
        syncHitZones();
    return QWidget::event(event);
}

bool ClientSideFrameEdgeOverlay::eventFilter(QObject* watched, QEvent* event) {
    auto* zone = qobject_cast<QWidget*>(watched);
    if (!zone || !event)
        return QWidget::eventFilter(watched, event);

    const QEvent::Type type = event->type();
    if (type == QEvent::Enter || type == QEvent::HoverEnter
        || type == QEvent::HoverMove) {
        // Alien/translucent child widgets do not reliably push their cursor to
        // the native WSLg/X11 surface. Mirror the active zone cursor on the
        // overlay, which owns that surface, whenever the pointer enters it.
        // zh_CN: 透明的非原生子控件在 WSLg/X11 下不一定会把光标同步到原生表面；
        // 指针进入命中区时，同时在拥有该表面的 overlay 上设置缩放光标。
        setCursor(zone->cursor());
        if (QWidget* topLevel = zone->window())
            topLevel->setCursor(zone->cursor());
        return QWidget::eventFilter(watched, event);
    }
    if (type == QEvent::Leave || type == QEvent::HoverLeave) {
        unsetCursor();
        if (QWidget* topLevel = zone->window())
            topLevel->unsetCursor();
        return QWidget::eventFilter(watched, event);
    }

    if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove
        && type != QEvent::MouseButtonRelease) {
        return QWidget::eventFilter(watched, event);
    }

    if (type == QEvent::MouseMove) {
        setCursor(zone->cursor());
        if (QWidget* topLevel = zone->window())
            topLevel->setCursor(zone->cursor());
    }
    if (!m_mouseEventHandler)
        return QWidget::eventFilter(watched, event);

    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (!m_mouseEventHandler(zone, mouseEvent))
        return QWidget::eventFilter(watched, event);

    mouseEvent->accept();
    return true;
}

void ClientSideFrameEdgeOverlay::paintEvent(QPaintEvent*) {
    if (m_frameRadius <= 0.0 || rect().isEmpty())
        return;

    const QRect visualRect = m_frameVisualRect.isValid() ? m_frameVisualRect : rect();
    QColor stroke = m_frameStroke.isValid() ? m_frameStroke : QColor(0, 0, 0, 34);
    stroke.setAlpha(qMax(stroke.alpha(), 58));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(stroke, 1.0));
    painter.drawRoundedRect(QRectF(visualRect).adjusted(0.5, 0.5, -0.5, -0.5),
                            m_frameRadius,
                            m_frameRadius);
}

void ClientSideFrameEdgeOverlay::resizeEvent(QResizeEvent*) {
    syncHitZones();
}

void ClientSideFrameEdgeOverlay::syncHitZones() {
    if (rect().isEmpty())
        return;

    const QRect r = rect();
    const QRect visual = (m_frameVisualRect.isValid() ? m_frameVisualRect : r)
                             .intersected(r);
    if (visual.isEmpty())
        return;

    const int width = qMin(m_resizeHitWidth,
                           qMax(1, qMin(visual.width(), visual.height()) / 2));
    // Include the transparent shadow margin outside the frame plus `width`
    // logical pixels inside the visible edge. Previously the zones occupied
    // only the outermost window strip, ending one pixel before an inset Linux
    // frame, so neither its cursor nor its antialiased stroke reached the edge
    // the user actually sees.
    // zh_CN: 命中区同时覆盖可见窗框外侧的透明阴影边距和内侧 width 像素。此前它只在
    // 顶层窗口最外圈，恰好在 Linux 内缩窗框前一像素结束，导致可见边缘没有缩放光标，描边也被裁掉。
    const int leftCornerRight = qMin(r.right(), visual.left() + width - 1);
    const int rightCornerLeft = qMax(r.left(), visual.right() - width + 1);
    const int topCornerBottom = qMin(r.bottom(), visual.top() + width - 1);
    const int bottomCornerTop = qMax(r.top(), visual.bottom() - width + 1);

    const auto boundedRect = [&r](int left, int top, int right, int bottom) {
        left = qBound(r.left(), left, r.right());
        right = qBound(r.left(), right, r.right());
        top = qBound(r.top(), top, r.bottom());
        bottom = qBound(r.top(), bottom, r.bottom());
        return right >= left && bottom >= top
            ? QRect(QPoint(left, top), QPoint(right, bottom))
            : QRect();
    };

    m_hitZones[0]->setGeometry(boundedRect(leftCornerRight + 1, r.top(),
                                           rightCornerLeft - 1, topCornerBottom));
    m_hitZones[1]->setGeometry(boundedRect(leftCornerRight + 1, bottomCornerTop,
                                           rightCornerLeft - 1, r.bottom()));
    m_hitZones[2]->setGeometry(boundedRect(r.left(), topCornerBottom + 1,
                                           leftCornerRight, bottomCornerTop - 1));
    m_hitZones[3]->setGeometry(boundedRect(rightCornerLeft, topCornerBottom + 1,
                                           r.right(), bottomCornerTop - 1));
    m_hitZones[4]->setGeometry(boundedRect(r.left(), r.top(),
                                           leftCornerRight, topCornerBottom));
    m_hitZones[5]->setGeometry(boundedRect(rightCornerLeft, r.top(),
                                           r.right(), topCornerBottom));
    m_hitZones[6]->setGeometry(boundedRect(r.left(), bottomCornerTop,
                                           leftCornerRight, r.bottom()));
    m_hitZones[7]->setGeometry(boundedRect(rightCornerLeft, bottomCornerTop,
                                           r.right(), r.bottom()));

    for (QWidget* zone : m_hitZones) {
        if (m_hitZonesAreSiblings)
            zone->move(zone->pos() + geometry().topLeft());
        zone->setVisible(m_resizeInputEnabled && isVisible());
        if (zone->isVisible())
            zone->raise();
    }
}

void paintClientSideFrame(QPainter& painter, const ClientSideFramePaintOptions& options) {
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(options.windowRect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    ::fluent::overlay::paintLayeredShadow(painter,
                                          options.frameRect.adjusted(0, 0, -1, -1),
                                          options.radius,
                                          options.shadow,
                                          options.dark ? 0.46 : 0.34);

    QColor fill = options.fill;
    if (options.useTranslucentMaterial) {
        const int alpha = options.effect == compatibility::BackdropEffect::Acrylic
                              ? (options.dark ? 142 : 164)
                              : (options.dark ? 188 : 214);
        fill.setAlpha(alpha);
    }

    painter.setPen(QPen(options.stroke, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(QRectF(options.frameRect).adjusted(0.5, 0.5, -0.5, -0.5),
                            options.radius,
                            options.radius);
}

Qt::Edges resizeEdgesForHitTest(compatibility::WindowChromeCompat::HitTest hit) {
    using HitTest = compatibility::WindowChromeCompat::HitTest;
    switch (hit) {
    case HitTest::Left:
        return Qt::LeftEdge;
    case HitTest::Right:
        return Qt::RightEdge;
    case HitTest::Top:
        return Qt::TopEdge;
    case HitTest::Bottom:
        return Qt::BottomEdge;
    case HitTest::TopLeft:
        return Qt::TopEdge | Qt::LeftEdge;
    case HitTest::TopRight:
        return Qt::TopEdge | Qt::RightEdge;
    case HitTest::BottomLeft:
        return Qt::BottomEdge | Qt::LeftEdge;
    case HitTest::BottomRight:
        return Qt::BottomEdge | Qt::RightEdge;
    case HitTest::Caption:
    case HitTest::Client:
        return Qt::Edges();
    }
    return Qt::Edges();
}

Qt::CursorShape cursorShapeForResizeEdges(Qt::Edges edges) {
    if ((edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::LeftEdge))
        || (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::RightEdge))) {
        return Qt::SizeFDiagCursor;
    }
    if ((edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::RightEdge))
        || (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::LeftEdge))) {
        return Qt::SizeBDiagCursor;
    }
    if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge))
        return Qt::SizeHorCursor;
    if (edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge))
        return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

QRect resizedGeometryForEdges(const QRect& startGeometry,
                              Qt::Edges edges,
                              const QPoint& delta,
                              const QSize& minimumSize) {
    QRect next = startGeometry;

    if (edges.testFlag(Qt::LeftEdge)) {
        const int proposedLeft = startGeometry.left() + delta.x();
        next.setLeft(qMin(proposedLeft, next.right() - minimumSize.width() + 1));
    }
    if (edges.testFlag(Qt::RightEdge)) {
        const int proposedRight = startGeometry.right() + delta.x();
        next.setRight(qMax(proposedRight, next.left() + minimumSize.width() - 1));
    }
    if (edges.testFlag(Qt::TopEdge)) {
        const int proposedTop = startGeometry.top() + delta.y();
        next.setTop(qMin(proposedTop, next.bottom() - minimumSize.height() + 1));
    }
    if (edges.testFlag(Qt::BottomEdge)) {
        const int proposedBottom = startGeometry.bottom() + delta.y();
        next.setBottom(qMax(proposedBottom, next.top() + minimumSize.height() - 1));
    }

    return next;
}

void WindowResizeSession::start(Qt::Edges edges, const QPoint& globalPos, const QRect& geometry) {
    m_edges = edges;
    m_startGlobal = globalPos;
    m_startGeometry = geometry;
}

void WindowResizeSession::finish() {
    m_edges = Qt::Edges();
    m_startGlobal = QPoint();
    m_startGeometry = QRect();
}

QRect WindowResizeSession::geometryForGlobalPoint(const QPoint& globalPos,
                                                  const QSize& minimumSize) const {
    return resizedGeometryForEdges(m_startGeometry, m_edges, globalPos - m_startGlobal, minimumSize);
}

} // namespace fluent::windowing
