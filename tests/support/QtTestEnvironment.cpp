#include "QtTestEnvironment.h"

#include <FluentQt/FluentQt.h>

#include "components/foundation/FluentElement.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringLiteral>
#include <QStyle>
#include <QWidget>

#ifndef FLUENT_QT_TEST_BINARY_DIR
#define FLUENT_QT_TEST_BINARY_DIR ""
#endif

#ifndef FLUENT_QT_TEST_TARGET
#define FLUENT_QT_TEST_TARGET "unknown_test"
#endif

#ifndef FLUENT_QT_VISUAL_BASELINE_DIR
#define FLUENT_QT_VISUAL_BASELINE_DIR ""
#endif

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

QString snapshotIdentityFileName(const QString& variant)
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

    return parts.join(QStringLiteral("__")) + QStringLiteral(".png");
}

QString visualDiffFilePath(const QString& actualPath)
{
    const QFileInfo info(actualPath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".diff.png"));
}

QImage normalizeSnapshotImage(const QImage& image)
{
    if (image.isNull())
        return image;
    if (image.format() == QImage::Format_ARGB32)
        return image;
    return image.convertToFormat(QImage::Format_ARGB32);
}

bool envFlagIsOn(const char* name)
{
    return qEnvironmentVariableIsSet(name)
        && qEnvironmentVariable(name) == QStringLiteral("1");
}
} // namespace

void configureOffscreenPlatformForAutomation()
{
    // Redirect QStandardPaths (AppLocalData/config/themes/logs) to an isolated test sandbox so the
    // suite never reads or mutates real user data — e.g. constructing GallerySettings/GalleryUserTheme
    // would otherwise export theme JSON into per-exe AppData. Must run before any path is resolved
    // (logging init + QApplication below both consult QStandardPaths). zh_CN: 把 QStandardPaths 重定向到
    // 隔离的测试沙盒,使测试绝不读写真实用户数据(否则构造 GallerySettings/GalleryUserTheme 会把主题 JSON 导出到
    // 每个 exe 的 AppData)。必须在任何路径解析(下方日志初始化 + QApplication)之前调用。
    QStandardPaths::setTestModeEnabled(true);

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

    fluent::initializeResources();
    qApp->setFont(Typography::Styles::Body.toQFont());
}

bool shouldSkipVisualTest()
{
    return qEnvironmentVariableIsSet("SKIP_VISUAL_TEST");
}

bool isHeadlessPlatform()
{
    const QString platform = QGuiApplication::platformName();
    return platform == QLatin1String("offscreen") || platform == QLatin1String("minimal");
}

bool isVisualSnapshotMode()
{
    return envFlagIsOn("VISUAL_SNAPSHOT");
}

bool isVisualCompareMode()
{
    return envFlagIsOn("VISUAL_COMPARE");
}

bool shouldUpdateVisualBaseline()
{
    return envFlagIsOn("VISUAL_UPDATE_BASELINE");
}

bool shouldCaptureVisualSnapshot()
{
    return isVisualSnapshotMode() && !shouldSkipVisualTest();
}

bool shouldRunVisualGate()
{
    return !shouldSkipVisualTest()
        && (isVisualSnapshotMode() || isVisualCompareMode() || shouldUpdateVisualBaseline());
}

bool isVisualGateApprovalHost()
{
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
    if (isHeadlessPlatform()
        || QGuiApplication::platformName() != QLatin1String("cocoa")) {
        return false;
    }

    const QStyle* style = QApplication::style();
    const bool fusionStyle = style
        && style->objectName().compare(QStringLiteral("fusion"),
                                       Qt::CaseInsensitive) == 0;
    const bool fixedScale = qEnvironmentVariable("QT_SCALE_FACTOR")
        == QLatin1String("1");
    const bool fixedFontDpi = qEnvironmentVariable("QT_FONT_DPI")
        == QLatin1String("96");
    const bool noPerScreenOverride =
        qEnvironmentVariableIsEmpty("QT_SCREEN_SCALE_FACTORS");
    return fusionStyle && fixedScale && fixedFontDpi && noPerScreenOverride;
#else
    return false;
#endif
}

QString visualSnapshotDirectory()
{
    return QDir(configuredBinaryDir()).filePath(QStringLiteral("visual"));
}

QString visualSnapshotFilePath(const QString& variant)
{
    return QDir(visualSnapshotDirectory()).filePath(snapshotIdentityFileName(variant));
}

