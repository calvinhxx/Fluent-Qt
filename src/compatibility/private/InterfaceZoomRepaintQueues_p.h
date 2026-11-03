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
// Qt remains responsible for consuming and clearing the queues.
// zh_CN: Qt 5.15/6.2 没有待绘区域访问器，新版访问器仍缺少 GPU 队列。
// 显式实例化允许引用私有成员；使用匹配头文件的成员指针，不复制布局或硬编码偏移。
// 队列仍由 Qt 消费和清理。
struct ZoomRasterRegionTag {
    using Type = QRegion QWidgetRepaintManager::*;
    friend Type zoomRepaintMember(ZoomRasterRegionTag);
};
struct ZoomRasterWidgetsTag {
    using Type = QVector<QWidget*> QWidgetRepaintManager::*;
    friend Type zoomRepaintMember(ZoomRasterWidgetsTag);
};
struct ZoomTextureWidgetsTag {
    using Type = QVector<QWidget*> QWidgetRepaintManager::*;
    friend Type zoomRepaintMember(ZoomTextureWidgetsTag);
};
// An explicit return type also works with MSVC's hidden-friend lookup. QVector
// names the Qt 5 container and aliases QList on Qt 6; the probe checks the type.
// zh_CN: 显式返回类型兼容 MSVC 的隐藏友元查找；QVector 在 Qt 6 中是 QList 别名，类型由编译探测校验。
template <class Tag, typename Tag::Type Member> struct ZoomRepaintMember {
    friend typename Tag::Type zoomRepaintMember(Tag) { return Member; }
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
    // Called during an existing UpdateRequest, before Qt consumes its queues.
    // Extend the top-level invalid buffer region so opaque children are included;
    // a QWidget::update() would instead mark only that widget's own pixels.
    // Qt's invalidateBackingStore/markDirty templates are not exported by all kits.
    // zh_CN: 在已有 UpdateRequest 中、Qt 消费队列前扩大顶层缓冲区失效区域，包含不透明子控件；
    // update() 只标记控件自身像素。部分 Qt 套件不导出 invalidateBackingStore/markDirty 模板。
    auto* data = QWidgetPrivate::get(window);
    auto* manager = data->maybeRepaintManager();
    const QRegion clipped = damage & data->clipRect();
    if (!manager || clipped.isEmpty())
        return;
    data->invalidateGraphicsEffectsRecursively();
    manager->*zoomRepaintMember(ZoomRasterRegionTag{}) |= clipped;
}

} // namespace
} // namespace compatibility::detail
