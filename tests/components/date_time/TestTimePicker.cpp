#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QLocale>
#include <QPalette>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTime>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/date_time/TimePicker.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "QtTestEnvironment.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::date_time::TimePicker;
using fluent::dialogs_flyouts::Flyout;
using fluent::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

class TimePickerTestWindow : public QWidget, public fluent::FluentElement {
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

void showWindow(TimePickerTestWindow* window)
{
    window->show();
    const bool exposed = QTest::qWaitForWindowExposed(window);
    Q_UNUSED(exposed);
    processEvents();
}

Flyout* openPopupFor(TimePicker* picker, TimePickerTestWindow* window)
{
    showWindow(window);
    picker->show();
    processEvents();
    picker->openPicker();
    processEvents();
    return window->findChild<Flyout*>(QStringLiteral("TimePickerFlyout"));
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
    constexpr int kPickerRowHeight = 40;
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

class TimePickerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<QTime>("QTime");
        qRegisterMetaType<fluent::date_time::TimePicker::TimeField>(
            "fluent::date_time::TimePicker::TimeField");
        qRegisterMetaType<fluent::date_time::TimePicker::ClockIdentifier>(
            "fluent::date_time::TimePicker::ClockIdentifier");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new TimePickerTestWindow();
        window->resize(760, 540);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    TimePickerTestWindow* window = nullptr;
};

TEST_F(TimePickerTest, DefaultsAndInheritanceMatchComponentPattern)
{
    TimePicker picker;

    EXPECT_TRUE(picker.time().isValid());
    EXPECT_FALSE(picker.selectedTime().isValid());
    EXPECT_EQ(picker.minuteIncrement(), 1);
    EXPECT_EQ(picker.clockIdentifier(), TimePicker::ClockIdentifier::TwelveHourClock);
    EXPECT_FALSE(picker.isDropDownOpen());
    EXPECT_FALSE(picker.isOpen());
    EXPECT_TRUE(picker.fieldDisplayText(TimePicker::TimeField::Hour).isEmpty());
    EXPECT_TRUE(picker.fieldDisplayText(TimePicker::TimeField::Minute).isEmpty());
    EXPECT_TRUE(picker.fieldDisplayText(TimePicker::TimeField::Period).isEmpty());
    EXPECT_TRUE(picker.confirmButtonAccessibleName().isEmpty());
    EXPECT_TRUE(picker.cancelButtonAccessibleName().isEmpty());
    EXPECT_EQ(picker.focusPolicy(), Qt::StrongFocus);
#ifdef Q_OS_MAC
    EXPECT_FALSE(picker.testAttribute(Qt::WA_MacShowFocusRect));
#endif
    EXPECT_FALSE(picker.sizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<Button*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&picker), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&picker), nullptr);
}

TEST_F(TimePickerTest, SelectedTimeClearAndFormattingDriveSegments)
{
    TimePicker picker;
    picker.setPlaceholderText(TimePicker::TimeField::Hour, QStringLiteral("hour"));
    picker.setPlaceholderText(TimePicker::TimeField::Minute, QStringLiteral("minute"));
    picker.setPlaceholderText(TimePicker::TimeField::Period, QStringLiteral("AM/PM"));
    picker.setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    QSignalSpy selectedSpy(&picker, &TimePicker::selectedTimeChanged);
    QSignalSpy timeSpy(&picker, &TimePicker::timeChanged);

    picker.setSelectedTime(QTime(0, 5));

    ASSERT_EQ(selectedSpy.count(), 1);
    ASSERT_EQ(timeSpy.count(), 1);
    EXPECT_EQ(picker.selectedTime(), QTime(0, 5));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("12"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("05"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("AM"));

    picker.setSelectedTime(QTime(12, 0));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("12"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("00"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("PM"));

    picker.setSelectedTime(QTime(12, 0));
    EXPECT_EQ(selectedSpy.count(), 2);
    EXPECT_EQ(timeSpy.count(), 2);

    picker.clearSelectedTime();
    ASSERT_EQ(selectedSpy.count(), 3);
    EXPECT_FALSE(picker.selectedTime().isValid());
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("hour"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("minute"));
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period), QStringLiteral("AM/PM"));
}

