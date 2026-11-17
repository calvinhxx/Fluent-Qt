#include "DebugOverlay.h"
#include <QPainter>
#include <QEvent>
#include <QPen>

DebugOverlay::DebugOverlay(QWidget* target, const QColor& color, QWidget* parent)
    : QWidget(parent ? parent : (target ? target->parentWidget() : nullptr)), m_target(target), m_color(color)
{
    if (!m_target)
        return;

    // Click-through overlay that never steals focus.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);

    m_target->installEventFilter(this);

    updateGeometryToTarget();
    show();
}

bool DebugOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_target) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            updateGeometryToTarget();
        } else if (event->type() == QEvent::Hide) {
            hide();
        } else if (event->type() == QEvent::Show) {
            show();
            updateGeometryToTarget();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DebugOverlay::updateGeometryToTarget()
{
    if (m_target) {
        // Assumes target and overlay share the same parent so geometry() maps 1:1.
        setGeometry(m_target->geometry());
        raise();
    }
}

void DebugOverlay::paintEvent(QPaintEvent*)
{
    if (!m_target)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dashed stroke keeps widget alignment legible against solid borders.
    QPen pen(m_color, m_thickness);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);

    painter.drawRect(QRectF(0, 0, width() - m_thickness, height() - m_thickness));
}