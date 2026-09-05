#ifndef FLUENTWINDOWBACKDROPEVENTS_P_H
#define FLUENTWINDOWBACKDROPEVENTS_P_H

class QEvent;
class QWidget;

namespace compatibility::detail {

// Internal platform-backend hook. Requests are posted and coalesced by Window.
void requestWindowBackdropReevaluation(QWidget* widget);
bool isWindowBackdropReevaluationEvent(const QEvent* event);

} // namespace compatibility::detail

#endif // FLUENTWINDOWBACKDROPEVENTS_P_H