TEST_F(TimePickerTest, ApplicationOwnedTextUpdatesEntryAndFlyoutActions)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 40, 360, picker->sizeHint().height());
    QSignalSpy placeholderSpy(picker, &TimePicker::placeholderTextChanged);
    QSignalSpy confirmSpy(picker, &TimePicker::confirmButtonAccessibleNameChanged);
    QSignalSpy cancelSpy(picker, &TimePicker::cancelButtonAccessibleNameChanged);

    picker->setPlaceholderText(TimePicker::TimeField::Hour, QStringLiteral("hour"));
    picker->setPlaceholderText(TimePicker::TimeField::Minute, QStringLiteral("minute"));
    picker->setPlaceholderText(TimePicker::TimeField::Period, QStringLiteral("AM/PM"));
    picker->setConfirmButtonAccessibleName(QStringLiteral("Apply time"));
    picker->setCancelButtonAccessibleName(QStringLiteral("Dismiss time picker"));

    EXPECT_EQ(placeholderSpy.count(), 3);
    EXPECT_EQ(confirmSpy.count(), 1);
    EXPECT_EQ(cancelSpy.count(), 1);
    EXPECT_EQ(picker->fieldDisplayText(TimePicker::TimeField::Hour),
              QStringLiteral("hour"));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    ASSERT_NE(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), nullptr);
    ASSERT_NE(buttonFor(popup, QStringLiteral("TimePickerCancelButton")), nullptr);
    EXPECT_EQ(buttonFor(popup, QStringLiteral("TimePickerConfirmButton"))->accessibleName(),
              QStringLiteral("Apply time"));
    EXPECT_EQ(buttonFor(popup, QStringLiteral("TimePickerCancelButton"))->accessibleName(),
              QStringLiteral("Dismiss time picker"));

    picker->setCancelButtonAccessibleName(QStringLiteral("Cancel time"));
    EXPECT_EQ(buttonFor(popup, QStringLiteral("TimePickerCancelButton"))->accessibleName(),
              QStringLiteral("Cancel time"));
}

TEST_F(TimePickerTest, LocaleTracksTheInheritedQWidgetProperty)
{
    TimePicker picker;
    picker.setSelectedTime(QTime(13, 5));
    QSignalSpy localeSpy(&picker, &TimePicker::localeChanged);
    const QLocale chinese(QLocale::Chinese, QLocale::China);
    const QLocale us(QLocale::English, QLocale::UnitedStates);
    const QLocale targetLocale = picker.locale() == chinese ? us : chinese;

    QWidget* base = &picker;
    base->setLocale(targetLocale);

    EXPECT_EQ(picker.locale(), targetLocale);
    EXPECT_EQ(picker.fieldDisplayText(TimePicker::TimeField::Period),
              targetLocale.pmText());
    EXPECT_EQ(localeSpy.count(), 1);
}

TEST_F(TimePickerTest, TwentyFourHourModeHidesPeriodAndPreservesAbsoluteTime)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 40, 360, picker->sizeHint().height());
    picker->setSelectedTime(QTime(23, 45));

    QSignalSpy modeSpy(picker, &TimePicker::clockIdentifierChanged);
    picker->setClockIdentifier(TimePicker::ClockIdentifier::TwentyFourHourClock);

    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(picker->selectedTime(), QTime(23, 45));
    EXPECT_EQ(picker->fieldDisplayText(TimePicker::TimeField::Hour), QStringLiteral("23"));
    EXPECT_EQ(picker->fieldDisplayText(TimePicker::TimeField::Minute), QStringLiteral("45"));
    EXPECT_TRUE(picker->fieldDisplayText(TimePicker::TimeField::Period).isEmpty());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(columnFor(popup, QStringLiteral("TimePickerHourColumn"))->isVisible());
    EXPECT_TRUE(columnFor(popup, QStringLiteral("TimePickerMinuteColumn"))->isVisible());
    EXPECT_FALSE(columnFor(popup, QStringLiteral("TimePickerPeriodColumn"))->isVisible());
}

