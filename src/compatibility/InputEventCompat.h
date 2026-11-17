#pragma once

// Input event types and coordinates across supported Qt versions.

#include <QEnterEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <type_traits>

/**
 * @brief Enter-event parameter type for QWidget::enterEvent() overrides (QEnterEvent on Qt 6, QEvent on Qt 5).
 * zh_CN: QWidget::enterEvent() 重写使用的进入事件类型（Qt 6 为 QEnterEvent，Qt 5 为 QEvent）。
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentEnterEvent = QEnterEvent;
#else
using FluentEnterEvent = QEvent;
#endif

static_assert(std::is_base_of<QEvent, FluentEnterEvent>::value, "FluentEnterEvent must derive from QEvent");

enum class FluentWheelInputKind { PhaseBased, NoPhasePixel, NoPhaseDiscrete };

/**
 * @brief Local cursor position from a mouse event across Qt 5 and Qt 6.
 * zh_CN: 跨 Qt 5/6 读取鼠标事件的控件局部坐标。
 */
inline QPoint fluentMousePos(const QMouseEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

/**
 * @brief Reads the first touch point's global position, returning false for an empty event.
 * zh_CN: 读取首个触点的全局位置；事件没有触点时返回 false。
 */
inline bool fluentFirstTouchGlobalPos(const QTouchEvent* event, QPoint& position)
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

inline QPoint fluentMouseGlobalPos(const QMouseEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->globalPosition().toPoint();
#else
    return e->globalPos();
#endif
}

inline QPoint fluentHoverPos(const QHoverEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

inline QPoint fluentEnterPos(const FluentEnterEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    const auto* enter = dynamic_cast<const QEnterEvent*>(e);
    return enter ? enter->localPos().toPoint() : QPoint();
#endif
}

inline QKeySequence fluentKeySequence(const QKeyEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QKeySequence(e->keyCombination());
#else
    return QKeySequence(static_cast<int>(e->modifiers()) | e->key());
#endif
}

/**
 * @brief Native gesture event alias for wheel and gesture position helpers.
 * zh_CN: 原生手势事件类型别名。
 */
using FluentNativeGestureEvent = QNativeGestureEvent;

inline QPointF fluentWheelPosition(const QWheelEvent* e)
{
    return e->position();
}

inline FluentWheelInputKind fluentWheelInputKind(const QWheelEvent* e)
{
    if (e->phase() != Qt::NoScrollPhase)
        return FluentWheelInputKind::PhaseBased;
    return e->pixelDelta().isNull() ? FluentWheelInputKind::NoPhaseDiscrete : FluentWheelInputKind::NoPhasePixel;
}

inline qreal fluentWheelDeltaY(const QWheelEvent* e)
{
    if (!e->pixelDelta().isNull())
        return static_cast<qreal>(e->pixelDelta().y());
    if (!e->angleDelta().isNull())
        return static_cast<qreal>(e->angleDelta().y());
    return 0.0;
}

inline QPointF fluentNativeGesturePosition(const FluentNativeGestureEvent* e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return e->localPos();
#endif
}
