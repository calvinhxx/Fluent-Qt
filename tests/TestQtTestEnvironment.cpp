#include "QtTestEnvironment.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QWidget>

#include <gtest/gtest.h>

namespace {
class EnvVarGuard {
public:
    EnvVarGuard(const char* name, const QByteArray& value)
        : m_name(name), m_hadValue(qEnvironmentVariableIsSet(name)), m_previous(qgetenv(name))
    {
        qputenv(m_name, value);
    }

    explicit EnvVarGuard(const char* name)
        : m_name(name), m_hadValue(qEnvironmentVariableIsSet(name)), m_previous(qgetenv(name))
    {
        qunsetenv(m_name);
    }

    ~EnvVarGuard()
    {
        if (m_hadValue)
            qputenv(m_name, m_previous);
        else
            qunsetenv(m_name);
    }

private:
    const char* m_name;
    bool m_hadValue;
    QByteArray m_previous;
};
} // namespace

TEST(QtTestEnvironmentTest, SnapshotModeRespectsSkipPrecedence)
{
    EnvVarGuard skipGuard("SKIP_VISUAL_TEST", QByteArray("1"));
    EnvVarGuard snapshotGuard("VISUAL_SNAPSHOT", QByteArray("1"));

    EXPECT_TRUE(tests::support::shouldSkipVisualTest());
    EXPECT_TRUE(tests::support::isVisualSnapshotMode());
    EXPECT_FALSE(tests::support::shouldCaptureVisualSnapshot());
}

TEST(QtTestEnvironmentTest, SnapshotFilePathUsesIdentityAndVariant)
{
    const QString path = tests::support::visualSnapshotFilePath(QStringLiteral("Light Theme"));
    const QFileInfo info(path);

    EXPECT_EQ(info.dir().dirName(), QStringLiteral("visual"));
    EXPECT_EQ(info.fileName(),
              QStringLiteral("test_qt_test_environment__QtTestEnvironmentTest__SnapshotFilePathUsesIdentityAndVariant__Light_Theme.png"));
}

TEST(QtTestEnvironmentTest, SnapshotCaptureSavesNonEmptyPng)
{
    EnvVarGuard skipGuard("SKIP_VISUAL_TEST");
    EnvVarGuard snapshotGuard("VISUAL_SNAPSHOT", QByteArray("1"));

    QWidget window;
    window.setStyleSheet(QStringLiteral("background-color: white;"));

    tests::support::VisualSnapshotOptions options;
    options.windowSize = QSize(240, 160);
    options.variant = QStringLiteral("HelperSmoke");

    const QString outputPath = tests::support::visualSnapshotFilePath(options.variant);
    QFile::remove(outputPath);

    ASSERT_TRUE(tests::support::captureVisualSnapshot(&window, options));

    const QFileInfo outputInfo(outputPath);
    EXPECT_TRUE(outputInfo.exists());
    EXPECT_GT(outputInfo.size(), 0);
    EXPECT_EQ(QImageReader(outputPath).size(), options.windowSize);
}

TEST(QtTestEnvironmentLabelMetadata, SlowMetadataLabel)
{
    SUCCEED();
}

TEST(QtTestEnvironmentLabelMetadata, WindowsMetadataLabel)
{
    SUCCEED();
}

TEST(QtTestEnvironmentLabelMetadata, MacOSMetadataLabel)
{
    SUCCEED();
}

TEST(QtTestEnvironmentLabelMetadata, AnimationMetadataLabel)
{
    SUCCEED();
}

TEST(QtTestEnvironmentLabelMetadata, InteractiveMetadataLabel)
{
    SUCCEED();
}