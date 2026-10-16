#pragma once

// Synthetic input events retained for existing QtCompat.h consumers.

#include "InputEventCompat.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QPointingDevice>
#endif

constexpr bool fluentWheelEventSupportsPhase()
{
    // Qt 5.15 already provides the pixel/angle-delta constructor with ScrollPhase.
    return true;
}

inline const char* fluentWheelEventPhaseSkipReason()
{
    return "Wheel phase events are available on every supported Qt version";
}

// Test helper for constructing enter events.
// zh_CN: 构造 enter 事件的测试辅助宏。
// Qt 6 has QEnterEvent(localPos, scenePos, globalPos); Qt 5 falls back to QEvent::Enter.
//
// Usage:
//   FLUENT_MAKE_ENTER_EVENT(ev, 5, 5);
//   QApplication::sendEvent(widget, &ev);
/**
 * @brief Constructs an enter event with distinct local, scene and global positions.
 * zh_CN: 使用独立的本地、场景及全局位置构造 enter 事件；Qt 5 保持普通 Enter 事件。
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_MAKE_ENTER_EVENT_AT(name, local, scene, global)                                                         \
    QEnterEvent name                                                                                                   \
    {                                                                                                                  \
        QPointF(local), QPointF(scene), QPointF(global)                                                                \
    }
#else
#define FLUENT_MAKE_ENTER_EVENT_AT(name, local, scene, global) QEvent name(QEvent::Enter)
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_MAKE_ENTER_EVENT(name, x, y) QEnterEvent name(QPointF((x), (y)), QPointF((x), (y)), QPointF((x), (y)))
#else
#define FLUENT_MAKE_ENTER_EVENT(name, x, y) QEvent name(QEvent::Enter)
#endif

// Test helper for constructing wheel events.
// zh_CN: 构造 wheel 事件的测试辅助宏。
// Qt 5.15 and Qt 6 share the modern pixel/angle delta constructor.
#define FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(name, localPos, globalPos, pixelDeltaValue, angleDeltaValue, buttonsValue,  \
    modifiersValue, phaseValue, invertedValue)                                                                         \
    QWheelEvent name(QPointF{(localPos)}, QPointF{(globalPos)}, (pixelDeltaValue), (angleDeltaValue), (buttonsValue),  \
        (modifiersValue), (phaseValue), (invertedValue))

// Common no-phase wheel constructor used by focused unit tests.
// zh_CN: 单元测试常用的无 phase wheel 事件构造宏。
#define FLUENT_MAKE_WHEEL_EVENT(name, localX, localY, angleDeltaY, modifiers)                                          \
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(name, QPoint((localX), (localY)), QPoint((localX), (localY)), QPoint(),         \
        QPoint(0, (angleDeltaY)), Qt::NoButton, (modifiers), Qt::NoScrollPhase, false)

// Test helper for constructing mouse events.
// zh_CN: 构造 mouse 事件的测试辅助宏。
// Qt 6 takes floating-point local/global positions; Qt 5 keeps QPoint overloads.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_MAKE_MOUSE_EVENT(name, typeValue, target, localPosValue, buttonValue, buttonsValue, modifiersValue)     \
    const QPoint name##GlobalPos = (target)->mapToGlobal(localPosValue);                                               \
    QMouseEvent name((typeValue), QPointF(localPosValue), QPointF(name##GlobalPos), (buttonValue), (buttonsValue),     \
        (modifiersValue))
#else
#define FLUENT_MAKE_MOUSE_EVENT(name, typeValue, target, localPosValue, buttonValue, buttonsValue, modifiersValue)     \
    QMouseEvent name((typeValue), (localPosValue), (target)->mapToGlobal(localPosValue), (buttonValue),                \
        (buttonsValue), (modifiersValue))
#endif

// Test helper for constructing native gesture events.
// zh_CN: 构造 native gesture 事件的测试辅助宏。
// Qt 6.2+ exposes the constructor shape used by ScrollView tests. Qt 5 builds
// keep a no-op construction macro so test files do not need version branches.
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#define FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR 1
#define FLUENT_MAKE_NATIVE_GESTURE_EVENT(name, target, gestureType, localX, localY, gestureValue)                      \
    const QPointF name##LocalPos(QPointF((localX), (localY)));                                                         \
    const QPointF name##GlobalPos((target)->mapToGlobal(name##LocalPos.toPoint()));                                    \
    QNativeGestureEvent name((gestureType), QPointingDevice::primaryPointingDevice(), 2, name##LocalPos,               \
        name##LocalPos, name##GlobalPos, (gestureValue), QPointF(), 1)
#else
#define FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR 0
#define FLUENT_MAKE_NATIVE_GESTURE_EVENT(name, target, gestureType, localX, localY, gestureValue)                      \
    QEvent name(QEvent::None)
#endif

constexpr bool fluentCanConstructNativeGestureEvent()
{
    return FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR != 0;
}

inline const char* fluentNativeGestureEventSkipReason()
{
    return "Native gesture event construction requires Qt 6.2+";
}
