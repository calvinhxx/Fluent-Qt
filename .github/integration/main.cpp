#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QLocale>

// Compile/link fixture for external add_subdirectory consumers.
// CI builds this target but does not start its event loop.
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    fluent::initializeResources();

    auto theme = fluent::ThemeRegistry::instance().snapshot();
    theme.fontScale = 1.0;
    fluent::ThemeRegistry::instance().applySnapshot(theme);

    fluent::collections::ListView list;
    list.setSelectionMode(fluent::collections::SelectionMode::Single);
    list.setFontRole(Typography::FontRole::Body);

    fluent::scrolling::ScrollView scrollView;
    scrollView.setContentWidget(
        new fluent::textfields::Label(QStringLiteral("External source consumer")),
        fluent::WidgetOwnership::Owned);

    fluent::date_time::CalendarView calendar;
    calendar.setLocale(QLocale::English);
    calendar.resetFirstDayOfWeek();

    fluent::basicinput::Button button(QStringLiteral("FluentQt external integration"));
    return 0;
}
