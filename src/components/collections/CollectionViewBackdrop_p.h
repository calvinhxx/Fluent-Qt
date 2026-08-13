#pragma once

#include <QAbstractItemView>

#include "components/windowing/WindowBackdrop.h"

namespace fluent::collections::detail {

inline bool preservesParentSurface(const QAbstractItemView* view) {
    return view
        && (view->property("fluentPreserveParentSurface").toBool()
            || (view->viewport()
                && view->viewport()->property("fluentPreserveParentSurface").toBool()));
}

inline bool shouldClearCompositedViewport(const QAbstractItemView* view) {
    const QWidget* hostWindow = view ? view->window() : nullptr;
    return view
        && !preservesParentSurface(view)
        && hostWindow
        && hostWindow->testAttribute(Qt::WA_TranslucentBackground)
        && windowing::windowBackdropRequiresTransparentClear(hostWindow);
}

} // namespace fluent::collections::detail
