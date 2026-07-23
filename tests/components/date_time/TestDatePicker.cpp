#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QLocale>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>

#include <QImage>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/date_time/DatePicker.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::date_time::DatePicker;
using fluent::dialogs_flyouts::Flyout;
using fluent::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

class DatePickerTestWindow : public QWidget, public fluent::FluentElement {
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

void showWindow(DatePickerTestWindow* window)
{
    window->show();
    const bool exposed = QTest::qWaitForWindowExposed(window);
    Q_UNUSED(exposed);
    processEvents();
}

Flyout* openPopupFor(DatePicker* picker, DatePickerTestWindow* window)
{
    showWindow(window);
    picker->show();
    processEvents();
    picker->openPicker();
    processEvents();
    return window->findChild<Flyout*>(QStringLiteral("DatePickerFlyout"));
}

QWidget* columnFor(Flyout* popup, const QString& objectName)
{
    return popup ? popup->findChild<QWidget*>(objectName) : nullptr;
}

Button* buttonFor(Flyout* popup, const QString& objectName)
{
    return popup ? popup->findChild<Button*>(objectName) : nullptr;
}

QPoint nextRowCenter(QWidget* column)
{
    constexpr int kPickerRowHeight = 36;
    return column->rect().center() + QPoint(0, kPickerRowHeight);
}

QString pickerChevronUpGlyph()
{
    return Typography::Icons::FlipViewPrevV;
}

QString pickerChevronDownGlyph()
{
    return Typography::Icons::FlipViewNextV;
}

} // namespace

class DatePickerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<QDate>("QDate");
        qRegisterMetaType<fluent::date_time::DatePicker::DateField>(
            "fluent::date_time::DatePicker::DateField");
        qRegisterMetaType<fluent::date_time::DatePicker::MonthFormat>(
            "fluent::date_time::DatePicker::MonthFormat");
        qRegisterMetaType<fluent::date_time::DatePicker::DayFormat>(
            "fluent::date_time::DatePicker::DayFormat");
        qRegisterMetaType<fluent::date_time::DatePicker::YearFormat>(
            "fluent::date_time::DatePicker::YearFormat");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new DatePickerTestWindow();
        window->resize(760, 540);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    DatePickerTestWindow* window = nullptr;
};

TEST_F(DatePickerTest, DefaultsAndInheritanceMatchComponentPattern)
{
    DatePicker picker;

    EXPECT_TRUE(picker.date().isValid());
    EXPECT_FALSE(picker.selectedDate().isValid());
    EXPECT_LE(picker.minimumDate(), picker.date());
    EXPECT_GE(picker.maximumDate(), picker.date());
    EXPECT_TRUE(picker.monthVisible());
    EXPECT_TRUE(picker.dayVisible());
    EXPECT_TRUE(picker.yearVisible());
    EXPECT_FALSE(picker.isDropDownOpen());
    EXPECT_FALSE(picker.isOpen());
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Month), QStringLiteral("month"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Day), QStringLiteral("day"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Year), QStringLiteral("year"));
    EXPECT_EQ(picker.focusPolicy(), Qt::StrongFocus);
#ifdef Q_OS_MAC
    EXPECT_FALSE(picker.testAttribute(Qt::WA_MacShowFocusRect));
#endif
    EXPECT_FALSE(picker.sizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<Button*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<QWidget*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&picker), nullptr);

    picker.setLocale(QLocale(QLocale::Chinese, QLocale::China));
    picker.setSelectedDate(QDate(2026, 7, 21));
    picker.setMonthFormat(DatePicker::MonthFormat::FullMonthName);
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Month),
              QLocale(QLocale::Chinese, QLocale::China).monthName(7, QLocale::LongFormat));
}

