#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_FLIPVIEWACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_FLIPVIEWACCESSIBILITY_P_H

namespace fluent::collections {

class FlipView;

namespace detail {

void ensureFlipViewAccessibilityFactory();
void notifyFlipViewAccessibilityStructureChanged(
    FlipView* view, int oldCount, int oldIndex);
void notifyFlipViewAccessibilityCurrentChanged(
    FlipView* view, int oldIndex);
void notifyFlipViewAccessibilityOrientationChanged(FlipView* view);

} // namespace detail
} // namespace fluent::collections

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_FLIPVIEWACCESSIBILITY_P_H
