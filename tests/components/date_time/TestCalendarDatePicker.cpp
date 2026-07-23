#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/date_time/CalendarDatePicker.h"
#include "components/date_time/CalendarView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::date_time::CalendarDatePicker;
using fluent::date_time::CalendarView;
using fluent::dialogs_flyouts::Flyout;
using fluent::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

class CalendarDatePickerTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(palette);
        setAutoFillBackground(true);
    }
};

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

void showWindow(CalendarDatePickerTestWindow* window)
{
    window->show();
    const bool exposed = QTest::qWaitForWindowExposed(window);
    Q_UNUSED(exposed);
    processEvents();
}

Flyout* openPopupFor(CalendarDatePicker* picker, CalendarDatePickerTestWindow* window)
{
    showWindow(window);
    picker->show();
    processEvents();
    picker->openCalendar();
    processEvents();
    return window->findChild<Flyout*>(QStringLiteral("CalendarDatePickerPopup"));
}

CalendarView* calendarViewFor(Flyout* popup)
{
    if (!popup)
        return nullptr;
    return popup->findChild<CalendarView*>(QStringLiteral("CalendarDatePickerCalendarView"));
}

QRect cellRectForDate(CalendarView* calendarView, const QDate& date)
{
    const QVariantMap cells = calendarView->property("dateCellRects").toMap();
    return cells.value(date.toString(Qt::ISODate)).toRect();
}

QPoint cellCenterForDate(CalendarView* calendarView, const QDate& date)
{
    const QRect rect = cellRectForDate(calendarView, date);
    EXPECT_TRUE(rect.isValid()) << date.toString(Qt::ISODate).toStdString();
    return rect.center();
}

} // namespace

class CalendarDatePickerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<QDate>("QDate");
        qRegisterMetaType<Qt::DayOfWeek>("Qt::DayOfWeek");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new CalendarDatePickerTestWindow();
        window->resize(720, 520);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    CalendarDatePickerTestWindow* window = nullptr;
};

TEST_F(CalendarDatePickerTest, DefaultsAndInheritanceMatchComponentPattern)
{
    CalendarDatePicker picker;

    EXPECT_TRUE(picker.placeholderText().isEmpty());
    EXPECT_FALSE(picker.date().isValid());
    EXPECT_FALSE(picker.minDate().isValid());
    EXPECT_FALSE(picker.maxDate().isValid());
    EXPECT_TRUE(picker.displayFormat().isEmpty());
    EXPECT_FALSE(picker.isCalendarOpen());
    EXPECT_FALSE(picker.isOpen());
    EXPECT_TRUE(picker.displayText().isEmpty());
    EXPECT_EQ(picker.focusPolicy(), Qt::TabFocus);
    EXPECT_FALSE(picker.sizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<Button*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<QWidget*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&picker), nullptr);
}

TEST_F(CalendarDatePickerTest, PlaceholderAndFormatDriveDisplayText)
{
    CalendarDatePicker picker;
    QSignalSpy placeholderSpy(&picker, &CalendarDatePicker::placeholderTextChanged);
    QSignalSpy formatSpy(&picker, &CalendarDatePicker::displayFormatChanged);

    const QSize defaultSize = picker.sizeHint();
    picker.setPlaceholderText(QStringLiteral("Choose a date"));
    picker.setDisplayFormat(QStringLiteral("yyyy-MM-dd"));

    EXPECT_EQ(placeholderSpy.count(), 1);
    EXPECT_EQ(formatSpy.count(), 1);
    EXPECT_EQ(picker.sizeHint().height(), defaultSize.height());
    EXPECT_EQ(picker.displayText(), QStringLiteral("Choose a date"));
    EXPECT_EQ(picker.text(), QStringLiteral("Choose a date"));

    picker.setDate(QDate(2026, 5, 21));
    EXPECT_EQ(picker.displayText(), QStringLiteral("2026-05-21"));
    EXPECT_EQ(picker.text(), QStringLiteral("2026-05-21"));

    picker.setPlaceholderText(QStringLiteral("Choose a date"));
    picker.setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    EXPECT_EQ(placeholderSpy.count(), 1);
    EXPECT_EQ(formatSpy.count(), 1);
}

