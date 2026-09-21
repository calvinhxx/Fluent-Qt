#ifndef GALLERYSPATIALRENDERPOLICY_H
#define GALLERYSPATIALRENDERPOLICY_H

#include <QSizeF>
#include <QtMath>
#include <array>

namespace fluent::gallery::spatial_render {

// Two panel textures share a multisampled paint target and a matching resolve
// target. Include color and potentially separate depth/stencil storage.
constexpr qint64 kCacheBudgetBytes = 192LL * 1024 * 1024;
constexpr qint64 kCacheBytesPerPixel = 4;
constexpr int kPaintSamples = 2;

struct CachePlan {
    std::array<QSize, 2> sizes{};
    qreal dpr = 0;
    qint64 pixels = 0;
    QSize paintSize;
    qint64 estimatedBytes = 0;
    bool valid() const { return dpr > 0; }
};

// Preserve perspective sampling headroom. Large panels reuse a shorter paint
// target in strips instead of lowering density to fit a full-height MSAA buffer.
inline CachePlan planCaches(const std::array<QSizeF, 2>& panels, qreal nativeDpr, int maxDimension,
                            qreal maxExtraSampling = 2, qint64 budget = kCacheBudgetBytes,
                            int paintSamples = kPaintSamples)
{
    if (!qIsFinite(nativeDpr) || nativeDpr <= 0 || maxDimension <= 0 || budget <= 0 ||
        paintSamples <= 1)
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
            plan.paintSize = plan.paintSize.expandedTo(plan.sizes[i]);
        }
        if (!fits || plan.paintSize.isEmpty())
            continue;
        const qint64 textureBytes = plan.pixels * kCacheBytesPerPixel;
        const qint64 paintRowBytes = qint64(plan.paintSize.width()) * (12 * paintSamples + 4);
        const qint64 availableRows = (budget - textureBytes) / paintRowBytes;
        if (availableRows < qMin(32, plan.paintSize.height()))
            continue;
        plan.paintSize.setHeight(int(qMin(qint64(plan.paintSize.height()), availableRows)));
        plan.estimatedBytes = textureBytes + paintRowBytes * plan.paintSize.height();
        if (fits && plan.estimatedBytes <= budget)
            return plan;
    }
    return {};
}
} // namespace fluent::gallery::spatial_render
#endif
