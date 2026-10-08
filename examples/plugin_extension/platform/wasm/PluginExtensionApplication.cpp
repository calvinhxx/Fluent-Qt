#include "PluginExtensionApplication.h"
#include "PluginExtensionWindow.h"
#include "PluginContractProbe.h"

#include <FluentQt/WebAssembly.h>
#include <QApplication>
#include <QJsonDocument>
#include <QLayout>
#include <QScreen>
#include <QTimer>
#include <emscripten.h>
#include <memory>

namespace fluentqt::plugin_extension {
namespace {
std::unique_ptr<QApplication> application;
std::unique_ptr<PluginExtensionWindow> window;

void publishState()
{
    auto state = window->pluginState();
    auto* content = window->findChild<QWidget*>(QStringLiteral("pluginContent"));
    const auto widgetState = [content](const QString& name) {
        auto* widget = content ? content->findChild<QWidget*>(name) : nullptr;
        if (!widget)
            return QVariantMap{};
        const QPoint origin = widget->mapTo(content, QPoint());
        return QVariantMap{{QStringLiteral("x"), origin.x()},
                           {QStringLiteral("y"), origin.y()},
                           {QStringLiteral("width"), widget->width()},
                           {QStringLiteral("height"), widget->height()},
                           {QStringLiteral("text"), widget->property("text")},
                           {QStringLiteral("enabled"), widget->isEnabled()}};
    };
    if (content && content->layout()) {
        const QMargins margins = content->layout()->contentsMargins();
        state.insert(
            QStringLiteral("presentation"),
            QVariantMap{
                {QStringLiteral("window"), QVariantList{window->width(), window->height()}},
                {QStringLiteral("content"), QVariantList{content->width(), content->height()}},
                {QStringLiteral("margins"),
                 QVariantList{margins.left(), margins.top(), margins.right(), margins.bottom()}},
                {QStringLiteral("spacing"), content->layout()->spacing()},
                {QStringLiteral("activation"), widgetState(QStringLiteral("pluginActivation"))},
                {QStringLiteral("creation"), widgetState(QStringLiteral("pluginCreateGreeting"))},
                {QStringLiteral("status"), widgetState(QStringLiteral("pluginStatus"))}});
    }
    const auto json = QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact);
    // Publish native observations; JavaScript does not implement plugin state.
    EM_ASM({ document.documentElement.dataset.fluentQtPluginState = UTF8ToString($0); },
           json.constData());
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtPluginActivate(int active)
{
    return window && window->setExtensionActive(active != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtPluginCreateGreeting()
{
    return window && window->createGreeting();
}

extern "C" EMSCRIPTEN_KEEPALIVE int fluentQtPluginRunContract()
{
    const auto result = runPluginContractChecks();
    const auto json = QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact);
    EM_ASM({ document.documentElement.dataset.fluentQtPluginContract = UTF8ToString($0); },
           json.constData());
    return result.value(QStringLiteral("allPassed")).toBool();
}

int runApplication(int argc, char** argv)
{
    fluent::webassembly::configureRuntime();
    fluent::prepareHighDpiApplication();
    application = std::make_unique<QApplication>(argc, argv);
    fluent::initializeResources();
    application->setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
    window = std::make_unique<PluginExtensionWindow>();
    if (!window->isReady())
        return 1;
    QObject::connect(window.get(), &PluginExtensionWindow::stateChanged, window.get(),
                     [] { QTimer::singleShot(0, window.get(), publishState); });
    const auto* screen = application->primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 900, 700);
    const QSize size = window->size().boundedTo(available.size() - QSize(48, 48));
    const QRect normal(available.center() - QPoint(size.width() / 2, size.height() / 2), size);
    fluent::webassembly::showWindow(window.get(), normal);
    QTimer::singleShot(0, window.get(), publishState);
    return 0;
}

} // namespace fluentqt::plugin_extension
