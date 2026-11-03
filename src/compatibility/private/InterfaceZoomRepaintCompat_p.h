#pragma once

// Gallery-only: compensate for Cocoa's integer native-point damage regions.
// zh_CN: 仅供 Gallery 使用，适配 Cocoa 以整数原生点记录的刷新区域。
#include <QRect>
#include <QRegion>
#include <QVector>
#include <cmath>

namespace compatibility::detail {

inline int interfaceZoomDamageGrid(qreal factor)
{
    if (!std::isfinite(factor) || factor <= 0.0)
        return 0;
    for (int grid = 1; grid <= 1000; ++grid) {
        const qreal scaled = factor * grid;
        if (std::abs(scaled - std::round(scaled)) < 1e-8)
            return grid;
    }
    return 0;
}

inline QRect alignedInterfaceZoomDamage(const QRect& damage, int grid, const QRect& bounds)
{
    if (grid <= 0)
        return bounds;
    const QRect clipped = damage.intersected(bounds);
    if (clipped.isEmpty())
        return {};
    const int left = int(std::floor(qreal(clipped.x()) / grid)) * grid;
    const int top = int(std::floor(qreal(clipped.y()) / grid)) * grid;
    const int right = int(std::ceil(qreal(clipped.x() + clipped.width()) / grid)) * grid;
    const int bottom = int(std::ceil(qreal(clipped.y() + clipped.height()) / grid)) * grid;
    return QRect(left, top, right - left, bottom - top).intersected(bounds);
}

void installInterfaceZoomRepaintGuard();

inline QVector<QRect> interfaceZoomRasterTiles(const QRegion& region)
{
    if (region.isEmpty())
        return {};
    const QRect bounds = region.boundingRect();
    qint64 area = 0;
    for (const QRect& rect : region)
        area += qint64(rect.width()) * rect.height();
    // Keep dense damage in one pass. Separate distant patches so a cursor and
    // a progress indicator cannot allocate/clear an almost whole-window tile.
    // zh_CN: 密集区域保留一次绘制；分开远距离的小区域，避免光标和进度条触发近整窗图块分配与清零。
    if (qint64(bounds.width()) * bounds.height() <= area * 4)
        return {bounds};
    QVector<QRect> tiles;
    tiles.reserve(region.rectCount());
    for (const QRect& rect : region)
        tiles.append(rect);
    return tiles;
}

} // namespace compatibility::detail
