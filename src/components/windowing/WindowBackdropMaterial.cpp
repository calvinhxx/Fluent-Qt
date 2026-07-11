#include "WindowBackdropMaterial.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <QHash>
#include <QImage>
#include <QLinearGradient>
#include <QMutex>
#include <QMutexLocker>
#include <QPaintDevice>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QtGlobal>

namespace fluent::windowing {

namespace {

constexpr int GrainTileLogicalSize = 96;
constexpr quint32 GrainSeed = 0x8f3d7a21U;

qreal clampedUnit(qreal value)
{
    return std::max<qreal>(0.0, std::min<qreal>(1.0, value));
}

QColor opaqueColor(const QColor& color, const QColor& fallback)
{
    QColor result = color.isValid() ? color.toRgb() : fallback.toRgb();
    if (!result.isValid())
        result = QColor(243, 243, 243);
    result.setAlpha(255);
    return result;
}

QColor blendRgb(const QColor& from, const QColor& to, qreal amount)
{
    const qreal t = clampedUnit(amount);
    const QColor source = opaqueColor(from, QColor(243, 243, 243));
    const QColor target = opaqueColor(to, source);
    return QColor::fromRgbF(source.redF() + (target.redF() - source.redF()) * t,
                            source.greenF() + (target.greenF() - source.greenF()) * t,
                            source.blueF() + (target.blueF() - source.blueF()) * t,
                            1.0);
}

QColor withOpacity(const QColor& color, qreal opacity)
{
    QColor result = color.toRgb();
    result.setAlphaF(clampedUnit(opacity));
    return result;
}

QColor defaultNormal(bool dark)
{
    return dark ? QColor(32, 32, 32) : QColor(243, 243, 243);
}

QColor resolvedNormal(const WindowBackdropMaterialOptions& options)
{
    return opaqueColor(options.normalColor, defaultNormal(options.dark));
}

QColor resolvedAccent(const WindowBackdropMaterialOptions& options)
{
    // The default is the Fluent blue used only as a restrained color field;
    // callers normally pass their active theme accent token.
    return opaqueColor(options.accentColor, QColor(0, 120, 212));
}

qreal resolvedDevicePixelRatio(QPainter& painter,
                               const WindowBackdropMaterialOptions& options)
{
    qreal ratio = options.devicePixelRatio;
    if (ratio <= 0.0 && painter.device())
        ratio = painter.device()->devicePixelRatioF();
    if (!std::isfinite(ratio) || ratio <= 0.0)
        ratio = 1.0;
    return std::max<qreal>(0.5, std::min<qreal>(4.0, ratio));
}

QPixmap grainTile(qreal devicePixelRatio, BackdropEffect effect)
{
    // Quarter-step DPR buckets cover common desktop scale factors while keeping
    // this process-wide texture cache strictly bounded.
    const int ratioKey = qRound(devicePixelRatio * 4.0);
    const int effectKey = effect == BackdropEffect::Acrylic ? 2 : 1;
    const int key = ratioKey * 10 + effectKey;

    static QMutex cacheMutex;
    static QHash<int, QPixmap> cache;
    QMutexLocker locker(&cacheMutex);
    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd())
        return cached.value();

    const qreal quantizedRatio = ratioKey / 4.0;
    const int size = qMax(1, qRound(GrainTileLogicalSize * quantizedRatio));
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);

    quint32 state = GrainSeed ^ static_cast<quint32>(effectKey * 0x45d9f3bU)
                    ^ static_cast<quint32>(ratioKey);
    for (int y = 0; y < size; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < size; ++x) {
            // Fixed xorshift state makes the texture stable across every repaint.
            state ^= state << 13U;
            state ^= state >> 17U;
            state ^= state << 5U;
            const int value = static_cast<int>((state >> 24U) & 0xffU);
            scanLine[x] = qRgb(value, value, value);
        }
    }

    QPixmap texture = QPixmap::fromImage(image);
    texture.setDevicePixelRatio(quantizedRatio);
    cache.insert(key, texture);
    return texture;
}

void paintGrain(QPainter& painter,
                const QRectF& rect,
                const WindowBackdropMaterialOptions& options,
                qreal opacity)
{
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setOpacity(clampedUnit(opacity));
    painter.drawTiledPixmap(rect,
                            grainTile(resolvedDevicePixelRatio(painter, options), options.effect),
                            QPointF(0.0, 0.0));
    painter.restore();
}

