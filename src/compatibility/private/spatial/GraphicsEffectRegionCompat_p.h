#ifndef FLUENTQT_GRAPHICSEFFECTREGIONCOMPAT_P_H
#define FLUENTQT_GRAPHICSEFFECTREGIONCOMPAT_P_H

#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QRectF>
#include <QRegion>
#include <QTransform>

namespace compatibility::detail {

// Qt 5.15 and 6.x set the paint engine's system clip to the dirty region before
// calling a widget's QGraphicsEffect::draw() from the backing store: the region in
// widget coordinates, translated by the painter's widget offset, then scaled by the
// device pixel ratio. Map it back to the effect source's logical coordinates. Any
// other pass (QWidget::render, grabs, transformed painters) has no reliable mapping
// and returns an empty region, which callers treat as "everything changed".
// zh_CN: Qt 5.15 与 6.x 在后备存储调用控件 QGraphicsEffect::draw() 前，把脏区设为绘制引擎的
// 系统裁剪区（控件坐标加偏移，再乘 DPR）。此处换算回特效源的逻辑坐标；QWidget::render、
// 截图或带变换的绘制无法可靠换算，返回空区域，调用方按整块失效处理。
inline QRegion graphicsEffectDirtyRegion(const QPainter* painter, const QPaintDevice* backingStoreDevice)
{
    if (!painter || !painter->isActive() || !backingStoreDevice || painter->device() != backingStoreDevice)
        return {};
    const QPaintEngine* engine = painter->paintEngine();
    const QTransform world = painter->worldTransform();
    if (!engine || world.type() > QTransform::TxTranslate)
        return {};
    const QRegion device = engine->systemClip();
    const qreal dpr = backingStoreDevice->devicePixelRatioF();
    if (device.isEmpty() || !(dpr > 0))
        return {};
    const QPointF offset(world.dx(), world.dy());
    QRegion logical;
    // Expand fractional device rectangles outward so no dirty pixel is lost.
    // zh_CN: 分数 DPR 下向外取整，确保不遗漏任何脏像素。
    for (const QRect& rect : device)
        logical += QRectF(QPointF(rect.topLeft()) / dpr - offset, QSizeF(rect.size()) / dpr).toAlignedRect();
    return logical;
}

} // namespace compatibility::detail
#endif
