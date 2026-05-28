#include "QtTestEnvironment.h"

#include "view/foundation/FluentElement.h"

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QPixmap>
#include <QRegularExpression>
#include <QStringLiteral>
#include <QWidget>

#ifndef FLUENT_QT_TEST_BINARY_DIR
#define FLUENT_QT_TEST_BINARY_DIR ""
#endif

#ifndef FLUENT_QT_TEST_TARGET
#define FLUENT_QT_TEST_TARGET "unknown_test"
#endif

static void initializeFluentQtResources()
{
    Q_INIT_RESOURCE(resources);
}

namespace tests::support {

namespace {
constexpr QSize kDefaultSnapshotSize(960, 640);

QString sanitizeSnapshotPart(QString value)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    value.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return value.isEmpty() ? QStringLiteral("snapshot") : value;
}

QString configuredBinaryDir()
{
    const QString binaryDir = QString::fromUtf8(FLUENT_QT_TEST_BINARY_DIR);
    return binaryDir.isEmpty() ? QDir::currentPath() : binaryDir;
}
} // namespace

void configureOffscreenPlatformForAutomation()
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    if (isVisualSnapshotMode()
        && qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR")
        && qEnvironmentVariableIsEmpty("QT_SCREEN_SCALE_FACTORS")) {
        qputenv("QT_SCALE_FACTOR", QByteArray("1"));
    }
}

void initializeQtTestEnvironment()
{
    QApplication::setStyle(QStringLiteral("Fusion"));

    initializeFluentQtResources();
    QFontDatabase::addApplicationFont(QStringLiteral(":/res/Segoe Fluent Icons.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/res/SegoeUI-VF.ttf"));
}

bool shouldSkipVisualTest()
{
    return qEnvironmentVariableIsSet("SKIP_VISUAL_TEST");
}

bool isVisualSnapshotMode()
{
    return qEnvironmentVariableIsSet("VISUAL_SNAPSHOT")
        && qEnvironmentVariable("VISUAL_SNAPSHOT") == QStringLiteral("1");
}

bool shouldCaptureVisualSnapshot()
{
    return isVisualSnapshotMode() && !shouldSkipVisualTest();
}

QString visualSnapshotDirectory()
{
    return QDir(configuredBinaryDir()).filePath(QStringLiteral("visual"));
}

QString visualSnapshotFilePath(const QString& variant)
{
    QStringList parts;
    parts << sanitizeSnapshotPart(QString::fromUtf8(FLUENT_QT_TEST_TARGET));

    if (const auto* info = ::testing::UnitTest::GetInstance()->current_test_info()) {
        parts << sanitizeSnapshotPart(QString::fromUtf8(info->test_suite_name()));
        parts << sanitizeSnapshotPart(QString::fromUtf8(info->name()));
    } else {
        parts << QStringLiteral("unknown_suite") << QStringLiteral("unknown_test");
    }

    if (!variant.trimmed().isEmpty())
        parts << sanitizeSnapshotPart(variant);

    return QDir(visualSnapshotDirectory()).filePath(parts.join(QStringLiteral("__")) + QStringLiteral(".png"));
}

::testing::AssertionResult captureVisualSnapshot(QWidget* window, const VisualSnapshotOptions& options)
{
    if (!window)
        return ::testing::AssertionFailure() << "Cannot capture a null VisualCheck window";

    const QSize snapshotSize = options.windowSize.isValid() ? options.windowSize
                               : window->size().isValid()  ? window->size()
                                                           : kDefaultSnapshotSize;
    if (snapshotSize.isEmpty())
        return ::testing::AssertionFailure() << "Visual snapshot window size is empty";

    const auto previousTheme = FluentElement::currentTheme();
    FluentElement::setTheme(options.theme == VisualSnapshotTheme::Dark
                                ? FluentElement::Dark
                                : FluentElement::Light);

    window->setFixedSize(snapshotSize);
    window->ensurePolished();
    window->show();

    for (int i = 0; i < 3; ++i) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    const QPixmap snapshot = window->grab();
    FluentElement::setTheme(previousTheme);

    if (snapshot.isNull())
        return ::testing::AssertionFailure() << "Visual snapshot grab returned a null pixmap";

    QImage snapshotImage = snapshot.toImage();
    if (snapshotImage.size() != snapshotSize)
        snapshotImage = snapshotImage.scaled(snapshotSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const QString outputDir = visualSnapshotDirectory();
    if (!QDir().mkpath(outputDir)) {
        return ::testing::AssertionFailure()
               << "Failed to create visual snapshot directory: " << outputDir.toStdString();
    }

    const QString outputPath = visualSnapshotFilePath(options.variant);
    QFile::remove(outputPath);
    if (!snapshotImage.save(outputPath, "PNG")) {
        return ::testing::AssertionFailure()
               << "Failed to save visual snapshot PNG: " << outputPath.toStdString();
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists() || outputInfo.size() <= 0) {
        return ::testing::AssertionFailure()
               << "Visual snapshot PNG is empty: " << outputPath.toStdString();
    }

    return ::testing::AssertionSuccess()
           << "Saved visual snapshot: " << outputPath.toStdString();
}

} // namespace tests::support
