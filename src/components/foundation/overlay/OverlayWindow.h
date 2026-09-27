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

/** @brief Resolve the native overlay host, including embedded widget presentations.
 * zh_CN: 解析原生浮层宿主，包括嵌入控件的显示宿主。 */
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
    if (overlay && overlay->isVisible())
        overlay->raise();
}

/** @brief Resolve the event receiver's native host, falling back to the focused widget.
 * zh_CN: 解析事件接收者的原生宿主；非控件接收者使用焦点控件。 */
QWidget* eventTopLevel(QObject* watched);

} // namespace fluent::overlay

#endif // OVERLAYWINDOW_H
