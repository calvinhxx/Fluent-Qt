#include "components/foundation/FontIcon.h"

#include <QEvent>
#include <QPaintEvent>
#include <QPainter>

namespace fluent {

FontIcon::FontIcon(QWidget* parent)
    : FontIcon(QString(), parent)
{
}

FontIcon::FontIcon(const QString& glyph, QWidget* parent)
    : QWidget(parent),
      m_glyph(glyph)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void FontIcon::setGlyph(const QString& glyph)
{
    if (m_glyph == glyph)
        return;

    m_glyph = glyph;
    update();
    emit glyphChanged(m_glyph);
}

void FontIcon::setIconSize(int size)
{
    size = qMax(1, size);
    if (m_iconSize == size)
        return;

    m_iconSize = size;
    updateGeometry();
    update();
    emit iconSizeChanged(m_iconSize);
}

void FontIcon::setColor(const QColor& color)
{
    if (m_color == color)
        return;

    m_color = color;
    update();
    emit colorChanged(m_color);
}

void FontIcon::setRotation(qreal degrees)
{
    if (qFuzzyCompare(m_rotation + 1.0, degrees + 1.0))
        return;

    m_rotation = degrees;
    update();
    emit rotationChanged(m_rotation);
}

QSize FontIcon::sizeHint() const
{
    return QSize(m_iconSize, m_iconSize);
}

QSize FontIcon::minimumSizeHint() const
{
    return sizeHint();
}

void FontIcon::onThemeUpdated()
{
    update();
}

void FontIcon::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    if (m_glyph.isEmpty())
        return;

    QPainter painter(this);
    painter.setPen(resolvedColor());
    if (!qFuzzyIsNull(m_rotation)) {
        const QPointF center = QRectF(rect()).center();
        painter.translate(center);
        painter.rotate(m_rotation);
        painter.translate(-center);
    }
    Typography::Icons::paintGlyph(
        painter, QRectF(rect()), m_glyph, m_iconSize, Qt::AlignCenter);
}

void FontIcon::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event && event->type() == QEvent::EnabledChange)
        update();
}

QColor FontIcon::resolvedColor() const
{
    if (m_color.isValid())
        return m_color;
    const auto& colors = themeColorsRef();
    return isEnabled() ? colors.textPrimary : colors.textDisabled;
}

} // namespace fluent
