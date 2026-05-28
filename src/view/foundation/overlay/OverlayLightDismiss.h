#ifndef OVERLAYLIGHTDISMISS_H
#define OVERLAYLIGHTDISMISS_H

#include <QEvent>
#include <QKeyEvent>

namespace view::overlay {

inline bool isEscapeKeyPress(QEvent* event)
{
    if (!event || event->type() != QEvent::KeyPress)
        return false;
    return static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape;
}

inline bool allowsImplicitClose(bool noAutoClose, bool closeFlagEnabled)
{
    return !noAutoClose && closeFlagEnabled;
}

} // namespace view::overlay

#endif // OVERLAYLIGHTDISMISS_H