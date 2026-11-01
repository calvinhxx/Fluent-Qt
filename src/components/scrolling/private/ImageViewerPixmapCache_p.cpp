#include "ImageViewerPixmapCache_p.h"

#include <algorithm>
#include <utility>

namespace fluent::scrolling::detail {

ImageViewerPixmapCache::ImageViewerPixmapCache(qint64 smallerLevelBudget)
    : m_smallerLevelBudget(std::max<qint64>(0, smallerLevelBudget))
{}

const QPixmap& ImageViewerPixmapCache::sourceForScale(const QPixmap& original, qreal deviceScale)
{
    if (m_sourceKey != original.cacheKey()) {
        clear();
        m_sourceKey = original.cacheKey();
    }
    if (original.isNull() || deviceScale >= 0.5)
        return original;

    qreal level = 1.0;
    while (level * 0.5 >= deviceScale && level > 1.0 / 256.0)
        level *= 0.5;
    const QSize size(std::max(1, qRound(original.width() * level)), std::max(1, qRound(original.height() * level)));
    if (m_current.size() == size)
        return m_current;
    if (m_previous.size() == size) {
        m_current.swap(m_previous);
        return m_current;
    }

    // Further reductions start from the nearest larger cached image instead of rescanning the original.
    // zh_CN: 继续缩小时，从最近的较大缓存生成，避免再次遍历原图。
    const QPixmap* source = &original;
    for (const QPixmap* cached : {&m_current, &m_previous}) {
        if (!cached->isNull() && cached->width() >= size.width() && cached->height() >= size.height()
            && pixelBytes(*cached) < pixelBytes(*source))
            source = cached;
    }
    QPixmap reduced = source->scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (reduced.isNull())
        return original;
    reduced.setDevicePixelRatio(1.0);
    m_previous = std::move(m_current);
    m_current = std::move(reduced);
    if (std::min(pixelBytes(m_current), pixelBytes(m_previous)) > m_smallerLevelBudget)
        m_previous = QPixmap();
    return m_current;
}

void ImageViewerPixmapCache::clear()
{
    m_sourceKey = 0;
    m_current = QPixmap();
    m_previous = QPixmap();
}

qint64 ImageViewerPixmapCache::retainedBytes() const
{
    return pixelBytes(m_current) + pixelBytes(m_previous);
}

qint64 ImageViewerPixmapCache::pixelBytes(const QPixmap& pixmap)
{
    return ((static_cast<qint64>(pixmap.width()) * pixmap.depth() + 31) / 32) * 4 * pixmap.height();
}

} // namespace fluent::scrolling::detail
