#include "QtTestEnvironment.h"

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <QXmlStreamReader>
#include <QtTest>

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
              QStringLiteral("test_qt_test_environment__QtTestEnvironmentTest__"
                             "SnapshotFilePathUsesIdentityAndVariant__Light_Theme.png"));
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
    const QString path =
        tests::support::visualBaselineFilePath(QStringLiteral("button-states-light-ltr"));
    const QFileInfo info(path);

    EXPECT_EQ(
        info.fileName(),
        QStringLiteral("test_qt_test_environment__QtTestEnvironmentTest__"
                       "BaselineFilePathUsesIdentityAndVariant__button-states-light-ltr.png"));
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
    EXPECT_TRUE(
        QString::fromUtf8(result.message()).contains(QStringLiteral("Missing visual baseline")));
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
    const QString baselinePath = tests::support::visualBaselineFilePath(QStringLiteral("mismatch"));
    ASSERT_TRUE(expected.save(baselinePath, "PNG"));

    QImage actual(24, 16, QImage::Format_ARGB32);
    actual.fill(Qt::red);
    const QString actualPath = temp.filePath(QStringLiteral("actual.png"));
    ASSERT_TRUE(actual.save(actualPath, "PNG"));

    EXPECT_FALSE(
        tests::support::compareVisualSnapshotToBaseline(actualPath, QStringLiteral("mismatch")));
}

// These two disabled cases run only in the subprocess contract below. A genuine
// failure must be serialized by GTest, while a later event-loop wait still runs.
TEST(QtTestReporterProbe, DISABLED_FailureReachesGTest)
{
    ASSERT_TRUE(QTest::qWaitFor([] { return false; }, 1));
}

TEST(QtTestReporterProbe, DISABLED_SubsequentWaitStillProcessesEvents)
{
    QObject context;
    bool delivered = false;
    QTimer::singleShot(30, &context, [&] { delivered = true; });
    ASSERT_TRUE(QTest::qWaitFor([&] { return delivered; }, 1000));
    EXPECT_FALSE(QTest::currentTestFailed());
}

TEST(QtTestEnvironmentTest, Contract_EventLoopFailuresReachExitCodeAndXml)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QString xmlPath = temporary.filePath(QStringLiteral("assertions.xml"));
    QProcess child;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("SKIP_VISUAL_TEST"), QStringLiteral("1"));
    child.setProcessEnvironment(environment);
    child.start(QCoreApplication::applicationFilePath(),
                {QStringLiteral("--gtest_also_run_disabled_tests"),
                 QStringLiteral("--gtest_filter=QtTestReporterProbe.*"),
                 QStringLiteral("--gtest_output=xml:") + xmlPath});
    ASSERT_TRUE(child.waitForStarted(10000)) << child.errorString().toStdString();
    ASSERT_TRUE(child.waitForFinished(10000)) << child.errorString().toStdString();
    EXPECT_EQ(child.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(child.exitCode(), 1) << child.readAllStandardOutput().toStdString();
    QFile file(xmlPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QXmlStreamReader xml(file.readAll());
    int cases = 0;
    int failures = 0;
    QString failedCase;
    QString currentCase;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QStringLiteral("testcase")) {
            ++cases;
            currentCase = xml.attributes().value(QStringLiteral("name")).toString();
            EXPECT_EQ(xml.attributes().value(QStringLiteral("status")), QStringLiteral("run"));
        } else if (xml.name() == QStringLiteral("failure")) {
            ++failures;
            failedCase = currentCase;
        }
    }
    EXPECT_FALSE(xml.hasError()) << xml.errorString().toStdString();
    EXPECT_EQ(cases, 2);
    EXPECT_EQ(failures, 1);
    EXPECT_EQ(failedCase, QStringLiteral("DISABLED_FailureReachesGTest"));
}

TEST(QtTestEnvironmentTest, Contract_TestSourcesUseTheGTestFailureReporter)
{
    // Qt input/event-loop helpers are valid; its standalone test-runner macros
    // do not report to this suite's GTest main and can silently pass on failure.
    const QRegularExpression forbidden(
        QStringLiteral("\\bQ(?:TRY_[A-Z_]+|VERIFY2?|COMPARE|FAIL|SKIP)\\s*\\("));
    const QString root = QString::fromUtf8(FLUENT_QT_TEST_SOURCE_DIR);
    ASSERT_TRUE(QDir(root).exists());
    QDirIterator files(root, {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files,
                       QDirIterator::Subdirectories);
    int checked = 0;
    while (files.hasNext()) {
        QFile source(files.next());
        ASSERT_TRUE(source.open(QIODevice::ReadOnly));
        const auto match = forbidden.match(QString::fromUtf8(source.readAll()));
        EXPECT_FALSE(match.hasMatch())
            << source.fileName().toStdString() << ": " << match.captured().toStdString();
        ++checked;
    }
    EXPECT_GT(checked, 100);
}
