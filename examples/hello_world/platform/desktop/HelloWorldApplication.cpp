#include "HelloWorldApplication.h"

#include <FluentQt/FluentQt.h>

#include <QApplication>

#include "HelloWorldWindow.h"

namespace fluentqt::hello_world {

int runApplication(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    QApplication application(argc, argv);
    fluent::initializeResources();
    application.setFont(
        Typography::fontStyle(Typography::FontRole::Body).toQFont());

    auto window = createWindow();
    window->show();
    return application.exec();
}

} // namespace fluentqt::hello_world
