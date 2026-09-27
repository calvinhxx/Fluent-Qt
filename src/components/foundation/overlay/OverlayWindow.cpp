#include "OverlayWindow.h"

#include "compatibility/WidgetPresentationCompat_p.h"

namespace fluent::overlay {

QWidget* resolveOwningTopLevel(const QPointer<QWidget>& originalParent, QWidget* currentParent)
{
    if (originalParent)
        return compat::widgetPresentationWindow(originalParent);
    if (currentParent)
        return compat::widgetPresentationWindow(currentParent);
    return nullptr;
}

QWidget* eventTopLevel(QObject* watched)
{
    if (auto* widget = qobject_cast<QWidget*>(watched))
        return compat::widgetPresentationWindow(widget);
    return compat::widgetPresentationWindow(QApplication::focusWidget());
}

} // namespace fluent::overlay
