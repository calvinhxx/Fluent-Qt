#include "QtTestEnvironment.h"

#include <FluentQt/FluentQt.h>

#include "support/logging/Log.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    tests::support::configureOffscreenPlatformForAutomation();
    fluent::prepareHighDpiApplication();

    ::testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);

    // Each CTest/GTest process owns its data, including concurrent runs of the same binary.
    // Keep the executable name for snapshot identity, independently of this temporary app name.
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(QStringLiteral("FluentQtTests"));
    const QDir dataParent =
        QFileInfo(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).dir();
    if (!QDir().mkpath(dataParent.absolutePath()))
        qFatal("Cannot create the Qt test data directory");
    QTemporaryDir dataScope(dataParent.filePath(QStringLiteral("FluentQtTests-XXXXXX")));
    if (!dataScope.isValid())
        qFatal("Cannot isolate the Qt test data directory");
    QCoreApplication::setApplicationName(QFileInfo(dataScope.path()).fileName());

    fluent::support::logging::InitializationOptions loggingOptions;
    loggingOptions.installQtMessageHandler = true;
    fluent::support::logging::initialize(loggingOptions);

    tests::support::initializeQtTestEnvironment();

    const int result = RUN_ALL_TESTS();
    fluent::support::logging::shutdown();
    return result;
}
