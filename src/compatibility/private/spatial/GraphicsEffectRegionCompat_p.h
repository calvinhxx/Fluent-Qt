#ifndef FLUENTQT_GRAPHICSEFFECTREGIONCOMPAT_P_H
#define FLUENTQT_GRAPHICSEFFECTREGIONCOMPAT_P_H

#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QRectF>
#include <QRegion>
#include <QTransform>
#include "compatibility/private/NativePaintDeviceCompat_p.h"

namespace compatibility::detail {

// Map the paint engine's device-pixel systemClip back to logical widget coordinates during backing-store
// QGraphicsEffect::draw() passes; returns empty for render()/grab()/transformed painters (treated as full repaint).
// zh_CN: 在绘制缓冲调用 QGraphicsEffect::draw() 时，把引擎系统裁剪区从物理像素换算回逻辑坐标；
// QWidget::render、截图或带变换的绘制无法可靠映射，返回空区域以触发完整重绘。
inline QRegion graphicsEffectDirtyRegion(const QPainter* painter, const QPaintDevice* backingStoreDevice)
{
    if (!painter || !painter->isActive())
        return {};
    const QPaintDevice* paintDevice = painter->device();
    if (!paintDevice || (paintDevice != backingStoreDevice && paintDevice != nativeRasterDevice))
        return {};
    const QPaintEngine* engine = painter->paintEngine();
    const QTransform world = painter->worldTransform();
    if (!engine || world.type() > QTransform::TxTranslate)
        return {};
    const QRegion device = engine->systemClip();
    const qreal dpr = paintDevice->devicePixelRatioF();
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
