#include "platform/GalleryPlatform.h"

#include <FluentQt/WebAssembly.h>

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QWidget>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSettings>

#include <emscripten.h>
#include <emscripten/html5_webgl.h>
#include <cstdlib>

#include <utility>

namespace fluent::gallery::platform {
namespace {

QPointer<QObject> hostThemeContext;
HostThemeChangedHandler hostThemeChangedHandler;

struct ClipboardRequest {
    QPointer<QObject> context;
    std::function<void(bool)> completed;
    QMetaObject::Connection destroyedConnection;
};

QHash<quint32, ClipboardRequest> clipboardRequests;
quint32 nextClipboardRequest = 0;

// clang-format off
// EM_JS bodies are JavaScript; treating `===` as C++ tokens corrupts them.
EM_JS(int, fluentQtGalleryEmbeddedHost, (), {
    return window.fluentQtEmbedded === true ? 1 : 0;
});

EM_JS(void, fluentQtGalleryCopyText, (unsigned int request, const char* utf8), {
    const complete = success => Module['_fluentQtGalleryCopyCompleted'](request, success ? 1 : 0);
    try {
        const text = UTF8ToString(utf8);
        if (!navigator.clipboard || !navigator.clipboard.writeText) {
            complete(false);
            return;
        }
        // Invoke in the original input turn, before awaiting browser permission.
        navigator.clipboard.writeText(text).then(() => complete(true), () => complete(false));
    } catch (_) {
        complete(false);
    }
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

QString graphicsRendererOverride()
{
    const auto context = emscripten_webgl_get_current_context();
    if (!context || !emscripten_webgl_enable_extension(context, "WEBGL_debug_renderer_info"))
        return {};
    char* name = emscripten_webgl_get_parameter_utf8(0x9246); // UNMASKED_RENDERER_WEBGL
    const QString result = QString::fromUtf8(name ? name : "");
    std::free(name);
    return result;
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryCopyCompleted(unsigned int request, int success)
{
    const auto found = clipboardRequests.find(request);
    if (found == clipboardRequests.end())
        return;
    const ClipboardRequest pending = found.value();
    clipboardRequests.erase(found);
    QObject::disconnect(pending.destroyedConnection);
    if (pending.context)
        pending.completed(success != 0);
}

void copyText(QObject* context, const QString& text, std::function<void(bool)> completed)
{
    if (!context || !completed)
        return;
    do {
        ++nextClipboardRequest;
    } while (nextClipboardRequest == 0 || clipboardRequests.contains(nextClipboardRequest));
    const quint32 request = nextClipboardRequest;
    ClipboardRequest pending{context, std::move(completed), {}};
    pending.destroyedConnection = QObject::connect(
        context, &QObject::destroyed, [request] { clipboardRequests.remove(request); });
    clipboardRequests.insert(request, std::move(pending));
    // Qt's WASM clipboard cache is not proof of a successful browser write.
    // zh_CN: Qt 的 WASM 剪贴板缓存不能证明浏览器写入已成功。
    fluentQtGalleryCopyText(request, text.toUtf8().constData());
}

void chooseFiles(QWidget* context, const QString& filter,
                 std::function<void(const QString&, qint64)> selected)
{
    if (!context || !selected)
        return;
    const QPointer<QWidget> guard(context);
    QFileDialog::getOpenFileContent(
        filter, [guard, selected](const QString& name, const QByteArray& content) {
            if (guard && !name.isEmpty())
                selected(QFileInfo(name).fileName(), content.size());
        });
}

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
