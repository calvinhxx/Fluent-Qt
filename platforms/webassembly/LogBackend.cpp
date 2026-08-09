#include "support/logging/Log.h"

#include <QByteArray>
#include <QMessageLogContext>
#include <QtGlobal>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr const char* kLevelEnvVar = "SPDLOG_LEVEL";

fluent::support::logging::Level g_level = fluent::support::logging::Level::Warn;
QtMessageHandler g_previousQtHandler = nullptr;
bool g_initialized = false;
bool g_qtHandlerInstalled = false;
thread_local bool g_inQtMessageHandler = false;

int levelRank(fluent::support::logging::Level level)
{
    return static_cast<int>(level);
}

const char* levelName(fluent::support::logging::Level level)
{
    switch (level) {
    case fluent::support::logging::Level::Trace: return "trace";
    case fluent::support::logging::Level::Debug: return "debug";
    case fluent::support::logging::Level::Info: return "info";
    case fluent::support::logging::Level::Warn: return "warn";
    case fluent::support::logging::Level::Error: return "error";
    case fluent::support::logging::Level::Critical: return "critical";
    case fluent::support::logging::Level::Off: return "off";
    }
    return "warn";
}

void writeQtMessage(QtMsgType type,
                    const QMessageLogContext& context,
                    const QString& message)
{
    if (g_inQtMessageHandler)
        return;

    g_inQtMessageHandler = true;
    fluent::support::logging::Level messageLevel =
        fluent::support::logging::Level::Debug;
    switch (type) {
    case QtDebugMsg: messageLevel = fluent::support::logging::Level::Debug; break;
    case QtInfoMsg: messageLevel = fluent::support::logging::Level::Info; break;
    case QtWarningMsg: messageLevel = fluent::support::logging::Level::Warn; break;
    case QtCriticalMsg: messageLevel = fluent::support::logging::Level::Error; break;
    case QtFatalMsg: messageLevel = fluent::support::logging::Level::Critical; break;
    }

    const QString categorized = context.category && *context.category
        ? QStringLiteral("[%1] %2").arg(QString::fromLatin1(context.category), message)
        : message;
    fluent::support::logging::log(messageLevel,
                                  context.file ? context.file : "",
                                  context.line,
                                  context.function ? context.function : "",
                                  categorized);
    g_inQtMessageHandler = false;
}

void qtMessageHandler(QtMsgType type,
                      const QMessageLogContext& context,
                      const QString& message)
{
    writeQtMessage(type, context, message);
    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

namespace fluent::support::logging {

void initialize(const InitializationOptions& options)
{
    g_level = levelFromName(qEnvironmentVariable(kLevelEnvVar), options.defaultLevel);
    g_initialized = true;
    if (options.installQtMessageHandler)
        installQtMessageHandler();
}

void shutdown()
{
    if (g_qtHandlerInstalled) {
        qInstallMessageHandler(g_previousQtHandler);
        g_previousQtHandler = nullptr;
        g_qtHandlerInstalled = false;
    }
    g_initialized = false;
}

void flush()
{
    std::fflush(stdout);
    std::fflush(stderr);
}

bool isInitialized()
{
    return g_initialized;
}

Level level()
{
    return g_level;
}

Level levelFromName(const QString& name, Level fallback)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("trace")) return Level::Trace;
    if (normalized == QStringLiteral("debug")) return Level::Debug;
    if (normalized == QStringLiteral("info")) return Level::Info;
    if (normalized == QStringLiteral("warn") || normalized == QStringLiteral("warning"))
        return Level::Warn;
    if (normalized == QStringLiteral("error") || normalized == QStringLiteral("err"))
        return Level::Error;
    if (normalized == QStringLiteral("critical") || normalized == QStringLiteral("fatal"))
        return Level::Critical;
    if (normalized == QStringLiteral("off")) return Level::Off;
    return fallback;
}

void setLevel(Level level)
{
    g_level = level;
}

QString defaultLogFilePath()
{
    return {};
}

void installQtMessageHandler()
{
    if (g_qtHandlerInstalled)
        return;
    g_previousQtHandler = qInstallMessageHandler(qtMessageHandler);
    g_qtHandlerInstalled = true;
}

void log(Level messageLevel,
         const char* file,
         int line,
         const char* function,
         const QString& message)
{
    if (!isInitialized())
        initialize();
    if (g_level == Level::Off || levelRank(messageLevel) < levelRank(g_level))
        return;

    const QByteArray encoded = message.toUtf8();
    FILE* stream = messageLevel >= Level::Warn ? stderr : stdout;
    std::fprintf(stream,
                 "[fluentqt] [%s] [%s:%d] [%s] %s\n",
                 levelName(messageLevel),
                 file ? file : "",
                 line,
                 function ? function : "",
                 encoded.constData());
    if (messageLevel >= Level::Info)
        std::fflush(stream);
}

void log(Level messageLevel,
         const char* file,
         int line,
         const char* function,
         const std::string& message)
{
    log(messageLevel, file, line, function, QString::fromStdString(message));
}

void log(Level messageLevel,
         const char* file,
         int line,
         const char* function,
         const char* message)
{
    log(messageLevel,
        file,
        line,
        function,
        QString::fromLocal8Bit(message ? message : ""));
}

} // namespace fluent::support::logging
