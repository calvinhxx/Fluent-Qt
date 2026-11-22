#include "platform/wasm/GalleryStudioPreview.h"

#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "platform/GalleryPlatform.h"
#include "view/widgets/GalleryStudioCanvas.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>
#include <emscripten.h>
#if defined(FLUENT_QT_HAS_QML_BRIDGE)
#include <QQuickWidget>
#endif

namespace fluent::gallery::platform {
namespace {

QPointer<QWidget> studioHostWindow;
QPointer<GalleryStudioCanvas> studioCanvas;

FluentElement::Theme toFluentTheme(HostTheme host)
{
    if (host == HostTheme::HighContrast)
        return FluentElement::HighContrast;
    if (host == HostTheme::Dark)
        return FluentElement::Dark;
    return FluentElement::Light;
}

void applyStudioTheme(HostTheme host)
{
    const auto theme = toFluentTheme(host);
    if (studioCanvas)
        studioCanvas->setTheme(theme);
    if (studioHostWindow) {
        auto palette = studioHostWindow->palette();
        palette.setColor(QPalette::Window, ThemeRegistry::instance().colors(theme).bgCanvas);
        studioHostWindow->setPalette(palette);
    }
}

void emitJsonCallback(const char* jsFn, const QJsonObject& payload)
{
    const QByteArray encoded = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    // clang-format off
    EM_ASM({
        const fn = window[UTF8ToString($0)];
        if (typeof fn === 'function') fn(UTF8ToString($1));
    }, jsFn, encoded.constData());
    // clang-format on
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryStudioApply()
{
    if (!studioCanvas)
        return 0;
    const char* raw = emscripten_run_script_string("JSON.stringify(window.fluentQtStudioRequest || null)");
    const QByteArray bytes = raw ? QByteArray(raw) : QByteArray();
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        emitJsonCallback("fluentQtStudioApplied",
            QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-json")},
                {QStringLiteral("requestId"), 0}});
        return 0;
    }

    const QJsonObject envelope = parsed.object();
    const int requestId = envelope.value(QStringLiteral("requestId")).toInt(0);
    const bool preview = envelope.value(QStringLiteral("preview")).toBool(false);
    const QJsonObject doc = envelope.contains(QStringLiteral("document"))
        ? envelope.value(QStringLiteral("document")).toObject()
        : envelope;

    QString error;
    const bool ok = studioCanvas->applyDocument(doc, &error, !preview);
    emitJsonCallback("fluentQtStudioApplied",
        QJsonObject{{QStringLiteral("ok"), ok}, {QStringLiteral("error"), error},
            {QStringLiteral("requestId"), requestId}, {QStringLiteral("nodeCount"), studioCanvas->nodeCount()}});
    return ok ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtGalleryStudioPatch()
{
    if (!studioCanvas)
        return 0;
    const char* raw = emscripten_run_script_string("JSON.stringify(window.fluentQtStudioPatchRequest || null)");
    const QByteArray bytes = raw ? QByteArray(raw) : QByteArray();
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        emitJsonCallback("fluentQtStudioApplied",
            QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("invalid-json")},
                {QStringLiteral("requestId"), 0}, {QStringLiteral("patched"), false}});
        return 0;
    }

    const QJsonObject envelope = parsed.object();
    const int requestId = envelope.value(QStringLiteral("requestId")).toInt(0);
    const bool preview = envelope.value(QStringLiteral("preview")).toBool(false);
    const QString nodeId = envelope.value(QStringLiteral("nodeId")).toString().trimmed();
    const QJsonObject patch = envelope.value(QStringLiteral("patch")).toObject();

    if (!nodeId.isEmpty() && !patch.isEmpty() && studioCanvas->patchNodeProperties(nodeId, patch, !preview)) {
        emitJsonCallback("fluentQtStudioApplied",
            QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("error"), QString()},
                {QStringLiteral("requestId"), requestId}, {QStringLiteral("nodeCount"), studioCanvas->nodeCount()},
                {QStringLiteral("patched"), true}});
        return 1;
    }

    const QJsonValue docVal = envelope.value(QStringLiteral("document"));
    if (!docVal.isObject()) {
        emitJsonCallback("fluentQtStudioApplied",
            QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("patch-rejected")},
                {QStringLiteral("requestId"), requestId}, {QStringLiteral("patched"), false}});
        return 0;
    }

    QString error;
    const bool ok = studioCanvas->applyDocument(docVal.toObject(), &error, !preview);
    emitJsonCallback("fluentQtStudioApplied",
        QJsonObject{{QStringLiteral("ok"), ok}, {QStringLiteral("error"), error},
            {QStringLiteral("requestId"), requestId}, {QStringLiteral("nodeCount"), studioCanvas->nodeCount()},
            {QStringLiteral("patched"), false}});
    return ok ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryStudioSelect()
{
    if (!studioCanvas)
        return;
    const char* raw = emscripten_run_script_string("String(window.fluentQtStudioSelectedId || '')");
    studioCanvas->setSelectedNodeId(QString::fromUtf8(raw ? raw : ""));
}

extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtGalleryStudioRefresh()
{
    if (!studioCanvas)
        return;
    if (studioHostWindow && studioHostWindow->parentWidget()) {
        const QRect parentRect = studioHostWindow->parentWidget()->rect();
        if (!parentRect.isEmpty() && studioHostWindow->geometry() != parentRect)
            studioHostWindow->setGeometry(parentRect);
    }
    studioCanvas->refreshResponsiveLayout();
}

QWidget* createStudioPreview()
{
    auto* window = new QWidget(nullptr, Qt::FramelessWindowHint);
    window->setObjectName(QStringLiteral("galleryStudioPreview"));
    window->setAutoFillBackground(true);
    studioHostWindow = window;

    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    studioCanvas = new GalleryStudioCanvas(window);
    layout->addWidget(studioCanvas);

    QObject::connect(studioCanvas, &GalleryStudioCanvas::layoutChanged, window, [](const QJsonObject& snapshot) {
        emitJsonCallback("fluentQtStudioLayoutChanged", snapshot);
    });
    QObject::connect(studioCanvas, &GalleryStudioCanvas::interactionEvent, window,
        [](const QString& nodeId, const QString& eventName, const QJsonValue& value) {
            emitJsonCallback("fluentQtStudioEvent",
                QJsonObject{{QStringLiteral("nodeId"), nodeId}, {QStringLiteral("event"), eventName},
                    {QStringLiteral("value"), value}});
        });
    QObject::connect(studioCanvas, &GalleryStudioCanvas::nodeInspected, window, [](const QJsonObject& inspection) {
        emitJsonCallback("fluentQtStudioInspected", inspection);
    });

    applyStudioTheme(hostTheme());
    setHostThemeChangedHandler(window, [](HostTheme next) { applyStudioTheme(next); });

#if defined(FLUENT_QT_HAS_QML_BRIDGE)
    // Let Qt WASM choose the Quick/OpenGL backing store before showTopLevelWindow()
    // locks the browser window surface; matches GalleryComponentPreview.cpp.
    auto* quickPrimer = new QQuickWidget(window);
    quickPrimer->setObjectName(QStringLiteral("studioQuickSurfacePrimer"));
    quickPrimer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    quickPrimer->setFocusPolicy(Qt::NoFocus);
    quickPrimer->setGeometry(0, 0, 1, 1);
    quickPrimer->hide();
#endif

    // clang-format off
    EM_ASM({ window.fluentQtStudioReady?.(); });
    // clang-format on
    return window;
}

} // namespace fluent::gallery::platform
