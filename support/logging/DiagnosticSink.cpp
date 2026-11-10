#include "Log.h"

#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex sinkMutex;
fluent::support::logging::DiagnosticSink diagnosticSink;
thread_local bool forwarding = false;
} // namespace

namespace fluent::support::logging {
void setDiagnosticSink(DiagnosticSink sink)
{
    QMutexLocker lock(&sinkMutex);
    diagnosticSink = std::move(sink);
}

void forwardDiagnosticMessage(QtMsgType type, const QString& category, const QString& message)
{
    if (forwarding)
        return;
    DiagnosticSink sink;
    {
        QMutexLocker lock(&sinkMutex);
        sink = diagnosticSink;
    }
    if (sink) {
        forwarding = true;
        sink(type, category, message);
        forwarding = false;
    }
}
} // namespace fluent::support::logging
