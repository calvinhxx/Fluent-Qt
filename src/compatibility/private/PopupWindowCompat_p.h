#pragma once

#include <QPointer>
#include <QPoint>

class QEvent;
class QObject;
class QScreen;
class QWidget;
class QWindow;

namespace compatibility::detail {

// Tracks and restores transientParent for an externally owned popup across show/hide without taking QObject ownership.
// zh_CN: 在弹窗显示/关闭期间关联并还原 transientParent，不接管外部弹窗的 QObject 所有权。
class PopupWindowHost {
public:
    PopupWindowHost() = default;
    explicit PopupWindowHost(QWidget* popup);
    void attach(QWidget* popup, QWidget* host);
    void restore();

private:
    QPointer<QWindow> m_window;
    QPointer<QWindow> m_host;
    QPointer<QWindow> m_previousParent;
    QPointer<QWindow> m_openingParent;
    bool m_attached = false;
};

QWidget* widgetForEventReceiver(QObject* receiver);
QWidget* widgetForKeyEventReceiver(QObject* receiver);
bool eventReceiverBelongsToWindow(QObject* receiver, QWidget* host);

// Normalize native popup grabs without changing the component's dismissal
// policy. Only presses outside the entire popup chain can belong to its host.
// zh_CN: 统一原生弹窗抓取，不改变组件关闭策略；仅弹窗整条链外的按下可归属宿主。
bool popupGrabPressBelongsToWindow(QObject* receiver, QEvent* event, QWidget* host);
bool popupContainsEvent(QWidget* popup, QObject* receiver, QEvent* event);

// Keep the full native frame inside the screen after independent position/size
// rounding, including application zoom below 100%.
// zh_CN: 位置和尺寸分别取整后，仍把完整原生窗框限制在屏幕内，包含低于 100% 的应用缩放。
QPoint clampPopupPosition(const QWidget* popup, const QPoint& position, const QScreen* screen);

} // namespace compatibility::detail