TEST_F(DatePickerTest, SelectedDateClearAndFormattingDriveSegments)
{
    DatePicker picker;
    picker.setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    picker.setMonthFormat(DatePicker::MonthFormat::FullMonthName);
    picker.setDayFormat(DatePicker::DayFormat::DayIntegerWithAbbreviatedWeekday);
    picker.setYearFormat(DatePicker::YearFormat::FullYear);
    QSignalSpy selectedSpy(&picker, &DatePicker::selectedDateChanged);
    QSignalSpy dateSpy(&picker, &DatePicker::dateChanged);

    picker.setSelectedDate(QDate(2026, 7, 21));

    ASSERT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(dateSpy.count(), 1);
    EXPECT_EQ(picker.selectedDate(), QDate(2026, 7, 21));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Month), QStringLiteral("July"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Day), QStringLiteral("21 (Tue)"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Year), QStringLiteral("2026"));

    picker.setSelectedDate(QDate(2026, 7, 21));
    EXPECT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(dateSpy.count(), 1);

    picker.setMonthFormat(DatePicker::MonthFormat::TwoDigitMonth);
    picker.setDayFormat(DatePicker::DayFormat::TwoDigitDay);
    picker.setYearFormat(DatePicker::YearFormat::TwoDigitYear);
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Month), QStringLiteral("07"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Day), QStringLiteral("21"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Year), QStringLiteral("26"));

    picker.clearSelectedDate();
    ASSERT_EQ(selectedSpy.count(), 2);
    EXPECT_FALSE(picker.selectedDate().isValid());
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Month), QStringLiteral("month"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Day), QStringLiteral("day"));
    EXPECT_EQ(picker.fieldDisplayText(DatePicker::DateField::Year), QStringLiteral("year"));

    picker.clearSelectedDate();
    EXPECT_EQ(selectedSpy.count(), 2);
}

TEST_F(DatePickerTest, RangeClampsProgrammaticDatesAndInvalidRange)
{
    DatePicker picker;
    QSignalSpy minSpy(&picker, &DatePicker::minimumDateChanged);
    QSignalSpy maxSpy(&picker, &DatePicker::maximumDateChanged);

    picker.setDateRange(QDate(2026, 5, 10), QDate(2026, 5, 20));
    EXPECT_EQ(minSpy.count(), 1);
    EXPECT_EQ(maxSpy.count(), 1);

    picker.setSelectedDate(QDate(2026, 5, 1));
    EXPECT_EQ(picker.selectedDate(), QDate(2026, 5, 10));

    picker.setSelectedDate(QDate(2026, 5, 30));
    EXPECT_EQ(picker.selectedDate(), QDate(2026, 5, 20));

    picker.setDateRange(QDate(2026, 9, 10), QDate(2026, 4, 1));
    EXPECT_EQ(picker.minimumDate(), QDate(2026, 9, 10));
    EXPECT_EQ(picker.maximumDate(), QDate(2026, 9, 10));
    EXPECT_EQ(picker.selectedDate(), QDate(2026, 9, 10));
}

TEST_F(DatePickerTest, VisibilityAndFormatsAffectEntryAndFlyoutColumns)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(40, 40, 360, picker->sizeHint().height());
    picker->setSelectedDate(QDate(2026, 7, 21));
    picker->setDayFormat(DatePicker::DayFormat::DayIntegerWithAbbreviatedWeekday);

    picker->setYearVisible(false);
    EXPECT_FALSE(picker->yearVisible());
    EXPECT_TRUE(picker->fieldDisplayText(DatePicker::DateField::Year).isEmpty());

    picker->setMonthVisible(false);
    picker->setDayVisible(false);
    EXPECT_TRUE(picker->dayVisible()) << "The control keeps at least one visible field";

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    EXPECT_FALSE(columnFor(popup, QStringLiteral("DatePickerMonthColumn"))->isVisible());
    EXPECT_TRUE(columnFor(popup, QStringLiteral("DatePickerDayColumn"))->isVisible());
    EXPECT_FALSE(columnFor(popup, QStringLiteral("DatePickerYearColumn"))->isVisible());
}

