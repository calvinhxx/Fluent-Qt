#include <FluentQt/Collections.h>
#include <FluentQt/DateTime.h>
#include <FluentQt/Layout.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/FluentQt.h>
#include <FluentQt/Foundation.h>
#include <QApplication>
#include <QLayout>
#include <QWidget>
#include <cstdio>
#include <memory>

QWidget* createStudioPage(QWidget* parent);

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if ((argc != 2 && argc != 3) || !fluent::initializeResources())
        return 1;
    bool validCount = false;
    const int expectedWidgets = QString::fromLatin1(argv[1]).toInt(&validCount);
    if (!validCount || expectedWidgets < 0)
        return 1;
    QWidget host;
    const QColor hostAccent(QStringLiteral("#AA3366"));
    fluent::UserTheme::applyAccentOverride(hostAccent);
    std::unique_ptr<QWidget> page(createStudioPage(&host));
    if (fluent::ThemeRegistry::instance().colors(fluent::FluentElement::Light).accentDefault != hostAccent) {
        std::fprintf(stderr, "Creating the exported page changed the host accent.\n");
        return 1;
    }
    if (!page)
        return 1;
    host.show();
    page->show();
    app.processEvents();
    const auto* layout = page->layout();
    if (!page->isVisible() || !layout)
        return 1;

    if (argc == 3 && QString::fromLatin1(argv[2]) == QStringLiteral("device-monitor")) {
        auto* grid = page->findChild<fluent::collections::DataGrid*>();
        const auto* model = grid ? grid->model() : nullptr;
        if (!model || model->rowCount() != 4 || model->columnCount() != 4
            || model->headerData(3, Qt::Horizontal).toString() != QStringLiteral("Status")
            || model->index(0, 0).data().toString() != QStringLiteral("plc-edge-01")) {
            std::fprintf(stderr, "Exported device table does not match the sample.\n");
            return 1;
        }
    }

    if (argc == 3 && QString::fromLatin1(argv[2]) == QStringLiteral("accordion-pickers")) {
        auto* accordion = page->findChild<fluent::layout::Accordion*>();
        const auto dates = page->findChildren<fluent::date_time::DatePicker*>();
        const auto times = page->findChildren<fluent::date_time::TimePicker*>();
        if (!accordion || accordion->count() != 2
            || accordion->itemAt(0)->headerText() != QStringLiteral("Release Notes")
            || !accordion->itemAt(0)->findChild<fluent::textfields::Label*>()
            || accordion->itemAt(1)->headerText() != QStringLiteral("Details") || dates.size() != 2 || times.size() != 2
            || dates[0]->selectedDate().isValid() || times[0]->selectedTime().isValid()
            || dates[1]->selectedDate() != QDate(2031, 3, 14) || times[1]->selectedTime() != QTime(16, 12)) {
            std::fprintf(stderr, "Exported Accordion sections or picker selections differ from the document.\n");
            return 1;
        }
    }

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
