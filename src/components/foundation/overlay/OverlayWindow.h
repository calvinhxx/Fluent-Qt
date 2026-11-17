#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <QApplication>
#include <QPointer>
#include <QVariant>
#include <QWidget>

namespace fluent::overlay {

inline constexpr char kOverlaySurfaceProperty[] = "_fluent_qt_overlay_surface";

inline void markOverlaySurface(QWidget* overlay)
{
    if (overlay)
        overlay->setProperty(kOverlaySurfaceProperty, true);
}

inline QWidget* enclosingOverlaySurface(QWidget* widget)
{
    for (QWidget* current = widget; current; current = current->parentWidget()) {
        if (current->property(kOverlaySurfaceProperty).toBool())
            return current;
    }
    return nullptr;
}

/**
 * @brief Resolves the native top-level overlay host, including embedded widget presentations.
 * zh_CN: 解析浮层所属的原生顶层窗口（兼容嵌入场景中的控件宿主）。
 */
QWidget* resolveOwningTopLevel(const QPointer<QWidget>& originalParent, QWidget* currentParent);

inline void attachToTopLevel(QWidget* overlay, QWidget* topLevel)
{
    if (!overlay || !topLevel)
        return;
    if (overlay->parentWidget() != topLevel || overlay->windowType() != Qt::Widget) {
        overlay->setParent(topLevel);
        overlay->setWindowFlags(Qt::Widget);
    }
}

inline void raiseOverlayStack(QWidget* scrim, QWidget* overlay)
{
    if (scrim)
        scrim->raise();
    // Raise unhidden overlays even before their host is shown so scrims stay underneath.
    if (overlay && !overlay->isHidden())
        overlay->raise();
}

/**
 * @brief Resolves the event receiver's native top-level host, falling back to the focused widget for non-widget targets.
 * zh_CN: 解析事件接收者所属的顶层窗口；若接收者不是控件，则改用当前焦点控件所属窗口。
 */
QWidget* eventTopLevel(QObject* watched);

/**
 * @brief Refreshes descendant Fluent widgets after an overlay inherits a local theme override.
 * zh_CN: 当浮层继承局部主题覆盖时，递归刷新其内部的 Fluent 控件。
 */
void refreshFluentDescendants(QWidget* root);

} // namespace fluent::overlay

#endif // OVERLAYWINDOW_H
