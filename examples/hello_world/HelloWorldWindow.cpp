#include "HelloWorldWindow.h"

#include <FluentQt/FluentQt.h>

#include <QVBoxLayout>
#include <QWidget>

namespace fluentqt::hello_world {

std::unique_ptr<fluent::windowing::Window> createWindow()
{
    auto window = std::make_unique<fluent::windowing::Window>();
    window->setWindowTitle(QStringLiteral("FluentQt Hello World"));
    window->resize(720, 520);

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 32, 32, 32);

    auto* button = new fluent::basicinput::Button(
        QStringLiteral("Hello from FluentQt"), content);
    button->setFluentStyle(fluent::basicinput::Button::Accent);
    layout->addStretch();
    layout->addWidget(button, 0, Qt::AlignCenter);
    layout->addStretch();

    window->setContentWidget(content);
    return window;
}

} // namespace fluentqt::hello_world
