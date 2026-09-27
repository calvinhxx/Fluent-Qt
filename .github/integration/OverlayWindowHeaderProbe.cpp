#include <components/foundation/overlay/OverlayWindow.h>

// Compile and link without umbrella or private/source-tree headers.
bool verifyOverlayWindowHeaders(QWidget* child, QWidget* top)
{
    const QPointer<QWidget> original(child);
    return fluent::overlay::resolveOwningTopLevel(original, nullptr) == top &&
           fluent::overlay::resolveOwningTopLevel({}, child) == top &&
           fluent::overlay::resolveOwningTopLevel({}, nullptr) == nullptr &&
           fluent::overlay::eventTopLevel(child) == top;
}
