#ifndef FLUENT_IMAGEVIEWERPIXMAPCACHE_P_H
#define FLUENT_IMAGEVIEWERPIXMAPCACHE_P_H

#include <QPixmap>

namespace fluent::scrolling::detail {

// Two power-of-two reductions, never one pixmap per zoom value. The larger image is at most
// half the source in each dimension; the smaller one has an additional byte budget.
// zh_CN: 仅保留两个二分缩略级别，不为每个缩放值建图。较大缓存的宽高至多为源图的一半，
// 较小缓存另受字节预算限制。仅在 GUI 线程访问。
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