TEST_F(DatePickerTest, EntryOpensWithMouseAndKeyboardAndDisabledStateBlocksOpen)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 80, 320, picker->sizeHint().height());
    QSignalSpy openSpy(picker, &DatePicker::dropDownOpenChanged);

    showWindow(window);
    QTest::mouseClick(picker, Qt::LeftButton, Qt::NoModifier, picker->rect().center());
    processEvents();
    EXPECT_TRUE(picker->isDropDownOpen());
    EXPECT_TRUE(picker->isOpen());
    ASSERT_EQ(openSpy.count(), 1);
    picker->closePicker();
    processEvents();
    EXPECT_FALSE(picker->isOpen());

    picker->setFocus();
    QTest::keyClick(picker, Qt::Key_Space);
    processEvents();
    EXPECT_TRUE(picker->isDropDownOpen());
    picker->closePicker();
    processEvents();

    picker->setEnabled(false);
    picker->openPicker();
    processEvents();
    EXPECT_FALSE(picker->isDropDownOpen());
}

TEST_F(DatePickerTest, FlyoutCommitCancelEscapeAndLightDismiss)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setDateRange(QDate(2026, 5, 1), QDate(2026, 5, 31));
    picker->setSelectedDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    QTest::keyClick(dayColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerCancelButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 21));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    QTest::keyClick(dayColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 22));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    QTest::keyClick(dayColumn, Qt::Key_Down);
    QTest::keyClick(dayColumn, Qt::Key_Return);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 23));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    QTest::keyClick(dayColumn, Qt::Key_Down);
    QTest::keyClick(popup, Qt::Key_Escape);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 23));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    QTest::keyClick(dayColumn, Qt::Key_Down);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 23));
    EXPECT_FALSE(picker->isDropDownOpen());
}

TEST_F(DatePickerTest, FlyoutDisabledValuesOutsideRangeAreNotSelectable)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setDateRange(QDate(2026, 5, 21), QDate(2026, 5, 21));
    picker->setSelectedDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    QTest::mouseClick(dayColumn, Qt::LeftButton, Qt::NoModifier, nextRowCenter(dayColumn));
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();

    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 21));
    EXPECT_FALSE(picker->isDropDownOpen());
}

TEST_F(DatePickerTest, FlyoutUsesNaturalColumnWidthForWideEntries)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(40, 60, 680, picker->sizeHint().height());
    picker->setDateRange(QDate(2026, 5, 10), QDate(2026, 5, 31));
    picker->setSelectedDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    EXPECT_LT(popup->width(), picker->width());
    EXPECT_GT(popup->width(), 300);
}

TEST_F(DatePickerTest, FlyoutLaysOutVisibleColumnsSideBySide)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);

    QWidget* monthColumn = columnFor(popup, QStringLiteral("DatePickerMonthColumn"));
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    QWidget* yearColumn = columnFor(popup, QStringLiteral("DatePickerYearColumn"));
    ASSERT_NE(monthColumn, nullptr);
    ASSERT_NE(dayColumn, nullptr);
    ASSERT_NE(yearColumn, nullptr);

    ASSERT_TRUE(monthColumn->isVisible());
    ASSERT_TRUE(dayColumn->isVisible());
    ASSERT_TRUE(yearColumn->isVisible());
    EXPECT_GT(monthColumn->width(), 0);
    EXPECT_GT(dayColumn->width(), 0);
    EXPECT_GT(yearColumn->width(), 0);
    EXPECT_LT(monthColumn->geometry().right(), dayColumn->geometry().left());
    EXPECT_LT(dayColumn->geometry().right(), yearColumn->geometry().left());
    EXPECT_EQ(monthColumn->geometry().top(), dayColumn->geometry().top());
    EXPECT_EQ(dayColumn->geometry().top(), yearColumn->geometry().top());
}

TEST_F(DatePickerTest, FlyoutColumnsUseCaretGlyphsAndNoFocusFrame)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);

    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    dayColumn->setFocus();
    processEvents();

    EXPECT_EQ(dayColumn->property("previousButtonGlyph").toString(), pickerChevronUpGlyph());
    EXPECT_EQ(dayColumn->property("nextButtonGlyph").toString(), pickerChevronDownGlyph());
    EXPECT_EQ(dayColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignHCenter));
    EXPECT_FALSE(dayColumn->property("focusFrameVisible").toBool());
