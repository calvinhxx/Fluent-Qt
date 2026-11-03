#pragma once

#include <QPoint>
#include <QRect>

class QPainter;
class QPixmap;
class QWidget;

namespace compatibility::detail {

// The optional Gallery adapter supplies a matching-kit renderer for DPR < 1.
// Keep Qt private headers and the zoom dependency out of the Widgets library.
// zh_CN: 可选 Gallery 适配层为 DPR < 1 提供匹配套件的绘制器；基础控件库不依赖私有头或缩放后端。
using LowDprSnapshotRenderer = QPixmap (*)(QWidget*, const QRect&);
void setLowDprSnapshotRenderer(LowDprSnapshotRenderer renderer);

// Draw inside a widget-local visible clip, including the root effect's outer bounds.
// Preserve graphics effects without changing visibility, geometry or effect state.
// zh_CN: 在控件局部可见区域内绘制，包含根特效的外扩边界；保留图形效果，不改可见性、几何或效果状态。
void renderWidgetSnapshot(QPainter& painter, QWidget* widget, const QPoint& origin, const QRect& visibleRect);

} // namespace compatibility::detail
