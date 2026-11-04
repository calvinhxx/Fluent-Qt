#pragma once

// Gallery-only; compile against the matching Qt private headers, never a copied ABI.
// zh_CN: 仅供 Gallery 使用；针对匹配的 Qt 私有头编译，不复制 ABI 布局。
#include <QBackingStore>
#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsEffect>
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QPointer>
#include <QScopedValueRollback>
#include <QtWidgets/private/qgraphicseffect_p.h>
#include <qpa/qplatformbackingstore.h>

#include "InterfaceZoomRepaintQueues_p.h"
#include "InterfaceZoomRepaintCompat_p.h"
#include "NativePaintDeviceCompat_p.h"

namespace compatibility::detail {
namespace {

struct ZoomEffectCacheTag {
    using Type = QPixmapCache::Key QGraphicsEffectSourcePrivate::*;
    friend Type zoomRepaintMember(ZoomEffectCacheTag);
};
struct ZoomFlushRegionTag {
    using Type = QRegion QWidgetRepaintManager::*;
    friend Type zoomRepaintMember(ZoomFlushRegionTag);
};
template struct ZoomRepaintMember<ZoomEffectCacheTag, &QGraphicsEffectSourcePrivate::m_cacheKey>;
template struct ZoomRepaintMember<ZoomFlushRegionTag, &QWidgetRepaintManager::topLevelNeedsFlush>;

inline thread_local int lowDprSnapshotDepth = 0;

inline bool interfaceZoomTexturesLocked(QWidget* widget)
{
    const auto* top = QWidgetPrivate::get(widget)->maybeTopData();
    if (top) {
        for (const auto& textures : top->widgetTextures)
            if (textures->isLocked())
                return true;
    }
    return false;
}

class ScopedRasterOnlyTextures {
public:
    explicit ScopedRasterOnlyTextures(QWidget* widget)
    {
        for (const auto& textures : QWidgetPrivate::get(widget)->topData()->widgetTextures) {
            for (int i = 0; i < textures->count(); ++i) {
                auto* source = static_cast<QWidget*>(textures->source(i));
                auto* data = QWidgetPrivate::get(source);
                if (data->renderToTextureReallyDirty) {
                    pending.append(source);
                    data->renderToTextureReallyDirty = false;
                }
            }
        }
    }
    ~ScopedRasterOnlyTextures()
    {
        for (const auto& widget : pending)
            if (widget)
                QWidgetPrivate::get(widget)->renderToTextureReallyDirty = true;
    }

private:
    QList<QPointer<QWidget>> pending;
};

inline void discardLowDprEffectCaches(QWidget* widget, const QRegion& region)
{
    auto* effect = widget->graphicsEffect();
    const QRect bounds =
        effect && effect->isEnabled() ? effect->boundingRectFor(widget->rect()).toAlignedRect() : widget->rect();
    if (!region.intersects(bounds))
        return;
    if (effect && effect->isEnabled()) {
        const auto* data = static_cast<QGraphicsEffectPrivate*>(QObjectPrivate::get(effect));
        if (data->source) {
            // Qt's cache key does not distinguish a low-DPR native pass from
            // the unit-density pass. Use its typed member on this source's
            // d-pointer: a downcast needs RTTI that Qt does not export to UBSan.
            // zh_CN: 两种密度共用 Qt 缓存键；通过成员指针只清像素，避免依赖 Qt 未导出的 RTTI。
            const auto cacheKey = static_cast<QPixmapCache::Key QObjectPrivate::*>(
                zoomRepaintMember(ZoomEffectCacheTag{}));
            QPixmapCache::remove(QObjectPrivate::get(data->source)->*cacheKey);
        }
    }
    for (QObject* object : widget->children()) {
        auto* child = qobject_cast<QWidget*>(object);
        if (child && !child->isWindow() && !child->isHidden())
            discardLowDprEffectCaches(child, region.translated(-child->pos()));
    }
}

inline QPixmap lowDprWidgetSnapshot(QWidget* widget, const QRect& region)
{
    QScopedValueRollback<int> capture(lowDprSnapshotDepth, lowDprSnapshotDepth + 1);
    QPixmap image(region.size());
    image.fill(Qt::transparent);
    discardLowDprEffectCaches(widget, region);
    // The same private entry point as grab(), with a unit-density target.
    // Public render(QPainter*) shares effect painters and can duplicate offsets.
    // zh_CN: 使用 grab() 的同一私有入口，但以单位密度绘制；公开的 painter 重载会共享特效画笔并重复偏移。
    QWidgetPrivate::get(widget)->render(&image, QPoint(), region,
        QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
    return image;
}

inline bool repaintLowDprRegion(QWidget* widget, const QRegion& requested)
{
    const QRegion region = requested & widget->rect();
    auto* store = widget->backingStore();
    auto* data = QWidgetPrivate::get(widget);
    auto* manager = data->maybeRepaintManager();
    const qreal dpr = widget->devicePixelRatioF();
    // Qt may defer sync while creating/replacing a GPU surface. On old Cocoa,
    // beginPaint before Qt's initial resize allocates an invalid IOSurface.
    // zh_CN: GPU 表面创建或替换期间 Qt 可延后同步；旧 Cocoa 在首次 resize 前 beginPaint 会分配无效 IOSurface。
    if (!store || !manager || region.isEmpty() || dpr <= 0.0 || dpr >= 1.0 || store->size().isEmpty()
        || store->size() != widget->size() || !widget->testAttribute(Qt::WA_Mapped) || !widget->isVisible())
        return false;

    store->beginPaint(region);
    auto* device = store->paintDevice();
    if (!device || device->devType() != QInternal::Image) {
        store->endPaint();
        return false;
    }
    auto* native = static_cast<QImage*>(device);
    ScopedNativeRasterDevice nativePass(nullptr);
    const auto tiles = interfaceZoomRasterTiles(region);
    for (const QRect& bounds : tiles) {
        QImage image(bounds.size(), QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) {
            store->endPaint();
            return false;
        }
        image.fill(Qt::transparent);
        nativePass.setDevice(&image);
        const QRegion tileRegion = region & bounds;
        {
            QScopedValueRollback<int> capture(lowDprSnapshotDepth, lowDprSnapshotDepth + 1);
            ScopedRasterOnlyTextures rasterOnly(widget);
            const QRegion previousFlush = manager->*zoomRepaintMember(ZoomFlushRegionTag{});
            discardLowDprEffectCaches(widget, tileRegion);
            // Keep the native draw path: GPU children retain their textures instead
            // of being read back by QWidget::render. Only the dirty region is painted.
            // zh_CN: 保留原生绘制路径，GPU 子控件沿用纹理，不通过 render 回读；只绘制脏区域。
            data->drawWidget(&image, tileRegion, -bounds.topLeft(),
                QWidgetPrivate::DrawAsRoot | QWidgetPrivate::DrawRecursive, nullptr, manager);
            // drawWidget publishes tile-relative offsets for alien children. Replace
            // those with the actual window region; native children use their own coordinates.
            // zh_CN: 普通子控件提交的是图块内偏移，替换成窗口中的实际区域；原生子窗口使用自身坐标。
            manager->*zoomRepaintMember(ZoomFlushRegionTag{}) = previousFlush | tileRegion;
        }
        nativePass.setDevice(nullptr);
        {
            // A pixel-coordinate view avoids Qt 5.15/6.2's minimum painter DPR of 1.
            // It borrows the native buffer and never changes its DPR or ownership.
            // zh_CN: 使用像素坐标视图，绕开 Qt 5.15/6.2 画笔的最小 DPR 1；不改原生缓冲区倍率或所有权。
            QImage pixels(native->bits(), native->width(), native->height(), native->bytesPerLine(), native->format());
            QPainter painter(&pixels);
            painter.setClipRegion(QTransform::fromScale(dpr, dpr).map(tileRegion));
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawImage(QRectF(QPointF(bounds.topLeft()) * dpr, QSizeF(bounds.size()) * dpr), image);
        }
    }
    store->endPaint();
    // Publish only the new raster pixels. Qt still owns GPU composition and
    // consumes every pending paint/flush queue, including texture-only frames.
    // zh_CN: 仅提交新绘制的位图区域；GPU 合成及所有绘制/提交队列仍由 Qt 消费。
    manager->*zoomRepaintMember(ZoomFlushRegionTag{}) |= region;
    if (pendingInterfaceZoomRasterDamage(manager, widget).isEmpty()) {
        data->syncBackingStore();
    } else {
        // A capture/effect can dirty another widget synchronously. Re-enter
        // through the adapter next time; direct sync would paint it at low DPR
        // while our event-filter reentrancy guard is still active.
        // zh_CN: 抓图或特效可同步弄脏其他控件。下一次仍经适配器绘制，避免重入保护期间直接低 DPR 重绘。
        QCoreApplication::postEvent(widget, new QEvent(QEvent::UpdateRequest), Qt::LowEventPriority);
    }
    nativePass.commit();
    return true;
}

} // namespace
} // namespace compatibility::detail
