#pragma once

#include <QMouseEvent>
#include <QTouchEvent>
#include <QVariant>

namespace compatibility {

// Keep QVariant's type-id conversion behind the Qt compatibility boundary.
// zh_CN: 将 QVariant 的类型 ID 转换差异集中在 Qt 兼容层。
inline bool convertVariantToType(QVariant& value, int type)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return value.convert(QMetaType(type));
#else
    return value.convert(type);
#endif
}

// Touch event coordinates changed from screenPos() to globalPosition().
// zh_CN: 统一新旧触摸事件的全局坐标读取方式。
inline bool firstTouchGlobalPosition(const QTouchEvent* event, QPoint& position)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (event->points().isEmpty())
        return false;
    position = event->points().first().globalPosition().toPoint();
#else
    if (event->touchPoints().isEmpty())
        return false;
    position = event->touchPoints().first().screenPos().toPoint();
#endif
    return true;
}

// An embedded scene uses the widget-local point as its window-local point.
// zh_CN: 嵌入场景以控件局部坐标作为窗口局部坐标，并保留输入设备和事件来源。
inline QMouseEvent mouseEventForLocalScene(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QMouseEvent(event->type(), event->position(), event->position(), event->globalPosition(),
                       event->button(), event->buttons(), event->modifiers(), event->source(),
                       event->pointingDevice());
#else
    return QMouseEvent(event->type(), event->localPos(), event->localPos(), event->screenPos(),
                       event->button(), event->buttons(), event->modifiers(), event->source());
#endif
}

} // namespace compatibility
