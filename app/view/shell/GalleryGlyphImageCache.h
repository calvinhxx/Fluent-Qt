#ifndef GALLERYGLYPHIMAGECACHE_H
#define GALLERYGLYPHIMAGECACHE_H

#include <QCache>
#include <QHash>
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
        // The key only selects a candidate; the exact comparison below decides reuse.
        // A cryptographic digest cost more than rasterizing the tile, notably with a
        // Debug Qt runtime. A nonzero seed selects Qt's accelerated hash where available.
        // zh_CN: 键只用于挑选候选项，是否复用由下方精确比较决定。加密摘要比栅格化本身更慢
        // （Debug Qt 运行时尤甚）；非零种子在可用时启用 Qt 的硬件加速哈希。
        constexpr uint seed = 0x9e3779b9u;
        const auto digest = qHashBits(image.constBits(), size_t(bytes), seed);
        QByteArray key(reinterpret_cast<const char*>(&digest), int(sizeof(digest)));
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
