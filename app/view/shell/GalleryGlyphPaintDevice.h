#ifndef GALLERYGLYPHPAINTDEVICE_H
#define GALLERYGLYPHPAINTDEVICE_H

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QtMath>
#include "components/spatial/SpatialRuntime.h"

namespace fluent::gallery::spatial_render {

// Use Qt's rasterizer for low-DPI shaped text; geometry and composition stay on the GPU.
// zh_CN: 低 DPI 已排版字形由 Qt 栅格化，几何与合成仍由 GPU 完成。
inline bool needsNativeGlyphCoverage(qreal nativeDpr)
{
    return spatial::SpatialRuntime::needsNativeGlyphCoverage(nativeDpr);
}

inline qreal glyphRasterDpr(const QFont& font, qreal nativeDpr, qreal cacheDpr)
{
    // Unhinted outlines retain more detail at cache density. Hinted glyphs must
    // keep the output pixel grid; rerasterizing them at another size changes
    // their grid fitting and can reduce native small-text contrast.
    // zh_CN: 无 hinting 的轮廓按缓存密度保留细节；有 hinting 的字形保留输出像素网格，
    // 避免重新栅格化改变网格对齐并降低小字对比度。按实际字形字体选择，不依赖平台分支。
    return font.hintingPreference() == QFont::PreferNoHinting ? cacheDpr : nativeDpr;
}

class GalleryGlyphPaintDevice final : public QPaintDevice {
public:
    GalleryGlyphPaintDevice(QPaintDevice& target, qreal nativeDpr, QPointF rasterOrigin = {})
        : m_target(target), m_nativeDpr(nativeDpr), m_rasterOrigin(rasterOrigin), m_engine(*this)
    {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    qreal rasterDpr(const QFont& font) const
    {
        return glyphRasterDpr(font, m_nativeDpr, m_target.devicePixelRatioF());
    }
    int glyphItems() const { return m_engine.glyphItems; }
    int glyphTiles() const { return m_engine.glyphTiles; }
    int systemClipApplications() const { return m_engine.systemClipApplications; }
    static const GalleryGlyphPaintDevice* fromPainter(const QPainter& painter)
    {
        const auto* engine = dynamic_cast<const Engine*>(painter.paintEngine());
        return engine ? &engine->device : nullptr;
    }
    static bool delegatesToOpenGL(const QPainter& painter)
    {
        const auto* device = fromPainter(painter);
        return device && device->m_target.paintEngine()->type() == QPaintEngine::OpenGL2;
    }

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
            return m_target.width();
        case PdmHeight:
            return m_target.height();
        case PdmWidthMM:
            return m_target.widthMM();
        case PdmHeightMM:
            return m_target.heightMM();
        case PdmNumColors:
            return m_target.colorCount();
        case PdmDepth:
            return m_target.depth();
        case PdmDpiX:
            return m_target.logicalDpiX();
        case PdmDpiY:
            return m_target.logicalDpiY();
        case PdmPhysicalDpiX:
            return m_target.physicalDpiX();
        case PdmPhysicalDpiY:
            return m_target.physicalDpiY();
        case PdmDevicePixelRatio:
            return qRound(m_target.devicePixelRatioF());
        case PdmDevicePixelRatioScaled:
            return qRound(m_target.devicePixelRatioF() * devicePixelRatioFScale());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        case PdmDevicePixelRatioF_EncodedA:
        case PdmDevicePixelRatioF_EncodedB:
            return QPaintDevice::encodeMetricF(metric, m_target.devicePixelRatioF());
#endif
        default:
            return 0;
        }
    }

private:
    static QPaintEngine::PaintEngineFeatures targetFeatures(QPaintDevice& target)
    {
        QPaintEngine::PaintEngineFeatures features;
        const auto* engine = target.paintEngine();
        if (!engine)
            return features;
        // A forwarding engine cannot promise operations its delegate cannot draw.
        // In particular, native line editors select XOR caret painting when
        // RasterOpModes is advertised; the OpenGL painter cannot execute it.
        // zh_CN: 转发引擎只能声明后端支持的能力；否则输入框会选择 OpenGL 不支持
        // 的 XOR 光标绘制，导致着色器状态无效甚至崩溃。
        for (quint32 bit = 1; bit != 0; bit <<= 1) {
            const auto feature = static_cast<QPaintEngine::PaintEngineFeature>(bit);
            if (engine->hasFeature(feature))
                features |= feature;
        }
        return features;
    }

