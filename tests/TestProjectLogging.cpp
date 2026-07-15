#include "support/logging/Log.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>
#include <gtest/gtest.h>

namespace {

Q_LOGGING_CATEGORY(fluentQtBridgeTestCategory, "fluentqt.test")

class EnvVarGuard {
public:
    explicit EnvVarGuard(const char* name)
        : m_name(name)
        , m_hadValue(qEnvironmentVariableIsSet(name))
        , m_value(qgetenv(name))
    {
    }

    ~EnvVarGuard()
    {
        if (m_hadValue)
            qputenv(m_name.constData(), m_value);
        else
            qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    bool m_hadValue = false;
    QByteArray m_value;
};

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    return QString::fromUtf8(file.readAll());
}

QByteArray filePathBytes(const QString& path)
{
    return path.toLocal8Bit();
}

QString levelSampleMessage(const QString& sink, const QString& level)
{
    return QStringLiteral("ProjectLoggingTest sink=%1 level=%2 example=all-levels").arg(sink, level);
}

void emitAllLevelSamples(const QString& sink)
{
    LOG_TRACE(levelSampleMessage(sink, QStringLiteral("trace")));
    LOG_DEBUG(levelSampleMessage(sink, QStringLiteral("debug")));
    LOG_INFO(levelSampleMessage(sink, QStringLiteral("info")));
    LOG_WARN(levelSampleMessage(sink, QStringLiteral("warn")));
    LOG_ERROR(levelSampleMessage(sink, QStringLiteral("error")));
    LOG_CRITICAL(levelSampleMessage(sink, QStringLiteral("critical")));
}

void expectContainsText(const QString& text, const QString& needle)
{
    EXPECT_TRUE(text.contains(needle)) << needle.toStdString();
}

} // namespace

TEST(ProjectLoggingTest, EnvironmentLevelControlsLogger)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    qunsetenv("SPDLOG_FILE");
    qputenv("SPDLOG_LEVEL", "debug");

    fluent::support::logging::shutdown();
    fluent::support::logging::initialize();

    EXPECT_TRUE(fluent::support::logging::isInitialized());
    EXPECT_EQ(fluent::support::logging::Level::Debug, fluent::support::logging::level());

    fluent::support::logging::shutdown();
}

TEST(ProjectLoggingTest, InvalidEnvironmentLevelFallsBackToDefault)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    qunsetenv("SPDLOG_FILE");
    qputenv("SPDLOG_LEVEL", "not-a-level");

    fluent::support::logging::shutdown();
    fluent::support::logging::initialize();

    EXPECT_EQ(fluent::support::logging::Level::Warn, fluent::support::logging::level());

    fluent::support::logging::shutdown();
}

TEST(ProjectLoggingTest, RepeatedInitializationDoesNotDuplicateSinks)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString logPath = dir.filePath(QStringLiteral("project-logging.log"));
    qputenv("SPDLOG_LEVEL", "warn");
    qputenv("SPDLOG_FILE", filePathBytes(logPath));

    fluent::support::logging::shutdown();
    fluent::support::logging::initialize();
    fluent::support::logging::initialize();

    LOG_WARN("project logging idempotent smoke");
    fluent::support::logging::flush();

    const QString logText = readTextFile(logPath);
    EXPECT_EQ(1, logText.count(QStringLiteral("project logging idempotent smoke")));

    fluent::support::logging::shutdown();
}

TEST(ProjectLoggingTest, AllLevelsWriteToTerminalAndDebuggerConsole)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    qunsetenv("SPDLOG_FILE");
    qputenv("SPDLOG_LEVEL", "trace");

    fluent::support::logging::shutdown();
    fluent::support::logging::initialize();

    EXPECT_EQ(fluent::support::logging::Level::Trace, fluent::support::logging::level());
    emitAllLevelSamples(QStringLiteral("terminal"));
#ifdef Q_OS_WIN
    emitAllLevelSamples(QStringLiteral("vs-debug-console"));
#endif
    fluent::support::logging::flush();

    fluent::support::logging::shutdown();
}