TEST_F(CalendarDatePickerTest, DateSetClearAndDuplicateSignals)
{
    CalendarDatePicker picker;
    picker.setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    QSignalSpy dateSpy(&picker, &CalendarDatePicker::dateChanged);

    picker.setDate(QDate(2026, 5, 21));
    ASSERT_EQ(dateSpy.count(), 1);
    EXPECT_EQ(picker.date(), QDate(2026, 5, 21));
    EXPECT_EQ(picker.displayText(), QStringLiteral("2026-05-21"));

    picker.setDate(QDate(2026, 5, 21));
    EXPECT_EQ(dateSpy.count(), 1);

    picker.clearDate();
    ASSERT_EQ(dateSpy.count(), 2);
    EXPECT_FALSE(picker.date().isValid());
    EXPECT_EQ(picker.displayText(), picker.placeholderText());

    picker.clearDate();
    EXPECT_EQ(dateSpy.count(), 2);
}

TEST_F(CalendarDatePickerTest, ConstraintsClampProgrammaticDates)
{
    CalendarDatePicker picker;
    QSignalSpy dateSpy(&picker, &CalendarDatePicker::dateChanged);

    picker.setMinDate(QDate(2026, 5, 10));
    picker.setMaxDate(QDate(2026, 5, 20));

    picker.setDate(QDate(2026, 5, 1));
    EXPECT_EQ(picker.date(), QDate(2026, 5, 10));

    picker.setDate(QDate(2026, 5, 30));
    EXPECT_EQ(picker.date(), QDate(2026, 5, 20));

    picker.clearDate();
    EXPECT_FALSE(picker.date().isValid());

    picker.setDate(QDate(2026, 5, 15));
    picker.setMinDate(QDate(2026, 5, 18));
    EXPECT_EQ(picker.date(), QDate(2026, 5, 18));
    EXPECT_GE(dateSpy.count(), 4);
}

TEST_F(CalendarDatePickerTest, FirstDayOfWeekUpdatesMonthGrid)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(40, 40, 180, picker->sizeHint().height());
    picker->setDate(QDate(2026, 5, 21));
    picker->setFirstDayOfWeek(Qt::Sunday);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);
    const QRect sundayCell = cellRectForDate(calendarView, QDate(2026, 5, 3));

    picker->setFirstDayOfWeek(Qt::Monday);
    processEvents();
    const QRect mondayCell = cellRectForDate(calendarView, QDate(2026, 5, 3));

    EXPECT_NE(sundayCell, mondayCell);
    EXPECT_EQ(calendarView->property("visibleMonth").toDate(), QDate(2026, 5, 1));
}

TEST_F(CalendarDatePickerTest, OpenCloseAndLightDismiss)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 80, 180, picker->sizeHint().height());
    QSignalSpy openSpy(picker, &CalendarDatePicker::calendarOpenChanged);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(picker->isCalendarOpen());
    EXPECT_TRUE(picker->isOpen());
    EXPECT_TRUE(popup->isOpen());
    ASSERT_EQ(openSpy.count(), 1);
    EXPECT_TRUE(openSpy.at(0).at(0).toBool());

    QTest::keyClick(popup, Qt::Key_Escape);
    processEvents();
    EXPECT_FALSE(picker->isCalendarOpen());
    EXPECT_FALSE(picker->isOpen());
    EXPECT_FALSE(popup->isOpen());
    ASSERT_EQ(openSpy.count(), 2);
    EXPECT_FALSE(openSpy.at(1).at(0).toBool());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    processEvents();
    EXPECT_FALSE(picker->isCalendarOpen());
}

TEST_F(CalendarDatePickerTest, PopupLeavesRoundedChromeVisible)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 80, 180, picker->sizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);
    EXPECT_FALSE(calendarView->isFrameVisible());

    const QMargins margins = popup->contentsMargins();
    EXPECT_GT(calendarView->geometry().left(), margins.left());
    EXPECT_GT(calendarView->geometry().top(), margins.top());
    EXPECT_LT(calendarView->geometry().right(), popup->width() - margins.right());
    EXPECT_LT(calendarView->geometry().bottom(), popup->height() - margins.bottom());
}