#ifdef Q_OS_MAC
    EXPECT_FALSE(dayColumn->testAttribute(Qt::WA_MacShowFocusRect));
#endif
    EXPECT_TRUE(dayColumn->property("selectedRowHasBackground").toBool());

    FLUENT_MAKE_ENTER_EVENT(enterEvent, dayColumn->rect().center().x(), dayColumn->rect().center().y());
    QApplication::sendEvent(dayColumn, &enterEvent);
    QTest::qWait(180);
    processEvents();
    EXPECT_TRUE(dayColumn->property("columnHovered").toBool());
    EXPECT_GT(dayColumn->property("navButtonOpacity").toReal(), 0.9);
}

TEST_F(DatePickerTest, FieldTextAlignmentConfiguresFlyoutColumns)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedDate(QDate(2026, 5, 21));

    EXPECT_EQ(picker->fieldTextAlignment(DatePicker::DateField::Month), Qt::AlignLeft);
    EXPECT_EQ(picker->fieldTextAlignment(DatePicker::DateField::Day), Qt::AlignHCenter);
    EXPECT_EQ(picker->fieldTextAlignment(DatePicker::DateField::Year), Qt::AlignHCenter);

    picker->setFieldTextAlignment(DatePicker::DateField::Month, Qt::AlignRight);
    picker->setFieldTextAlignment(DatePicker::DateField::Day, Qt::AlignCenter);
    picker->setFieldTextAlignment(DatePicker::DateField::Year, Qt::AlignLeft);

    EXPECT_EQ(picker->fieldTextAlignment(DatePicker::DateField::Month), Qt::AlignRight);
    EXPECT_EQ(picker->fieldTextAlignment(DatePicker::DateField::Day), Qt::AlignHCenter);
    EXPECT_EQ(picker->fieldTextAlignment(DatePicker::DateField::Year), Qt::AlignLeft);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* monthColumn = columnFor(popup, QStringLiteral("DatePickerMonthColumn"));
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    QWidget* yearColumn = columnFor(popup, QStringLiteral("DatePickerYearColumn"));
    ASSERT_NE(monthColumn, nullptr);
    ASSERT_NE(dayColumn, nullptr);
    ASSERT_NE(yearColumn, nullptr);

    EXPECT_EQ(monthColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignRight));
    EXPECT_EQ(dayColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignHCenter));
    EXPECT_EQ(yearColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignLeft));
}

TEST_F(DatePickerTest, FlyoutScrollsMonthAndDayAsCyclicFieldsWithoutDateCarry)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setDateRange(QDate(2020, 1, 1), QDate(2030, 12, 31));

    picker->setSelectedDate(QDate(2026, 5, 31));
    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    QTest::keyClick(dayColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 1));

    picker->setSelectedDate(QDate(2026, 5, 1));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    QTest::keyClick(dayColumn, Qt::Key_Up);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 31));

    picker->setSelectedDate(QDate(2026, 12, 15));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* monthColumn = columnFor(popup, QStringLiteral("DatePickerMonthColumn"));
    ASSERT_NE(monthColumn, nullptr);
    QTest::keyClick(monthColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 1, 15));

    picker->setSelectedDate(QDate(2026, 1, 15));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    monthColumn = columnFor(popup, QStringLiteral("DatePickerMonthColumn"));
    ASSERT_NE(monthColumn, nullptr);
    QTest::keyClick(monthColumn, Qt::Key_Up);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 12, 15));
}

