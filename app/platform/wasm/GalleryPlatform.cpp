#include "platform/GalleryPlatform.h"

#include <FluentQt/WebAssembly.h>

#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSettings>

#include <emscripten.h>

#include <utility>

namespace fluent::gallery::platform {
namespace {

QPointer<QObject> hostThemeContext;
HostThemeChangedHandler hostThemeChangedHandler;

EM_JS(int, fluentQtGalleryEmbeddedHost, (), {
    return window.fluentQtEmbedded === true ? 1 : 0;
});

EM_JS(int, fluentQtGalleryHostTheme, (), {
    if (window.fluentQtHostTheme === 'dark')
        return 2;
    if (window.fluentQtHostTheme === 'light')
        return 1;
    return 0;
});

HostTheme normalizedHostTheme(int value)
{
    if (value == 2)
        return HostTheme::Dark;
    if (value == 1)
        return HostTheme::Light;
    return HostTheme::System;
}

} // namespace

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
        result.hostControlsTheme = fluentQtGalleryEmbeddedHost() != 0;
        result.showsIntroTour = false;
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

HostTheme hostTheme()
{
    if (!capabilities().hostControlsTheme)
        return HostTheme::System;
    return normalizedHostTheme(fluentQtGalleryHostTheme());
}

void setHostThemeChangedHandler(QObject* context,
                                HostThemeChangedHandler handler)
{
    hostThemeContext = context;
    hostThemeChangedHandler = std::move(handler);
}

extern "C" EMSCRIPTEN_KEEPALIVE
void fluentQtGalleryApplyHostTheme(int value)
{
    const HostTheme next = normalizedHostTheme(value);
    if (next == HostTheme::System || !hostThemeContext
        || !hostThemeChangedHandler) {
        return;
    }
    hostThemeChangedHandler(next);
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
