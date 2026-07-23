#include "components/foundation/overlay/OverlayScrim.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>

namespace fluent::overlay {

OverlayScrim::OverlayScrim(QWidget* parent, const QString& objectName)
    : QWidget(parent)
{
    if (!objectName.isEmpty())
        setObjectName(objectName);
    // Shared-backing SourceOver dim — same contract as former Dialog SmokeOverlay. Do not use
    // WA_TranslucentBackground or CompositionMode_Source clears on Mica hosts.
    // zh_CN: 共享后备缓冲上的 SourceOver 压暗——与原先 Dialog SmokeOverlay 契约一致。Mica 宿主上不要用
    // WA_TranslucentBackground 或 CompositionMode_Source 清屏。
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    setModalAndDim(true, true);
}

void OverlayScrim::setModalAndDim(bool modal, bool dim)
{
    m_modal = modal;
    m_dim = dim;
    setAttribute(Qt::WA_TransparentForMouseEvents, !m_modal);
    update();
}

void OverlayScrim::setOpacityProgress(qreal progress)
{
    const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
    if (std::abs(m_opacityProgress - normalized) <= 0.0001)
        return;
    m_opacityProgress = normalized;
    update();
}

void OverlayScrim::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    update();
}

void OverlayScrim::setSurfaceRadius(int radius)
{
    radius = qMax(0, radius);
    if (m_surfaceRadius == radius)
        return;
    m_surfaceRadius = radius;
    update();
}

void OverlayScrim::setSpotlightEnabled(bool enabled)
{
    if (m_spotEnabled == enabled)
        return;
    m_spotEnabled = enabled;
    update();
}

void OverlayScrim::setSpotlightRect(const QRect& rect)
{
    if (m_spot == rect)
        return;
    m_spot = rect;
    if (m_spotEnabled)
        update();
}

void OverlayScrim::setSpotlightRadius(int radius)
{
    if (m_spotRadius == radius)
        return;
    m_spotRadius = radius;
    if (m_spotEnabled)
        update();
}

void OverlayScrim::onThemeUpdated()
{
    update();
}

void OverlayScrim::paintEvent(QPaintEvent*)
{
    if (!m_dim)
        return;

    QPainter painter(this);
    const auto smoke = themeSmoke();
    QColor color = m_color.isValid() ? m_color : smoke.baseColor;
    const qreal baseAlpha = m_color.isValid() ? m_color.alphaF() : smoke.opacity;
    color.setAlphaF(baseAlpha * m_opacityProgress);

    QPainterPath surface;
    if (m_surfaceRadius > 0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        surface.addRoundedRect(QRectF(rect()), m_surfaceRadius, m_surfaceRadius);
    } else {
        surface.addRect(QRectF(rect()));
    }

    if (!m_spotEnabled || m_spot.isEmpty()) {
        painter.fillPath(surface, color);
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath hole;
    hole.addRoundedRect(QRectF(m_spot), m_spotRadius, m_spotRadius);
    painter.fillPath(surface.subtracted(hole), color);
}

void OverlayScrim::mousePressEvent(QMouseEvent* event) { handlePointerEvent(event); }
void OverlayScrim::mouseReleaseEvent(QMouseEvent* event) { handlePointerEvent(event); }
void OverlayScrim::mouseMoveEvent(QMouseEvent* event) { handlePointerEvent(event); }
void OverlayScrim::wheelEvent(QWheelEvent* event) { handlePointerEvent(event); }

void OverlayScrim::handlePointerEvent(QEvent* event)
{
    if (m_modal)
        event->accept();
    else
        event->ignore();
}

} // namespace fluent::overlay
