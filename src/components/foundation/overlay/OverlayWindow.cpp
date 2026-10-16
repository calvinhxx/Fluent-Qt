#include "OverlayWindow.h"

#include "compatibility/private/WidgetPresentationCompat_p.h"

namespace fluent::overlay {

QWidget* resolveOwningTopLevel(const QPointer<QWidget>& originalParent, QWidget* currentParent)
{
    if (originalParent)
        return ::compatibility::detail::widgetPresentationWindow(originalParent);
    if (currentParent)
        return ::compatibility::detail::widgetPresentationWindow(currentParent);
    return nullptr;
}

QWidget* eventTopLevel(QObject* watched)
{
    if (auto* widget = qobject_cast<QWidget*>(watched))
        return ::compatibility::detail::widgetPresentationWindow(widget);
    return ::compatibility::detail::widgetPresentationWindow(QApplication::focusWidget());
}

} // namespace fluent::overlay
