#ifndef GALLERYGLYPHIMAGECACHE_H
#define GALLERYGLYPHIMAGECACHE_H

#include <QCache>
#include <QCryptographicHash>
#include <QImage>
#include "GallerySpatialRenderPolicy.h"

namespace fluent::gallery::spatial_render {

// Retain exact raster results so Qt can reuse their image texture keys. Shaping,
// fallback fonts, hinting and fractional coverage remain owned by the rasterizer.
// zh_CN: 复用精确栅格结果及 Qt 纹理键；字形塑形、回退字体、hinting 和亚像素覆盖仍由原栅格器决定。
class GlyphImageCache {
public:
    explicit GlyphImageCache(int budget = kGlyphCacheBytes) : m_images(qBound(0, budget, kGlyphCacheBytes)) {}

    QImage retain(const QImage& image)
    {
        if (image.isNull() || image.format() != QImage::Format_ARGB32_Premultiplied)
            return image;
        // The fixed admission charge also bounds keys, nodes and image metadata.
        // Qt's corresponding texture storage is reserved separately by the plan.
        constexpr int metadataBytes = 1024;
        const qint64 bytes = qint64(image.bytesPerLine()) * image.height();
        if (bytes > m_images.maxCost() - metadataBytes)
            return image;
        QByteArray key = QCryptographicHash::hash(
            QByteArray::fromRawData(reinterpret_cast<const char*>(image.constBits()), int(bytes)),
            QCryptographicHash::Sha256);
        const int width = image.width(), height = image.height();
        const qreal dpr = image.devicePixelRatioF();
        key.append(reinterpret_cast<const char*>(&width), sizeof(width));
        key.append(reinterpret_cast<const char*>(&height), sizeof(height));
        key.append(reinterpret_cast<const char*>(&dpr), sizeof(dpr));
        if (const auto* cached = m_images.object(key); cached && *cached == image)
            return *cached;
        m_images.insert(key, new QImage(image), int(bytes) + metadataBytes);
        return image;
    }

    void clear() { m_images.clear(); }
    int cost() const { return m_images.totalCost(); }
    int entryCount() const { return m_images.size(); }

private:
    QCache<QByteArray, QImage> m_images;
};

} // namespace fluent::gallery::spatial_render
#endif
