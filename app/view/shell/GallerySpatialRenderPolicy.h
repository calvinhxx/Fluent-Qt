#ifndef GALLERYSPATIALRENDERPOLICY_H
#define GALLERYSPATIALRENDERPOLICY_H

#include <QSizeF>
#include <QtMath>
#include <array>

namespace fluent::gallery::spatial_render {

// Budget for both panel caches, excluding the window's own framebuffer. Reserve
// 12 bytes per pixel for RGBA8 and drivers with separate depth/stencil storage.
constexpr qint64 kCacheBudgetBytes = 192LL * 1024 * 1024;
constexpr qint64 kCacheBytesPerPixel = 12;

struct CachePlan {
    std::array<QSize, 2> sizes{};
    qreal dpr = 0;
    qint64 pixels = 0;
    bool valid() const { return dpr > 0; }
};

// Keep the extra samples that protect perspective text when resources allow.
// Never render below the window's native density to make a cache fit.
inline CachePlan planCaches(const std::array<QSizeF, 2>& panels, qreal nativeDpr, int maxDimension,
                            qreal maxExtraSampling = 2, qint64 budget = kCacheBudgetBytes)
{
    if (!qIsFinite(nativeDpr) || nativeDpr <= 0 || maxDimension <= 0 || budget <= 0)
        return {};
    for (qreal extra : {2., 1.75, 1.5, 1.25, 1.}) {
        if (extra > maxExtraSampling)
            continue;
        CachePlan plan;
        plan.dpr = nativeDpr * extra;
        bool fits = true;
        for (size_t i = 0; i < panels.size(); ++i) {
            if (panels[i].isEmpty())
                continue;
            const qreal width = panels[i].width() * plan.dpr;
            const qreal height = panels[i].height() * plan.dpr;
            if (!qIsFinite(width) || !qIsFinite(height) || width > maxDimension ||
                height > maxDimension) {
                fits = false;
                break;
            }
            plan.sizes[i] = QSize(qCeil(width), qCeil(height));
            plan.pixels += qint64(plan.sizes[i].width()) * plan.sizes[i].height();
        }
        if (fits && plan.pixels <= budget / kCacheBytesPerPixel)
            return plan;
    }
    return {};
}
} // namespace fluent::gallery::spatial_render
#endif
