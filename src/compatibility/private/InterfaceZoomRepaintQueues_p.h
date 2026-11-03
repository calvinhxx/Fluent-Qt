#pragma once

// Application-only, compiled against the exact Qt kit by a CMake capability probe.
// zh_CN: 仅供应用使用，由 CMake 针对当前 Qt 套件编译探测。
#include <QtWidgets/private/qwidget_p.h>
#include <QtWidgets/private/qwidgetrepaintmanager_p.h>

namespace compatibility::detail {
namespace {

// Qt 5.15/6.2 have no pending-damage getters; newer getters still omit the GPU
// queue. Explicit instantiation permits naming private members ([temp.explicit]).
// Use typed pointers from the matching header, never a copied layout or offsets.
// Only read the queues; Qt remains responsible for consuming and clearing them.
// zh_CN: Qt 5.15/6.2 没有待绘区域访问器，新版访问器仍缺少 GPU 队列。
// 显式实例化允许引用私有成员；使用匹配头文件的成员指针，不复制布局或硬编码偏移。
// 队列只读，由 Qt 消费和清理。
struct ZoomRasterRegionTag {
    friend auto zoomRepaintMember(ZoomRasterRegionTag);
};
struct ZoomRasterWidgetsTag {
    friend auto zoomRepaintMember(ZoomRasterWidgetsTag);
};
struct ZoomTextureWidgetsTag {
    friend auto zoomRepaintMember(ZoomTextureWidgetsTag);
};
template <class Tag, auto Member> struct ZoomRepaintMember {
    friend auto zoomRepaintMember(Tag) { return Member; }
};
template struct ZoomRepaintMember<ZoomRasterRegionTag, &QWidgetRepaintManager::dirty>;
template struct ZoomRepaintMember<ZoomRasterWidgetsTag, &QWidgetRepaintManager::dirtyWidgets>;
template struct ZoomRepaintMember<ZoomTextureWidgetsTag, &QWidgetRepaintManager::dirtyRenderToTextureWidgets>;

QRegion pendingInterfaceZoomRasterDamage(QWidgetRepaintManager* manager, QWidget* window)
{
    QRegion damage = manager->*zoomRepaintMember(ZoomRasterRegionTag{});
    const auto& widgets = manager->*zoomRepaintMember(ZoomRasterWidgetsTag{});
    for (QWidget* widget : widgets) {
        const auto* data = QWidgetPrivate::get(widget);
        damage += (data->dirty & data->clipRect()).translated(widget->mapTo(window, QPoint()));
    }
    damage &= window->rect();
    // Qt flushes pure texture updates without beginPaint(): leave that fast path
    // alone. For a mixed frame it adds dirty texture rectangles to raster damage
    // after processing widgets; include those before aligning the region.
    // zh_CN: 纯纹理帧无需写入位图，不扩大刷新；混合帧中 Qt 还会追加待绘纹理的矩形，
    // 必须在对齐前一起计入，避免追加区域再次产生分数边界。
    if (damage.isEmpty())
        return {};
    const auto& textures = manager->*zoomRepaintMember(ZoomTextureWidgetsTag{});
    for (QWidget* widget : textures) {
        if (widget->isVisibleTo(window))
            damage += QRect(widget->mapTo(window, QPoint()), widget->size());
    }
    return damage & window->rect();
}

void invalidateInterfaceZoomRasterDamage(QWidget* window, const QRegion& damage)
{
    // This QRect entry point is exported by Qt 5.15/6.2 too; the repaint
    // manager's markDirty instantiations are not exported by those kits.
    // zh_CN: Qt 5.15/6.2 也导出此 QRect 入口，而它们不导出 manager 的 markDirty 实例。
    auto* data = QWidgetPrivate::get(window);
    for (const QRect& rect : damage)
        data->invalidateBackingStore(rect);
}

} // namespace
} // namespace compatibility::detail
