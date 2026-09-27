#ifndef FLUENTWINDOWBACKDROPTRANSITION_P_H
#define FLUENTWINDOWBACKDROPTRANSITION_P_H

#include "compatibility/WindowBackdropTypes.h"

class QWidget;

namespace compatibility::detail {

// Native material teardown must not race a still-transparent client frame.
bool requiresOpaqueBackdropCommit(const QWidget* window,
                                  const fluent::windowing::BackdropState& previous,
                                  fluent::windowing::BackdropEffect requested);

// Flush a submitted opaque frame without dispatching input or arbitrary events.
bool flushWindowBackdropSurface(QWidget* window);

} // namespace compatibility::detail

#endif // FLUENTWINDOWBACKDROPTRANSITION_P_H