TEST_F(TimePickerTest, MinuteIncrementClampsAndSnapsWithoutRollingHour)
{
    TimePicker picker;
    QSignalSpy incrementSpy(&picker, &TimePicker::minuteIncrementChanged);
    QSignalSpy selectedSpy(&picker, &TimePicker::selectedTimeChanged);

    picker.setMinuteIncrement(0);
    EXPECT_EQ(picker.minuteIncrement(), 1);
    EXPECT_EQ(incrementSpy.count(), 0);

    picker.setMinuteIncrement(15);
    ASSERT_EQ(incrementSpy.count(), 1);
    EXPECT_EQ(picker.minuteIncrement(), 15);

    picker.setSelectedTime(QTime(9, 58));
    ASSERT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(picker.selectedTime(), QTime(9, 45));
    EXPECT_EQ(picker.time(), QTime(9, 45));

    picker.setMinuteIncrement(99);
    EXPECT_EQ(picker.minuteIncrement(), 59);
    EXPECT_EQ(picker.selectedTime(), QTime(9, 59));
}

TEST_F(TimePickerTest, EntryOpensWithMouseAndKeyboardAndDisabledStateBlocksOpen)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 80, 320, picker->sizeHint().height());
    QSignalSpy openSpy(picker, &TimePicker::dropDownOpenChanged);

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

TEST_F(TimePickerTest, OpenStateHandlerCanSynchronouslyDeletePicker)
{
    auto* picker = new TimePicker(window);
    QPointer<TimePicker> guard(picker);
    QObject::connect(
        picker, &TimePicker::dropDownOpenChanged, qApp,
        [picker](bool open) {
            if (open)
                delete picker;
        });

    picker->openPicker();

    EXPECT_TRUE(guard.isNull());
}

TEST_F(TimePickerTest, FlyoutCommitCancelEscapeAndLightDismiss)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QTest::keyClick(minuteColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerCancelButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 30));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QTest::keyClick(minuteColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 31));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QTest::keyClick(hourColumn, Qt::Key_Down);
    QTest::keyClick(hourColumn, Qt::Key_Return);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(10, 31));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QTest::keyClick(hourColumn, Qt::Key_Down);
    QTest::keyClick(popup, Qt::Key_Escape);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(10, 31));
    EXPECT_FALSE(picker->isDropDownOpen());

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QTest::keyClick(hourColumn, Qt::Key_Down);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(10, 31));
    EXPECT_FALSE(picker->isDropDownOpen());
}

TEST_F(TimePickerTest, FlyoutWheelRowsAndPeriodColumnUpdatePendingTime)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setMinuteIncrement(15);
    picker->setSelectedTime(QTime(11, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QTest::mouseClick(minuteColumn, Qt::LeftButton, Qt::NoModifier, nextRowCenter(minuteColumn));
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(11, 45));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(periodColumn, nullptr);
    EXPECT_EQ(periodColumn->property("visibleItemCount").toInt(), 2);
    QTest::keyClick(periodColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(23, 45));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(periodColumn, nullptr);
    QTest::keyClick(periodColumn, Qt::Key_Down);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(23, 45));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(periodColumn, nullptr);
    QTest::keyClick(periodColumn, Qt::Key_Up);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(11, 45));

    picker->setSelectedTime(QTime(23, 45));
    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    const QPoint wheelPoint = minuteColumn->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(minuteColumn, &wheel);
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(23, 0));
}

TEST_F(TimePickerTest, FontChangeUpdatesEntryAndOpenFlyout)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    const QSize defaultEntrySize = picker->sizeHint();
    picker->setGeometry(40, 60, defaultEntrySize.width(), defaultEntrySize.height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    ASSERT_NE(hourColumn, nullptr);
    const QSize defaultPopupSize = popup->size();
    const int defaultColumnHeight = hourColumn->height();

    QFont largeFont = picker->font();
    largeFont.setPixelSize(30);
    picker->setFont(largeFont);
    picker->resize(picker->sizeHint());
    processEvents();

    EXPECT_EQ(picker->font().pixelSize(), 30);
    EXPECT_GT(picker->sizeHint().height(), defaultEntrySize.height());
    EXPECT_GT(popup->height(), defaultPopupSize.height());
    EXPECT_GT(hourColumn->height(), defaultColumnHeight);
    EXPECT_EQ(hourColumn->font().pixelSize(), 30);
    EXPECT_EQ(hourColumn->property("selectedRowHeight").toInt(),
              QFontMetrics(largeFont).height() + 12);
}

