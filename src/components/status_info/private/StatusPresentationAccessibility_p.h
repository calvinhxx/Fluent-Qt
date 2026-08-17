#ifndef FLUENTQT_COMPONENTS_STATUS_INFO_PRIVATE_STATUSPRESENTATIONACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_STATUS_INFO_PRIVATE_STATUSPRESENTATIONACCESSIBILITY_P_H

#include <QString>

namespace fluent::status_info {

class InfoBar;
class Shimmer;

namespace detail {

class InfoBarAccessible;

void ensureStatusPresentationAccessibilityFactory();

QString infoBarDismissAccessibleName();
void notifyInfoBarAccessibilityContentChanged(InfoBar* bar);
void notifyInfoBarAccessibilitySeverityChanged(InfoBar* bar);
void notifyInfoBarAccessibilityOpenChanged(InfoBar* bar);
void notifyInfoBarAccessibilityDismissChanged(InfoBar* bar);
void notifyInfoBarAccessibilityStructureChanged(InfoBar* bar);
void notifyInfoBarAccessibilityEnabledChanged(InfoBar* bar);

void notifyShimmerAccessibilityActiveChanged(Shimmer* shimmer);
void notifyShimmerAccessibilityAnimationChanged(Shimmer* shimmer);
void notifyShimmerAccessibilityEnabledChanged(Shimmer* shimmer);

} // namespace detail
} // namespace fluent::status_info

#endif // FLUENTQT_COMPONENTS_STATUS_INFO_PRIVATE_STATUSPRESENTATIONACCESSIBILITY_P_H