    class Engine final : public QPaintEngine {
        friend class GalleryGlyphPaintDevice;

    public:
        explicit Engine(GalleryGlyphPaintDevice& device)
            : QPaintEngine(targetFeatures(device.m_target)), device(device)
        {}
        Type type() const override { return User; }
        bool begin(QPaintDevice*) override
        {
            const bool active = painter.begin(&device.m_target);
            setActive(active);
            return active;
        }
        bool end() override
        {
            if (systemClipApplied)
                painter.restore();
            systemClipApplied = false;
            const bool ended = painter.end();
            setActive(false);
            return ended;
        }
        void updateState(const QPaintEngineState& state) override
        {
            const auto dirty = state.state();
            if (dirty & (DirtyTransform | DirtyClipRegion | DirtyClipPath | DirtyClipEnabled))
                restoreSystemClip();
            if (dirty & DirtyTransform) {
                const qreal inverseDpr = 1 / device.devicePixelRatioF();
                painter.setWorldTransform(state.transform() *
                                          QTransform::fromScale(inverseDpr, inverseDpr));
            }
            if (dirty & DirtyPen)
                painter.setPen(state.pen());
            if (dirty & DirtyBrush)
                painter.setBrush(state.brush());
            if (dirty & DirtyBrushOrigin)
                painter.setBrushOrigin(state.brushOrigin());
            if (dirty & DirtyFont)
                painter.setFont(state.font());
            if (dirty & DirtyBackground)
                painter.setBackground(state.backgroundBrush());
            if (dirty & DirtyBackgroundMode)
                painter.setBackgroundMode(state.backgroundMode());
            if (dirty & DirtyHints) {
                painter.setRenderHints(painter.renderHints(), false);
                painter.setRenderHints(state.renderHints());
            }
            if (dirty & DirtyCompositionMode)
                painter.setCompositionMode(state.compositionMode());
            if (dirty & DirtyOpacity)
                painter.setOpacity(state.opacity());
            if (dirty & DirtyClipRegion)
                painter.setClipRegion(state.clipRegion(), state.clipOperation());
            if (dirty & DirtyClipPath)
                painter.setClipPath(state.clipPath(), state.clipOperation());
            if (dirty & DirtyClipEnabled)
                painter.setClipping(state.isClipEnabled());
        }

        void drawRects(const QRect* rects, int count) override
        {
            forward([&] { painter.drawRects(rects, count); });
        }
        void drawRects(const QRectF* rects, int count) override
        {
            forward([&] { painter.drawRects(rects, count); });
        }
        void drawLines(const QLine* lines, int count) override
        {
            forward([&] { painter.drawLines(lines, count); });
        }
        void drawLines(const QLineF* lines, int count) override
        {
            forward([&] { painter.drawLines(lines, count); });
        }
        void drawPoints(const QPoint* points, int count) override
        {
            forward([&] { painter.drawPoints(points, count); });
        }
        void drawPoints(const QPointF* points, int count) override
        {
            forward([&] { painter.drawPoints(points, count); });
        }
        void drawEllipse(const QRect& rect) override
        {
            forward([&] { painter.drawEllipse(rect); });
        }
        void drawEllipse(const QRectF& rect) override
        {
            forward([&] { painter.drawEllipse(rect); });
        }
        void drawPath(const QPainterPath& path) override
        {
            forward([&] { painter.drawPath(path); });
        }
        void drawPixmap(const QRectF& rect, const QPixmap& pixmap, const QRectF& source) override
        {
            forward([&] { painter.drawPixmap(rect, pixmap, source); });
        }
        void drawImage(const QRectF& rect, const QImage& image, const QRectF& source,
                       Qt::ImageConversionFlags flags) override
        {
            forward([&] { painter.drawImage(rect, image, source, flags); });
        }
        void drawTiledPixmap(const QRectF& rect, const QPixmap& pixmap,
                             const QPointF& offset) override
        {
            forward([&] { painter.drawTiledPixmap(rect, pixmap, offset); });
        }
        void drawPolygon(const QPoint* points, int count, PolygonDrawMode mode) override
        {
            polygon(points, count, mode);
        }
        void drawPolygon(const QPointF* points, int count, PolygonDrawMode mode) override
        {
            polygon(points, count, mode);
        }

