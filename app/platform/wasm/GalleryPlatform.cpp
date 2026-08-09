#include "platform/GalleryPlatform.h"

#include <FluentQt/WebAssembly.h>

#include <QCoreApplication>
#include <QRect>
#include <QSettings>

namespace fluent::gallery::platform {

const Capabilities& capabilities()
{
    static const Capabilities value = [] {
        Capabilities result;
        result.persistsWindowPlacement = false;
        result.exposesCloseBehavior = false;
        result.checksForUpdates = false;
        result.editsThemeFiles = false;
        result.prewarmsRoutes = false;
        result.usesClientSideTitleBar = true;
        result.maxResidentRoutes = 16;
        result.applicationName = QStringLiteral("Fluent-Qt C++ Web Gallery");
        result.windowTitle = result.applicationName;
        result.distributionSectionTitle = QStringLiteral("Web version");
        result.distributionTitle = QStringLiteral("C++ Web Gallery");
        result.distributionDescription = QStringLiteral(
            "Runs the same C++ Qt Widgets catalog in the browser sandbox");
        result.runtimeLabel = QStringLiteral("WebAssembly");
        result.distributionActionText = QStringLiteral("View source");
        result.distributionActionUrl = QUrl(
            QStringLiteral("https://github.com/calvinhxx/Fluent-Qt"));
        return result;
    }();
    return value;
}

bool persistenceAvailable()
{
    return !QCoreApplication::organizationName().isEmpty()
        && !QCoreApplication::applicationName().isEmpty();
}

QSettings createSettings()
{
    return QSettings(QSettings::WebLocalStorageFormat,
                     QSettings::UserScope,
                     QCoreApplication::organizationName(),
                     QCoreApplication::applicationName());
}

void showTopLevelWindow(QWidget* window,
                        const QRect& normalGeometry,
                        bool maximized)
{
    fluent::webassembly::showWindow(
        window,
        normalGeometry,
        maximized
            ? fluent::webassembly::WindowPresentation::Maximized
            : fluent::webassembly::WindowPresentation::Windowed);
}

} // namespace fluent::gallery::platform
