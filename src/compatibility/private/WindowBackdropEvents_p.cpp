#include "WindowBackdropEvents_p.h"

#include <QCoreApplication>
#include <QEvent>
#include <QWidget>

namespace compatibility::detail {
namespace {

QEvent::Type reevaluationEventType()
{
    static const int type = QEvent::registerEventType();
    return static_cast<QEvent::Type>(type);
}

} // namespace

void requestWindowBackdropReevaluation(QWidget* widget)
{
    QWidget* topLevel = widget ? widget->window() : nullptr;
    if (!topLevel || !QCoreApplication::instance())
        return;
    QCoreApplication::postEvent(topLevel, new QEvent(reevaluationEventType()));
}

bool isWindowBackdropReevaluationEvent(const QEvent* event)
{
    return event && event->type() == reevaluationEventType();
}

} // namespace compatibility::detail
