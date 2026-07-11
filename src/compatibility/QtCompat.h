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
#include <QAbstractItemView>
#include <QEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QLayoutItem>
#include <QMetaType>
#include <QNativeGestureEvent>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QMouseEvent>
#include <QSize>
#include <QStyleHints>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

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

enum class FluentSystemColorScheme {
    Unknown,
    Light,
    Dark
};

enum class FluentWheelInputKind {
    PhaseBased,
    NoPhasePixel,
    NoPhaseDiscrete
};

inline FluentSystemColorScheme fluentSystemColorScheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark)
            return FluentSystemColorScheme::Dark;
        if (scheme == Qt::ColorScheme::Light)
            return FluentSystemColorScheme::Light;
    }
#endif
    return FluentSystemColorScheme::Unknown;
}

template <typename Context, typename Functor>
QMetaObject::Connection fluentConnectSystemColorSchemeChanged(Context* context, Functor&& functor) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (!QGuiApplication::styleHints())
        return {};
    return QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                            context,
                            [slot = std::forward<Functor>(functor)](Qt::ColorScheme) mutable {
                                slot();
                            });
#else
    Q_UNUSED(context);
    Q_UNUSED(functor);
    return {};
#endif
}

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

inline QPoint fluentHoverPos(const QHoverEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

inline QPoint fluentEnterPos(const FluentEnterEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    Q_UNUSED(e);
    return QPoint();
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

inline int fluentAdjacentButtonRowSpacing(int requestedSpacing) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && (defined(Q_OS_MACOS) || defined(Q_OS_MAC))
    return requestedSpacing + 10;
#else
    return requestedSpacing;
#endif
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

template <typename CheckBoxType, typename Context, typename Functor>
QMetaObject::Connection fluentConnectCheckStateChanged(CheckBoxType* checkBox,
                                                       Context* context,
                                                       Functor&& functor) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return QObject::connect(checkBox, &CheckBoxType::checkStateChanged,
                            context, std::forward<Functor>(functor));
#else
    return QObject::connect(checkBox, &CheckBoxType::stateChanged,
                            context,
                            [slot = std::forward<Functor>(functor)](int state) mutable {
                                slot(static_cast<Qt::CheckState>(state));
                            });
#endif
}

inline QPixmap fluentLabelPixmapValue(const QLabel* label) {
    if (!label)
        return {};
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return label->pixmap(Qt::ReturnByValue);
#else
    const QPixmap* pixmap = label->pixmap();
    return pixmap ? *pixmap : QPixmap();
#endif
}

inline QSize fluentPixmapLogicalSize(const QPixmap& pixmap) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return pixmap.deviceIndependentSize().toSize();
#else
    return QSize(qRound(pixmap.width() / pixmap.devicePixelRatioF()),
                 qRound(pixmap.height() / pixmap.devicePixelRatioF()));
#endif
}

inline QRectF fluentPixmapSourceRectForDraw(const QRectF& logicalSource,
                                            const QPixmap& pixmap) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const qreal dpr = pixmap.devicePixelRatioF();
    return QRectF(logicalSource.left() * dpr,
                  logicalSource.top() * dpr,
                  logicalSource.width() * dpr,
                  logicalSource.height() * dpr);
#else
    Q_UNUSED(pixmap);
    return logicalSource;
#endif
}

inline QPixmap fluentIconPixmapForLogicalExtent(const QIcon& icon,
                                                const QSize& logicalExtent,
                                                qreal devicePixelRatio = 1.0) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
    QPixmap pixmap = icon.pixmap(QSize(qMax(1, qRound(logicalExtent.width() * dpr)),
                                      qMax(1, qRound(logicalExtent.height() * dpr))));
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
#else
    Q_UNUSED(devicePixelRatio);
    return icon.pixmap(logicalExtent);
#endif
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

inline FluentWheelInputKind fluentWheelInputKind(const QWheelEvent* e) {
    if (e->phase() != Qt::NoScrollPhase)
        return FluentWheelInputKind::PhaseBased;
    return e->pixelDelta().isNull() ? FluentWheelInputKind::NoPhaseDiscrete
                                    : FluentWheelInputKind::NoPhasePixel;
}

inline qreal fluentWheelDeltaY(const QWheelEvent* e) {
    if (!e->pixelDelta().isNull())
        return static_cast<qreal>(e->pixelDelta().y());
    if (!e->angleDelta().isNull())
        return static_cast<qreal>(e->angleDelta().y());
    return 0.0;
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

/**
 * @brief Returns a stable item-view row height when visualRect() has no height yet.
 * zh_CN: 当 visualRect() 暂时没有高度时，返回稳定的 item-view 行高。
 *
 * Some Qt/platform/offscreen combinations expose a valid item index before
 * visualRect(index).height() is available. Fall back through the view/delegate
 * size hints so reveal animations can still make layout decisions without
 * scattering Qt-version checks in components.
 * zh_CN: 某些 Qt/平台/offscreen 组合会先暴露有效索引，但 visualRect(index).height()
 * zh_CN: 仍为 0；这里统一回退到 view/delegate 的 size hint，避免组件代码散落 Qt 版本判断。
 */
inline int fluentItemViewRowHeight(const QAbstractItemView* view,
                                   const QModelIndex& index,
                                   const QRect& visualRect) {
    if (visualRect.height() > 0)
        return visualRect.height();

    if (!view || !index.isValid())
        return qMax(0, visualRect.height());

    const int indexHint = view->sizeHintForIndex(index).height();
    if (indexHint > 0)
        return indexHint;

    const int rowHint = view->sizeHintForRow(index.row());
    if (rowHint > 0)
        return rowHint;

    return qMax(0, view->fontMetrics().height());
}

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
    QWheelEvent name(QPointF{(localPos)}, \
                     QPointF{(globalPos)}, \
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
    QWheelEvent name(QPointF{(localPos)}, \
                     QPointF{(globalPos)}, \
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

/**
 * @brief Returns true when an event reports a device-pixel-ratio change.
 * zh_CN: 判断事件是否表示设备像素比发生变化。
 */
inline bool fluentIsDevicePixelRatioChangeEvent(const QEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    return event && event->type() == QEvent::DevicePixelRatioChange;
#else
    Q_UNUSED(event);
    return false;
#endif
}
