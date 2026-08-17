#ifndef FLUENTQT_COMPONENTS_STATUS_INFO_PRIVATE_TOOLTIPACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_STATUS_INFO_PRIVATE_TOOLTIPACCESSIBILITY_P_H

namespace fluent::status_info {

class ToolTip;

namespace detail {

class ToolTipAccessible;

void ensureToolTipAccessibilityFactory();
void notifyToolTipAccessibilityTextChanged(ToolTip* toolTip);
void notifyToolTipAccessibilityVisibilityChanged(ToolTip* toolTip);
void notifyToolTipAccessibilityTargetChanged(ToolTip* toolTip);

} // namespace detail
} // namespace fluent::status_info

#endif // FLUENTQT_COMPONENTS_STATUS_INFO_PRIVATE_TOOLTIPACCESSIBILITY_P_H
