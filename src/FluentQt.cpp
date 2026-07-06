#include <FluentQt/FluentQt.h>

#include <QFontDatabase>
#include <QResource>
#include <QString>

static void initializeFluentQtResourceFile()
{
    Q_INIT_RESOURCE(resources);
}

namespace {

bool loadBundledFonts()
{
    const int iconFontId =
        QFontDatabase::addApplicationFont(QStringLiteral(":/res/Segoe Fluent Icons.ttf"));
    const int uiFontId =
        QFontDatabase::addApplicationFont(QStringLiteral(":/res/SegoeUI-VF.ttf"));
    return iconFontId >= 0 && uiFontId >= 0;
}

} // namespace

namespace fluent {

bool initializeResources()
{
    static const bool initialized = [] {
        initializeFluentQtResourceFile();
        return loadBundledFonts();
    }();
    return initialized;
}

} // namespace fluent
