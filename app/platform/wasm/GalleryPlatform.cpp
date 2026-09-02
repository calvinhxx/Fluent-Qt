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

// clang-format off
// EM_JS bodies are JavaScript; treating `===` as C++ tokens corrupts them.
EM_JS(int, fluentQtGalleryEmbeddedHost, (), {
    return window.fluentQtEmbedded === true ? 1 : 0;
});

EM_JS(int, fluentQtGalleryHostTheme, (), {
    if (window.fluentQtHostTheme === 'high-contrast')
        return 3;
    if (window.fluentQtHostTheme === 'dark')
        return 2;
    if (window.fluentQtHostTheme === 'light')
        return 1;
    return 0;
});

HostTheme normalizedHostTheme(int value)
{
    if (value == 3)
        return HostTheme::HighContrast;
    if (value == 2)
        return HostTheme::Dark;
    if (value == 1)
        return HostTheme::Light;
    return HostTheme::System;
}

EM_JS(void, fluentQtGalleryPublishHostTheme, (int value), {
    document.documentElement.dataset.fluentQtGalleryHostTheme = value === 3
        ? 'high-contrast'
        : (value === 2 ? 'dark' : (value === 1 ? 'light' : 'system'));
});
// clang-format on

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
        result.showsBilingualDocumentation = true;
        result.showsIntroTour = false;
        result.maxResidentRoutes = 16;
        result.applicationName = QStringLiteral("Fluent-Qt C++ Web Gallery");
        result.windowTitle = result.applicationName;
        result.distributionSectionTitle = QStringLiteral("Web version");
        result.distributionTitle = QStringLiteral("C++ Web Gallery");
        result.distributionDescription =
            QStringLiteral("Runs the same C++ Qt Widgets catalog in the browser sandbox");
        result.runtimeLabel = QStringLiteral("WebAssembly");
        result.distributionActionText = QStringLiteral("View source");
        result.distributionActionUrl =
            QUrl(QStringLiteral("https://github.com/calvinhxx/Fluent-Qt"));
        return result;
    }();
    return value;
}

bool persistenceAvailable()
{
    return !QCoreApplication::organizationName().isEmpty() &&
           !QCoreApplication::applicationName().isEmpty();
}

QSettings createSettings()
{
    return QSettings(QSettings::WebLocalStorageFormat, QSettings::UserScope,
                     QCoreApplication::organizationName(), QCoreApplication::applicationName());
}

HostTheme hostTheme()
{
    if (!capabilities().hostControlsTheme)
        return HostTheme::System;
    const int value = fluentQtGalleryHostTheme();
    fluentQtGalleryPublishHostTheme(value);
    return normalizedHostTheme(value);
}

void setHostThemeChangedHandler(QObject* context, HostThemeChangedHandler handler)
{
    hostThemeContext = context;
    hostThemeChangedHandler = std::move(handler);
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryApplyHostTheme(int value)
{
    const HostTheme next = normalizedHostTheme(value);
    if (next == HostTheme::System || !hostThemeContext || !hostThemeChangedHandler) {
        return;
    }
    hostThemeChangedHandler(next);
    fluentQtGalleryPublishHostTheme(value);
}

void showTopLevelWindow(QWidget* window, const QRect& normalGeometry, bool maximized)
{
    fluent::webassembly::showWindow(window, normalGeometry,
                                    maximized ? fluent::webassembly::WindowPresentation::Maximized
                                              : fluent::webassembly::WindowPresentation::Windowed);
}

} // namespace fluent::gallery::platform