TEST_F(DatePickerTest, FlyoutKeepsYearColumnFinite)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setDateRange(QDate(2020, 1, 1), QDate(2030, 12, 31));
    picker->setSelectedDate(QDate(2030, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* yearColumn = columnFor(popup, QStringLiteral("DatePickerYearColumn"));
    ASSERT_NE(yearColumn, nullptr);
    QTest::keyClick(yearColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2030, 5, 21));

    picker->setSelectedDate(QDate(2020, 5, 21));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    yearColumn = columnFor(popup, QStringLiteral("DatePickerYearColumn"));
    ASSERT_NE(yearColumn, nullptr);
    QTest::keyClick(yearColumn, Qt::Key_Up);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2020, 5, 21));
}

TEST_F(DatePickerTest, FlyoutWheelUsesThresholdAccumulation)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setDateRange(QDate(2026, 5, 1), QDate(2026, 5, 31));
    picker->setSelectedDate(QDate(2026, 5, 21));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    const QPoint wheelPoint = dayColumn->rect().center();
    for (int i = 0; i < 3; ++i) {
        FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -30, Qt::NoModifier);
        QApplication::sendEvent(dayColumn, &wheel);
        EXPECT_TRUE(wheel.isAccepted());
    }
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 21));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    for (int i = 0; i < 4; ++i) {
        FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -30, Qt::NoModifier);
        QApplication::sendEvent(dayColumn, &wheel);
        EXPECT_TRUE(wheel.isAccepted());
    }
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 22));
}

TEST_F(DatePickerTest, FlyoutNormalizesMonthYearAndWheelChanges)
{
    DatePicker* picker = new DatePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setDateRange(QDate(2024, 1, 1), QDate(2027, 12, 31));

    picker->setSelectedDate(QDate(2026, 3, 31));
    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* monthColumn = columnFor(popup, QStringLiteral("DatePickerMonthColumn"));
    QTest::keyClick(monthColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 4, 30));

    picker->setSelectedDate(QDate(2024, 2, 29));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* yearColumn = columnFor(popup, QStringLiteral("DatePickerYearColumn"));
    QTest::keyClick(yearColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2025, 2, 28));

    picker->setSelectedDate(QDate(2026, 5, 21));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* dayColumn = columnFor(popup, QStringLiteral("DatePickerDayColumn"));
    ASSERT_NE(dayColumn, nullptr);
    const QPoint wheelPoint = dayColumn->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(dayColumn, &wheel);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("DatePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedDate(), QDate(2026, 5, 22));
}

TEST_F(DatePickerTest, ThemeUpdateRefreshesVisiblePopup)
{
    DatePicker* picker = new DatePicker(window);
    picker->setSelectedDate(QDate(2026, 5, 21));
    EXPECT_EQ(picker->sizeHint().height(), picker->minimumSizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    EXPECT_TRUE(popup->isVisible());
}

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// DatePicker is a Button-derived control whose CLOSED FIELD surface (DatePicker::paintEvent) must
// re-skin per design language (Fluent / Material 3 / macOS) crossed with each App theme (Light /
// Dark). Fluent stays byte-for-byte unchanged; Material 3 adopts the Outlined field (transparent +
// strokeStrong outline → accent when open), macOS adopts the push-button bezel (bgLayerAlt fill +
// hairline + small radius). Design language + theme are GLOBAL singletons, so the fixture restores
// both in TearDown. The grabbed image exercises the closed field only — PickerColumn lives in a
// flyout whose headless animation does not advance, so its highlight is tested via the existing
// flyout suite, not here. zh_CN: DatePicker 是 Button 派生控件,其闭合字段表面(DatePicker::paintEvent)
// 需在三种设计语言(Fluent/Material 3/macOS)× 两种主题(Light/Dark)下换肤。Fluent 逐字节不变;Material 3
// 采用描边字段(透明 + strokeStrong 描边 → 展开时 accent),macOS 采用按钮 bezel(bgLayerAlt 填充 + 发丝边 +
// 小圆角)。设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。抓取的图像仅覆盖闭合字段——PickerColumn
// 位于无头动画不推进的 flyout 中,其高亮由既有 flyout 套件覆盖,此处不测。
class DatePickerDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a closed DatePicker sized like a real field and grab it as an image. This drives
    // DatePicker::paintEvent's brand field surface. zh_CN: 以真实字段尺寸构建闭合 DatePicker 并抓取为
    // 图像,驱动 DatePicker::paintEvent 的品牌字段表面。
    static QImage grabField()
    {
        DatePicker picker;
        picker.setSelectedDate(QDate(2026, 7, 21));
        picker.resize(250, 36);
        return picker.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img)
    {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    static int differingPixels(const QImage& a, const QImage& b)
    {
        if (a.size() != b.size())
            return -1;
        int diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x)
                if (a.pixel(x, y) != b.pixel(x, y))
                    ++diff;
        return diff;
    }
};

