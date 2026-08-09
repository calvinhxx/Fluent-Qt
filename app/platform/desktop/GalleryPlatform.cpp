#include "platform/GalleryPlatform.h"

#include <QCoreApplication>
#include <QRect>
#include <QSettings>
#include <QStandardPaths>
#include <QWidget>

#ifndef FLUENT_QT_GALLERY_DISPLAY_NAME
#define FLUENT_QT_GALLERY_DISPLAY_NAME "Fluent-Qt Gallery"
#endif

namespace fluent::gallery::platform {

const Capabilities& capabilities()
{
    static const Capabilities value = [] {
        Capabilities result;
        result.applicationName = QStringLiteral(FLUENT_QT_GALLERY_DISPLAY_NAME);
        result.windowTitle = QStringLiteral("Fluent-Qt Gallery");
        result.distributionSectionTitle = QStringLiteral("Updates");
        result.distributionTitle = QStringLiteral("Gallery updates");
        result.distributionDescription = QStringLiteral(
            "Check GitHub Releases and open the latest package for this platform");
        return result;
    }();
    return value;
}

bool persistenceAvailable()
{
    return QCoreApplication::organizationName() == QStringLiteral("Fluent-Qt")
        && QCoreApplication::applicationName() == capabilities().applicationName;
}

QSettings createSettings()
{
    const QString path = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + QStringLiteral("/config.ini");
    return QSettings(path, QSettings::IniFormat);
}

void showTopLevelWindow(QWidget* window,
                        const QRect& normalGeometry,
                        bool maximized)
{
    if (!window)
        return;
    window->setGeometry(normalGeometry);
    if (maximized)
        window->showMaximized();
    else
        window->show();
}

} // namespace fluent::gallery::platform