QString visualBaselineDirectory()
{
    const QString fromEnv = qEnvironmentVariable("FLUENT_QT_VISUAL_BASELINE_DIR");
    if (!fromEnv.trimmed().isEmpty())
        return fromEnv;

    const QString fromCompile = QString::fromUtf8(FLUENT_QT_VISUAL_BASELINE_DIR);
    if (!fromCompile.trimmed().isEmpty())
        return fromCompile;

    return QDir(configuredBinaryDir()).filePath(QStringLiteral("visual-baselines"));
}

QString visualBaselineFilePath(const QString& variant)
{
    return QDir(visualBaselineDirectory()).filePath(snapshotIdentityFileName(variant));
}

::testing::AssertionResult compareVisualImages(const QImage& actual, const QImage& expected)
{
    if (actual.isNull())
        return ::testing::AssertionFailure() << "Actual visual snapshot image is null";
    if (expected.isNull())
        return ::testing::AssertionFailure() << "Expected visual baseline image is null";

    const QImage actualArgb = normalizeSnapshotImage(actual);
    const QImage expectedArgb = normalizeSnapshotImage(expected);
    if (actualArgb.size() != expectedArgb.size()) {
        return ::testing::AssertionFailure()
               << "Visual snapshot size " << actualArgb.width() << "x" << actualArgb.height()
               << " does not match baseline " << expectedArgb.width() << "x"
               << expectedArgb.height();
    }

    int mismatchedPixels = 0;
    int maxChannelDelta = 0;
    for (int y = 0; y < actualArgb.height(); ++y) {
        const auto* actualLine = reinterpret_cast<const QRgb*>(actualArgb.constScanLine(y));
        const auto* expectedLine = reinterpret_cast<const QRgb*>(expectedArgb.constScanLine(y));
        for (int x = 0; x < actualArgb.width(); ++x) {
            const QRgb actualPixel = actualLine[x];
            const QRgb expectedPixel = expectedLine[x];
            if (actualPixel == expectedPixel)
                continue;

            ++mismatchedPixels;
            const int delta = qMax(qMax(qAbs(qRed(actualPixel) - qRed(expectedPixel)),
                                        qAbs(qGreen(actualPixel) - qGreen(expectedPixel))),
                                   qMax(qAbs(qBlue(actualPixel) - qBlue(expectedPixel)),
                                        qAbs(qAlpha(actualPixel) - qAlpha(expectedPixel))));
            maxChannelDelta = qMax(maxChannelDelta, delta);
        }
    }

    if (mismatchedPixels == 0)
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
           << mismatchedPixels << " of "
           << (actualArgb.width() * actualArgb.height())
           << " pixels differ from the visual baseline (max channel delta "
           << maxChannelDelta << ")";
}