        void drawTextItem(const QPointF& position, const QTextItem& item) override
        {
            forward([&] {
                // Transformed, patterned or opaque-background text keeps Qt's semantics.
                // The compatibility case is ordinary translated widget text.
                const QPainter* sourcePainter = QPaintEngine::painter();
                if (painter.worldTransform().type() > QTransform::TxTranslate ||
                    painter.pen().brush().style() != Qt::SolidPattern ||
                    (sourcePainter && sourcePainter->backgroundMode() == Qt::OpaqueMode) ||
                    painter.compositionMode() != QPainter::CompositionMode_SourceOver) {
                    painter.paintEngine()->syncState();
                    painter.paintEngine()->drawTextItem(position, item);
                    return;
                }
                qreal dpr = device.rasterDpr(item.font());
                const QFontMetricsF metrics(item.font());
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
                // Match QImage's fixed-point DPR metric on older Qt versions.
                const qreal scale = QPaintDevice::devicePixelRatioFScale();
                dpr = qFloor(dpr * scale) / scale;
#endif
                // Qt's rasterizer can round negative glyph origins differently
                // (notably Qt 5). Keep seam-crossing origins in a positive guard
                // band while reserving that guard inside the fixed tile budget.
                // zh_CN: Qt 栅格化对负字形原点的取整可能不同（尤其是 Qt 5）；在固定
                // 分块预算内预留保护区，让跨缝字形原点为正，保留原始亚像素覆盖率。
                const QSize padding(
                    qCeil((metrics.maxWidth() + qMax(qreal(0), -metrics.minRightBearing()) + 2) *
                          dpr),
                    qCeil((metrics.descent() + 2) * dpr));
                if (padding.width() >= 256 || padding.height() >= 64) {
                    // Oversized glyphs retain Qt's direct path instead of
                    // exceeding the fixed tile budget or leaving a partial guard.
                    painter.paintEngine()->syncState();
                    painter.paintEngine()->drawTextItem(position, item);
                    return;
                }
                const QPointF origin(painter.worldTransform().dx() + device.m_rasterOrigin.x(),
                                     painter.worldTransform().dy() + device.m_rasterOrigin.y());
                const QPointF baseline = position + origin;
                QRectF ink = metrics.boundingRect(item.text());
                ink = ink.united(
                    QRectF(0, -item.ascent(), item.width(), item.ascent() + item.descent()));
                ink = ink.adjusted(-2, -2, 2, 2).translated(baseline);
                ++glyphItems;
                // Bound fallback-font overestimates and offscreen text to the
                // visible paint strip before allocating or iterating glyph tiles.
                const qreal targetDpr = device.m_target.devicePixelRatioF();
                QRectF visible(0, 0, device.m_target.width() / targetDpr,
                               device.m_target.height() / targetDpr);
                if (painter.hasClipping())
                    visible = visible.intersected(
                        painter.worldTransform().mapRect(painter.clipBoundingRect()));
                ink = ink.intersected(visible.translated(device.m_rasterOrigin));
                if (ink.isEmpty())
                    return;
                const QRect pixels = QTransform::fromScale(dpr, dpr).mapRect(ink).toAlignedRect();
                // One 256 KiB CPU tile plus its upload is reserved by the cache plan.
                // Tiles and their Qt texture-cache keys die immediately; no glyph LRU.
                const QSize contentSize = QSize(512, 128) - padding;
                for (int top = pixels.top(); top <= pixels.bottom(); top += contentSize.height()) {
                    for (int left = pixels.left(); left <= pixels.right();
                         left += contentSize.width()) {
                        const QSize size(qMin(contentSize.width(), pixels.right() - left + 1),
                                         qMin(contentSize.height(), pixels.bottom() - top + 1));
                        QImage glyph(size + padding, QImage::Format_ARGB32_Premultiplied);
                        if (glyph.isNull())
                            continue;
                        ++glyphTiles;
                        glyph.fill(Qt::transparent);
                        const QPointF tileOrigin(left / dpr, top / dpr);
                        QPainter raster(&glyph);
                        raster.setPen(painter.pen());
                        // Keep opacity in the glyph rasterizer: native font backends
                        // may select different coverage than a later image blend.
                        raster.setOpacity(painter.opacity());
                        raster.setRenderHints(raster.renderHints(), false);
                        raster.setRenderHints(painter.renderHints());
                        raster.setBackground(painter.background());
                        raster.setBackgroundMode(painter.backgroundMode());
                        // Keep the shaped baseline unchanged and translate in
                        // physical pixels; a logical-pixel round trip can alter
                        // glyph phase at fractional strip origins.
                        // zh_CN: 保留已排版基线，直接用物理像素平移，避免逻辑坐标往返
                        // 在分数分块原点处改变字形的亚像素相位。
                        raster.setWorldTransform(
                            QTransform(dpr, 0, 0, dpr, origin.x() * dpr + padding.width() - left,
                                       origin.y() * dpr + padding.height() - top));
                        // QTextItem retains the fallback font, shaping, bidi order and decorations.
                        // Never re-layout item.text() as a new drawText call.
                        // QPainter paints underline/strikeout separately on the outer
                        // device. Dispatch directly to avoid drawing decorations twice.
                        raster.paintEngine()->syncState();
                        raster.paintEngine()->drawTextItem(position, item);
                        raster.end();
                        glyph.setDevicePixelRatio(dpr);
                        painter.save();
                        painter.setOpacity(1);
                        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
                        painter.drawImage(QRectF(tileOrigin - origin, QSizeF(size) / dpr), glyph,
                                          QRectF(QPointF(padding.width(), padding.height()), size));
                        painter.restore();
                    }
                }
            });
        }

