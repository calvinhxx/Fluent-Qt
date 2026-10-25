#include "platform/GalleryPlatform.h"
#include <QLocale>

int main(int argc, char** argv)
{
    // Gallery copy and sample dates use English on every host; UILib keeps caller locales.
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    return fluent::gallery::platform::runApplication(argc, argv);
}
