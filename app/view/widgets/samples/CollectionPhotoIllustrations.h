#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPixmapCache>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QtGlobal>

#include <cstdint>

#include "compatibility/PaintCompat.h"

namespace fluent::gallery::detail {

namespace photo_card_detail {

inline std::uint32_t deterministicHash(const QString& text)
{
    std::uint32_t hash = 2166136261u;
    for (const QChar ch : text) {
        hash ^= static_cast<std::uint32_t>(ch.unicode());
        hash *= 16777619u;
    }
    return hash;
}

inline int sceneIndexFromColor(const QColor& color)
{
    const QRgb rgb = color.rgb() & 0x00FFFFFFu;
    switch (rgb) {
    case 0xF7975Bu:
    case 0xC25B2Bu:
        return 0; // Sunrise / Canyon
    case 0x1E6FD9u:
    case 0x378BC4u:
        return 1; // Ocean / Harbor
    case 0x2F9E44u:
        return 2; // Forest / Garden
    case 0x6B4FA2u:
    case 0x7A5FC9u:
        return 3; // Dusk / Studio
    case 0xC86B2Du:
        return 4; // Desert / Atrium
    case 0x3D8BA3u:
    case 0xD96C75u:
        return 5; // Glacier / Dawn
    default:
        return (color.red() * 3 + color.green() * 5 + color.blue() * 7) % 6;
    }
}

inline void paintSunriseCanyonScene(QPainter& p, const QRectF& r)
{
    const qreal x = r.left(), y = r.top(), w = r.width(), h = r.height();

    QLinearGradient sky(x, y, x, y + h * 0.72);
    sky.setColorAt(0.0, QColor(0xE86A3B));
    sky.setColorAt(0.55, QColor(0xF5A45B));
    sky.setColorAt(1.0, QColor(0xFDE09D));
    p.fillRect(r, sky);

    const QPointF sunCenter(x + w * 0.72, y + h * 0.34);
    const qreal glowR = qMin(w, h) * 0.42;
    QRadialGradient glow(sunCenter, glowR);
    glow.setColorAt(0.0, QColor(255, 248, 214, 180));
    glow.setColorAt(0.45, QColor(255, 224, 148, 80));
    glow.setColorAt(1.0, QColor(255, 210, 120, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(sunCenter, glowR, glowR);

    const qreal sunR = qMin(w, h) * 0.11;
    p.setBrush(QColor(0xFFF6D6));
    p.drawEllipse(sunCenter, sunR, sunR);

    QPainterPath distantMesa;
    distantMesa.moveTo(x, y + h);
    distantMesa.lineTo(x, y + h * 0.56);
    distantMesa.lineTo(x + w * 0.16, y + h * 0.52);
    distantMesa.lineTo(x + w * 0.24, y + h * 0.44);
    distantMesa.lineTo(x + w * 0.45, y + h * 0.44);
    distantMesa.lineTo(x + w * 0.54, y + h * 0.57);
    distantMesa.lineTo(x + w * 0.78, y + h * 0.53);
    distantMesa.lineTo(x + w, y + h * 0.60);
    distantMesa.lineTo(x + w, y + h);
    distantMesa.closeSubpath();
    p.setBrush(QColor(0xB8563A));
    p.drawPath(distantMesa);

    QPainterPath midRidge;
    midRidge.moveTo(x, y + h);
    midRidge.lineTo(x, y + h * 0.68);
    midRidge.lineTo(x + w * 0.28, y + h * 0.61);
    midRidge.lineTo(x + w * 0.52, y + h * 0.70);
    midRidge.lineTo(x + w * 0.74, y + h * 0.60);
    midRidge.lineTo(x + w, y + h * 0.67);
    midRidge.lineTo(x + w, y + h);
    midRidge.closeSubpath();
    p.setBrush(QColor(0x88382B));
    p.drawPath(midRidge);

    QPainterPath foreRidge;
    foreRidge.moveTo(x, y + h);
    foreRidge.lineTo(x, y + h * 0.79);
    foreRidge.cubicTo(x + w * 0.32, y + h * 0.72, x + w * 0.66, y + h * 0.85, x + w, y + h * 0.76);
    foreRidge.lineTo(x + w, y + h);
    foreRidge.closeSubpath();
    p.setBrush(QColor(0x52211D));
    p.drawPath(foreRidge);
}

inline void paintOceanHarborScene(QPainter& p, const QRectF& r)
{
    const qreal x = r.left(), y = r.top(), w = r.width(), h = r.height();

    QLinearGradient sky(x, y, x, y + h * 0.60);
    sky.setColorAt(0.0, QColor(0x164B96));
    sky.setColorAt(0.60, QColor(0x499EE6));
    sky.setColorAt(1.0, QColor(0xB8E7F5));
    p.fillRect(r, sky);

    const QPointF sunPos(x + w * 0.28, y + h * 0.44);
    const qreal sunGlowR = qMin(w, h) * 0.30;
    QRadialGradient sunGlow(sunPos, sunGlowR);
    sunGlow.setColorAt(0.0, QColor(255, 252, 232, 210));
    sunGlow.setColorAt(0.45, QColor(255, 240, 200, 90));
    sunGlow.setColorAt(1.0, QColor(255, 240, 200, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(sunGlow);
    p.drawEllipse(sunPos, sunGlowR, sunGlowR);

    p.setBrush(QColor(255, 255, 255, 58));
    p.drawRoundedRect(QRectF(x + w * 0.48, y + h * 0.17, w * 0.34, h * 0.08), h * 0.04, h * 0.04);
    p.drawRoundedRect(QRectF(x + w * 0.56, y + h * 0.13, w * 0.20, h * 0.08), h * 0.04, h * 0.04);
    p.drawRoundedRect(QRectF(x + w * 0.08, y + h * 0.22, w * 0.24, h * 0.06), h * 0.03, h * 0.03);

    const QRectF seaRect(x, y + h * 0.56, w, h * 0.44);
    QLinearGradient sea(seaRect.topLeft(), seaRect.bottomLeft());
    sea.setColorAt(0.0, QColor(0x1E7FB3));
    sea.setColorAt(0.45, QColor(0x115284));
    sea.setColorAt(1.0, QColor(0x0A2E52));
    p.fillRect(seaRect, sea);

    p.setBrush(QColor(255, 246, 214, 105));
    for (int i = 0; i < 4; ++i) {
        const qreal wy = y + h * (0.60 + i * 0.055);
        const qreal ww = w * (0.16 - i * 0.025);
        const qreal wx = sunPos.x() - ww * 0.5 + (i % 2 == 0 ? -w * 0.01 : w * 0.015);
        p.drawRoundedRect(QRectF(wx, wy, ww, qMax<qreal>(1.5, h * 0.02)), 1.0, 1.0);
    }

    QPainterPath headland;
    headland.moveTo(x + w * 0.48, y + h * 0.58);
    headland.lineTo(x + w * 0.68, y + h * 0.49);
    headland.lineTo(x + w * 0.84, y + h * 0.51);
    headland.lineTo(x + w, y + h * 0.45);
    headland.lineTo(x + w, y + h * 0.62);
    headland.closeSubpath();
    p.setBrush(QColor(0x153B5E));
    p.drawPath(headland);
}

inline void paintForestGardenScene(QPainter& p, const QRectF& r)
{
    const qreal x = r.left(), y = r.top(), w = r.width(), h = r.height();

    QLinearGradient sky(x, y, x, y + h * 0.68);
    sky.setColorAt(0.0, QColor(0x1E655D));
    sky.setColorAt(0.55, QColor(0x56A86E));
    sky.setColorAt(1.0, QColor(0xCBE9AE));
    p.fillRect(r, sky);

    QPainterPath farHill;
    farHill.moveTo(x, y + h);
    farHill.lineTo(x, y + h * 0.54);
    farHill.cubicTo(x + w * 0.28, y + h * 0.42, x + w * 0.62, y + h * 0.58, x + w, y + h * 0.48);
    farHill.lineTo(x + w, y + h);
    farHill.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2F745A));
    p.drawPath(farHill);

    QLinearGradient mist(x, y + h * 0.46, x, y + h * 0.72);
    mist.setColorAt(0.0, QColor(235, 248, 224, 0));
    mist.setColorAt(0.5, QColor(235, 248, 224, 115));
    mist.setColorAt(1.0, QColor(235, 248, 224, 0));
    p.fillRect(QRectF(x, y + h * 0.46, w, h * 0.26), mist);

    auto drawPineLayer = [&](qreal baseYFrac, qreal ampFrac, int peaks, const QColor& color) {
        QPainterPath trees;
        trees.moveTo(x, y + h);
        trees.lineTo(x, y + h * baseYFrac);
        for (int i = 0; i < peaks; ++i) {
            const qreal px0 = x + (w * i) / peaks;
            const qreal px1 = x + (w * (i + 1)) / peaks;
            const qreal midX = (px0 + px1) * 0.5;
            const qreal tipOffset = ((i * 37) % 5) * 0.015;
            trees.lineTo(midX, y + h * (baseYFrac - ampFrac - tipOffset));
            trees.lineTo(px1, y + h * (baseYFrac + 0.02));
        }
        trees.lineTo(x + w, y + h);
        trees.closeSubpath();
        p.setBrush(color);
        p.drawPath(trees);
    };

    drawPineLayer(0.68, 0.14, 9, QColor(0x1C5240));
    drawPineLayer(0.80, 0.15, 7, QColor(0x0E3629));
}

inline void paintDuskStudioScene(QPainter& p, const QRectF& r)
{
    const qreal x = r.left(), y = r.top(), w = r.width(), h = r.height();

    QLinearGradient sky(x, y, x, y + h * 0.68);
    sky.setColorAt(0.0, QColor(0x1D1A4A));
    sky.setColorAt(0.48, QColor(0x64489B));
    sky.setColorAt(0.82, QColor(0xC96D98));
    sky.setColorAt(1.0, QColor(0xEFA47F));
    p.fillRect(r, sky);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 248, 226, 210));
    const qreal starR = qMax<qreal>(0.9, qMin(w, h) * 0.012);
    for (const QPointF& rel :
        {QPointF(0.14, 0.16), QPointF(0.26, 0.24), QPointF(0.43, 0.14), QPointF(0.60, 0.21), QPointF(0.86, 0.30)}) {
        p.drawEllipse(QPointF(x + w * rel.x(), y + h * rel.y()), starR, starR);
    }

    const qreal moonR = qMin(w, h) * 0.095;
    const QPointF moonPos(x + w * 0.78, y + h * 0.23);
    QPainterPath moonOuter, moonCut;
    moonOuter.addEllipse(moonPos, moonR, moonR);
    moonCut.addEllipse(moonPos + QPointF(moonR * 0.42, -moonR * 0.18), moonR * 0.88, moonR * 0.88);
    p.setBrush(QColor(0xFFF3D4));
    p.drawPath(moonOuter.subtracted(moonCut));

    QPainterPath mountain;
    mountain.moveTo(x, y + h * 0.68);
    mountain.lineTo(x + w * 0.18, y + h * 0.50);
    mountain.lineTo(x + w * 0.36, y + h * 0.60);
    mountain.lineTo(x + w * 0.58, y + h * 0.44);
    mountain.lineTo(x + w * 0.80, y + h * 0.58);
    mountain.lineTo(x + w, y + h * 0.51);
    mountain.lineTo(x + w, y + h * 0.68);
    mountain.closeSubpath();
    p.setBrush(QColor(0x2D2259));
    p.drawPath(mountain);

    const QRectF lakeRect(x, y + h * 0.67, w, h * 0.33);
    QLinearGradient lake(lakeRect.topLeft(), lakeRect.bottomLeft());
    lake.setColorAt(0.0, QColor(0x4E3A7A));
    lake.setColorAt(1.0, QColor(0x18133A));
    p.fillRect(lakeRect, lake);

    p.setBrush(QColor(255, 210, 190, 90));
    p.drawRoundedRect(QRectF(x + w * 0.72, y + h * 0.72, w * 0.13, qMax<qreal>(1.5, h * 0.02)), 1.0, 1.0);
    p.drawRoundedRect(QRectF(x + w * 0.74, y + h * 0.77, w * 0.09, qMax<qreal>(1.5, h * 0.018)), 1.0, 1.0);
}

inline void paintDesertAtriumScene(QPainter& p, const QRectF& r)
{
    const qreal x = r.left(), y = r.top(), w = r.width(), h = r.height();

    QLinearGradient sky(x, y, x, y + h * 0.62);
    sky.setColorAt(0.0, QColor(0xA64B2A));
    sky.setColorAt(0.55, QColor(0xDA8642));
    sky.setColorAt(1.0, QColor(0xF6CE88));
    p.fillRect(r, sky);

    const QPointF sunPos(x + w * 0.24, y + h * 0.40);
    const qreal glowR = qMin(w, h) * 0.28;
    QRadialGradient glow(sunPos, glowR);
    glow.setColorAt(0.0, QColor(255, 248, 218, 200));
    glow.setColorAt(1.0, QColor(255, 224, 150, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(sunPos, glowR, glowR);

    QPainterPath backDune;
    backDune.moveTo(x, y + h);
    backDune.lineTo(x, y + h * 0.58);
    backDune.cubicTo(x + w * 0.30, y + h * 0.45, x + w * 0.62, y + h * 0.62, x + w, y + h * 0.50);
    backDune.lineTo(x + w, y + h);
    backDune.closeSubpath();
    p.setBrush(QColor(0xD98744));
    p.drawPath(backDune);

    QPainterPath midDune;
    midDune.moveTo(x, y + h);
    midDune.lineTo(x, y + h * 0.68);
    midDune.cubicTo(x + w * 0.35, y + h * 0.58, x + w * 0.58, y + h * 0.48, x + w, y + h * 0.64);
    midDune.lineTo(x + w, y + h);
    midDune.closeSubpath();
    p.setBrush(QColor(0xB85F2C));
    p.drawPath(midDune);

    QPainterPath frontShadow;
    frontShadow.moveTo(x, y + h);
    frontShadow.lineTo(x + w * 0.14, y + h * 0.74);
    frontShadow.cubicTo(x + w * 0.40, y + h * 0.64, x + w * 0.66, y + h * 0.80, x + w, y + h * 0.72);
    frontShadow.lineTo(x + w, y + h);
    frontShadow.closeSubpath();
    p.setBrush(QColor(0x7C381C));
    p.drawPath(frontShadow);
}

inline void paintGlacierDawnScene(QPainter& p, const QRectF& r)
{
    const qreal x = r.left(), y = r.top(), w = r.width(), h = r.height();

    QLinearGradient sky(x, y, x, y + h * 0.66);
    sky.setColorAt(0.0, QColor(0x214869));
    sky.setColorAt(0.55, QColor(0x5193B0));
    sky.setColorAt(1.0, QColor(0xD3EEF3));
    p.fillRect(r, sky);

    p.setPen(Qt::NoPen);

    // Shadowed mountain backs
    QPainterPath peaks;
    peaks.moveTo(x, y + h * 0.72);
    peaks.lineTo(x + w * 0.22, y + h * 0.38);
    peaks.lineTo(x + w * 0.42, y + h * 0.56);
    peaks.lineTo(x + w * 0.68, y + h * 0.30);
    peaks.lineTo(x + w, y + h * 0.64);
    peaks.lineTo(x + w, y + h * 0.72);
    peaks.closeSubpath();
    p.setBrush(QColor(0x294E6B));
    p.drawPath(peaks);

    // Sunlit snow facets
    QPainterPath snowLeft;
    snowLeft.moveTo(x + w * 0.22, y + h * 0.38);
    snowLeft.lineTo(x + w * 0.31, y + h * 0.53);
    snowLeft.lineTo(x + w * 0.20, y + h * 0.58);
    snowLeft.lineTo(x + w * 0.11, y + h * 0.55);
    snowLeft.closeSubpath();
    p.setBrush(QColor(0xE8F6FA));
    p.drawPath(snowLeft);

    QPainterPath snowRight;
    snowRight.moveTo(x + w * 0.68, y + h * 0.30);
    snowRight.lineTo(x + w * 0.85, y + h * 0.52);
    snowRight.lineTo(x + w * 0.70, y + h * 0.56);
    snowRight.lineTo(x + w * 0.58, y + h * 0.48);
    snowRight.closeSubpath();
    p.setBrush(QColor(0xF0FAFC));
    p.drawPath(snowRight);

    const QRectF fjord(x, y + h * 0.70, w, h * 0.30);
    QLinearGradient water(fjord.topLeft(), fjord.bottomLeft());
    water.setColorAt(0.0, QColor(0x246E86));
    water.setColorAt(1.0, QColor(0x133B52));
    p.fillRect(fjord, water);

    p.setBrush(QColor(225, 248, 255, 90));
    p.drawRoundedRect(QRectF(x + w * 0.56, y + h * 0.75, w * 0.22, qMax<qreal>(1.5, h * 0.02)), 1.0, 1.0);
    p.drawRoundedRect(QRectF(x + w * 0.14, y + h * 0.78, w * 0.18, qMax<qreal>(1.5, h * 0.018)), 1.0, 1.0);
}

inline void paintScenicBackdrop(QPainter& painter, const QRectF& rect, int sceneIndex)
{
    switch ((sceneIndex % 6 + 6) % 6) {
    case 0:
        paintSunriseCanyonScene(painter, rect);
        break;
    case 1:
        paintOceanHarborScene(painter, rect);
        break;
    case 2:
        paintForestGardenScene(painter, rect);
        break;
    case 3:
        paintDuskStudioScene(painter, rect);
        break;
    case 4:
        paintDesertAtriumScene(painter, rect);
        break;
    default:
        paintGlacierDawnScene(painter, rect);
        break;
    }
}

} // namespace photo_card_detail

/**
 * @brief Renders a vector landscape pixmap for Gallery sample models and fallbacks.
 * zh_CN: 为 Gallery 示例模型与离线回退渲染矢量风景位图。
 */
inline QPixmap renderScenicPhotoPixmap(const QSize& logicalSize,
    const QColor& tint,
    const QString& seedText = QString(),
    qreal devicePixelRatio = 1.0)
{
    const QSize targetSize(qMax(1, logicalSize.width()), qMax(1, logicalSize.height()));
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    const int sceneIndex = tint.isValid()
        ? photo_card_detail::sceneIndexFromColor(tint)
        : static_cast<int>(photo_card_detail::deterministicHash(seedText) % 6u);
    const QString key =
        QStringLiteral("fluentqt.gallery.scenic/%1/%2/%3/%4")
            .arg(sceneIndex)
            .arg(targetSize.width())
            .arg(targetSize.height())
            .arg(dpr, 0, 'g', 17);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;
    QPixmap pixmap(QSize(qMax(1, qRound(targetSize.width() * dpr)), qMax(1, qRound(targetSize.height() * dpr))));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    FluentRasterCachePainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    photo_card_detail::paintScenicBackdrop(painter, QRectF(0.0, 0.0, targetSize.width(), targetSize.height()),
        sceneIndex);
    painter.end();
    QPixmapCache::insert(key, pixmap);
    return pixmap;
}

} // namespace fluent::gallery::detail
