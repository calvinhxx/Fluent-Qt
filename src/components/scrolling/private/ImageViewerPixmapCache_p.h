#ifndef FLUENT_IMAGEVIEWERPIXMAPCACHE_P_H
#define FLUENT_IMAGEVIEWERPIXMAPCACHE_P_H

#include <QPixmap>

namespace fluent::scrolling::detail {

// Caches up to two power-of-two downscaled levels (at most half source dimensions;
// smaller level bounded by byte budget) for GUI-thread rendering.
class ImageViewerPixmapCache {
public:
    explicit ImageViewerPixmapCache(qint64 smallerLevelBudget = 16 * 1024 * 1024);

    const QPixmap& sourceForScale(const QPixmap& original, qreal deviceScale);
    void clear();
    qint64 retainedBytes() const;

private:
    static qint64 pixelBytes(const QPixmap& pixmap);

    qint64 m_sourceKey = 0;
    qint64 m_smallerLevelBudget;
    QPixmap m_current;
    QPixmap m_previous;
};

} // namespace fluent::scrolling::detail

#endif // FLUENT_IMAGEVIEWERPIXMAPCACHE_P_H
