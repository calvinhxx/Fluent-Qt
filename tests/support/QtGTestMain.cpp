#include "QtTestEnvironment.h"

#include "utils/Log.h"

#include <QApplication>
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    tests::support::configureOffscreenPlatformForAutomation();

    utils::logging::InitializationOptions loggingOptions;
    loggingOptions.installQtMessageHandler = true;
    utils::logging::initialize(loggingOptions);

    ::testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    tests::support::initializeQtTestEnvironment();

    return RUN_ALL_TESTS();
}