TEST_F(TimePickerTest, WinUiLayoutUsesContinuousSelectionAndStretchedActions)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    picker->setGeometry(40, 60, picker->sizeHint().width(), picker->sizeHint().height());

    EXPECT_EQ(picker->sizeHint().width(), 242);
    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* panel = popup->findChild<QWidget*>(QStringLiteral("TimePickerFlyoutPanel"));
    QWidget* hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    QWidget* periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    Button* confirm = buttonFor(popup, QStringLiteral("TimePickerConfirmButton"));
    Button* cancel = buttonFor(popup, QStringLiteral("TimePickerCancelButton"));
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(hourColumn, nullptr);
    ASSERT_NE(minuteColumn, nullptr);
    ASSERT_NE(periodColumn, nullptr);
    ASSERT_NE(confirm, nullptr);
    ASSERT_NE(cancel, nullptr);

    EXPECT_EQ(panel->width(), 242);
    EXPECT_NEAR(hourColumn->width(), minuteColumn->width(), 1);
    EXPECT_NEAR(minuteColumn->width(), periodColumn->width(), 1);
    EXPECT_TRUE(hourColumn->property("selectedRowContinuous").toBool());
    EXPECT_EQ(hourColumn->property("selectedRowLeftInset").toInt(), 4);
    EXPECT_EQ(hourColumn->property("selectedRowRightInset").toInt(), 0);
    EXPECT_EQ(minuteColumn->property("selectedRowLeftInset").toInt(), 0);
    EXPECT_EQ(minuteColumn->property("selectedRowRightInset").toInt(), 0);
    EXPECT_EQ(periodColumn->property("selectedRowLeftInset").toInt(), 0);
    EXPECT_EQ(periodColumn->property("selectedRowRightInset").toInt(), 4);
    EXPECT_EQ(hourColumn->property("selectedRowHeight").toInt(), 40);

    EXPECT_EQ(confirm->geometry().left(), 4);
    EXPECT_EQ(panel->width() - 1 - cancel->geometry().right(), 4);
    EXPECT_EQ(cancel->geometry().left() - confirm->geometry().right() - 1, 4);
    EXPECT_EQ(confirm->size(), cancel->size());
    EXPECT_GT(confirm->width(), 90);
}

TEST_F(TimePickerTest, FlyoutAlignsSelectedRowWithClosedField)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    picker->setGeometry(220, 250, picker->sizeHint().width(), picker->sizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* panel = popup->findChild<QWidget*>(QStringLiteral("TimePickerFlyoutPanel"));
    ASSERT_NE(panel, nullptr);

    const int selectedRowCenterY = panel->property("selectedRowCenterY").toInt();
    ASSERT_GT(selectedRowCenterY, 0);
    const QPoint selectedRowCenter = panel->mapTo(
        window, QPoint(panel->rect().center().x(), selectedRowCenterY));
    const QPoint fieldCenter = picker->mapTo(window, picker->rect().center());
    const QPoint panelTopLeft = panel->mapTo(window, QPoint(0, 0));
    const QPoint fieldTopLeft = picker->mapTo(window, QPoint(0, 0));

    EXPECT_NEAR(selectedRowCenter.y(), fieldCenter.y(), 1);
    EXPECT_EQ(panelTopLeft.x(), fieldTopLeft.x());
}