TEST_F(CalendarDatePickerTest, DisabledPickerDoesNotOpen)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(60, 60, 180, picker->sizeHint().height());
    picker->setEnabled(false);

    showWindow(window);
    picker->openCalendar();
    processEvents();
    EXPECT_FALSE(picker->isCalendarOpen());
    EXPECT_EQ(window->findChild<Flyout*>(QStringLiteral("CalendarDatePickerPopup")), nullptr);

    QTest::mouseClick(picker, Qt::LeftButton, Qt::NoModifier, picker->rect().center());
    processEvents();
    EXPECT_FALSE(picker->isCalendarOpen());
}

TEST_F(CalendarDatePickerTest, ReopenUsesSelectedMonthAndNavigationDoesNotChangeDate)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());
    picker->setDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);
    EXPECT_EQ(calendarView->property("visibleMonth").toDate(), QDate(2026, 5, 1));

    const QRect nextButton = calendarView->property("nextButtonRect").toRect();
    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, nextButton.center());
    processEvents();

    EXPECT_EQ(calendarView->property("visibleMonth").toDate(), QDate(2026, 6, 1));
    EXPECT_EQ(picker->date(), QDate(2026, 5, 21));

    picker->closeCalendar();
    processEvents();
    popup = openPopupFor(picker, window);
    calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);
    EXPECT_EQ(calendarView->property("visibleMonth").toDate(), QDate(2026, 5, 1));
}

TEST_F(CalendarDatePickerTest, SelectingDayUpdatesDateAndClosesFlyout)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());
    picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    picker->setDate(QDate(2026, 5, 21));
    QSignalSpy dateSpy(picker, &CalendarDatePicker::dateChanged);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 5, 22)));
    processEvents();

    EXPECT_EQ(picker->date(), QDate(2026, 5, 22));
    EXPECT_EQ(picker->displayText(), QStringLiteral("2026-05-22"));
    EXPECT_FALSE(picker->isCalendarOpen());
    ASSERT_EQ(dateSpy.count(), 1);
}

TEST_F(CalendarDatePickerTest, AdjacentMonthSelectionIsSupported)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());
    picker->setDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 6, 1)));
    processEvents();

    EXPECT_EQ(picker->date(), QDate(2026, 6, 1));
    EXPECT_EQ(picker->visibleMonth(), QDate(2026, 6, 1));
}

TEST_F(CalendarDatePickerTest, OpeningCalendarLandsOnSelectedMonthWithoutTransition)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());
    // Two months back guarantees the selected month differs from the calendar's
    // initial (current) month in any test run, year-round.
    // zh_CN: 往前推两个月，保证任何时间运行时选中月份都与日历初始（当前）月份不同。
    const QDate today = QDate::currentDate();
    const QDate selected = QDate(today.year(), today.month(), 1).addMonths(-2).addDays(14);
    picker->setDate(selected);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);

    // The flyout must land on the selected month instantly: while an entrance
    // transition runs, day-cell hits are rejected and the first click is lost.
    // zh_CN: flyout 必须直接停在选中月份：入场动画进行期间日期单元格命中被拒绝，
    // 首次点击会丢失。
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("monthTransitionProgress").toReal(), 1.0);

    const QDate target = selected.addDays(1);
    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, target));
    processEvents();

    EXPECT_EQ(picker->date(), target);
    EXPECT_FALSE(picker->isCalendarOpen());
}

TEST_F(CalendarDatePickerTest, OutOfRangeCellsAreDisabled)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());
    picker->setMinDate(QDate(2026, 5, 10));
    picker->setMaxDate(QDate(2026, 5, 20));
    picker->setDate(QDate(2026, 5, 15));
    QSignalSpy dateSpy(picker, &CalendarDatePicker::dateChanged);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 5, 9)));
    processEvents();
    EXPECT_EQ(picker->date(), QDate(2026, 5, 15));
    EXPECT_EQ(dateSpy.count(), 0);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 5, 21)));
    processEvents();
    EXPECT_EQ(picker->date(), QDate(2026, 5, 15));
    EXPECT_EQ(dateSpy.count(), 0);
}

