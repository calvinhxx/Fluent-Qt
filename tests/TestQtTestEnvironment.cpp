#include "QtTestEnvironment.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>
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

TEST(QtTestEnvironmentTest, BaselineFilePathUsesIdentityAndVariant)
{
    const QString path = tests::support::visualBaselineFilePath(QStringLiteral("button-states-light-ltr"));
    const QFileInfo info(path);

    EXPECT_EQ(info.fileName(),
              QStringLiteral("test_qt_test_environment__QtTestEnvironmentTest__BaselineFilePathUsesIdentityAndVariant__button-states-light-ltr.png"));
}

TEST(QtTestEnvironmentTest, VisualCompareIdenticalImagesSucceeds)
{
    QImage image(24, 16, QImage::Format_ARGB32);
    image.fill(Qt::white);
    EXPECT_TRUE(tests::support::compareVisualImages(image, image));
}

TEST(QtTestEnvironmentTest, VisualCompareDifferentImagesFails)
{
    QImage actual(24, 16, QImage::Format_ARGB32);
    actual.fill(Qt::white);
    QImage expected(24, 16, QImage::Format_ARGB32);
    expected.fill(Qt::black);
    EXPECT_FALSE(tests::support::compareVisualImages(actual, expected));
}

TEST(QtTestEnvironmentTest, VisualCompareSizeMismatchFails)
{
    QImage actual(24, 16, QImage::Format_ARGB32);
    actual.fill(Qt::white);
    QImage expected(12, 8, QImage::Format_ARGB32);
    expected.fill(Qt::white);
    EXPECT_FALSE(tests::support::compareVisualImages(actual, expected));
}

TEST(QtTestEnvironmentTest, VisualCompareMissingBaselineFails)
{
    QTemporaryDir temp;
    ASSERT_TRUE(temp.isValid());
    EnvVarGuard baselineGuard("FLUENT_QT_VISUAL_BASELINE_DIR", temp.path().toUtf8());

    QImage actual(24, 16, QImage::Format_ARGB32);
    actual.fill(Qt::white);
    const QString actualPath = temp.filePath(QStringLiteral("actual.png"));
    ASSERT_TRUE(actual.save(actualPath, "PNG"));

    const auto result = tests::support::compareVisualSnapshotToBaseline(
        actualPath, QStringLiteral("missing-baseline"));
    EXPECT_FALSE(result);
    EXPECT_TRUE(QString::fromUtf8(result.message()).contains(QStringLiteral("Missing visual baseline")));
}

TEST(QtTestEnvironmentTest, VisualGateRejectsWrongScale)
{
    EnvVarGuard scaleGuard("QT_SCALE_FACTOR", QByteArray("2"));
    EnvVarGuard dpiGuard("QT_FONT_DPI", QByteArray("96"));

    EXPECT_FALSE(tests::support::isVisualGateApprovalHost());
}

TEST(QtTestEnvironmentTest, VisualCompareToBaselineDetectsMismatch)
{
    QTemporaryDir temp;
    ASSERT_TRUE(temp.isValid());
    EnvVarGuard baselineGuard("FLUENT_QT_VISUAL_BASELINE_DIR", temp.path().toUtf8());

    QImage expected(24, 16, QImage::Format_ARGB32);
    expected.fill(Qt::white);
    const QString baselinePath =
        tests::support::visualBaselineFilePath(QStringLiteral("mismatch"));
    ASSERT_TRUE(expected.save(baselinePath, "PNG"));

    QImage actual(24, 16, QImage::Format_ARGB32);
    actual.fill(Qt::red);
    const QString actualPath = temp.filePath(QStringLiteral("actual.png"));
    ASSERT_TRUE(actual.save(actualPath, "PNG"));

    EXPECT_FALSE(tests::support::compareVisualSnapshotToBaseline(
        actualPath, QStringLiteral("mismatch")));
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
