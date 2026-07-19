#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QGuiApplication>
#include <QProcess>

#include "support/logging/Log.h"
#include "view/shell/AppIcon.h"
#include "view/shell/GalleryApplicationController.h"
#include "view/shell/GalleryApplicationLifecycle.h"
#include "view/shell/GallerySingleInstance.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowPlacement.h"
#include "viewmodel/GallerySettings.h"

#ifndef FLUENT_QT_GALLERY_VERSION
#define FLUENT_QT_GALLERY_VERSION "0.0.0"
#endif

namespace {

bool launchRestartedApplication(const QString& program,
                                const QStringList& arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    return process.startDetached();
}

int runGalleryApplication(int argc,
                          char** argv,
                          QString* restartProgram,
                          QStringList* restartArguments)
{
    // The scale preference must be available before QApplication reads the
    // platform DPI. Static application identity also makes the config path
    // deterministic during this pre-application phase.
    // zh_CN: 缩放偏好必须在 QApplication 读取平台 DPI 前生效；预先设置静态应用标识，
    // 也能确保此阶段解析到稳定的配置路径。
    QCoreApplication::setApplicationName(QStringLiteral(FLUENT_QT_GALLERY_DISPLAY_NAME));
    QCoreApplication::setOrganizationName(QStringLiteral(FLUENT_QT_GALLERY_ORGANIZATION_NAME));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(FLUENT_QT_GALLERY_VERSION));
    const int startupScalePercent =
        fluent::gallery::GallerySettings::applyStartupUiScalePreference();
    fluent::prepareHighDpiApplication();

    QApplication app(argc, argv);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral(FLUENT_QT_GALLERY_APP_ID));
#endif
    app.setQuitOnLastWindowClosed(false);

    fluent::initializeResources();
    app.setFont(Typography::Styles::Body.toQFont());
    auto& settings = fluent::gallery::GallerySettings::instance();

    fluent::support::logging::InitializationOptions loggingOptions;
    loggingOptions.defaultLevel = fluent::support::logging::Level::Info;
    loggingOptions.installQtMessageHandler = true;
    loggingOptions.logFilePath = fluent::support::logging::defaultLogFilePath();
    fluent::support::logging::initialize(loggingOptions);
    LOG_INFO(QStringLiteral("GalleryApp startup appName=%1 organization=%2 logFile=%3 uiScalePercent=%4")
                 .arg(QApplication::applicationName(), QApplication::organizationName(),
                      loggingOptions.logFilePath)
                 .arg(startupScalePercent));
    app.setWindowIcon(fluent::gallery::appicon::icon());

    fluent::gallery::GallerySingleInstance singleInstance(
        QStringLiteral(FLUENT_QT_GALLERY_APP_ID), &app);
    const auto instanceResult = singleInstance.start();
    if (instanceResult
        == fluent::gallery::GallerySingleInstance::StartResult::ExistingInstanceNotified) {
        return 0;
    }
    if (instanceResult == fluent::gallery::GallerySingleInstance::StartResult::Error) {
        LOG_CRITICAL(QStringLiteral("Gallery single-instance startup failed: %1")
                         .arg(singleInstance.errorString()));
        return 1;
    }

    fluent::gallery::GalleryWindow window;
    fluent::gallery::GalleryWindowPlacement placement(
        &window, &settings, startupScalePercent);
    fluent::gallery::GalleryApplicationController applicationController(&window, &app);
    QObject::connect(&singleInstance,
                     &fluent::gallery::GallerySingleInstance::activationRequested,
                     &applicationController,
                     &fluent::gallery::GalleryApplicationController::restoreWindow);

    if (placement.restore())
        window.showMaximized();
    else
        window.show();

    const int exitCode = app.exec();
    placement.saveNow();
    LOG_INFO(QStringLiteral("GalleryApp event loop exited code=%1").arg(exitCode));

    if (exitCode == fluent::gallery::RestartExitCode) {
        *restartProgram = QCoreApplication::applicationFilePath();
        *restartArguments = QCoreApplication::arguments();
        if (!restartArguments->isEmpty())
            restartArguments->removeFirst();
    }
    return exitCode;
}

} // namespace

int main(int argc, char** argv)
{
    QString restartProgram;
    QStringList restartArguments;
    const int exitCode = runGalleryApplication(
        argc, argv, &restartProgram, &restartArguments);

    // runGalleryApplication has released the single-instance lock before the
    // replacement starts, avoiding a race with the old process's teardown.
    if (exitCode == fluent::gallery::RestartExitCode) {
        const bool launched = launchRestartedApplication(restartProgram,
                                                         restartArguments);
        if (!launched)
            LOG_CRITICAL(QStringLiteral("GalleryApp failed to launch the restarted process"));
        return launched ? 0 : 1;
    }
    return exitCode;
}