void paintMicaColorFields(QPainter& painter,
                          const QRectF& rect,
                          const WindowBackdropMaterialOptions& options)
{
    const qreal activity = options.active ? 1.0 : 0.42;
    const qreal span = qMax(rect.width(), rect.height());
    const qreal radius = qMax<qreal>(1.0, span * 0.92);

    QRadialGradient accentField(QPointF(rect.left() + rect.width() * 0.10,
                                        rect.top() + rect.height() * 0.02),
                                radius);
    accentField.setColorAt(0.0,
                           withOpacity(resolvedAccent(options),
                                       (options.dark ? 0.13 : 0.085) * activity));
    accentField.setColorAt(0.56,
                           withOpacity(resolvedAccent(options),
                                       (options.dark ? 0.035 : 0.022) * activity));
    accentField.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect, accentField);

    const QColor counterTone = options.dark ? QColor(255, 255, 255)
                                             : QColor(66, 45, 28);
    QRadialGradient counterField(QPointF(rect.right() - rect.width() * 0.08,
                                         rect.bottom() - rect.height() * 0.04),
                                 qMax<qreal>(1.0, span * 0.78));
    counterField.setColorAt(0.0,
                            withOpacity(counterTone,
                                        (options.dark ? 0.032 : 0.025) * activity));
    counterField.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect, counterField);
}

void paintAcrylicColorFields(QPainter& painter,
                             const QRectF& rect,
                             const WindowBackdropMaterialOptions& options)
{
    const qreal activity = options.active ? 1.0 : 0.48;
    const qreal blurScale = std::max<qreal>(12.0, options.acrylic.blurRadius);
    const qreal span = qMax(rect.width(), rect.height());
    const qreal radius = qMax(span * 1.08, blurScale * 10.0);

    // A software fallback cannot sample and blur the desktop. Reconstruct the
    // same visual hierarchy with a neutral luminosity veil plus broad,
    // low-frequency accent/cool fields. Keeping those fields broad avoids a
    // decorative "gradient wallpaper" look while making Acrylic visibly more
    // glass-like than the stable, restrained Mica material.
    // zh_CN: 软件回退无法采样并模糊桌面，因此以中性明度幕布和宽幅、低频的
    // 强调色/冷色场重建层次；色场足够宽，既避免普通渐变壁纸感，也让 Acrylic
    // 明显比稳定克制的 Mica 更像毛玻璃。
    QLinearGradient luminosity(rect.topLeft(), rect.bottomLeft());
    luminosity.setColorAt(0.0,
                          withOpacity(Qt::white,
                                      (options.dark ? 0.075 : 0.18) * activity));
    luminosity.setColorAt(0.40, Qt::transparent);
    luminosity.setColorAt(1.0,
                          withOpacity(Qt::black,
                                      (options.dark ? 0.11 : 0.035) * activity));
    painter.fillRect(rect, luminosity);

    QRadialGradient accentField(QPointF(rect.right() - rect.width() * 0.10,
                                        rect.top() + rect.height() * 0.02),
                                qMax<qreal>(1.0, radius));
    accentField.setColorAt(0.0,
                           withOpacity(resolvedAccent(options),
                                       (options.dark ? 0.25 : 0.24) * activity));
    accentField.setColorAt(0.58,
                           withOpacity(resolvedAccent(options),
                                       (options.dark ? 0.075 : 0.065) * activity));
    accentField.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect, accentField);

    QColor coolField = resolvedAccent(options).lighter(options.dark ? 128 : 145);
    QRadialGradient coolGlow(QPointF(rect.right() - rect.width() * 0.04,
                                     rect.bottom() - rect.height() * 0.04),
                             qMax<qreal>(1.0, span * 0.86));
    coolGlow.setColorAt(0.0,
                        withOpacity(coolField,
                                    (options.dark ? 0.13 : 0.16) * activity));
    coolGlow.setColorAt(0.62,
                        withOpacity(coolField,
                                    (options.dark ? 0.035 : 0.028) * activity));
    coolGlow.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect, coolGlow);

    QRadialGradient glassField(QPointF(rect.left() + rect.width() * 0.12,
                                       rect.top() + rect.height() * 0.08),
                               qMax<qreal>(1.0, span * 0.72));
    glassField.setColorAt(0.0,
                          withOpacity(Qt::white,
                                      (options.dark ? 0.065 : 0.12) * activity));
    glassField.setColorAt(0.44,
                          withOpacity(Qt::white,
                                      (options.dark ? 0.018 : 0.028) * activity));
    glassField.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect, glassField);
}

} // namespace

