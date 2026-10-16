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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentEnterEvent = QEnterEvent;
#else
using FluentEnterEvent = QEvent;
#endif

static_assert(std::is_base_of<QEvent, FluentEnterEvent>::value, "FluentEnterEvent must derive from QEvent");

enum class FluentWheelInputKind { PhaseBased, NoPhasePixel, NoPhaseDiscrete };

// Mouse event coordinates.
// zh_CN: 鼠标事件坐标。
// Qt 6: QMouseEvent::position() / globalPosition() return QPointF.
// Qt 5: QMouseEvent::pos() / globalPos() return QPoint.
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

// Wheel and native gesture coordinates.
// zh_CN: 滚轮和原生手势事件坐标。
// Qt 5.15+/Qt 6: QWheelEvent::position().
// Qt 6: QNativeGestureEvent::position(); Qt 5: localPos().
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
