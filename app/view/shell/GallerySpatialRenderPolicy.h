#ifndef GALLERYSPATIALRENDERPOLICY_H
#define GALLERYSPATIALRENDERPOLICY_H

#include <QRegion>
#include <QSizeF>
#include <QtMath>
#include <algorithm>
#include <array>
#include <vector>

namespace fluent::gallery::spatial_render {

// Two panel textures share a multisampled paint target and a matching resolve
// target. Include color, potentially separate depth/stencil storage, and the
// native-density backdrop texture when the window uses a painted material.
constexpr qint64 kCacheBudgetBytes = 192LL * 1024 * 1024;
constexpr qint64 kCacheBytesPerPixel = 4;
constexpr int kPaintSamples = 2;
// One bounded glyph tile and its upload, only for the low-DPI native adapter.
constexpr qint64 kGlyphScratchBytes = 512LL * 1024;
// Retained raster images and their texture copies share the same overall budget.
constexpr int kGlyphCacheBytes = 2 * 1024 * 1024;
constexpr qint64 kGlyphWorkingSetBytes = kGlyphScratchBytes + 2LL * kGlyphCacheBytes;
// A continuous update covering at least this share of a panel paints at native density
// and is refined at cache density once updates pause for kRefineDelayMs.
// zh_CN: 连续更新覆盖面板的比例达到此值时按原生密度绘制，更新停止 kRefineDelayMs 后补全质量。
constexpr qreal kInteractiveDamageShare = .3;
constexpr int kInteractiveIntervalMs = 120;
constexpr int kRefineDelayMs = 160;
// Native-density frames slower than kSlowFrameMs for kSlowFrameStreak frames in a row
// make the Gallery suggest 2D once.
// zh_CN: 原生密度帧连续 kSlowFrameStreak 帧慢于 kSlowFrameMs 时，Gallery 提示一次 2D。
constexpr qreal kSlowFrameMs = 33;
constexpr int kSlowFrameStreak = 60;

// Per-frame texture cost and sustained-slowness streak. Both panels of a frame count
// together; a frame without native-density updates neither confirms nor clears a streak.
// zh_CN: 按整帧统计纹理耗时与持续慢帧；两块面板合计，没有原生密度更新的帧不计入也不清零。
struct SlowFrameTracker {
    qreal costMs = 0;            // every frame with texture work; drives adaptive thresholds
    qreal interactiveCostMs = 0; // native-density frames only; drives the streak
    int streak = 0;
    bool reported = false;

