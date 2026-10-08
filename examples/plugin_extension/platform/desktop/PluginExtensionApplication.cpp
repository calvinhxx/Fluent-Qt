#include "PluginExtensionApplication.h"
#include "PluginExtensionWindow.h"
#include "PluginContractProbe.h"

#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QTimer>
#include <cstdlib>

namespace fluentqt::plugin_extension {

int runApplication(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    QApplication application(argc, argv);
    fluent::initializeResources();
    application.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
    PluginExtensionWindow window;
    if (!window.isReady())
        return EXIT_FAILURE;
    window.show();
    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(100, &application, [&] {
            const auto contract = runPluginContractChecks();
            const bool passed =
                contract.value(QStringLiteral("allPassed")).toBool() && window.runSmokeTest();
            qInfo().noquote() << QJsonDocument::fromVariant(contract).toJson(
                QJsonDocument::Compact);
            qInfo().noquote()
                << QJsonDocument::fromVariant(window.pluginState()).toJson(QJsonDocument::Compact);
            application.exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }
    return application.exec();
}

} // namespace fluentqt::plugin_extension
