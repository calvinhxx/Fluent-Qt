#include <QCoreApplication>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include "view/shell/GallerySingleInstance.h"

using fluent::gallery::GallerySingleInstance;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const bool mirrorTestScope = qEnvironmentVariableIntValue(
                                     "FLUENT_QT_SINGLE_INSTANCE_TEST_MODE") == 1;
    if (mirrorTestScope) {
        QStandardPaths::setTestModeEnabled(true);
        app.setApplicationName(qEnvironmentVariable(
            "FLUENT_QT_SINGLE_INSTANCE_TEST_APP_NAME"));
        app.setOrganizationName(qEnvironmentVariable(
            "FLUENT_QT_SINGLE_INSTANCE_TEST_ORGANIZATION"));
    } else {
        app.setApplicationName(QStringLiteral("Fluent-Qt Gallery Single Instance Probe"));
        app.setOrganizationName(QStringLiteral("Fluent-Qt Tests"));
    }

    const QStringList arguments = app.arguments();
    if (arguments.size() != 2)
        return 2;

    QTextStream output(stdout);
    GallerySingleInstance instance(arguments.at(1), &app);
    const auto result = instance.start();
    switch (result) {
    case GallerySingleInstance::StartResult::Primary:
        output << "PRIMARY" << Qt::endl;
        break;
    case GallerySingleInstance::StartResult::ExistingInstanceNotified:
        output << "SECONDARY" << Qt::endl;
        return 0;
    case GallerySingleInstance::StartResult::Error:
        output << "ERROR " << instance.errorString() << Qt::endl;
        return 1;
    }

    QObject::connect(&instance, &GallerySingleInstance::activationRequested,
                     &app, [&app]() {
                         QTextStream(stdout) << "ACTIVATED" << Qt::endl;
                         app.quit();
                     });
    QTimer::singleShot(5000, &app, &QCoreApplication::quit);
    return app.exec();
}
