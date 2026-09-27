#include "OverlayPresentation.h"
#include "OverlayPresentation_p.h"

#include <QtMath>

namespace fluent::overlay {
namespace {

QTransform translation(const QPoint& point)
{
    return QTransform::fromTranslate(point.x(), point.y());
}

// Compose each QWidget segment, then cross a proxy boundary through its real
// viewport. This also composes an inner SpatialView with an outer compositor.
// zh_CN: 逐段组合 QWidget 坐标，经代理的真实视口跨越边界；可叠加内层 SpatialView
// 与外层合成器，而不是把卡片的逻辑 window() 当成应用窗口。
QTransform presentedTransform(const QWidget* source)
{
    QTransform result;
    for (int depth = 0; source && depth < 32; ++depth) {
        const QWidget* top = source->window();
        if (const QWidget* root = presentationRoot(source)) {
            result *= translation(source->mapTo(root, QPoint())) *
                      root->property(presentationTransformPropertyName()).value<QTransform>() *
                      translation(root->mapTo(top, QPoint()));
        } else {
            result *= translation(source->mapTo(top, QPoint()));
        }
        const auto bridge = compat::widgetPresentationBridge(source);
        if (!bridge.viewport)
            break;
        result *= bridge.rootToViewport;
        source = bridge.viewport;
    }
    return result;
}

} // namespace

QPoint presentedPointInTopLevel(const QWidget* source, const QPoint& localPoint)
{
    return source ? presentedTransform(source).map(QPointF(localPoint)).toPoint() : QPoint();
}

QRect presentedRectInTopLevel(const QWidget* source)
{
    // QRectF uses size, not QRect's inclusive bottom/right. Identity and integer
    // translations therefore retain the exact ordinary 2D rectangle.
    // zh_CN: 使用 size 构造浮点矩形，保留普通 2D 路径精确的整数边界。
    return source ? presentedTransform(source)
                        .mapRect(QRectF(QPointF(), source->size()))
                        .toAlignedRect()
                  : QRect();
}

QPoint presentedPointToGlobal(const QWidget* source, const QPoint& localPoint)
{
    const QWidget* top = presentedTopLevel(source);
    return top ? top->mapToGlobal(presentedPointInTopLevel(source, localPoint)) : QPoint();
}

QPoint localPointFromPresentedGlobal(const QWidget* source, const QPoint& globalPoint)
{
    const QWidget* top = presentedTopLevel(source);
    if (!top)
        return {};
    bool invertible = false;
    const QTransform inverse = presentedTransform(source).inverted(&invertible);
    return invertible ? inverse.map(QPointF(top->mapFromGlobal(globalPoint))).toPoint() : QPoint();
}

bool hasPresentedTransform(const QWidget* source)
{
    return source &&
           (presentationRoot(source) || compat::widgetPresentationBridge(source).viewport);
}

bool anchorGeometryMayChange(QObject* watched, QEvent* event, QWidget* anchor)
{
    if (!event || !anchor)
        return false;
    switch (event->type()) {
    case QEvent::Wheel:
        return true;
    case QEvent::DynamicPropertyChange:
        if (static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() !=
            presentationTransformPropertyName())
            return false;
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ParentChange:
    case QEvent::LayoutRequest:
        break;
    default:
        return false;
    }
    const auto* changed = qobject_cast<QWidget*>(watched);
    if (!changed)
        return false;
    for (int depth = 0; anchor && depth < 32; ++depth) {
        if (changed == anchor || changed->isAncestorOf(anchor))
            return true;
        anchor = const_cast<QWidget*>(compat::widgetPresentationBridge(anchor).viewport);
    }
    return false;
}

bool isAnchorVisibleInTopLevel(QWidget* anchor)
{
    if (!anchor)
        return false;
    QRect visibleRect = presentedRectInTopLevel(anchor);
    for (int depth = 0; anchor && depth < 32; ++depth) {
        const QWidget* top = anchor->window();
        for (const QWidget* node = anchor; node; node = node->parentWidget()) {
            if (!node->isVisibleTo(top))
                return false;
            visibleRect &= presentedRectInTopLevel(node);
            if (visibleRect.isEmpty())
                return false;
            if (node == top)
                break;
        }
        const auto bridge = compat::widgetPresentationBridge(anchor);
        if (!bridge.viewport)
            return top->isVisible();
        if (!bridge.visible)
            return false;
        anchor = const_cast<QWidget*>(bridge.viewport);
    }
    return false;
}

} // namespace fluent::overlay

namespace fluent::overlay::presentation {

void setTransform(QWidget* root, const QTransform& transform)
{
    if (!root)
        return;
    const bool finite =
        qIsFinite(transform.m11()) && qIsFinite(transform.m12()) && qIsFinite(transform.m13()) &&
        qIsFinite(transform.m21()) && qIsFinite(transform.m22()) && qIsFinite(transform.m23()) &&
        qIsFinite(transform.m31()) && qIsFinite(transform.m32()) && qIsFinite(transform.m33());
    root->setProperty(presentationTransformPropertyName(), finite && transform.isInvertible()
                                                               ? QVariant::fromValue(transform)
                                                               : QVariant());
}

void clearTransform(QWidget* root)
{
    if (root)
        root->setProperty(presentationTransformPropertyName(), QVariant());
}

bool hasTransform(const QWidget* widget)
{
    return hasPresentedTransform(widget);
}

QPoint mapToGlobal(const QWidget* widget, const QPoint& localPoint)
{
    return presentedPointToGlobal(widget, localPoint);
}

MenuAnchor menuAnchor(const QMenu* menu)
{
    if (!menu)
        return {};
    return {menu->property(presentedMenuSourcePropertyName()).value<QWidget*>(),
            menu->property(presentedMenuPointPropertyName()).toPoint()};
}

void popupMenu(QMenu* menu, QWidget* source, const QPoint& localPoint, const QPoint& offset)
{
    popupMenuAt(menu, source, localPoint, offset);
}

} // namespace fluent::overlay::presentation
