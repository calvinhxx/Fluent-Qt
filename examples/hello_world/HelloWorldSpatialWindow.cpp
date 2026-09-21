#include "HelloWorldWindow.h"

#include <FluentQt/FluentQt.h>

#include <QVBoxLayout>

namespace fluentqt::hello_world {

std::unique_ptr<fluent::windowing::Window> createWindow()
{
    auto window = std::make_unique<fluent::windowing::Window>();
    window->setWindowTitle(QStringLiteral("FluentQt Hello World — 3D"));
    window->resize(720, 520);

    auto* card = new fluent::layout::Card;
    card->setFixedSize(320, 200);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(32, 32, 32, 32);

    auto* button = new fluent::basicinput::Button(QStringLiteral("Hello from FluentQt"), card);
    button->setFluentStyle(fluent::basicinput::Button::Accent);
    QObject::connect(button, &fluent::basicinput::Button::clicked, button,
                     [button] { button->setText(QStringLiteral("Hello, you!")); });
    layout->addWidget(button, 0, Qt::AlignCenter);

    auto* view = new fluent::spatial::SpatialView;
    auto* item = view->addWidget(card, fluent::WidgetOwnership::Owned);
    item->setRotation(QVector3D(6, -12, 0));
    item->setSurfaceIntensity(0.6);
    view->setMaximumTilt(QPointF(3, 5));
    window->setContentWidget(view);
    return window;
}

} // namespace fluentqt::hello_world
