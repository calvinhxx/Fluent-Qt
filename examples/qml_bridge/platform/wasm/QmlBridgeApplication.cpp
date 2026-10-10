#include "QmlBridgeApplication.h"
#include "QmlBridgePanel.h"
#include "QmlBridgeWindow.h"

#include <FluentQt/QmlBridge.h>
#include <FluentQt/WebAssembly.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QScreen>
#include <QTimer>
#include <emscripten.h>
#include <memory>

namespace fluentqt::qml_bridge {
namespace {
std::unique_ptr<QApplication> application;
std::unique_ptr<QmlBridgeWindow> window;
bool smokeFinished = false;
bool smokePassed = false;

void publishState()
{
    auto state = window->panel()->bridgeState();
    if (smokeFinished)
        state.insert(QStringLiteral("smokePassed"), smokePassed);
    const auto json = QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact);
    EM_ASM({ document.documentElement.dataset.fluentQtQmlBridgeState = UTF8ToString($0); },
           json.constData());
}
} // namespace

int runApplication(int argc, char** argv)
{
    fluent::webassembly::configureRuntime();
    fluent::prepareHighDpiApplication();
    fluent::qmlbridge::QmlSession::prepareApplication();
    application = std::make_unique<QApplication>(argc, argv);
    fluent::initializeResources();
    if (EM_ASM_INT({ return new URLSearchParams(window.location.search).get('theme') == 'dark'; }))
        fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    application->setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
    window = std::make_unique<QmlBridgeWindow>();
    QObject::connect(window->panel(), &QmlBridgePanel::stateChanged, window.get(),
                     [] { QTimer::singleShot(0, window.get(), publishState); });
    const auto* screen = application->primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1000, 800);
    const QSize size = window->size().boundedTo(available.size() - QSize(48, 48));
    const QRect normal(available.center() - QPoint(size.width() / 2, size.height() / 2), size);
    fluent::webassembly::showWindow(window.get(), normal);
    QTimer::singleShot(0, window.get(), publishState);
    if (emscripten_run_script_int("new URLSearchParams(location.search).has('smoke-test')")) {
        auto clock = std::make_shared<QElapsedTimer>();
        clock->start();
        auto* poll = new QTimer(window.get());
        poll->setInterval(50);
        QObject::connect(poll, &QTimer::timeout, window.get(), [poll, clock] {
            const auto state = window->panel()->bridgeState();
            if (!state.value(QStringLiteral("ready")).toBool() &&
                state.value(QStringLiteral("error")).toString().isEmpty() &&
                clock->elapsed() < 5000)
                return;
            poll->stop();
            smokePassed =
                state.value(QStringLiteral("ready")).toBool() && window->panel()->runSmokeTest();
            smokeFinished = true;
            publishState();
            poll->deleteLater();
        });
        poll->start();
    }
    return 0;
}

} // namespace fluentqt::qml_bridge