TEST_F(DatePickerDesignLanguageTest, AllLanguagesAndThemesPaintFieldWithoutOpaqueBlack)
{
    struct LangCase {
        fluent::FluentElement::DesignLanguage lang;
        const char* name;
    };
    struct ThemeCase {
        fluent::FluentElement::Theme theme;
        const char* name;
    };

    const LangCase langs[] = {
        {fluent::FluentElement::DesignFluent, "Fluent"},
        {fluent::FluentElement::DesignMaterial, "Material"},
        {fluent::FluentElement::DesignCupertino, "Cupertino"},
    };
    const ThemeCase themes[] = {
        {fluent::FluentElement::Light, "Light"},
        {fluent::FluentElement::Dark, "Dark"},
    };

    for (const auto& th : themes) {
        // Capture the Fluent baseline for this theme so each brand can be compared against it.
        // zh_CN: 为该主题先抓取 Fluent 基线,供各品牌与之比较。
        fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignFluent);
        fluent::FluentElement::setTheme(th.theme);
        const QImage fluentField = grabField();

        for (const auto& lang : langs) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const QImage field = grabField();
            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // 1. The field paints a valid, non-empty image with content. zh_CN: 字段绘制出有效、非空且有内容的图像。
            ASSERT_FALSE(field.isNull()) << ctx;
            EXPECT_GT(field.width(), 0) << ctx;
            EXPECT_GT(field.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(field)) << "field painted nothing: " << ctx;

            // 2. Material re-skins the field away from the Fluent baseline. zh_CN: Material 字段相对 Fluent 基线换肤。
            if (lang.lang == fluent::FluentElement::DesignMaterial) {
                const int diff = differingPixels(fluentField, field);
                ASSERT_GE(diff, 0) << ctx << " (size mismatch)";
                EXPECT_GT(diff, 0)
                    << "Material DatePicker field is indistinguishable from Fluent: " << ctx;
            }

            // 3. Invalid-QColor guard: no opaque near-black fill at rest. Sample a left-edge
            // field-background pixel (clear of the segment text). alpha>200 && luminance<16 must be
            // FALSE. zh_CN: 无效 QColor 守卫:静息下无不透明近黑填充。采样左缘字段背景像素(避开分段文字),
            // alpha>200 且亮度<16 必须为 FALSE。
            const QColor c = field.pixelColor(6, field.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "DatePicker field painted an opaque black surface at rest: " << ctx << " rgba=("
                << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
        }
    }
}

