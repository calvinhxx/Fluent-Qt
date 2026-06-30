#pragma once

/**
 * @brief Qt 5/Qt 6 compatibility layer for shared component code.
 * zh_CN: 面向共享组件代码的 Qt 5/Qt 6 兼容层。
 *
 * This header centralizes API differences between Qt 5.15+ and Qt 6.2+ so
 * component headers and tests can use one spelling for event types, coordinates,
 * item-view options, color component pointers, and test event construction.
 * zh_CN: 该头文件集中处理 Qt 5.15+ 与 Qt 6.2+ 的 API 差异，让组件头文件和测试
 * zh_CN: 对事件类型、坐标、item-view option、颜色分量指针和测试事件构造使用统一写法。
 *
 * Usage:
 * - Use FluentEnterEvent in enterEvent() overrides.
 * - Use fluentMousePos() / fluentMouseGlobalPos() for mouse coordinates.
 * - Use fluentKeySequence() for QKeyEvent shortcut matching.
 * - Use fluentConnectSingleShot() for one-shot signal connections.
 * - Use FLUENT_INIT_VIEW_ITEM_OPTION() inside QAbstractItemView subclasses.
 * zh_CN:
 * zh_CN: - enterEvent() override 使用 FluentEnterEvent。
 * zh_CN: - 鼠标坐标统一通过 fluentMousePos() / fluentMouseGlobalPos() 读取。
 * zh_CN: - QKeyEvent 快捷键匹配统一使用 fluentKeySequence()。
 * zh_CN: - 一次性信号连接统一使用 fluentConnectSingleShot()。
 * zh_CN: - QAbstractItemView 子类中使用 FLUENT_INIT_VIEW_ITEM_OPTION() 初始化 option。
 */

#include <QtGlobal>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QList>
#include <QLayoutItem>
#include <QMetaType>
#include <QNativeGestureEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QMouseEvent>
#include <QVector>
#include <QWheelEvent>

#include <memory>
#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QPointingDevice>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
using FluentEnterEvent = QEnterEvent;
#else
using FluentEnterEvent = QEvent;
#endif

#include <type_traits>
static_assert(std::is_base_of<QEvent, FluentEnterEvent>::value,
              "FluentEnterEvent must derive from QEvent");

// Mouse event coordinates.
// zh_CN: 鼠标事件坐标。
// Qt 6: QMouseEvent::position() / globalPosition() return QPointF.
// Qt 5: QMouseEvent::pos() / globalPos() return QPoint.
inline QPoint fluentMousePos(const QMouseEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

inline QPoint fluentMouseGlobalPos(const QMouseEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->globalPosition().toPoint();
#else
    return e->globalPos();
#endif
}

inline QKeySequence fluentKeySequence(const QKeyEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QKeySequence(e->keyCombination());
#else
    return QKeySequence(static_cast<int>(e->modifiers()) | e->key());
#endif
}

inline QWidget* fluentLayoutItemWidget(const QLayoutItem* item) {
    if (!item)
        return nullptr;
    return const_cast<QLayoutItem*>(item)->widget();
}

template <typename Sender, typename Signal, typename Context, typename Functor>
QMetaObject::Connection fluentConnectSingleShot(Sender* sender, Signal signal, Context* context, Functor&& functor) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QObject::connect(sender, signal, context, std::forward<Functor>(functor), Qt::SingleShotConnection);
#else
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = QObject::connect(sender, signal, context,
                                   [connection, slot = std::forward<Functor>(functor)]() mutable {
                                       QObject::disconnect(*connection);
                                       slot();
                                   });
    return *connection;
#endif
}

template <typename T>
void fluentRegisterMetaTypeNames(const char* name) {
    qRegisterMetaType<T>(name);
}

template <typename T, typename... Names>
void fluentRegisterMetaTypeNames(const char* firstName, const char* secondName, Names... remainingNames) {
    qRegisterMetaType<T>(firstName);
    fluentRegisterMetaTypeNames<T>(secondName, remainingNames...);
}

// Wheel and native gesture coordinates.
// zh_CN: 滚轮和原生手势事件坐标。
// Qt 6: QWheelEvent::position() / QNativeGestureEvent::position().
// Qt 5: QWheelEvent::posF() / QNativeGestureEvent::localPos().
using FluentNativeGestureEvent = QNativeGestureEvent;

inline QPointF fluentWheelPosition(const QWheelEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return e->posF();
#endif
}

constexpr bool fluentWheelEventSupportsPhase() {
    return QT_VERSION >= QT_VERSION_CHECK(6, 0, 0);
}

inline const char* fluentWheelEventPhaseSkipReason() {
    return "Wheel phase event construction requires Qt 6+";
}

inline QPointF fluentNativeGesturePosition(const FluentNativeGestureEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return e->localPos();
#endif
}

// QAbstractItemModel::dataChanged roles container type.
// zh_CN: QAbstractItemModel::dataChanged roles 参数容器类型。
// Qt 6 uses QList<int>; Qt 5 uses QVector<int> in the virtual signature.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentItemDataRoles = QList<int>;
#else
using FluentItemDataRoles = QVector<int>;
#endif

// QAbstractItemView::initViewItemOption compatibility.
// zh_CN: QAbstractItemView::initViewItemOption 兼容封装。
// Qt 6: protected void QAbstractItemView::initViewItemOption(QStyleOptionViewItem*) const.
// Qt 5: protected QStyleOptionViewItem QAbstractItemView::viewOptions() const.
//
// Use inside a QAbstractItemView subclass:
//   QStyleOptionViewItem opt;
//   FLUENT_INIT_VIEW_ITEM_OPTION(&opt);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_INIT_VIEW_ITEM_OPTION(optPtr) initViewItemOption(optPtr)
#else
#define FLUENT_INIT_VIEW_ITEM_OPTION(optPtr) do { *(optPtr) = viewOptions(); } while (0)
#endif