::testing::AssertionResult compareVisualSnapshotToBaseline(const QString& actualPath,
                                                           const QString& variant)
{
    const QString baselinePath = visualBaselineFilePath(variant);
    const QFileInfo baselineInfo(baselinePath);
    if (!baselineInfo.exists() || baselineInfo.size() <= 0) {
        return ::testing::AssertionFailure()
               << "Missing visual baseline: " << baselinePath.toStdString()
               << ". Generate it on the approval host with VISUAL_SNAPSHOT=1 "
                  "VISUAL_UPDATE_BASELINE=1.";
    }

    const QImage actual(actualPath);
    const QImage expected(baselinePath);
    const auto comparison = compareVisualImages(actual, expected);
    if (comparison)
        return comparison;

    const QImage actualArgb = normalizeSnapshotImage(actual);
    const QImage expectedArgb = normalizeSnapshotImage(expected);
    if (!actualArgb.isNull() && actualArgb.size() == expectedArgb.size()) {
        QImage diff(actualArgb.size(), QImage::Format_ARGB32);
        for (int y = 0; y < actualArgb.height(); ++y) {
            const auto* actualLine = reinterpret_cast<const QRgb*>(actualArgb.constScanLine(y));
            const auto* expectedLine = reinterpret_cast<const QRgb*>(expectedArgb.constScanLine(y));
            auto* diffLine = reinterpret_cast<QRgb*>(diff.scanLine(y));
            for (int x = 0; x < actualArgb.width(); ++x) {
                if (actualLine[x] == expectedLine[x]) {
                    const QColor dim = QColor::fromRgba(actualLine[x]);
                    diffLine[x] = QColor(dim.red() / 3, dim.green() / 3, dim.blue() / 3, 255).rgba();
                } else {
                    diffLine[x] = qRgb(220, 32, 32);
                }
            }
        }

        const QString diffPath = visualDiffFilePath(actualPath);
        QFile::remove(diffPath);
        if (!diff.save(diffPath, "PNG")) {
            return ::testing::AssertionFailure()
                   << comparison.message() << "; also failed to write diff PNG "
                   << diffPath.toStdString();
        }
        return ::testing::AssertionFailure()
               << comparison.message() << "; actual=" << actualPath.toStdString()
               << " baseline=" << baselinePath.toStdString()
               << " diff=" << diffPath.toStdString();
    }

    return ::testing::AssertionFailure()
           << comparison.message() << "; actual=" << actualPath.toStdString()
           << " baseline=" << baselinePath.toStdString();
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

    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(options.theme == VisualSnapshotTheme::Dark
                                        ? fluent::FluentElement::Dark
                                        : fluent::FluentElement::Light);

    window->setFixedSize(snapshotSize);
    window->ensurePolished();
    window->show();

    for (int i = 0; i < 3; ++i) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    if (!options.focusObjectName.isEmpty()) {
        QWidget* focusWidget = window->findChild<QWidget*>(
            options.focusObjectName);
        if (!focusWidget) {
            fluent::FluentElement::setTheme(previousTheme);
            return ::testing::AssertionFailure()
                   << "Visual snapshot focus target was not found: "
                   << options.focusObjectName.toStdString();
        }
        // CTest may launch the Cocoa process without making its first window
        // active. setActiveWindow() gives the capture a deterministic Qt focus
        // owner; activateWindow() alone remains asynchronous on that path.
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        QApplication::setActiveWindow(window);
        QT_WARNING_POP
        window->activateWindow();
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        focusWidget->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!focusWidget->hasFocus()) {
            fluent::FluentElement::setTheme(previousTheme);
            return ::testing::AssertionFailure()
                   << "Visual snapshot focus target did not receive focus: "
                   << options.focusObjectName.toStdString();
        }
    }

    // Visual snapshots are deterministic artifacts, so the cursor's desktop
    // position must not leave one arbitrary control in its hover state.
    const auto clearHoverState = [](QWidget* widget) {
        if (!widget)
            return;
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(widget, &leaveEvent);
        widget->setAttribute(Qt::WA_UnderMouse, false);
    };
    clearHoverState(window);
    const auto childWidgets = window->findChildren<QWidget*>();
    for (QWidget* child : childWidgets)
        clearHoverState(child);

    // QWidget::grab() keeps the platform graphics context required by native
    // item-view/style painting on macOS. QWidget::render(QImage) can fall back
    // to incomplete native-style output (missing selection layers and frames).
    // Normalize the physical-DPR pixmap back to the requested logical size.
    // zh_CN: QWidget::grab() 会保留 macOS 原生 item-view/style 绘制所需的
    // graphics context；直接 render(QImage) 可能丢失选中层和边框。最终再把
    // 物理 DPR 图像归一到约定的逻辑尺寸。
    const QPixmap snapshot = window->grab();
    fluent::FluentElement::setTheme(previousTheme);

    if (snapshot.isNull())
        return ::testing::AssertionFailure() << "Visual snapshot grab returned a null pixmap";

    QImage snapshotImage = snapshot.toImage();
    if (snapshotImage.size() != snapshotSize) {
        snapshotImage = snapshotImage.scaled(
            snapshotSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

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

    if (shouldUpdateVisualBaseline()) {
        const QString baselineDir = visualBaselineDirectory();
        if (!QDir().mkpath(baselineDir)) {
            return ::testing::AssertionFailure()
                   << "Failed to create visual baseline directory: "
                   << baselineDir.toStdString();
        }
        const QString baselinePath = visualBaselineFilePath(options.variant);
        QFile::remove(baselinePath);
        if (!QFile::copy(outputPath, baselinePath)) {
            return ::testing::AssertionFailure()
                   << "Failed to copy visual snapshot to baseline: "
                   << baselinePath.toStdString();
        }
        return ::testing::AssertionSuccess()
               << "Updated visual baseline: " << baselinePath.toStdString();
    }

    if (isVisualCompareMode())
        return compareVisualSnapshotToBaseline(outputPath, options.variant);

    return ::testing::AssertionSuccess()
           << "Saved visual snapshot: " << outputPath.toStdString();
}

} // namespace tests::support
