#pragma once

class QWidget;

namespace compatibility::detail {

// Ask Qt to paint a render-to-texture widget (QOpenGLWidget, QRhiWidget) the next time
// a backing-store pass reaches it, instead of queuing it with QWidget::update().
// QWidgetRepaintManager repaints everything beneath a queued texture widget whenever
// any other widget is dirty in the same pass; a capture effect beneath it then sees the
// whole surface as changed. Returns false when Qt's private widget state is unavailable,
// in which case the caller keeps using update().
// zh_CN: 让 Qt 在下一次后备存储绘制经过纹理控件时直接绘制它，而不是通过 update() 排队。
// 排队的纹理控件只要与其他脏控件同处一次绘制，Qt 就会重绘其下方全部区域，下方的捕获特效
// 会误判整块表面变化。私有控件状态不可用时返回 false，调用方继续使用 update()。
bool schedulePaintInBackingStorePass(QWidget* textureWidget);

} // namespace compatibility::detail
