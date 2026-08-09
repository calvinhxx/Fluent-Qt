#include "HelloWorldApplication.h"

#include <FluentQt/FluentQt.h>
#include <FluentQt/WebAssembly.h>

#include <QApplication>
#include <QScreen>

#include <memory>

#include "HelloWorldWindow.h"

namespace fluentqt::hello_world {
namespace {

std::unique_ptr<QApplication> application;
std::unique_ptr<fluent::windowing::Window> window;

} // namespace

int runApplication(int argc, char** argv)
{
    fluent::webassembly::configureRuntime();
    fluent::prepareHighDpiApplication();
    application = std::make_unique<QApplication>(argc, argv);
    fluent::initializeResources();
    application->setFont(
        Typography::fontStyle(Typography::FontRole::Body).toQFont());

    window = createWindow();
    QScreen* screen = application->primaryScreen();
    const QRect available = screen ? screen->availableGeometry()
                                   : QRect(0, 0, 1280, 720);
    const QSize requested = window->size().boundedTo(
        QSize(qMax(1, available.width() - 48),
              qMax(1, available.height() - 48)));
    const QRect normalGeometry(
        available.x() + (available.width() - requested.width()) / 2,
        available.y() + (available.height() - requested.height()) / 2,
        requested.width(),
        requested.height());
    // The browser supplies a screen; the same reusable Fluent Window owns its
    // chrome and geometry on every platform.
    // zh_CN: 浏览器提供 screen；同一个可复用 Fluent Window 在各平台统一管理
    // chrome 与几何。
    fluent::webassembly::showWindow(window.get(), normalGeometry);
    return 0;
}

} // namespace fluentqt::hello_world
