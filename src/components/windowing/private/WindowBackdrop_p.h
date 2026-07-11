#ifndef FLUENTWINDOWBACKDROP_P_H
#define FLUENTWINDOWBACKDROP_P_H

class QEvent;
class QWidget;

namespace fluent::windowing {

// Internal platform-backend hook. Requests are posted and coalesced by Window.
void requestWindowBackdropReevaluation(QWidget* widget);
bool isWindowBackdropReevaluationEvent(const QEvent* event);

} // namespace fluent::windowing

#endif // FLUENTWINDOWBACKDROP_P_H