// QColor::getHsvF / getRgbF / getHslF component pointer type.
// zh_CN: QColor::getHsvF / getRgbF / getHslF 分量指针类型。
// Qt 6 takes float*; Qt 5 takes qreal* (= double*).
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentColorComponent = float;
#else
using FluentColorComponent = qreal;
#endif

// Test helper for constructing enter events.
// zh_CN: 构造 enter 事件的测试辅助宏。
// Qt 6 has QEnterEvent(localPos, scenePos, globalPos); Qt 5 falls back to QEvent::Enter.
//
// Usage:
//   FLUENT_MAKE_ENTER_EVENT(ev, 5, 5);
//   QApplication::sendEvent(widget, &ev);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_MAKE_ENTER_EVENT(name, x, y) \
    QEnterEvent name(QPointF((x), (y)), QPointF((x), (y)), QPointF((x), (y)))
#else
#define FLUENT_MAKE_ENTER_EVENT(name, x, y) \
    QEvent name(QEvent::Enter)
#endif

// Test helper for constructing wheel events.
// zh_CN: 构造 wheel 事件的测试辅助宏。
// Qt 6 removed the delta/orientation constructor used by Qt 5.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(name, localPos, globalPos, pixelDeltaValue, angleDeltaValue, buttonsValue, modifiersValue, phaseValue, invertedValue) \
    QWheelEvent name(QPointF(localPos), \
                     QPointF(globalPos), \
                     (pixelDeltaValue), \
                     (angleDeltaValue), \
                     (buttonsValue), \
                     (modifiersValue), \
                     (phaseValue), \
                     (invertedValue))
#else
#define FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(name, localPos, globalPos, pixelDeltaValue, angleDeltaValue, buttonsValue, modifiersValue, phaseValue, invertedValue) \
    Q_UNUSED(phaseValue); \
    Q_UNUSED(invertedValue); \
    QWheelEvent name(QPointF(localPos), \
                     QPointF(globalPos), \
                     (pixelDeltaValue), \
                     (angleDeltaValue), \
                     (angleDeltaValue).y(), \
                     Qt::Vertical, \
                     (buttonsValue), \
                     (modifiersValue))
#endif

// Common no-phase wheel constructor used by focused unit tests.
// zh_CN: 单元测试常用的无 phase wheel 事件构造宏。
#define FLUENT_MAKE_WHEEL_EVENT(name, localX, localY, angleDeltaY, modifiers) \
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(name, \
                                       QPoint((localX), (localY)), \
                                       QPoint((localX), (localY)), \
                                       QPoint(), \
                                       QPoint(0, (angleDeltaY)), \
                                       Qt::NoButton, \
                                       (modifiers), \
                                       Qt::NoScrollPhase, \
                                       false)

// Test helper for constructing mouse events.
// zh_CN: 构造 mouse 事件的测试辅助宏。
// Qt 6 takes floating-point local/global positions; Qt 5 keeps QPoint overloads.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define FLUENT_MAKE_MOUSE_EVENT(name, typeValue, target, localPosValue, buttonValue, buttonsValue, modifiersValue) \
    const QPoint name##GlobalPos = (target)->mapToGlobal(localPosValue); \
    QMouseEvent name((typeValue), \
                     QPointF(localPosValue), \
                     QPointF(name##GlobalPos), \
                     (buttonValue), \
                     (buttonsValue), \
                     (modifiersValue))
#else
#define FLUENT_MAKE_MOUSE_EVENT(name, typeValue, target, localPosValue, buttonValue, buttonsValue, modifiersValue) \
    QMouseEvent name((typeValue), \
                     (localPosValue), \
                     (target)->mapToGlobal(localPosValue), \
                     (buttonValue), \
                     (buttonsValue), \
                     (modifiersValue))
#endif

// Test helper for constructing native gesture events.
// zh_CN: 构造 native gesture 事件的测试辅助宏。
// Qt 6.2+ exposes the constructor shape used by ScrollView tests. Qt 5 builds
// keep a no-op construction macro so test files do not need version branches.
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#define FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR 1
#define FLUENT_MAKE_NATIVE_GESTURE_EVENT(name, target, gestureType, localX, localY, gestureValue) \
    const QPointF name##LocalPos(QPointF((localX), (localY))); \
    const QPointF name##GlobalPos((target)->mapToGlobal(name##LocalPos.toPoint())); \
    QNativeGestureEvent name((gestureType), \
                             QPointingDevice::primaryPointingDevice(), \
                             2, \
                             name##LocalPos, \
                             name##LocalPos, \
                             name##GlobalPos, \
                             (gestureValue), \
                             QPointF(), \
                             1)
#else
#define FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR 0
#define FLUENT_MAKE_NATIVE_GESTURE_EVENT(name, target, gestureType, localX, localY, gestureValue) \
    QEvent name(QEvent::None)
#endif

constexpr bool fluentCanConstructNativeGestureEvent() {
    return FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR != 0;
}

inline const char* fluentNativeGestureEventSkipReason() {
    return "Native gesture event construction requires Qt 6.2+";
}

/**
 * @brief Returns true when an event can change the native window safe-area insets.
 * zh_CN: 判断事件是否可能改变原生窗口安全区域边距。
 */
inline bool fluentIsWindowInsetChangeEvent(const QEvent* event) {
    if (!event)
        return false;

    if (event->type() == QEvent::WindowStateChange)
        return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (event->type() == QEvent::SafeAreaMarginsChange)
        return true;
#endif

    return false;
}
