#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QLayout>
#include <QWidget>
#include <cstdio>
#include <memory>

QWidget* createStudioPage(QWidget* parent);

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (argc != 2 || !fluent::initializeResources())
        return 1;
    bool validCount = false;
    const int expectedWidgets = QString::fromLatin1(argv[1]).toInt(&validCount);
    if (!validCount || expectedWidgets < 0)
        return 1;
    std::unique_ptr<QWidget> page(createStudioPage(nullptr));
    if (!page)
        return 1;
    page->show();
    app.processEvents();
    const auto* layout = page->layout();
    if (!page->isVisible() || !layout)
        return 1;

    int widgetCount = 0;
    for (int i = 0; i < layout->count(); ++i) {
        if (layout->itemAt(i)->widget())
            ++widgetCount;
    }
    if (widgetCount != expectedWidgets) {
        std::fprintf(stderr, "Expected %d direct widgets in the root layout, found %d\n", expectedWidgets, widgetCount);
        return 1;
    }
    return 0;
}
