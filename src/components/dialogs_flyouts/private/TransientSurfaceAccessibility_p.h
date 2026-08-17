#ifndef FLUENTQT_COMPONENTS_DIALOGS_FLYOUTS_PRIVATE_TRANSIENTSURFACEACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_DIALOGS_FLYOUTS_PRIVATE_TRANSIENTSURFACEACCESSIBILITY_P_H

class QWidget;

namespace fluent::dialogs_flyouts {

class CoachMark;
class Popup;

namespace detail {

void ensureTransientSurfaceAccessibilityFactory();
void notifyPopupAccessibilityOpenChanged(Popup* popup, bool open);
void notifyPopupAccessibilityModalChanged(Popup* popup);
void notifyPopupAccessibilityActionsChanged(Popup* popup);
void notifyTransientSurfaceAccessibilityRelationChanged(QWidget* surface);
void notifyCoachMarkAccessibilityOpenChanged(CoachMark* coachMark, bool open);

} // namespace detail
} // namespace fluent::dialogs_flyouts

#endif // FLUENTQT_COMPONENTS_DIALOGS_FLYOUTS_PRIVATE_TRANSIENTSURFACEACCESSIBILITY_P_H