    // Returns true once, when the streak first reaches kSlowFrameStreak.
    bool record(qreal frameMs, bool interactive)
    {
        const auto average = [frameMs](qreal current) { return current > 0 ? current * .8 + frameMs * .2 : frameMs; };
        costMs = average(costMs);
        if (!interactive)
            return false;
        interactiveCostMs = average(interactiveCostMs);
        streak = interactiveCostMs > kSlowFrameMs ? streak + 1 : 0;
        if (reported || streak < kSlowFrameStreak)
            return false;
        reported = true;
        return true;
    }
};

// Dense displays already resolve small glyphs. Keep perspective headroom without
// spending the shared paint budget on a second full doubling of both dimensions.
inline qreal preferredExtraSampling(qreal nativeDpr)
{
    return nativeDpr >= 2 ? 1.5 : 2;
}

struct CachePlan {
    std::array<QSize, 2> sizes{};
    qreal dpr = 0;
    qint64 pixels = 0;
    QSize paintSize;
    qint64 backdropBytes = 0;
    qint64 glyphScratchBytes = 0;
    qint64 estimatedBytes = 0;
    bool valid() const { return dpr > 0; }
};

// Preserve perspective sampling headroom. Large panels reuse a shorter paint
// target in strips instead of lowering density to fit a full-height MSAA buffer.
inline CachePlan planCaches(const std::array<QSizeF, 2>& panels,
    qreal nativeDpr,
    int maxDimension,
    qreal maxExtraSampling = 2,
    qint64 budget = kCacheBudgetBytes,
    int paintSamples = kPaintSamples,
    qint64 backdropBytes = 0,
    qint64 glyphScratchBytes = 0,
    qint64 reservedBytes = 0,
    int paintSampleBytes = 12)
{
    if (!qIsFinite(nativeDpr) || nativeDpr <= 0 || maxDimension <= 0 || budget <= 0 || paintSamples <= 1
        || backdropBytes < 0 || backdropBytes >= budget || glyphScratchBytes < 0
        || glyphScratchBytes >= budget - backdropBytes || reservedBytes < 0
        || reservedBytes >= budget - backdropBytes - glyphScratchBytes
        || (paintSampleBytes != 8 && paintSampleBytes != 12))
        return {};
    for (qreal extra : {2., 1.75, 1.5, 1.25, 1.}) {
        if (extra > maxExtraSampling)
            continue;
        CachePlan plan;
        plan.backdropBytes = backdropBytes;
        plan.glyphScratchBytes = glyphScratchBytes;
        plan.dpr = nativeDpr * extra;
        bool fits = true;
        for (size_t i = 0; i < panels.size(); ++i) {
            if (panels[i].isEmpty())
                continue;
            const qreal width = panels[i].width() * plan.dpr;
            const qreal height = panels[i].height() * plan.dpr;
            if (!qIsFinite(width) || !qIsFinite(height) || width > maxDimension || height > maxDimension) {
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
        const qint64 paintRowBytes = qint64(plan.paintSize.width()) * (paintSampleBytes * paintSamples + 4);
        const qint64 availableRows = (budget - backdropBytes - glyphScratchBytes - reservedBytes - textureBytes)
            / paintRowBytes;
        if (availableRows < qMin(32, plan.paintSize.height()))
            continue;
        plan.paintSize.setHeight(int(qMin(qint64(plan.paintSize.height()), availableRows)));
        plan.estimatedBytes = backdropBytes + glyphScratchBytes + textureBytes
            + paintRowBytes * plan.paintSize.height();
        if (fits && plan.estimatedBytes <= budget - reservedBytes)
            return plan;
    }
    return {};
}

// Texture damage accumulated for one panel since `base`, the revision its consumer
// last painted. Logical panel coordinates; `full` means the extent is unknown.
// zh_CN: 自 base（使用方最后绘制的版本）以来累积的面板脏区，逻辑坐标；full 表示范围未知。
struct PanelDamage {
    quint64 base = 0;
    QRegion region;
    bool full = true;

    void add(const QRegion& dirty)
    {
        if (dirty.isEmpty())
            full = true;
        if (full)
            return;
        region += dirty;
        if (region.rectCount() > 32)
            region = region.boundingRect();
    }
    // Hand the changes to a consumer and restart accumulation at revision.
    PanelDamage take(quint64 revision)
    {
        PanelDamage changes = *this;
        *this = {revision, {}, false};
        return changes;
    }
};

struct RowBand {
    int top = 0, bottom = 0;
    bool operator==(const RowBand& other) const { return top == other.top && bottom == other.bottom; }
};

inline int coveredRows(const std::vector<RowBand>& bands)
{
    int rows = 0;
    for (const auto& band : bands)
        rows += band.bottom - band.top;
    return rows;
}

// Texture rows covered by logical damage at dpr, aligned outward to quantum rows (the
// panel edge excepted) and merged when separated by less than a quantum-aligned gap.
// zh_CN: 将逻辑脏区换算为纹理行，向外对齐到 quantum（面板边缘除外），间隔过小的行段合并。
inline std::vector<RowBand> damageRows(const QRegion& damage, qreal dpr, int height, int quantum = 1)
{
    std::vector<RowBand> rows;
    if (!(dpr > 0) || height <= 0 || quantum <= 0)
        return rows;
    for (const QRect& rect : damage) {
        const int top = qBound(0, qFloor(rect.top() * dpr) / quantum * quantum, height);
        const int bottom = qBound(0, (qCeil((rect.top() + rect.height()) * dpr) + quantum - 1) / quantum * quantum,
            height);
        if (bottom > top)
            rows.push_back({top, bottom});
    }
    std::sort(rows.begin(), rows.end(), [](const RowBand& a, const RowBand& b) { return a.top < b.top; });
    std::vector<RowBand> merged;
    const int gap = qMax(8, quantum);
    for (const auto& band : rows) {
        if (!merged.empty() && band.top <= merged.back().bottom + gap)
            merged.back().bottom = qMax(merged.back().bottom, band.bottom);
        else
            merged.push_back(band);
    }
    return merged;
}

// Smallest logical row step that is a whole number of pixels at both densities, or 0.
// zh_CN: 在两种密度下都对应整数像素的最小逻辑行步长；不存在时为 0。
inline int sharedRowQuantum(qreal nativeDpr, qreal cacheDpr)
{
    const auto whole = [](qreal value) { return qAbs(value - qRound(value)) < 1e-6; };
    for (int step = 1; step <= 8; ++step) {
        if (whole(step * nativeDpr) && whole(step * cacheDpr))
            return step;
    }
    return 0;
}

struct PaintStrip {
    int top = 0, height = 0;
    int paintTop = 0, paintBottom = 0;
};

// Split rows [top, bottom) of a panel `height` rows tall into strips whose painted range,
// including `guard` rows on each side inside the panel, fits `capacity`. Heights are
// multiples of quantum except a strip that ends at `bottom`. Empty when nothing fits.
// zh_CN: 将 [top, bottom) 拆成条带，含两侧保护行的绘制范围不超过 capacity；除末条外高度为 quantum 的整数倍。
inline std::vector<PaintStrip> planStrips(int top, int bottom, int height, int capacity, int guard, int quantum = 1)
{
    std::vector<PaintStrip> strips;
    if (top < 0 || bottom > height || capacity <= 0 || guard < 0 || quantum <= 0)
        return strips;
    while (top < bottom) {
        const int paintTop = qMax(0, top - guard);
        const int lead = top - paintTop;
        int rows = bottom - top;
        if (lead + rows + qMin(guard, height - bottom) > capacity) {
            rows = (capacity - lead - guard) / quantum * quantum;
            if (rows <= 0)
                return {};
        }
        strips.push_back({top, rows, paintTop, qMin(height, top + rows + guard)});
        top += rows;
    }
    return strips;
}
} // namespace fluent::gallery::spatial_render
#endif
