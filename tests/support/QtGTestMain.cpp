#include "QtTestEnvironment.h"

#include "support/logging/Log.h"

#include <QApplication>
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    tests::support::configureOffscreenPlatformForAutomation();

    fluent::support::logging::InitializationOptions loggingOptions;
    loggingOptions.installQtMessageHandler = true;
    fluent::support::logging::initialize(loggingOptions);

    ::testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    tests::support::initializeQtTestEnvironment();

    return RUN_ALL_TESTS();
}