TEST(ProjectLoggingTest, AllLevelsWriteToConfiguredLogFile)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString logPath = dir.filePath(QStringLiteral("all-levels.log"));
    qputenv("SPDLOG_LEVEL", "trace");
    qputenv("SPDLOG_FILE", filePathBytes(logPath));

    fluent::support::logging::shutdown();
    fluent::support::logging::initialize();

    emitAllLevelSamples(QStringLiteral("file"));
    fluent::support::logging::flush();

    const QString logText = readTextFile(logPath);
    expectContainsText(logText, QStringLiteral("[trace]"));
    expectContainsText(logText, QStringLiteral("[debug]"));
    expectContainsText(logText, QStringLiteral("[info]"));
    expectContainsText(logText, QStringLiteral("[warning]"));
    expectContainsText(logText, QStringLiteral("[error]"));
    expectContainsText(logText, QStringLiteral("[critical]"));
    expectContainsText(logText, levelSampleMessage(QStringLiteral("file"), QStringLiteral("trace")));
    expectContainsText(logText, levelSampleMessage(QStringLiteral("file"), QStringLiteral("debug")));
    expectContainsText(logText, levelSampleMessage(QStringLiteral("file"), QStringLiteral("info")));
    expectContainsText(logText, levelSampleMessage(QStringLiteral("file"), QStringLiteral("warn")));
    expectContainsText(logText, levelSampleMessage(QStringLiteral("file"), QStringLiteral("error")));
    expectContainsText(logText, levelSampleMessage(QStringLiteral("file"), QStringLiteral("critical")));

    fluent::support::logging::shutdown();
}

TEST(ProjectLoggingTest, RotationKeepsAtMostThreeFilesOnDisk)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    qunsetenv("SPDLOG_LEVEL");
    qunsetenv("SPDLOG_FILE");
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    fluent::support::logging::shutdown();
    fluent::support::logging::InitializationOptions options;
    options.logFilePath = dir.filePath(QStringLiteral("rotation.log"));
    // A tiny size cap forces several rotations from a short burst of lines.
    // zh_CN: 极小的尺寸上限让一小段日志就触发多次轮转。
    options.maxFileSizeBytes = 1024;
    options.maxRotatedFiles = 2;
    fluent::support::logging::initialize(options);

    for (int i = 0; i < 200; ++i) {
        LOG_WARN(QStringLiteral("ProjectLoggingTest rotation filler line=%1 padding=%2")
                     .arg(i)
                     .arg(QString(80, QLatin1Char('x'))));
    }
    fluent::support::logging::flush();

    const QStringList files = QDir(dir.path()).entryList(QDir::Files, QDir::Name);
    EXPECT_EQ(3, files.size()) << files.join(QStringLiteral(", ")).toStdString();
    EXPECT_TRUE(files.contains(QStringLiteral("rotation.log")));
    EXPECT_TRUE(files.contains(QStringLiteral("rotation.1.log")));
    EXPECT_TRUE(files.contains(QStringLiteral("rotation.2.log")));

    fluent::support::logging::shutdown();
}

TEST(ProjectLoggingTest, DefaultLogFilePathUsesAppLocalLogsDirectory)
{
    const QString path = fluent::support::logging::defaultLogFilePath();
    ASSERT_FALSE(path.isEmpty());

    const QString expectedDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs/");
    EXPECT_TRUE(path.startsWith(expectedDir)) << path.toStdString();
    EXPECT_TRUE(path.endsWith(QStringLiteral(".log"))) << path.toStdString();
}

TEST(ProjectLoggingTest, QtWarningBridgeWritesThroughProjectLogger)
{
    EnvVarGuard levelGuard("SPDLOG_LEVEL");
    EnvVarGuard fileGuard("SPDLOG_FILE");
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString logPath = dir.filePath(QStringLiteral("qt-bridge.log"));
    qputenv("SPDLOG_LEVEL", "warn");
    qputenv("SPDLOG_FILE", filePathBytes(logPath));

    fluent::support::logging::shutdown();
    fluent::support::logging::InitializationOptions options;
    options.installQtMessageHandler = true;
    fluent::support::logging::initialize(options);

    qCWarning(fluentQtBridgeTestCategory) << "project logging qt bridge smoke";
    fluent::support::logging::flush();

    const QString logText = readTextFile(logPath);
    EXPECT_TRUE(logText.contains(QStringLiteral("project logging qt bridge smoke")));
    EXPECT_TRUE(logText.contains(QStringLiteral("[fluentqt.test]")));

    fluent::support::logging::shutdown();
}
