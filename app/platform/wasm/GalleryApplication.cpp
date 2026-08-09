#include "platform/GalleryPlatform.h"

#include <FluentQt/FluentQt.h>
#include <FluentQt/WebAssembly.h>

#include <QApplication>
#include <QCoreApplication>
#include <QScreen>

#include <emscripten/emscripten.h>

#include <memory>

#include "platform/wasm/WasmSmokeRunner.h"
#include "support/logging/Log.h"
#include "view/shell/AppIcon.h"
#include "view/shell/GalleryWindow.h"
#include "viewmodel/GallerySettings.h"

#ifndef FLUENT_QT_GALLERY_VERSION
#define FLUENT_QT_GALLERY_VERSION "0.0.0"
#endif

namespace fluent::gallery::platform {
namespace {

std::unique_ptr<QApplication> application;
std::unique_ptr<GalleryWindow> galleryWindow;

QString requestedWindowMode()
{
    const char* rawMode = emscripten_run_script_string(
        "window.fluentQtWindowProfile?.mode || 'windowed'");
    return QString::fromUtf8(rawMode ? rawMode : "windowed");
}

void showGalleryWindow(GalleryWindow* window)
{
    if (!window)
        return;

    QScreen* screen = application ? application->primaryScreen() : nullptr;
    const QRect available = screen ? screen->availableGeometry()
                                   : QRect(0, 0, 1280, 720);
    constexpr int stageMargin = 24;
    constexpr int preferredMinimumWidth = 900;
    constexpr int preferredMinimumHeight = 600;
    constexpr int preferredMaximumWidth = 1440;
    constexpr int preferredMaximumHeight = 900;

    const int availableWidth = qMax(1, available.width() - stageMargin * 2);
    const int availableHeight = qMax(1, available.height() - stageMargin * 2);
    const int preferredWidth = qBound(
        preferredMinimumWidth,
        qRound(available.width() * 0.72),
        preferredMaximumWidth);
    const int preferredHeight = qBound(
        preferredMinimumHeight,
        qRound(available.height() * 0.78),
        preferredMaximumHeight);
    const QSize size(qMin(availableWidth, preferredWidth),
                     qMin(availableHeight, preferredHeight));
    const QPoint topLeft(
        available.x() + (available.width() - size.width()) / 2,
        available.y() + (available.height() - size.height()) / 2);
    showTopLevelWindow(
        window,
        QRect(topLeft, size),
        requestedWindowMode() == QStringLiteral("maximized"));
}

} // namespace

int runApplication(int argc, char** argv)
{
    fluent::webassembly::configureRuntime();
    QCoreApplication::setApplicationName(capabilities().applicationName);
    QCoreApplication::setOrganizationName(
        QStringLiteral(FLUENT_QT_GALLERY_ORGANIZATION_NAME));
    QCoreApplication::setApplicationVersion(
        QString::fromLatin1(FLUENT_QT_GALLERY_VERSION));
    fluent::prepareHighDpiApplication();

    application = std::make_unique<QApplication>(argc, argv);
    fluent::initializeResources();
    application->setFont(Typography::Styles::Body.toQFont());
    GallerySettings::instance();

    fluent::support::logging::InitializationOptions loggingOptions;
    loggingOptions.defaultLevel = fluent::support::logging::Level::Info;
    loggingOptions.installQtMessageHandler = true;
    fluent::support::logging::initialize(loggingOptions);
    LOG_INFO(QStringLiteral("GalleryApp startup appName=%1 organization=%2")
                 .arg(QApplication::applicationName(),
                      QApplication::organizationName()));
    application->setWindowIcon(appicon::icon());

    galleryWindow = std::make_unique<GalleryWindow>();
    // The browser owns only the Qt screen. The runtime adapter selects the
    // initial presentation while Fluent Window remains the single owner of
    // chrome, resizing, maximize/restore, and responsive content geometry.
    // zh_CN: 浏览器仅提供 Qt screen。运行时适配器选择初始形态，Fluent Window
    // 统一管理 chrome、缩放、最大化/还原与响应式内容几何。
    showGalleryWindow(galleryWindow.get());
    startWasmSmokeIfRequested(galleryWindow.get());
    LOG_INFO(QStringLiteral("GalleryApp browser event loop delegated to Qt"));
    return 0;
}

} // namespace fluent::gallery::platform