TEST_F(TimePickerTest, FlyoutClampsAtTopEdgeWithoutLeavingSurface)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    picker->setGeometry(220, 0, picker->sizeHint().width(), picker->sizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* panel = popup->findChild<QWidget*>(QStringLiteral("TimePickerFlyoutPanel"));
    ASSERT_NE(panel, nullptr);

    const QRect panelGeometry(panel->mapTo(window, QPoint(0, 0)), panel->size());
    const QRect usableSurface = window->rect().adjusted(4, 4, -4, -4);
    const int selectedRowCenterY = panel->property("selectedRowCenterY").toInt();
    const QPoint selectedRowCenter = panel->mapTo(
        window, QPoint(panel->rect().center().x(), selectedRowCenterY));
    const QPoint fieldCenter = picker->mapTo(window, picker->rect().center());

    EXPECT_TRUE(usableSurface.contains(panelGeometry));
    EXPECT_EQ(panelGeometry.top(), usableSurface.top());
    EXPECT_GT(selectedRowCenter.y(), fieldCenter.y());
}

TEST_F(TimePickerTest, FlyoutClampsAtBottomEdgeWithoutLeavingSurface)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    const QSize pickerSize = picker->sizeHint();
    picker->setGeometry(220, window->height() - pickerSize.height(),
                        pickerSize.width(), pickerSize.height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* panel = popup->findChild<QWidget*>(QStringLiteral("TimePickerFlyoutPanel"));
    ASSERT_NE(panel, nullptr);

    const QRect panelGeometry(panel->mapTo(window, QPoint(0, 0)), panel->size());
    const QRect usableSurface = window->rect().adjusted(4, 4, -4, -4);
    const int selectedRowCenterY = panel->property("selectedRowCenterY").toInt();
    const QPoint selectedRowCenter = panel->mapTo(
        window, QPoint(panel->rect().center().x(), selectedRowCenterY));
    const QPoint fieldCenter = picker->mapTo(window, picker->rect().center());

    EXPECT_TRUE(usableSurface.contains(panelGeometry));
    EXPECT_EQ(panelGeometry.bottom(), usableSurface.bottom());
    EXPECT_LT(selectedRowCenter.y(), fieldCenter.y());
}

TEST_F(TimePickerTest, FlyoutColumnsUseCaretGlyphsAndNoFocusFrame)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);

    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    minuteColumn->setFocus();
    processEvents();

    EXPECT_EQ(minuteColumn->property("previousButtonGlyph").toString(), pickerChevronUpGlyph());
    EXPECT_EQ(minuteColumn->property("nextButtonGlyph").toString(), pickerChevronDownGlyph());
    EXPECT_EQ(minuteColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignHCenter));
    EXPECT_FALSE(minuteColumn->property("focusFrameVisible").toBool());
#ifdef Q_OS_MAC
    EXPECT_FALSE(minuteColumn->testAttribute(Qt::WA_MacShowFocusRect));
#endif
    EXPECT_TRUE(minuteColumn->property("selectedRowHasBackground").toBool());

    FLUENT_MAKE_ENTER_EVENT(enterEvent, minuteColumn->rect().center().x(), minuteColumn->rect().center().y());
    QApplication::sendEvent(minuteColumn, &enterEvent);
    QTest::qWait(180);
    processEvents();
    EXPECT_TRUE(minuteColumn->property("columnHovered").toBool());
    EXPECT_GT(minuteColumn->property("navButtonOpacity").toReal(), 0.9);
}

TEST_F(TimePickerTest, FieldTextAlignmentConfiguresFlyoutColumns)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(40, 60, 420, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Hour), Qt::AlignLeft);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Minute), Qt::AlignHCenter);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Period), Qt::AlignHCenter);

    picker->setFieldTextAlignment(TimePicker::TimeField::Hour, Qt::AlignRight);
    picker->setFieldTextAlignment(TimePicker::TimeField::Minute, Qt::AlignLeft);
    picker->setFieldTextAlignment(TimePicker::TimeField::Period, Qt::AlignCenter);

    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Hour), Qt::AlignRight);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Minute), Qt::AlignLeft);
    EXPECT_EQ(picker->fieldTextAlignment(TimePicker::TimeField::Period), Qt::AlignHCenter);

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* hourColumn = columnFor(popup, QStringLiteral("TimePickerHourColumn"));
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    QWidget* periodColumn = columnFor(popup, QStringLiteral("TimePickerPeriodColumn"));
    ASSERT_NE(hourColumn, nullptr);
    ASSERT_NE(minuteColumn, nullptr);
    ASSERT_NE(periodColumn, nullptr);

    EXPECT_EQ(hourColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignRight));
    EXPECT_EQ(minuteColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignLeft));
    EXPECT_EQ(periodColumn->property("textAlignment").toInt(), static_cast<int>(Qt::AlignHCenter));
}

