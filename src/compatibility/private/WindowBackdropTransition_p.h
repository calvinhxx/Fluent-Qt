#ifndef FLUENTWINDOWBACKDROPTRANSITION_P_H
#define FLUENTWINDOWBACKDROPTRANSITION_P_H

#include "compatibility/WindowBackdropTypes.h"

class QWidget;
class QColor;

namespace compatibility::detail {

// Seed Qt and native surfaces before exposure; only a resolved compositor may clear them.
void prepareWindowBackdropSurface(QWidget* window, const QColor& activeColor,
                                  const QColor& inactiveColor, bool active,
                                  fluent::windowing::BackdropSurfaceMode mode);
QColor windowBackdropSurfaceColor(const QWidget* window);

bool windowBackdropSurfaceWasPainted(const QWidget* window);
void markWindowBackdropSurfacePainted(QWidget* window);
void resetWindowBackdropSurfacePaint(QWidget* window);

// Native material teardown must not race a still-transparent client frame.
bool requiresOpaqueBackdropCommit(const QWidget* window,
                                  const fluent::windowing::BackdropState& previous,
                                  fluent::windowing::BackdropSurfaceMode next);

// Flush a submitted opaque frame without dispatching input or arbitrary events.
bool flushWindowBackdropSurface(QWidget* window);

} // namespace compatibility::detail

#endif // FLUENTWINDOWBACKDROPTRANSITION_P_H
