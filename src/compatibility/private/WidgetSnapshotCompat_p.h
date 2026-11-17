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

// Draw widget contents and active graphics-effect bounds inside visibleRect without altering state.
// zh_CN: 在控件局部 visibleRect 内绘制内容与根特效扩展边界，不修改控件可见性或特效状态。
void renderWidgetSnapshot(QPainter& painter, QWidget* widget, const QPoint& origin, const QRect& visibleRect);

} // namespace compatibility::detail
