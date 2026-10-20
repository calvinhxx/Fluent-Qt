#include "OverlayWindow.h"

#include "compatibility/private/WidgetPresentationCompat_p.h"
#include "components/foundation/FluentElement.h"

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

void refreshFluentDescendants(QWidget* root)
{
    if (!root)
        return;
    const auto children = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children) {
        if (auto* fluentChild = dynamic_cast<FluentElement*>(child))
            fluentChild->onThemeUpdated();
        refreshFluentDescendants(child);
    }
}

} // namespace fluent::overlay