TEST_F(TimePickerTest, FlyoutWheelUsesThresholdAccumulation)
{
    TimePicker* picker = new TimePicker(window);
    picker->setGeometry(80, 60, 360, picker->sizeHint().height());
    picker->setSelectedTime(QTime(9, 30));

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    QWidget* minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    QPoint wheelPoint = minuteColumn->rect().center();
    for (int i = 0; i < 3; ++i) {
        FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -30, Qt::NoModifier);
        QApplication::sendEvent(minuteColumn, &wheel);
        EXPECT_TRUE(wheel.isAccepted());
    }
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 30));

    popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    minuteColumn = columnFor(popup, QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(minuteColumn, nullptr);
    wheelPoint = minuteColumn->rect().center();
    for (int i = 0; i < 4; ++i) {
        FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -30, Qt::NoModifier);
        QApplication::sendEvent(minuteColumn, &wheel);
        EXPECT_TRUE(wheel.isAccepted());
    }
    QTest::mouseClick(buttonFor(popup, QStringLiteral("TimePickerConfirmButton")), Qt::LeftButton);
    processEvents();
    EXPECT_EQ(picker->selectedTime(), QTime(9, 31));
}

TEST_F(TimePickerTest, ThemeUpdateRefreshesVisiblePopup)
{
    TimePicker* picker = new TimePicker(window);
    picker->setSelectedTime(QTime(9, 30));
    EXPECT_EQ(picker->sizeHint().height(), picker->minimumSizeHint().height());

    Flyout* popup = openPopupFor(picker, window);
    ASSERT_NE(popup, nullptr);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    EXPECT_TRUE(popup->isVisible());
}


