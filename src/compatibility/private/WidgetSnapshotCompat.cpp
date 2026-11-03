#include "WidgetSnapshotCompat_p.h"

#include <QGraphicsEffect>
#include <QPainter>
#include <QWidget>

namespace compatibility::detail {
namespace {

LowDprSnapshotRenderer lowDprRenderer = nullptr;

QRect effectPaintBounds(const QWidget* widget)
{
    auto* effect = widget->graphicsEffect();
    return effect && effect->isEnabled()
        ? effect->boundingRectFor(QRectF(widget->rect())).toAlignedRect()
        : widget->rect();
}

// Only effects that can paint inside @p region change the snapshot. Children are
// clipped to their parent, so subtrees outside the region are skipped.
// zh_CN: 只有可能绘制到 @p region 内的特效会影响快照；子控件受父控件裁剪，区域外的子树直接跳过。
bool hasVisibleGraphicsEffect(const QWidget* widget, const QRect& region)
{
    if (auto* effect = widget->graphicsEffect(); effect && effect->isEnabled())
        return true;
    for (QObject* object : widget->children()) {
        const auto* child = qobject_cast<QWidget*>(object);
        if (!child || child->isWindow() || child->isHidden())
            continue;
        const QRect childRegion = region.translated(-child->pos());
        if (effectPaintBounds(child).intersects(childRegion) && hasVisibleGraphicsEffect(child, childRegion))
            return true;
    }
    return false;
}

} // namespace

void setLowDprSnapshotRenderer(LowDprSnapshotRenderer renderer)
{
    lowDprRenderer = renderer;
}

void renderWidgetSnapshot(QPainter& painter, QWidget* widget, const QPoint& origin, const QRect& visibleRect)
{
    if (!widget || widget->size().isEmpty() || visibleRect.isEmpty())
        return;
    const QRect widgetRect = widget->rect();
    const QRect sourceRect = visibleRect.intersected(effectPaintBounds(widget));
    if (sourceRect.isEmpty())
        return;
    if (!hasVisibleGraphicsEffect(widget, sourceRect)) {
        widget->render(&painter, origin + sourceRect.topLeft(), QRegion(sourceRect),
            QWidget::DrawWindowBackground | QWidget::DrawChildren);
        return;
    }

    // QWidget::render(QPainter*) shares the transformed destination with effect
    // sources, which can apply offsets twice or lose a partially opaque layer.
    // grab() paints in widget-local coordinates and retains the source DPR.
    // Include the root effect's paint bounds, not just the widget rectangle.
    // zh_CN: render(QPainter*) 与效果源共享变换后的目标，可能重复偏移或丢失半透明层。
    // grab() 使用控件局部坐标并保留源 DPR；捕获范围包含根特效的绘制边界。
    QRect captureRect = sourceRect;
    if (!captureRect.intersects(widgetRect)) {
        // grab() rejects regions wholly outside rect(), even when an effect
        // paints there. Include the nearest source pixel, then clip it away
        // during composition; do not capture the entire source widget.
        // zh_CN: 仅阴影可见时 grab() 会拒绝与本体不相交的区域。加入最近的一个源像素，
        // 合成时裁掉这部分，不抓取整个源控件。
        const QPoint center = sourceRect.center();
        const QPoint anchor(qBound(widgetRect.left(), center.x(), widgetRect.right()),
            qBound(widgetRect.top(), center.y(), widgetRect.bottom()));
        captureRect |= QRect(anchor, QSize(1, 1));
    }
    const QRegion mask = widget->mask();
    const QPixmap snapshot = lowDprRenderer && widget->devicePixelRatioF() < 1.0
        ? lowDprRenderer(widget, captureRect)
        : widget->grab(captureRect);
    painter.save();
    painter.setClipRect(sourceRect.translated(origin), Qt::IntersectClip);
    // grab() ignores the root widget mask; restore the ordinary render contract.
    // zh_CN: grab() 忽略根控件遮罩，合成时恢复普通 render 的裁剪契约。
    if (!mask.isEmpty())
        painter.setClipRegion(mask.translated(origin), Qt::IntersectClip);
    painter.drawPixmap(origin + captureRect.topLeft(), snapshot);
    painter.restore();
}

} // namespace compatibility::detail
