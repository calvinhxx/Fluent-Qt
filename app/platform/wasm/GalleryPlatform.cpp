#include "platform/GalleryPlatform.h"

#include <FluentQt/WebAssembly.h>

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QWidget>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSettings>

#include <emscripten.h>
#include <emscripten/html5_webgl.h>
#include <cstdlib>

#include <utility>
#include "utils/DiagnosticSession.h"

namespace fluent::gallery::platform {

namespace {

QPointer<QObject> hostThemeContext;
HostThemeChangedHandler hostThemeChangedHandler;

// clang-format off
// EM_JS bodies are JavaScript; treating `===` as C++ tokens corrupts them.
EM_JS(int, fluentQtGalleryDownloadDiagnosticReport, (const char* data, int length), {
    let url;
    let link;
    try {
        const blob = new Blob([HEAPU8.slice(data, data + length)], {type: 'application/zip'});
        url = URL.createObjectURL(blob);
        link = document.createElement('a');
        link.href = url;
        link.download = 'fluentqt-report.zip';
        link.style.display = 'none';
        document.body.appendChild(link);
        link.click();
        return 1;
    } catch (_) {
        return 0;
    } finally {
        if (link) link.remove();
        if (url) setTimeout(() => URL.revokeObjectURL(url), 1000);
    }
});

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

void prepareDiagnosticFeedback(QWidget*,
    const QVariantMap& report,
    const QVariantMap& attachments,
    const QVariantList&,
    const QString&,
    std::function<void(bool, const QString&)> finished)
{
    auto snapshot = report;
    snapshot.insert("crash_files_included", 0);
    const auto bytes = diagnostics::DiagnosticSession::createBundle(snapshot, attachments);
    const bool requested = !bytes.isEmpty()
        && fluentQtGalleryDownloadDiagnosticReport(bytes.constData(), bytes.size()) != 0;
    finished(requested, requested ? QString() : QStringLiteral("Could not download the report. Try again."));
}
QString diagnosticStorageDirectory()
{
    return {};
}
QVariantMap diagnosticProcessIdentity()
{
    return {};
}
QVariantMap existingCrashAttachments(const QVariantList&, const QString&)
{
    return {};
}

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

void chooseFiles(QWidget* context, const QString& filter, std::function<void(const QString&, qint64)> selected)
{
    if (!context || !selected)
        return;
    const QPointer<QWidget> guard(context);
    QFileDialog::getOpenFileContent(filter, [guard, selected](const QString& name, const QByteArray& content) {
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
        result.distributionDescription = QStringLiteral("Runs the same C++ Qt Widgets catalog in the browser sandbox");
        result.runtimeLabel = QStringLiteral("WebAssembly");
        result.distributionActionText = QStringLiteral("View source");
        result.distributionActionUrl = QUrl(QStringLiteral("https://github.com/calvinhxx/Fluent-Qt"));
        return result;
    }();
    return value;
}

bool persistenceAvailable()
{
    return !QCoreApplication::organizationName().isEmpty() && !QCoreApplication::applicationName().isEmpty();
}

QSettings createSettings()
{
    return QSettings(QSettings::WebLocalStorageFormat, QSettings::UserScope, QCoreApplication::organizationName(),
        QCoreApplication::applicationName());
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

// Browsers already zoom the page with these shortcuts, so the Gallery leaves
// them to the browser. zh_CN: 浏览器本身已用这些快捷键缩放页面，Gallery 不接管。
bool supportsInterfaceZoom()
{
    return false;
}

bool supportsLiveInterfaceZoom()
{
    return false;
}
qreal appliedInterfaceZoom()
{
    return 1.0;
}
bool prepareInterfaceZoom(qreal zoom)
{
    Q_UNUSED(zoom);
    return false;
}

bool applyInterfaceZoom(qreal zoom)
{
    Q_UNUSED(zoom);
    return false;
}

} // namespace fluent::gallery::platform