TEST_F(TimePickerTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(920, 560);
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("TimePicker"), window);
    title->setFluentTypography(Typography::FontRole::Title);
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(title);

    auto* simpleLabel = new Label(QStringLiteral("Pick a time"), window);
    simpleLabel->setFluentTypography(Typography::FontRole::Body);
    simpleLabel->anchors()->top = {title, Edge::Bottom, 28};
    simpleLabel->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(simpleLabel);

    auto* simple = new TimePicker(window);
    simple->setPlaceholderText(TimePicker::TimeField::Hour, QStringLiteral("hour"));
    simple->setPlaceholderText(TimePicker::TimeField::Minute, QStringLiteral("minute"));
    simple->setPlaceholderText(TimePicker::TimeField::Period, QStringLiteral("AM/PM"));
    simple->anchors()->top = {simpleLabel, Edge::Bottom, 6};
    simple->anchors()->left = {simpleLabel, Edge::Left, 0};
    layout->addWidget(simple);

    auto* afternoonLabel = new Label(QStringLiteral("Afternoon"), window);
    afternoonLabel->setFluentTypography(Typography::FontRole::Body);
    afternoonLabel->anchors()->top = {simpleLabel, Edge::Top, 0};
    afternoonLabel->anchors()->left = {simple, Edge::Right, 48};
    layout->addWidget(afternoonLabel);

    auto* afternoon = new TimePicker(window);
    afternoon->setSelectedTime(QTime(14, 30));
    afternoon->anchors()->top = {afternoonLabel, Edge::Bottom, 6};
    afternoon->anchors()->left = {afternoonLabel, Edge::Left, 0};
    layout->addWidget(afternoon);

    auto* steppedLabel = new Label(QStringLiteral("15 minute increment"), window);
    steppedLabel->setFluentTypography(Typography::FontRole::Body);
    steppedLabel->anchors()->top = {simple, Edge::Bottom, 28};
    steppedLabel->anchors()->left = {simple, Edge::Left, 0};
    layout->addWidget(steppedLabel);

    auto* stepped = new TimePicker(window);
    stepped->setMinuteIncrement(15);
    stepped->setSelectedTime(QTime(9, 45));
    stepped->anchors()->top = {steppedLabel, Edge::Bottom, 6};
    stepped->anchors()->left = {steppedLabel, Edge::Left, 0};
    layout->addWidget(stepped);

    auto* twentyFourLabel = new Label(QStringLiteral("24 hour"), window);
    twentyFourLabel->setFluentTypography(Typography::FontRole::Body);
    twentyFourLabel->anchors()->top = {afternoon, Edge::Bottom, 28};
    twentyFourLabel->anchors()->left = {afternoon, Edge::Left, 0};
    layout->addWidget(twentyFourLabel);

    auto* twentyFour = new TimePicker(window);
    twentyFour->setFieldTextAlignment(TimePicker::TimeField::Hour, Qt::AlignCenter);
    twentyFour->setClockIdentifier(TimePicker::ClockIdentifier::TwentyFourHourClock);
    twentyFour->setSelectedTime(QTime(23, 5));
    twentyFour->anchors()->top = {twentyFourLabel, Edge::Bottom, 6};
    twentyFour->anchors()->left = {twentyFourLabel, Edge::Left, 0};
    layout->addWidget(twentyFour);

    auto* disabledLabel = new Label(QStringLiteral("Disabled"), window);
    disabledLabel->setFluentTypography(Typography::FontRole::Body);
    disabledLabel->anchors()->top = {twentyFour, Edge::Bottom, 28};
    disabledLabel->anchors()->left = {twentyFour, Edge::Left, 0};
    layout->addWidget(disabledLabel);

    auto* disabled = new TimePicker(window);
    disabled->setSelectedTime(QTime(6, 15));
    disabled->setEnabled(false);
    disabled->anchors()->top = {disabledLabel, Edge::Bottom, 6};
    disabled->anchors()->left = {disabledLabel, Edge::Left, 0};
    layout->addWidget(disabled);

    auto* customFontLabel = new Label(QStringLiteral("Custom 20 px font"), window);
    customFontLabel->setFluentTypography(Typography::FontRole::Body);
    customFontLabel->anchors()->top = {stepped, Edge::Bottom, 28};
    customFontLabel->anchors()->left = {stepped, Edge::Left, 0};
    layout->addWidget(customFontLabel);

    auto* customFont = new TimePicker(window);
    customFont->setSelectedTime(QTime(9, 30));
    QFont timePickerFont = customFont->font();
    timePickerFont.setPixelSize(20);
    customFont->setFont(timePickerFont);
    customFont->anchors()->top = {customFontLabel, Edge::Bottom, 6};
    customFont->anchors()->left = {customFontLabel, Edge::Left, 0};
    layout->addWidget(customFont);

    auto* clearButton = new Button(QStringLiteral("Clear"), window);
    clearButton->setFixedSize(96, 32);
    clearButton->anchors()->top = {customFont, Edge::Bottom, 28};
    clearButton->anchors()->left = {stepped, Edge::Left, 0};
    layout->addWidget(clearButton);
    QObject::connect(clearButton, &Button::clicked, simple, &TimePicker::clearSelectedTime);

    auto* openButton = new Button(QStringLiteral("Open"), window);
    openButton->setFixedSize(96, 32);
    openButton->anchors()->top = {clearButton, Edge::Top, 0};
    openButton->anchors()->left = {clearButton, Edge::Right, 12};
    layout->addWidget(openButton);
    QObject::connect(openButton, &Button::clicked, simple, &TimePicker::openPicker);

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
    if (tests::support::shouldCaptureVisualSnapshot()) {
        // Use a lower sample so the selected row stays aligned without the
        // window-edge clamp or popup overlap obscuring the page title.
        customFont->openPicker();
        processEvents();

        tests::support::VisualSnapshotOptions light;
        light.windowSize = QSize(920, 560);
        light.variant = QStringLiteral("light");
        light.theme = tests::support::VisualSnapshotTheme::Light;
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window, light));

        tests::support::VisualSnapshotOptions dark = light;
        dark.variant = QStringLiteral("dark");
        dark.theme = tests::support::VisualSnapshotTheme::Dark;
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window, dark));
        return;
    }
    qApp->exec();
}