TEST_F(DatePickerTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(920, 560);
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("DatePicker"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(title);

    auto* simpleLabel = new Label(QStringLiteral("Pick a date"), window);
    simpleLabel->setFluentTypography(Typography::FontRole::Body);
    simpleLabel->anchors()->top = {title, Edge::Bottom, 28};
    simpleLabel->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(simpleLabel);

    auto* simple = new DatePicker(window);
    simple->anchors()->top = {simpleLabel, Edge::Bottom, 6};
    simple->anchors()->left = {simpleLabel, Edge::Left, 0};
    layout->addWidget(simple);

    auto* formattedLabel = new Label(QStringLiteral("Formatted day, year hidden"), window);
    formattedLabel->setFluentTypography(Typography::FontRole::Body);
    formattedLabel->anchors()->top = {simple, Edge::Bottom, 28};
    formattedLabel->anchors()->left = {simple, Edge::Left, 0};
    layout->addWidget(formattedLabel);

    auto* formatted = new DatePicker(window);
    formatted->setSelectedDate(QDate(2026, 7, 21));
    formatted->setDayFormat(DatePicker::DayFormat::DayIntegerWithAbbreviatedWeekday);
    formatted->setYearVisible(false);
    formatted->anchors()->top = {formattedLabel, Edge::Bottom, 6};
    formatted->anchors()->left = {formattedLabel, Edge::Left, 0};
    layout->addWidget(formatted);

    auto* numericLabel = new Label(QStringLiteral("Numeric format"), window);
    numericLabel->setFluentTypography(Typography::FontRole::Body);
    numericLabel->anchors()->top = {formatted, Edge::Bottom, 28};
    numericLabel->anchors()->left = {formatted, Edge::Left, 0};
    layout->addWidget(numericLabel);

    auto* numeric = new DatePicker(window);
    numeric->setSelectedDate(QDate(2026, 5, 21));
    numeric->setMonthFormat(DatePicker::MonthFormat::TwoDigitMonth);
    numeric->setFieldTextAlignment(DatePicker::DateField::Month, Qt::AlignCenter);
    numeric->setDayFormat(DatePicker::DayFormat::TwoDigitDay);
    numeric->setYearFormat(DatePicker::YearFormat::TwoDigitYear);
    numeric->anchors()->top = {numericLabel, Edge::Bottom, 6};
    numeric->anchors()->left = {numericLabel, Edge::Left, 0};
    layout->addWidget(numeric);

    auto* constrainedLabel = new Label(QStringLiteral("Constrained"), window);
    constrainedLabel->setFluentTypography(Typography::FontRole::Body);
    constrainedLabel->anchors()->top = {simpleLabel, Edge::Top, 0};
    constrainedLabel->anchors()->left = {simple, Edge::Right, 48};
    layout->addWidget(constrainedLabel);

    auto* constrained = new DatePicker(window);
    constrained->setDateRange(QDate(2026, 5, 10), QDate(2026, 5, 31));
    constrained->setSelectedDate(QDate(2026, 5, 21));
    constrained->anchors()->top = {constrainedLabel, Edge::Bottom, 6};
    constrained->anchors()->left = {constrainedLabel, Edge::Left, 0};
    layout->addWidget(constrained);

    auto* disabledLabel = new Label(QStringLiteral("Disabled"), window);
    disabledLabel->setFluentTypography(Typography::FontRole::Body);
    disabledLabel->anchors()->top = {constrained, Edge::Bottom, 28};
    disabledLabel->anchors()->left = {constrained, Edge::Left, 0};
    layout->addWidget(disabledLabel);

    auto* disabled = new DatePicker(window);
    disabled->setSelectedDate(QDate(2026, 5, 21));
    disabled->setEnabled(false);
    disabled->anchors()->top = {disabledLabel, Edge::Bottom, 6};
    disabled->anchors()->left = {disabledLabel, Edge::Left, 0};
    layout->addWidget(disabled);

    auto* clearButton = new Button(QStringLiteral("Clear"), window);
    clearButton->setFixedSize(96, 32);
    clearButton->anchors()->top = {numeric, Edge::Bottom, 28};
    clearButton->anchors()->left = {numeric, Edge::Left, 0};
    layout->addWidget(clearButton);
    QObject::connect(clearButton, &Button::clicked, simple, &DatePicker::clearSelectedDate);

    auto* openButton = new Button(QStringLiteral("Open"), window);
    openButton->setFixedSize(96, 32);
    openButton->anchors()->top = {clearButton, Edge::Top, 0};
    openButton->anchors()->left = {clearButton, Edge::Right, 12};
    layout->addWidget(openButton);
    QObject::connect(openButton, &Button::clicked, simple, &DatePicker::openPicker);

    auto* themeButton = new Button(QStringLiteral("Dark"), window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setIconGlyph(Typography::Icons::Color, 16);
    themeButton->setFluentLayout(Button::IconBefore);
    themeButton->setFixedSize(112, 32);
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
