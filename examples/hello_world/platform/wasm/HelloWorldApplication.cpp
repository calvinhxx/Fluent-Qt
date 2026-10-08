#include "HelloWorldApplication.h"

#include <FluentQt/FluentQt.h>
#include <FluentQt/WebAssembly.h>

#include <QApplication>
#include <QScreen>

#include <memory>

#include "HelloWorldWindow.h"

#ifdef FLUENTQT_HELLO_WORLD_SPATIAL_OBSERVATIONS
#include <FluentQt/Spatial.h>
#include <QJsonDocument>
#include <QTransform>
#include <emscripten.h>
#endif

namespace fluentqt::hello_world {
namespace {

std::unique_ptr<QApplication> application;
std::unique_ptr<fluent::windowing::Window> window;

} // namespace

#ifdef FLUENTQT_HELLO_WORLD_SPATIAL_OBSERVATIONS
// The Gallery browser gate observes the actual widgets; independent consumers omit this bridge.
extern "C" EMSCRIPTEN_KEEPALIVE void fluentQtHelloWorldObserve()
{
    if (!window)
        return;
    auto* view =
        window->findChild<fluent::spatial::SpatialView*>(QStringLiteral("helloWorldSpatialView"));
    if (!view || view->items().isEmpty())
        return;
    auto* item = view->items().first();
    auto* card = qobject_cast<fluent::layout::Card*>(item->widget());
    auto* button =
        card ? card->findChild<fluent::basicinput::Button*>(QStringLiteral("helloWorldButton"))
             : nullptr;
    if (!card || !button)
        return;
    const QVector3D rotation = item->rotation();
    const QPointF tilt = view->maximumTilt();
    const QPolygonF polygon = item->projectedPolygon();
    const QPointF localCenter = QPointF(button->mapTo(card, QPoint())) +
                                QPointF(button->width() / 2.0, button->height() / 2.0);
    QPointF center = button->mapToGlobal(button->rect().center());
    QTransform projection;
    const bool projected =
        view->isSpatialEnabled() && polygon.size() >= 4 &&
        QTransform::quadToQuad({QPointF(0, 0), QPointF(card->width(), 0),
                                QPointF(card->width(), card->height()), QPointF(0, card->height())},
                               polygon.mid(0, 4), projection);
    if (projected) {
        center = QPointF(view->mapToGlobal(QPoint())) + projection.map(localCenter);
    }
    const auto geometry = [](const QWidget* widget) {
        return QVariantMap{{QStringLiteral("width"), widget->width()},
                           {QStringLiteral("height"), widget->height()}};
    };
    const QVariantMap state{
        {QStringLiteral("window"), geometry(window.get())},
        {QStringLiteral("view"), geometry(view)},
        {QStringLiteral("card"), geometry(card)},
        {QStringLiteral("button"), geometry(button)},
        {QStringLiteral("text"), button->text()},
        {QStringLiteral("rotation"), QVariantList{rotation.x(), rotation.y(), rotation.z()}},
        {QStringLiteral("maximumTilt"), QVariantList{tilt.x(), tilt.y()}},
        {QStringLiteral("surfaceIntensity"), item->surfaceIntensity()},
        {QStringLiteral("backend"), static_cast<int>(view->activeBackend())},
        {QStringLiteral("spatialEnabled"), view->isSpatialEnabled()},
        {QStringLiteral("projected"), projected},
        {QStringLiteral("buttonCenter"), QVariantList{center.x(), center.y()}}};
    const QByteArray json = QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact);
    EM_ASM({ document.documentElement.dataset.fluentQtHelloWorldState = UTF8ToString($0); },
           json.constData());
}
#endif

int runApplication(int argc, char** argv)
{
    fluent::webassembly::configureRuntime();
    fluent::prepareHighDpiApplication();
    application = std::make_unique<QApplication>(argc, argv);
    fluent::initializeResources();
    application->setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());

    window = createWindow();
    QScreen* screen = application->primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    const QSize requested = window->size().boundedTo(
        QSize(qMax(1, available.width() - 48), qMax(1, available.height() - 48)));
    const QRect normalGeometry(available.x() + (available.width() - requested.width()) / 2,
                               available.y() + (available.height() - requested.height()) / 2,
                               requested.width(), requested.height());
    // The browser supplies a screen; the same reusable Fluent Window owns its
    // chrome and geometry on every platform.
    // zh_CN: 浏览器提供 screen；同一个可复用 Fluent Window 在各平台统一管理
    // chrome 与几何。
    fluent::webassembly::showWindow(window.get(), normalGeometry);
    return 0;
}

} // namespace fluentqt::hello_world
