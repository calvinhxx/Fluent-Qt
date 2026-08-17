#ifndef FLUENTQT_COMPONENTS_NAVIGATION_PRIVATE_NAVIGATIONSELECTIONACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_NAVIGATION_PRIVATE_NAVIGATIONSELECTIONACCESSIBILITY_P_H

namespace fluent::navigation::detail {

class BreadcrumbAccessible;
class PivotAccessible;
class SelectorBarAccessible;
class TabViewAccessible;

// Installs the shared private logical-item factory for painted navigation
// controls. No adapter type is installed or application-facing API.
void ensureNavigationSelectionAccessibilityFactory();

} // namespace fluent::navigation::detail

#endif // FLUENTQT_COMPONENTS_NAVIGATION_PRIVATE_NAVIGATIONSELECTIONACCESSIBILITY_P_H