TEST_F(CalendarDatePickerTest, KeyboardNavigationActivatesFocusedDate)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());
    picker->setDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    CalendarView* calendarView = calendarViewFor(popup);
    ASSERT_NE(calendarView, nullptr);

    calendarView->setFocus();
    QTest::keyClick(calendarView, Qt::Key_Right);
    QTest::keyClick(calendarView, Qt::Key_Return);
    processEvents();

    EXPECT_EQ(picker->date(), QDate(2026, 5, 22));
    EXPECT_FALSE(picker->isCalendarOpen());
}

TEST_F(CalendarDatePickerTest, ThemeUpdateRefreshesVisibleControls)
{
    CalendarDatePicker* picker = new CalendarDatePicker(window);
    picker->setGeometry(80, 60, 180, picker->sizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isOpen());

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    EXPECT_TRUE(popup->isVisible());
}

TEST_F(CalendarDatePickerTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(820, 520);
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("CalendarDatePicker"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(title);

    auto* standardHeader = new Label(QStringLiteral("Calendar"), window);
    standardHeader->anchors()->top = {title, Edge::Bottom, 28};
    standardHeader->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(standardHeader);

    auto* standard = new CalendarDatePicker(window);
    standard->setPlaceholderText(QStringLiteral("Pick a date"));
    standard->anchors()->top = {standardHeader, Edge::Bottom, 6};
    standard->anchors()->left = {standardHeader, Edge::Left, 0};
    layout->addWidget(standard);

    auto* selectedHeader = new Label(QStringLiteral("Selected date"), window);
    selectedHeader->anchors()->top = {standard, Edge::Bottom, 28};
    selectedHeader->anchors()->left = {standard, Edge::Left, 0};
    layout->addWidget(selectedHeader);

    auto* selected = new CalendarDatePicker(window);
    selected->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    selected->setDate(QDate(2026, 5, 21));
    selected->anchors()->top = {selectedHeader, Edge::Bottom, 6};
    selected->anchors()->left = {selectedHeader, Edge::Left, 0};
    layout->addWidget(selected);

    auto* constrainedHeader = new Label(QStringLiteral("Constrained"), window);
    constrainedHeader->anchors()->top = {selected, Edge::Bottom, 28};
    constrainedHeader->anchors()->left = {selected, Edge::Left, 0};
    layout->addWidget(constrainedHeader);

    auto* constrained = new CalendarDatePicker(window);
    constrained->setMinDate(QDate(2026, 5, 10));
    constrained->setMaxDate(QDate(2026, 5, 20));
    constrained->setDate(QDate(2026, 5, 15));
    constrained->anchors()->top = {constrainedHeader, Edge::Bottom, 6};
    constrained->anchors()->left = {constrainedHeader, Edge::Left, 0};
    layout->addWidget(constrained);

    auto* disabledHeader = new Label(QStringLiteral("Disabled"), window);
    disabledHeader->anchors()->top = {constrained, Edge::Bottom, 28};
    disabledHeader->anchors()->left = {constrained, Edge::Left, 0};
    layout->addWidget(disabledHeader);

    auto* disabled = new CalendarDatePicker(window);
    disabled->setDate(QDate(2026, 5, 21));
    disabled->setEnabled(false);
    disabled->anchors()->top = {disabledHeader, Edge::Bottom, 6};
    disabled->anchors()->left = {disabledHeader, Edge::Left, 0};
    layout->addWidget(disabled);

    auto* themeButton = new Button(QStringLiteral("Dark"), window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setFixedSize(96, 32);
    themeButton->anchors()->top = {title, Edge::Top, 4};
    themeButton->anchors()->right = {title, Edge::Right, 0};
    layout->addWidget(themeButton);
    QObject::connect(themeButton, &Button::clicked, themeButton, [themeButton]() {
        const bool dark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
        fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Light : fluent::FluentElement::Dark);
        themeButton->setText(dark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
}
