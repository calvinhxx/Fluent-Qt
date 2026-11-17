#include "ButtonContentPainter_p.h"

#include "components/basicinput/Button.h"
#include "compatibility/PaintCompat.h"
#include "compatibility/TextPaintCompat.h"
#include <QPainter>

namespace fluent::basicinput {
namespace {
void drawCenteredIconGlyph(QPainter& painter,
    const QString& glyph,
    const QString& fontFamily,
    int pixelSize,
    const QRectF& targetRect,
    const QPoint& offset,
    qreal scale,
    qreal rotation)
{
    const bool usesFluentIcons = fontFamily == Typography::FontFamily::FluentIcons;
    const QRectF paintedRect = targetRect.translated(QPointF(offset.x(), offset.y()));
    const bool transformed = !qFuzzyCompare(scale, 1.0) || !qFuzzyIsNull(rotation);

    // Fluent icons use shared paintGlyph; keep transform wrapping for Button press/scale animations.
    if (usesFluentIcons) {
        if (!transformed) {
            Typography::Icons::paintGlyph(painter, paintedRect, glyph, pixelSize, Qt::AlignCenter);
            return;
        }
        const QPointF targetCenter = paintedRect.center();
        painter.save();
        painter.translate(targetCenter);
        painter.rotate(rotation);
        painter.scale(scale, scale);
        painter.translate(-targetCenter);
        Typography::Icons::paintGlyph(painter, paintedRect, glyph, pixelSize, Qt::AlignCenter);
        painter.restore();
        return;
    }

    QFont iconFont(fontFamily);
    iconFont.setPixelSize(pixelSize);
    painter.setFont(iconFont);
    if (!transformed) {
        painter.drawText(paintedRect, Qt::AlignCenter, glyph);
        return;
    }

    const QPointF targetCenter = paintedRect.center();
    painter.save();
    painter.translate(targetCenter);
    painter.rotate(rotation);
    painter.scale(scale, scale);
    painter.translate(-targetCenter);
    painter.drawText(paintedRect, Qt::AlignCenter, glyph);
    painter.restore();
}

} // namespace

void ButtonContentPainter::paintButtonIcon(const Button& button, QPainter& painter, const QRectF& bounds)
{
    painter.save();
    if (!button.iconGlyph().isEmpty()) {
        drawCenteredIconGlyph(painter, button.iconGlyph(), button.iconFontFamily(), button.iconPixelSize(), bounds,
            button.iconOffset(), button.iconScale(), button.iconRotation());
    } else if (!button.icon().isNull()) {
        const qreal dpr = painter.device()
            ? qMax<qreal>(1.0, painter.device()->devicePixelRatioF())
            : qMax<qreal>(1.0, button.devicePixelRatioF());
        const QPixmap pixmap = fluentIconPixmapForLogicalExtent(button.icon(), button.iconSize(), dpr,
            button.window() ? button.window()->windowHandle() : nullptr);
        if (!pixmap.isNull()) {
            const QSizeF size = QSizeF(pixmap.size()) / pixmap.devicePixelRatioF();
            painter.translate(bounds.center() + QPointF(button.iconOffset()));
            painter.rotate(button.iconRotation());
            painter.scale(button.iconScale(), button.iconScale());
            fluentDrawPixmapAt(painter, QPointF(-size.width() / 2.0, -size.height() / 2.0), pixmap);
        }
    }
    painter.restore();
}

QRectF ButtonContentPainter::paintHorizontalButtonContent(const Button& button,
    QPainter& painter,
    const QRectF& bounds,
    int gap,
    Qt::Alignment alignment)
{
    const auto layout = button.fluentLayout();
    const QString text = layout == Button::IconOnly ? QString() : button.text();
    const bool hasIcon = layout != Button::TextOnly && (!button.iconGlyph().isEmpty() || !button.icon().isNull());
    const qreal iconWidth =
        !hasIcon ? 0 : (!button.iconGlyph().isEmpty() ? button.iconPixelSize() : button.iconSize().width());
    const QFontMetrics metrics(button.font());
    const qreal textWidth =
        text.contains(QLatin1Char('\n')) ? metrics.size(0, text).width() : metrics.horizontalAdvance(text);
    const qreal spacing = hasIcon && !text.isEmpty() ? gap : 0;
    const qreal width = textWidth + iconWidth + spacing;
    const qreal left = bounds.left() + ((alignment & Qt::AlignHCenter) ? (bounds.width() - width) / 2.0 : 0);
    const bool after = layout == Button::IconAfter;
    QRectF iconRect(left + (after ? textWidth + spacing : 0), bounds.top(), iconWidth, bounds.height());
    QRectF textRect(left + (after ? 0 : iconWidth + spacing), bounds.top(), textWidth, bounds.height());
    if (button.layoutDirection() == Qt::RightToLeft) {
        iconRect.moveLeft(bounds.left() + bounds.right() - iconRect.right());
        textRect.moveLeft(bounds.left() + bounds.right() - textRect.right());
    }
    if (hasIcon)
        paintButtonIcon(button, painter, iconRect);
    painter.save();
    painter.setFont(button.font());
    if (!text.isEmpty())
        fluent::painting::drawVerticallyCenteredText(painter, textRect, text, Qt::AlignHCenter);
    painter.restore();
    return text.isEmpty() ? QRectF() : textRect;
}

} // namespace fluent::basicinput