        int glyphItems = 0;
        int glyphTiles = 0;
        int systemClipApplications = 0;

    private:
        GalleryGlyphPaintDevice& device;
        QPainter painter;
        QRegion lastSystemClip;
        QPainterPath systemClipPath;
        bool systemClipApplied = false;

        void restoreSystemClip()
        {
            if (!systemClipApplied)
                return;
            // Paint state may change while this device-space clip is active.
            // Restore only its clip layer, retaining the latest non-clip state.
            const QPen pen = painter.pen();
            const QBrush brush = painter.brush(), background = painter.background();
            const QPointF origin = painter.brushOrigin();
            const QFont font = painter.font();
            const Qt::BGMode backgroundMode = painter.backgroundMode();
            const auto hints = painter.renderHints();
            const auto composition = painter.compositionMode();
            const qreal opacity = painter.opacity();
            const QTransform transform = painter.worldTransform();
            painter.restore();
            systemClipApplied = false;
            painter.setPen(pen);
            painter.setBrush(brush);
            painter.setBackground(background);
            painter.setBrushOrigin(origin);
            painter.setFont(font);
            painter.setBackgroundMode(backgroundMode);
            painter.setRenderHints(painter.renderHints(), false);
            painter.setRenderHints(hints);
            painter.setCompositionMode(composition);
            painter.setOpacity(opacity);
            painter.setWorldTransform(transform);
        }

        template <class Draw> void forward(Draw draw)
        {
            const QRegion clip = systemClip();
            if (clip != lastSystemClip || (!systemClipApplied && !clip.isEmpty()))
                restoreSystemClip();
            if (clip != lastSystemClip) {
                lastSystemClip = clip;
                systemClipPath = {};
                const qreal dpr = device.devicePixelRatioF();
                for (const QRect& rect : clip)
                    systemClipPath.addRect(QRectF(rect.x() / dpr, rect.y() / dpr,
                                                  rect.width() / dpr, rect.height() / dpr));
            }
            if (!systemClipApplied && !clip.isEmpty()) {
                // A widget usually draws many primitives under the same clip.
                // Retain that stencil until a clip/transform changes instead of
                // retessellating it for every particle, glyph or rounded rect.
                painter.save();
                const QTransform transform = painter.worldTransform();
                painter.resetTransform();
                painter.setClipPath(systemClipPath, Qt::IntersectClip);
                painter.setWorldTransform(transform);
                systemClipApplied = true;
                ++systemClipApplications;
            }
            draw();
        }
        template <class Point> void polygon(const Point* points, int count, PolygonDrawMode mode)
        {
            forward([&] {
                if (mode == PolylineMode)
                    painter.drawPolyline(points, count);
                else if (mode == ConvexMode)
                    painter.drawConvexPolygon(points, count);
                else
                    painter.drawPolygon(points, count,
                                        mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
            });
        }
    };
    QPaintDevice& m_target;
    qreal m_nativeDpr;
    QPointF m_rasterOrigin;
    mutable Engine m_engine;
};

} // namespace fluent::gallery::spatial_render
#endif
