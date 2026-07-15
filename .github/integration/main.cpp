#include <FluentQt/FluentQt.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    fluent::initializeResources();

    fluent::basicinput::Button button(QStringLiteral("FluentQt external integration"));
    button.show();
    return 0;
}
