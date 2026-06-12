#ifndef UTILS_LOG_H
#define UTILS_LOG_H

#include <QString>
#include <QtGlobal>

#include <string>

namespace utils::logging {

/**
 * @brief Logging severity level shared by LOG_* macros and the spdlog backend.
 * zh_CN: LOG_* 宏和 spdlog 后端共用的日志级别。
 */
enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

/**
 * @brief Runtime logging configuration.
 * zh_CN: 日志运行时配置。
 */
struct InitializationOptions {
    Level defaultLevel = Level::Warn;
    bool installQtMessageHandler = false;

    // Explicit file path wins; empty falls back to SPDLOG_FILE.
    // zh_CN: 显式路径优先；为空时退回到 SPDLOG_FILE 环境变量。
    QString logFilePath;

    // Size-based rotation: a full file shifts into numbered backups
    // (<name>.1, <name>.2, ...); with 2 backups at most 3 files live on disk.
    // zh_CN: 按大小轮转：写满的文件平移为编号备份（<name>.1、<name>.2……）；
    // 保留 2 个备份时磁盘上最多存在 3 个文件。
    qint64 maxFileSizeBytes = 5 * 1024 * 1024;
    int maxRotatedFiles = 2;
};

/**
 * @brief Initializes the shared logger.
 * zh_CN: 初始化共享日志器。
 *
 * The effective level is resolved from SPDLOG_LEVEL when present, otherwise
 * options.defaultLevel is used.
 * zh_CN: 如果存在 SPDLOG_LEVEL，则优先使用环境变量中的级别；否则使用
 * options.defaultLevel。
 */
void initialize(const InitializationOptions& options = {});
void shutdown();
void flush();

bool isInitialized();
Level level();
Level levelFromName(const QString& name, Level fallback);
void setLevel(Level level);

/**
 * @brief Returns the platform-standard persistent log file path.
 * zh_CN: 返回平台标准持久化日志文件路径。
 *
 * macOS/Linux use AppLocalDataLocation/logs/<app>.log; Windows uses
 * AppLocalDataLocation/logs/<app>.log with the native separator. Pass the returned path to
 * InitializationOptions::logFilePath to enable file logging explicitly.
 */
QString defaultLogFilePath();
void installQtMessageHandler();

void log(Level level, const char* file, int line, const char* function, const QString& message);
void log(Level level, const char* file, int line, const char* function, const std::string& message);
void log(Level level, const char* file, int line, const char* function, const char* message);

} // namespace utils::logging

#define LOG_TRACE(message) \
    ::utils::logging::log(::utils::logging::Level::Trace, __FILE__, __LINE__, Q_FUNC_INFO, (message))

#define LOG_DEBUG(message) \
    ::utils::logging::log(::utils::logging::Level::Debug, __FILE__, __LINE__, Q_FUNC_INFO, (message))

#define LOG_INFO(message) \
    ::utils::logging::log(::utils::logging::Level::Info, __FILE__, __LINE__, Q_FUNC_INFO, (message))

#define LOG_WARN(message) \
    ::utils::logging::log(::utils::logging::Level::Warn, __FILE__, __LINE__, Q_FUNC_INFO, (message))

#define LOG_ERROR(message) \
    ::utils::logging::log(::utils::logging::Level::Error, __FILE__, __LINE__, Q_FUNC_INFO, (message))

#define LOG_CRITICAL(message) \
    ::utils::logging::log(::utils::logging::Level::Critical, __FILE__, __LINE__, Q_FUNC_INFO, (message))

#endif // UTILS_LOG_H
