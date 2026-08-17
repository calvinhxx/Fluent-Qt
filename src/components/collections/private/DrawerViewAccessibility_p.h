#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_DRAWERVIEWACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_DRAWERVIEWACCESSIBILITY_P_H

namespace fluent::collections {

class DrawerView;

namespace detail {

void ensureDrawerViewAccessibilityFactory();
void notifyDrawerViewAccessibilityOpenChanged(DrawerView* drawer);
void notifyDrawerViewAccessibilityModalChanged(DrawerView* drawer);
void notifyDrawerViewAccessibilityActionsChanged(DrawerView* drawer);
void notifyDrawerViewAccessibilityContentChanged(DrawerView* drawer);

} // namespace detail
} // namespace fluent::collections

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_DRAWERVIEWACCESSIBILITY_P_H
