#pragma once

#include <QOpenGLWidget>
#include <QPainter>

namespace compatibility::detail {

// Call from paintEvent, after QGraphicsView::viewportEvent has consumed its
// pending update. Use the viewport's explicit GL renderer below DPR 1, where
// old Qt's widget painter clamps the scale. Only this viewport is rendered.
// zh_CN: 在 viewportEvent 消费刷新状态后的 paintEvent 中调用。DPR < 1 时使用视口自己的 GL 绘制入口，
// 绕开旧 Qt 控件画笔的倍率钳制；只绘制本视口，不触发其他窗口区域刷新。
template <class Paint> bool paintLowDprOpenGLViewport(QOpenGLWidget* viewport, Paint paint)
{
    const qreal dpr = viewport->devicePixelRatioF();
    if (dpr >= 1.0 || dpr <= 0.0)
        return false;
    {
        // Let Qt prepare its context/FBO before the explicit renderer takes over.
        // zh_CN: 先让 Qt 准备上下文及 FBO，再交给显式绘制入口。
        QPainter prepare(viewport);
        if (!prepare.isActive())
            return false;
    }
    viewport->makeCurrent();
    paint();
    return true;
}

} // namespace compatibility::detail
