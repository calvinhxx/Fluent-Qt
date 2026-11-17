#pragma once

#include "components/foundation/FluentElement.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/MenuTokens_p.h"
#include "design/Typography.h"
#include <QPixmapCache>
#include <QFontMetricsF>

namespace fluent::menus_toolbars::detail {

// One baseline for labels and shortcuts, optically centered on the capitals.
// zh_CN: 标签和快捷键共用按大写字高居中的基线，避免字形是否含下伸部影响行对齐。
inline qreal menuTextBaseline(QPainter& painter, const QRectF& row)
{
    const qreal dpr = painter.device()->devicePixelRatioF();
    return qRound((row.center().y() + QFontMetricsF(painter.font()).capHeight() / 2.0) * dpr) / dpr;
}

// Menu symbols occupy an optical center, independently of the icon font's
// line bearings. Text labels continue to share their normal font baseline.
// zh_CN: 菜单符号按可见字形居中，不受图标字体行框留白影响；文字保留共同基线。
inline void paintMenuGlyph(QPainter& painter, const QRectF& rect, const QString& glyph, int size)
{
    const qreal dpr = painter.device()->devicePixelRatioF();
    const QColor color = painter.pen().color();
    const QString key =
        QStringLiteral("fluent.menu-glyph:%1:%2:%3:%4").arg(glyph).arg(size).arg(color.rgba()).arg(dpr, 0, 'g', 12);
    QPixmap symbol;
    if (!QPixmapCache::find(key, &symbol)) {
        // Font metrics can differ from the rasterized icon bearings. Cache
        // the actual ink at the target DPR, without resampling its strokes.
        // zh_CN: 字体度量可能不同于实际图标留白，按目标 DPR 缓存墨迹，不缩放描边。
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
