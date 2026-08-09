#include "platform/GalleryPlatform.h"

#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>

#include "support/logging/Log.h"
#include "view/shell/AppIcon.h"
#include "view/shell/GalleryApplicationController.h"
#include "view/shell/GallerySingleInstance.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowPlacement.h"
#include "viewmodel/GallerySettings.h"

#ifndef FLUENT_QT_GALLERY_VERSION
#define FLUENT_QT_GALLERY_VERSION "0.0.0"
#endif

namespace fluent::gallery::platform {

int runApplication(int argc, char** argv)
{
    QCoreApplication::setApplicationName(capabilities().applicationName);
    QCoreApplication::setOrganizationName(
        QStringLiteral(FLUENT_QT_GALLERY_ORGANIZATION_NAME));
    QCoreApplication::setApplicationVersion(
        QString::fromLatin1(FLUENT_QT_GALLERY_VERSION));
    fluent::prepareHighDpiApplication();

    QApplication app(argc, argv);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral(FLUENT_QT_GALLERY_APP_ID));
#endif
    app.setQuitOnLastWindowClosed(false);

    fluent::initializeResources();
    app.setFont(Typography::Styles::Body.toQFont());
    auto& settings = GallerySettings::instance();

    fluent::support::logging::InitializationOptions loggingOptions;
    loggingOptions.defaultLevel = fluent::support::logging::Level::Info;
    loggingOptions.installQtMessageHandler = true;
    loggingOptions.logFilePath = fluent::support::logging::defaultLogFilePath();
    fluent::support::logging::initialize(loggingOptions);
    LOG_INFO(QStringLiteral("GalleryApp startup appName=%1 organization=%2 logFile=%3")
                 .arg(QApplication::applicationName(),
                      QApplication::organizationName(),
                      loggingOptions.logFilePath));
    app.setWindowIcon(appicon::icon());

    GallerySingleInstance singleInstance(
        QStringLiteral(FLUENT_QT_GALLERY_APP_ID), &app);
    const auto instanceResult = singleInstance.start();
    if (instanceResult == GallerySingleInstance::StartResult::ExistingInstanceNotified)
        return 0;
    if (instanceResult == GallerySingleInstance::StartResult::Error) {
        LOG_CRITICAL(QStringLiteral("Gallery single-instance startup failed: %1")
                         .arg(singleInstance.errorString()));
        return 1;
    }

    GalleryWindow window;
    GalleryWindowPlacement placement(&window, &settings);
    GalleryApplicationController applicationController(&window, &app);
    QObject::connect(&singleInstance,
                     &GallerySingleInstance::activationRequested,
                     &applicationController,
                     &GalleryApplicationController::restoreWindow);

    if (placement.restore())
        window.showMaximized();
    else
        window.show();

    const int exitCode = app.exec();
    placement.saveNow();
    LOG_INFO(QStringLiteral("GalleryApp event loop exited code=%1").arg(exitCode));
    return exitCode;
}

} // namespace fluent::gallery::platform
