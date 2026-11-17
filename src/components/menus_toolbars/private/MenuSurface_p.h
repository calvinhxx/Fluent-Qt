#pragma once

#include "components/foundation/FluentElement.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/MenuTokens_p.h"
#include "design/Typography.h"
#include <QPixmapCache>
#include <QFontMetricsF>

namespace fluent::menus_toolbars::detail {

// Shared baseline optically centered on capital height so ascenders/descenders don't shift alignment.
inline qreal menuTextBaseline(QPainter& painter, const QRectF& row)
{
    const qreal dpr = painter.device()->devicePixelRatioF();
    return qRound((row.center().y() + QFontMetricsF(painter.font()).capHeight() / 2.0) * dpr) / dpr;
}

// Center menu glyphs by actual painted ink bounds rather than icon-font line bearings.
inline void paintMenuGlyph(QPainter& painter, const QRectF& rect, const QString& glyph, int size)
{
    const qreal dpr = painter.device()->devicePixelRatioF();
    const QColor color = painter.pen().color();
    const QString key =
        QStringLiteral("fluent.menu-glyph:%1:%2:%3:%4").arg(glyph).arg(size).arg(color.rgba()).arg(dpr, 0, 'g', 12);
    QPixmap symbol;
    if (!QPixmapCache::find(key, &symbol)) {
        // Cache exact ink bounds at target DPR because icon font metrics often mismatch visual bearings.
        const int extent = qMax(size, Typography::Icons::font(size).pixelSize()) + 8;
        QImage image(QSize(qCeil(extent * dpr), qCeil(extent * dpr)), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        QPainter glyphPainter(&image);
        glyphPainter.setPen(color);
        Typography::Icons::paintGlyph(glyphPainter, QRectF(0, 0, extent, extent), glyph, size, Qt::AlignCenter);
        glyphPainter.end();
        QRect ink;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0)
                    ink |= QRect(x, y, 1, 1);
            }
        }
        if (ink.isEmpty())
            return;
        symbol = QPixmap::fromImage(image.copy(ink));
        symbol.setDevicePixelRatio(dpr);
        QPixmapCache::insert(key, symbol);
    }
    const QPointF origin(qRound(rect.center().x() * dpr - symbol.width() / 2.0) / dpr,
        qRound(rect.center().y() * dpr - symbol.height() / 2.0) / dpr);
    painter.drawPixmap(origin, symbol);
}

inline void paintMenuSurface(QPainter& painter, const QRect& rect, const FluentElement& theme, bool shadow = true)
{
    const auto& colors = theme.themeColorsRef();
    const int radius = theme.themeRadius().overlay;
    if (shadow) {
        overlay::paintLayeredShadow(painter, rect, radius, theme.themeShadow(menu_tokens::shadowLevel),
            menu_tokens::shadowIntensity);
    }
    painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.border = colors.strokeCard;
    surface.radius = radius;
    painting::paintRoundedSurface(painter, QRectF(rect), surface);
}

} // namespace fluent::menus_toolbars::detail
