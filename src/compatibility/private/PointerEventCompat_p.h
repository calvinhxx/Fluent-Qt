#pragma once

#include <QCoreApplication>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QWidget>
#include <optional>

namespace compatibility::detail {

inline QPointF mouseLocalPosition(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

inline QPointF mouseGlobalPosition(const QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition();
#else
    return event->screenPos();
#endif
}

// QMouseEvent is non-copyable in newer Qt releases. Construct it in place so
// metadata survives without allocating a new event for each pointer movement.
// zh_CN: 新版 Qt 禁止复制 QMouseEvent；直接构造并保留元数据，避免每次移动都堆分配。
class TranslatedMouseEvent final : public QMouseEvent {
public:
    TranslatedMouseEvent(const QMouseEvent* event, const QPointF& local, const QPointF& window, const QPointF& global)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        : QMouseEvent(event->type(),
              local,
              window,
              global,
              event->button(),
              event->buttons(),
              event->modifiers(),
              event->source(),
              event->pointingDevice())
#else
        : QMouseEvent(event->type(),
              local,
              window,
              global,
              event->button(),
              event->buttons(),
              event->modifiers(),
              event->source())
#endif
    {
        setTimestamp(event->timestamp());
    }
};

// An embedded scene uses the widget-local point as its window-local point.
// zh_CN: 嵌入场景以控件局部坐标作为窗口局部坐标，并保留原始输入元数据。
inline TranslatedMouseEvent mouseEventForLocalScene(const QMouseEvent* event)
{
    const QPointF local = mouseLocalPosition(event);
    return TranslatedMouseEvent(event, local, local, mouseGlobalPosition(event));
}

// Reuse the same translation for direct handlers and ordinary Qt event delivery.
// zh_CN: 直接调用事件处理器和通过 Qt 分发事件共用坐标换算与输入元数据保留逻辑。
inline TranslatedMouseEvent mouseEventForWidget(const QMouseEvent* event, const QWidget* target)
{
    const QPointF global = mouseGlobalPosition(event);
    const QPointF local = global - target->mapToGlobal(QPoint());
    const QPointF window = global - target->window()->mapToGlobal(QPoint());
    return TranslatedMouseEvent(event, local, window, global);
}

/**
 * @brief Reads enter-event coordinates without consulting the current cursor.
 * zh_CN: 读取进入事件自身的坐标，不查询当前光标；普通合成 Enter 返回空值。
 */
inline std::optional<QPointF> enterEventGlobalPosition(const QEvent* event)
{
    const auto* enter = dynamic_cast<const QEnterEvent*>(event);
    if (!enter)
        return std::nullopt;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return enter->globalPosition();
#else
    return enter->screenPos();
#endif
}

/**
 * @brief Sends an enter event with coordinates translated to the receiving widget.
 * zh_CN: 换算为接收控件的坐标并同步转交进入事件；缺少事件坐标时不发送。
 */
inline bool sendEnterEvent(QWidget* target, const QEvent* event)
{
    const auto position = enterEventGlobalPosition(event);
    if (!target || !position)
        return false;

    const QPointF local = *position - target->mapToGlobal(QPoint());
    const QPointF window = *position - target->window()->mapToGlobal(QPoint());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto* enter = static_cast<const QEnterEvent*>(event);
    QEnterEvent redirected(local, window, *position, enter->pointingDevice());
    redirected.setTimestamp(enter->timestamp());
#else
    QEnterEvent redirected(local, window, *position);
#endif
    return QCoreApplication::sendEvent(target, &redirected);
}

/**
 * @brief Sends a translated mouse event, preserving input metadata and precision.
 * zh_CN: 换算坐标并同步转交鼠标事件，保留坐标精度、按键、来源、设备与时间戳。
 */
inline bool sendMouseEvent(QWidget* target, const QMouseEvent* event)
{
    if (!target || !event)
        return false;

    auto redirected = mouseEventForWidget(event, target);
    return QCoreApplication::sendEvent(target, &redirected);
}

} // namespace compatibility::detail
