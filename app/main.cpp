#include <QApplication>
#include <QFontDatabase>

#include "view/AppIcon.h"
#include "view/GalleryWindow.h"
#include "utils/Log.h"

static void initializeFluentQtResources()
{
    Q_INIT_RESOURCE(resources);
    QFontDatabase::addApplicationFont(QStringLiteral(":/res/Segoe Fluent Icons.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/res/SegoeUI-VF.ttf"));
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WinUI 3 Gallery"));
    QApplication::setOrganizationName(QStringLiteral("Fluent-QT"));

    initializeFluentQtResources();
    utils::logging::initialize();
    LOG_INFO(QStringLiteral("GalleryApp startup appName=%1 organization=%2")
                 .arg(QApplication::applicationName(), QApplication::organizationName()));
    app.setWindowIcon(fluent::gallery::appicon::icon());

    fluent::gallery::GalleryWindow window;
    window.resize(1180, 760);
    window.show();

    return app.exec();
}
