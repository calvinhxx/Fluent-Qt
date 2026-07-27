#include "components/layout/Card.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QVariant>

#include "components/foundation/private/SurfacePainter_p.h"

namespace fluent::layout {

Card::Card(QWidget* parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(false);
    publishSurfaceColor();
}

void Card::setAppearance(Appearance appearance)
{
    if (m_appearance == appearance)
        return;

    m_appearance = appearance;
    publishSurfaceColor();
    update();
    emit appearanceChanged(m_appearance);
}

void Card::setBorderVisible(bool visible)
{
    if (m_borderVisible == visible)
        return;

    m_borderVisible = visible;
    update();
    emit borderVisibleChanged(m_borderVisible);
}

void Card::onThemeUpdated()
{
    publishSurfaceColor();
    update();
}

void Card::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    const auto& colors = themeColorsRef();
    painting::RoundedSurfacePaint surface;
    surface.fill = surfaceColor();
    surface.border = m_borderVisible ? colors.strokeCard : QColor();
    surface.radius = themeRadius().control;

    QPainter painter(this);
    painting::paintRoundedSurface(painter, QRectF(rect()), surface);
}

QColor Card::surfaceColor() const
{
    const auto& colors = themeColorsRef();
    switch (m_appearance) {
    case LayerAlt:
        return colors.bgLayerAlt;
    case Canvas:
        return colors.bgCanvas;
    case Layer:
    default:
        return colors.bgLayer;
    }
}

void Card::publishSurfaceColor()
{
    const QColor color = surfaceColor();
    if (property("fluentSurfaceColor").value<QColor>() != color)
        setProperty("fluentSurfaceColor", color);

    QPalette cardPalette = palette();
    cardPalette.setColor(QPalette::Window, color);
    cardPalette.setColor(QPalette::Base, color);
    setPalette(cardPalette);
}

} // namespace fluent::layout
