#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <QApplication>
#include <QPointer>
#include <QWidget>

namespace fluent::overlay {

inline QWidget* resolveOwningTopLevel(const QPointer<QWidget>& originalParent, QWidget* currentParent)
{
    if (originalParent)
        return originalParent->window();
    if (currentParent)
        return currentParent->window();
    return nullptr;
}

inline void attachToTopLevel(QWidget* overlay, QWidget* topLevel)
{
    if (!overlay || !topLevel)
        return;
    if (overlay->parentWidget() != topLevel) {
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

inline QWidget* eventTopLevel(QObject* watched)
{
    if (auto* widget = qobject_cast<QWidget*>(watched))
        return widget->window();
    return QApplication::focusWidget() ? QApplication::focusWidget()->window() : nullptr;
}

} // namespace fluent::overlay

#endif // OVERLAYWINDOW_H