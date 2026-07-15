#include "support/logging/Log.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QString>
#include <QStandardPaths>
#include <QtGlobal>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef Q_OS_WIN
#include <spdlog/sinks/msvc_sink.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr const char* kLoggerName = "fluentqt";
constexpr const char* kLevelEnvVar = "SPDLOG_LEVEL";
constexpr const char* kFileEnvVar = "SPDLOG_FILE";

std::mutex g_mutex;
std::shared_ptr<spdlog::logger> g_logger;
fluent::support::logging::Level g_defaultLevel = fluent::support::logging::Level::Warn;
QtMessageHandler g_previousQtHandler = nullptr;
bool g_qtHandlerInstalled = false;
thread_local bool g_inQtMessageHandler = false;

spdlog::level::level_enum toSpdlogLevel(fluent::support::logging::Level level)
{
    switch (level) {
    case fluent::support::logging::Level::Trace:
        return spdlog::level::trace;
    case fluent::support::logging::Level::Debug:
        return spdlog::level::debug;
    case fluent::support::logging::Level::Info:
        return spdlog::level::info;
    case fluent::support::logging::Level::Warn:
        return spdlog::level::warn;
    case fluent::support::logging::Level::Error:
        return spdlog::level::err;
    case fluent::support::logging::Level::Critical:
        return spdlog::level::critical;
    case fluent::support::logging::Level::Off:
        return spdlog::level::off;
    }

    return spdlog::level::warn;
}

fluent::support::logging::Level fromSpdlogLevel(spdlog::level::level_enum level)
{
    switch (level) {
    case spdlog::level::trace:
        return fluent::support::logging::Level::Trace;
    case spdlog::level::debug:
        return fluent::support::logging::Level::Debug;
    case spdlog::level::info:
        return fluent::support::logging::Level::Info;
    case spdlog::level::warn:
        return fluent::support::logging::Level::Warn;
    case spdlog::level::err:
        return fluent::support::logging::Level::Error;
    case spdlog::level::critical:
        return fluent::support::logging::Level::Critical;
    case spdlog::level::off:
        return fluent::support::logging::Level::Off;
    default:
        return fluent::support::logging::Level::Warn;
    }
}

std::shared_ptr<spdlog::logger> logger()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_logger;
}

std::shared_ptr<spdlog::logger> createLogger(const fluent::support::logging::InitializationOptions& options)
{
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
#ifdef Q_OS_WIN
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

    // 显式 logFilePath 优先；其次读 SPDLOG_FILE 环境变量
    const QString filePath = options.logFilePath.isEmpty()
        ? qEnvironmentVariable(kFileEnvVar)
        : options.logFilePath;
    if (!filePath.isEmpty()) {
        const QFileInfo fileInfo(filePath);
        const QString dirPath = fileInfo.absolutePath();
        if (!dirPath.isEmpty())
            QDir().mkpath(dirPath);

        try {
            // spdlog keeps the active file plus maxRotatedFiles numbered backups,
            // so the on-disk total is maxRotatedFiles + 1.
            // zh_CN: spdlog 保留当前文件加 maxRotatedFiles 个编号备份，
            // 磁盘文件总数为 maxRotatedFiles + 1。
            const auto maxFileSize = static_cast<std::size_t>(
                std::max<qint64>(options.maxFileSizeBytes, 1024));
            const auto maxFiles = static_cast<std::size_t>(
                std::max(options.maxRotatedFiles, 1));
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                filePath.toStdString(), maxFileSize, maxFiles));
        } catch (const spdlog::spdlog_ex& ex) {
            std::fprintf(stderr, "spdlog: failed to open log file '%s': %s\n",
                         filePath.toLocal8Bit().constData(), ex.what());
        }
    }

    auto created = std::make_shared<spdlog::logger>(kLoggerName, sinks.begin(), sinks.end());
    created->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%s:%#] %v");
    created->set_level(toSpdlogLevel(fluent::support::logging::levelFromName(
        qEnvironmentVariable(kLevelEnvVar), options.defaultLevel)));
    // Flush from info up: lifecycle lines reach the file as they happen, and an
    // abnormal exit cannot lose them. Volume at info level is low enough that
    // per-line flushing costs nothing noticeable; debug/trace bursts stay buffered.
    // zh_CN: 从 info 起即刷盘：生命周期日志实时落入文件，异常退出也不会丢失。
    // info 级别量很小，逐行刷盘开销可忽略；debug/trace 洪峰仍走缓冲。
    created->flush_on(spdlog::level::info);
    spdlog::register_logger(created);
    spdlog::set_default_logger(created);
    return created;
}

void writeQtMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (g_inQtMessageHandler)
        return;

    g_inQtMessageHandler = true;

    fluent::support::logging::Level level = fluent::support::logging::Level::Debug;
    switch (type) {
    case QtDebugMsg:
        level = fluent::support::logging::Level::Debug;
        break;
    case QtInfoMsg:
        level = fluent::support::logging::Level::Info;
        break;
    case QtWarningMsg:
        level = fluent::support::logging::Level::Warn;
        break;
    case QtCriticalMsg:
        level = fluent::support::logging::Level::Error;
        break;
    case QtFatalMsg:
        level = fluent::support::logging::Level::Critical;
        break;
    }

    const QString categorizedMessage = context.category && *context.category
        ? QStringLiteral("[%1] %2").arg(QString::fromLatin1(context.category), message)
        : message;
    fluent::support::logging::log(level,
                              context.file ? context.file : "",
                              context.line,
                              context.function ? context.function : "",
                              categorizedMessage);

    g_inQtMessageHandler = false;
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    writeQtMessage(type, context, message);

    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

namespace fluent::support::logging {

void initialize(const InitializationOptions& options)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_defaultLevel = options.defaultLevel;
        if (!g_logger)
            g_logger = createLogger(options);
        else
            g_logger->set_level(toSpdlogLevel(levelFromName(qEnvironmentVariable(kLevelEnvVar), g_defaultLevel)));
    }

    if (options.installQtMessageHandler)
        installQtMessageHandler();
}

void shutdown()
{
    std::shared_ptr<spdlog::logger> loggerToFlush;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_qtHandlerInstalled) {
            qInstallMessageHandler(g_previousQtHandler);
            g_previousQtHandler = nullptr;
            g_qtHandlerInstalled = false;
        }

        loggerToFlush = g_logger;
        g_logger.reset();
    }

    if (loggerToFlush)
        loggerToFlush->flush();

    spdlog::drop(kLoggerName);
}

void flush()
{
    if (const auto activeLogger = logger())
        activeLogger->flush();
}

bool isInitialized()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<bool>(g_logger);
}

Level level()
{
    if (const auto activeLogger = logger())
        return fromSpdlogLevel(activeLogger->level());

    return g_defaultLevel;
}

Level levelFromName(const QString& name, Level fallback)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("trace"))
        return Level::Trace;
    if (normalized == QStringLiteral("debug"))
        return Level::Debug;
    if (normalized == QStringLiteral("info"))
        return Level::Info;
    if (normalized == QStringLiteral("warn") || normalized == QStringLiteral("warning"))
        return Level::Warn;
    if (normalized == QStringLiteral("error") || normalized == QStringLiteral("err"))
        return Level::Error;
    if (normalized == QStringLiteral("critical") || normalized == QStringLiteral("fatal"))
        return Level::Critical;
    if (normalized == QStringLiteral("off"))
        return Level::Off;

    return fallback;
}

void setLevel(Level level)
{
    if (const auto activeLogger = logger())
        activeLogger->set_level(toSpdlogLevel(level));
}

void installQtMessageHandler()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_qtHandlerInstalled)
        return;

    g_previousQtHandler = qInstallMessageHandler(qtMessageHandler);
    g_qtHandlerInstalled = true;
}

QString defaultLogFilePath()
{
    const QString appName = QCoreApplication::applicationName().isEmpty()
        ? QStringLiteral("app")
        : QCoreApplication::applicationName();
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty())
        return {};
    return dir + QStringLiteral("/logs/") + appName + QStringLiteral(".log");
}

void log(Level level, const char* file, int line, const char* function, const QString& message)
{
    if (!isInitialized())
        initialize();

    if (const auto activeLogger = logger()) {
        activeLogger->log(spdlog::source_loc(file ? file : "", line, function ? function : ""),
                          toSpdlogLevel(level),
                          "{}", message.toStdString());
    }
}

void log(Level level, const char* file, int line, const char* function, const std::string& message)
{
    log(level, file, line, function, QString::fromStdString(message));
}

void log(Level level, const char* file, int line, const char* function, const char* message)
{
    log(level, file, line, function, QString::fromLocal8Bit(message ? message : ""));
}

} // namespace fluent::support::logging