WindowBackdropMaterialOptions WindowBackdropMaterialOptions::forTheme(
    bool isDark,
    const QColor& normal,
    const QColor& accent)
{
    WindowBackdropMaterialOptions options;
    options.dark = isDark;
    options.normalColor = normal;
    options.accentColor = accent;
    options.mica = Material::Mica::get(isDark);
    options.acrylic = Material::Acrylic::get(isDark);
    return options;
}

void WindowBackdropMaterial::paint(QPainter& painter,
                                   const QRectF& rect,
                                   const WindowBackdropMaterialOptions& options)
{
    switch (options.effect) {
    case BackdropEffect::Mica:
        paintMica(painter, rect, options);
        break;
    case BackdropEffect::Acrylic:
        paintAcrylic(painter, rect, options);
        break;
    case BackdropEffect::Solid:
    default:
        paintNormal(painter, rect, options);
        break;
    }
}

void WindowBackdropMaterial::paintNormal(QPainter& painter,
                                         const QRectF& rect,
                                         const WindowBackdropMaterialOptions& options)
{
    if (rect.isEmpty())
        return;

    painter.save();
    painter.setOpacity(1.0);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, resolvedNormal(options));
    painter.restore();
}

void WindowBackdropMaterial::paintMica(QPainter& painter,
                                       const QRectF& rect,
                                       const WindowBackdropMaterialOptions& options)
{
    if (rect.isEmpty())
        return;

    WindowBackdropMaterialOptions micaOptions = options;
    micaOptions.effect = BackdropEffect::Mica;
    painter.save();
    painter.setOpacity(1.0);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, opaqueBaseColor(micaOptions));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    paintMicaColorFields(painter, rect, micaOptions);
    paintGrain(painter,
               rect,
               micaOptions,
               micaOptions.active ? 0.018 : 0.010);
    painter.restore();
}

void WindowBackdropMaterial::paintAcrylic(QPainter& painter,
                                          const QRectF& rect,
                                          const WindowBackdropMaterialOptions& options)
{
    if (rect.isEmpty())
        return;

    WindowBackdropMaterialOptions acrylicOptions = options;
    acrylicOptions.effect = BackdropEffect::Acrylic;
    painter.save();
    painter.setOpacity(1.0);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, opaqueBaseColor(acrylicOptions));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    paintAcrylicColorFields(painter, rect, acrylicOptions);
    paintGrain(painter,
               rect,
               acrylicOptions,
               acrylicOptions.active ? 0.052 : 0.030);
    painter.restore();
}

QColor WindowBackdropMaterial::opaqueBaseColor(
    const WindowBackdropMaterialOptions& options)
{
    const QColor normal = resolvedNormal(options);
    switch (options.effect) {
    case BackdropEffect::Mica: {
        const QColor micaBase = opaqueColor(options.mica.baseColor, normal);
        QColor result = blendRgb(normal, micaBase, options.mica.opacity);
        if (!options.active)
            result = blendRgb(result, normal, 0.34);
        return result;
    }
    case BackdropEffect::Acrylic: {
        const QColor tint = opaqueColor(options.acrylic.tintColor, normal);
        const QColor luminosityTarget = options.dark ? tint.lighter(122)
                                                     : tint.lighter(106);
        const qreal activity = options.active ? 1.0 : 0.72;
        QColor result = blendRgb(normal,
                                 luminosityTarget,
                                 options.acrylic.luminosityOpacity * activity);
        result = blendRgb(result,
                          tint,
                          options.acrylic.tintOpacity * activity);
        if (!options.active)
            result = blendRgb(result, normal, 0.18);
        return result;
    }
    case BackdropEffect::Solid:
    default:
        return normal;
    }
}

} // namespace fluent::windowing
