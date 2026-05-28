#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"
#include "view/basicinput/Button.h"
#include "view/date_time/CalendarView.h"
#include "view/textfields/Label.h"

using view::AnchorLayout;
using view::basicinput::Button;
using view::date_time::CalendarView;
using view::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

class CalendarViewTestWindow : public QWidget, public FluentElement {
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

void showWindow(CalendarViewTestWindow* window)
{
    window->show();
    const bool exposed = QTest::qWaitForWindowExposed(window);
    Q_UNUSED(exposed);
    processEvents();
}

QPoint cellCenterForDate(CalendarView* view, const QDate& date)
{
    const QRect rect = view->dateCellRect(date);
    EXPECT_TRUE(rect.isValid()) << date.toString(Qt::ISODate).toStdString();
    return rect.center();
}

bool sendCalendarWheel(CalendarView* view,
                       const QPoint& point,
                       const QPoint& pixelDelta,
                       const QPoint& angleDelta,
                       Qt::ScrollPhase phase = Qt::NoScrollPhase,
                       int waitMs = 0)
{
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, point, point, pixelDelta, angleDelta,
                                       Qt::NoButton, Qt::NoModifier, phase, false);
    QApplication::sendEvent(view, &event);
    if (waitMs > 0)
        QTest::qWait(waitMs);
    processEvents();
    return event.isAccepted();
}

bool sendCalendarWheel(CalendarView* view, const QPoint& point, int angleDeltaY,
                       Qt::ScrollPhase phase = Qt::NoScrollPhase)
{
    return sendCalendarWheel(view, point, QPoint(), QPoint(0, angleDeltaY), phase);
}

} // namespace

class CalendarViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<QDate>("QDate");
        qRegisterMetaType<Qt::DayOfWeek>("Qt::DayOfWeek");
        qRegisterMetaType<CalendarView::CalendarContentLevel>("CalendarView::CalendarContentLevel");
    }

    void SetUp() override
    {
        FluentElement::setTheme(FluentElement::Light);
        window = new CalendarViewTestWindow();
        window->resize(820, 520);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        FluentElement::setTheme(FluentElement::Light);
    }

    CalendarViewTestWindow* window = nullptr;
};

TEST_F(CalendarViewTest, DefaultsAndInheritanceMatchComponentPattern)
{
    CalendarView view;

    EXPECT_EQ(view.objectName(), QStringLiteral("CalendarView"));
    EXPECT_FALSE(view.selectedDate().isValid());
    EXPECT_TRUE(view.visibleMonth().isValid());
    EXPECT_FALSE(view.minDate().isValid());
    EXPECT_FALSE(view.maxDate().isValid());
    EXPECT_FALSE(view.sizeHint().isEmpty());
    EXPECT_LE(view.sizeHint().width(), 340);
    EXPECT_LE(view.sizeHint().height(), 380);
    EXPECT_EQ(view.focusPolicy(), Qt::StrongFocus);
    EXPECT_TRUE(view.isFrameVisible());
    EXPECT_NE(dynamic_cast<QWidget*>(&view), nullptr);
    EXPECT_NE(dynamic_cast<FluentElement*>(&view), nullptr);
    EXPECT_NE(dynamic_cast<view::QMLPlus*>(&view), nullptr);
}

TEST_F(CalendarViewTest, FrameVisibilityCanBeDisabledForPopupHosts)
{
    CalendarView view;
    QSignalSpy frameSpy(&view, &CalendarView::frameVisibleChanged);

    view.setFrameVisible(false);
    EXPECT_FALSE(view.isFrameVisible());
    EXPECT_FALSE(view.property("frameVisible").toBool());
    ASSERT_EQ(frameSpy.count(), 1);
    EXPECT_FALSE(frameSpy.at(0).at(0).toBool());

    view.setFrameVisible(false);
    EXPECT_EQ(frameSpy.count(), 1);

    view.setFrameVisible(true);
    EXPECT_TRUE(view.isFrameVisible());
    EXPECT_TRUE(view.property("frameVisible").toBool());
    ASSERT_EQ(frameSpy.count(), 2);
    EXPECT_TRUE(frameSpy.at(1).at(0).toBool());
}

TEST_F(CalendarViewTest, SelectedDateSignalsAndRangeClamping)
{
    CalendarView view;
    view.setDateRange(QDate(2026, 5, 10), QDate(2026, 5, 20));
    QSignalSpy selectedSpy(&view, &CalendarView::selectedDateChanged);

    view.setSelectedDate(QDate(2026, 5, 1));
    ASSERT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(view.selectedDate(), QDate(2026, 5, 10));

    view.setSelectedDate(QDate(2026, 5, 1));
    EXPECT_EQ(selectedSpy.count(), 1);

    view.setSelectedDate(QDate());
    ASSERT_EQ(selectedSpy.count(), 2);
    EXPECT_FALSE(view.selectedDate().isValid());
}

TEST_F(CalendarViewTest, FirstDayOfWeekUpdatesGrid)
{
    CalendarView view;
    view.setVisibleMonth(QDate(2026, 5, 1));
    view.setFirstDayOfWeek(Qt::Sunday);
    const QRect sundayCell = view.dateCellRect(QDate(2026, 5, 3));

    view.setFirstDayOfWeek(Qt::Monday);
    const QRect mondayCell = view.dateCellRect(QDate(2026, 5, 3));

    EXPECT_NE(sundayCell, mondayCell);
    EXPECT_EQ(view.property("visibleMonth").toDate(), QDate(2026, 5, 1));
}

TEST_F(CalendarViewTest, DateIndicatorRectsAreCircularAndCentered)
{
    CalendarView view;
    view.setVisibleMonth(QDate(2026, 5, 1));

    const QDate date(2026, 5, 21);
    const QVariantMap indicatorRects = view.property("dateIndicatorRects").toMap();
    ASSERT_TRUE(indicatorRects.contains(date.toString(Qt::ISODate)));
    const QRectF indicator = indicatorRects.value(date.toString(Qt::ISODate)).toRectF();
    const QRectF cell = QRectF(view.dateCellRect(date)).adjusted(2.0, 2.0, -2.0, -2.0);

    EXPECT_FALSE(indicator.isEmpty());
    EXPECT_NEAR(indicator.width(), indicator.height(), 0.01);
    EXPECT_NEAR(indicator.center().x(), cell.center().x(), 0.5);
    EXPECT_NEAR(indicator.center().y(), cell.center().y(), 0.5);
    EXPECT_TRUE(cell.contains(indicator));
}

TEST_F(CalendarViewTest, CurrentDateAndSelectedDateUseDistinctVisualStates)
{
    CalendarView view;
    const QDate today = QDate::currentDate();
    const QDate selected = today.day() > 1 ? today.addDays(-1) : today.addDays(1);
    view.setVisibleMonth(QDate(today.year(), today.month(), 1));
    view.setSelectedDate(selected);

    const QVariantMap states = view.property("dateVisualStates").toMap();
    EXPECT_EQ(states.value(today.toString(Qt::ISODate)).toString(), QStringLiteral("current"));
    EXPECT_EQ(states.value(selected.toString(Qt::ISODate)).toString(), QStringLiteral("selected"));

    view.setSelectedDate(today);
    const QVariantMap todaySelectedStates = view.property("dateVisualStates").toMap();
    EXPECT_EQ(todaySelectedStates.value(today.toString(Qt::ISODate)).toString(), QStringLiteral("current"));
}

TEST_F(CalendarViewTest, MonthNavigationUsesVerticalScrollGlyphs)
{
    CalendarView view;

    EXPECT_EQ(view.property("previousButtonGlyph").toString(), Typography::Icons::Up);
    EXPECT_EQ(view.property("nextButtonGlyph").toString(), Typography::Icons::Down);
    EXPECT_EQ(view.previousButtonRect().center().y(), view.nextButtonRect().center().y());
    EXPECT_LT(view.previousButtonRect().center().x(), view.nextButtonRect().center().x());
}

TEST_F(CalendarViewTest, TitleButtonUsesButtonSizedHitTarget)
{
    CalendarView view;
    view.setVisibleMonth(QDate(2026, 5, 1));

    const QRect titleButton = view.titleButtonRect();
    EXPECT_EQ(titleButton.top(), view.previousButtonRect().top());
    EXPECT_EQ(titleButton.height(), view.previousButtonRect().height());
    EXPECT_LT(titleButton.right(), view.previousButtonRect().left());
    EXPECT_GE(titleButton.width(), view.width() - 132);
    EXPECT_EQ(titleButton.right(), view.previousButtonRect().left() - 13);
}

TEST_F(CalendarViewTest, MonthNavigationDoesNotSelectDate)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    calendarView->setSelectedDate(QDate(2026, 5, 21));
    QSignalSpy activatedSpy(calendarView, &CalendarView::dateActivated);
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->nextButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(calendarView->selectedDate(), QDate(2026, 5, 21));
    EXPECT_EQ(activatedSpy.count(), 0);
}

TEST_F(CalendarViewTest, MonthNavigationStartsVerticalTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->nextButtonRect().center());

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(calendarView->property("previousVisibleMonth").toDate(), QDate(2026, 5, 1));
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 1);
    EXPECT_LT(calendarView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("contentTransitionProgress").toReal(), 1.0);
}

TEST_F(CalendarViewTest, MonthNavigationDoesNotDefaultFocusFirstDay)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2027, 1, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->nextButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2027, 2, 1));
    EXPECT_FALSE(calendarView->property("focusedDate").toDate().isValid());
    EXPECT_FALSE(calendarView->selectedDate().isValid());
}

TEST_F(CalendarViewTest, MouseMonthNavigationDoesNotShowFocusIndicator)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    const QDate today = QDate::currentDate();
    calendarView->setVisibleMonth(QDate(today.year(), today.month(), 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->nextButtonRect().center());
    processEvents();

    EXPECT_FALSE(calendarView->property("focusIndicatorVisible").toBool());
    EXPECT_FALSE(calendarView->selectedDate().isValid());
}

TEST_F(CalendarViewTest, ContentWheelStepCommitsOnePageWithTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    const QPoint wheelPoint = calendarView->gridRect().center();
    FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(calendarView, &wheel);
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_TRUE(wheel.isAccepted());
    EXPECT_EQ(calendarView->property("previousVisibleMonth").toDate(), QDate(2026, 5, 1));
    EXPECT_EQ(calendarView->property("transitionVisibleMonth").toDate(), QDate(2026, 6, 1));
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 1);
    EXPECT_LT(calendarView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 0);
}

TEST_F(CalendarViewTest, ContentWheelSubThresholdStaysIdle)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    QTest::qWait(180);
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());
    EXPECT_FALSE(calendarView->property("transitionVisibleMonth").toDate().isValid());
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(visibleMonthSpy.count(), 0);
}

TEST_F(CalendarViewTest, ContentWheelAccumulationCommitsAtThreshold)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());

    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));


    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    ASSERT_EQ(visibleMonthSpy.count(), 1);
    EXPECT_EQ(visibleMonthSpy.at(0).at(0).toDate(), QDate(2026, 6, 1));
}

TEST_F(CalendarViewTest, ContentWheelDirectionChangeResetsAccumulation)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -60));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 60));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -60));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());

    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -60));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
}

TEST_F(CalendarViewTest, ContentWheelConsumesSameClusterAfterPaging)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    const QPoint wheelPoint = calendarView->gridRect().center();
    FLUENT_MAKE_WHEEL_EVENT(firstWheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(calendarView, &firstWheel);
    processEvents();

    FLUENT_MAKE_WHEEL_EVENT(clusterTail, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(calendarView, &clusterTail);
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_TRUE(firstWheel.isAccepted());
    EXPECT_TRUE(clusterTail.isAccepted());
}

TEST_F(CalendarViewTest, NoPhaseDiscreteTouchpadBurstPagesOnceAtDayLevel)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -40));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(190);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    QTest::qWait(100);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, NoPhaseDiscreteTouchpadBurstPagesOnceAtMonthAndYearLevels)
{
    auto* monthView = new CalendarView(window);
    monthView->setGeometry(32, 32, monthView->sizeHint().width(), monthView->sizeHint().height());
    monthView->setVisibleMonth(QDate(2026, 5, 1));
    monthView->setContentLevel(CalendarView::CalendarContentLevel::Month);

    auto* yearView = new CalendarView(window);
    yearView->setGeometry(440, 32, yearView->sizeHint().width(), yearView->sizeHint().height());
    yearView->setVisibleMonth(QDate(2026, 5, 1));
    yearView->setContentLevel(CalendarView::CalendarContentLevel::Year);

    showWindow(window);
    QSignalSpy monthSpy(monthView, &CalendarView::visibleMonthChanged);
    QSignalSpy yearSpy(yearView, &CalendarView::visibleMonthChanged);

    const QPoint monthWheelPoint = monthView->contentRect().center();
    EXPECT_TRUE(sendCalendarWheel(monthView, monthWheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(monthView, monthWheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(monthView, monthWheelPoint, -40));
    EXPECT_EQ(monthView->visibleMonth(), QDate(2027, 5, 1));
    QTest::qWait(190);
    EXPECT_TRUE(sendCalendarWheel(monthView, monthWheelPoint, -30));
    QTest::qWait(100);
    EXPECT_TRUE(sendCalendarWheel(monthView, monthWheelPoint, -30));

    const QPoint yearWheelPoint = yearView->contentRect().center();
    EXPECT_TRUE(sendCalendarWheel(yearView, yearWheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(yearView, yearWheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(yearView, yearWheelPoint, -40));
    EXPECT_EQ(yearView->visibleMonth(), QDate(2038, 5, 1));
    QTest::qWait(190);
    EXPECT_TRUE(sendCalendarWheel(yearView, yearWheelPoint, -30));
    QTest::qWait(100);
    EXPECT_TRUE(sendCalendarWheel(yearView, yearWheelPoint, -30));

    EXPECT_EQ(monthView->visibleMonth(), QDate(2027, 5, 1));
    EXPECT_EQ(yearView->visibleMonth(), QDate(2038, 5, 1));
    EXPECT_EQ(monthSpy.count(), 1);
    EXPECT_EQ(yearSpy.count(), 1);
}

TEST_F(CalendarViewTest, FreshMouseWheelNotchesCanPageAfterClusterGap)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));
    QTest::qWait(330);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 7, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 2);
}

TEST_F(CalendarViewTest, FullMouseWheelNotchesCanPageAfterAnimation)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));
    QTest::qWait(270);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 7, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 2);
}

TEST_F(CalendarViewTest, SubNotchNoPhaseDiscreteTailDoesNotPageTwice)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(190);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));
    QTest::qWait(100);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, NoPhasePixelGesturePagesOnce)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(190);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    QTest::qWait(100);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -120), QPoint()));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, PhaseBasedTouchpadGesturePagesOnce)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 0, Qt::ScrollBegin));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint(), Qt::ScrollUpdate));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint(), Qt::ScrollUpdate));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    QTest::qWait(300);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -120), QPoint(), Qt::ScrollUpdate));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 0, Qt::ScrollEnd));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, OppositeDirectionTailDuringAnimationDoesNotNavigateBack)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -40));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -40));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(190);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 30));
    QTest::qWait(100);
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 30));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, FullMouseWheelReverseAfterAnimationCanPageBack)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));
    QTest::qWait(270);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 120));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 2);
}

TEST_F(CalendarViewTest, ContentWheelMomentumTailDoesNotNavigate)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120, Qt::ScrollMomentum));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
}

TEST_F(CalendarViewTest, ContentWheelEndBelowThresholdDoesNotRebound)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 0, Qt::ScrollBegin));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -30, Qt::ScrollUpdate));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 0, Qt::ScrollEnd));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());
    EXPECT_EQ(calendarView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(visibleMonthSpy.count(), 0);
}

TEST_F(CalendarViewTest, WheelDuringPageAnimationIsConsumedWithoutPaging)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->nextButtonRect().center());
    ASSERT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    ASSERT_LT(calendarView->property("monthTransitionProgress").toReal(), 1.0);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, -120));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
}

TEST_F(CalendarViewTest, WheelScrollsMonthAndYearLevelsWithPageTransition)
{
    auto* monthView = new CalendarView(window);
    monthView->setGeometry(32, 32, monthView->sizeHint().width(), monthView->sizeHint().height());
    monthView->setVisibleMonth(QDate(2026, 5, 1));
    monthView->setContentLevel(CalendarView::CalendarContentLevel::Month);

    auto* yearView = new CalendarView(window);
    yearView->setGeometry(440, 32, yearView->sizeHint().width(), yearView->sizeHint().height());
    yearView->setVisibleMonth(QDate(2026, 5, 1));
    yearView->setContentLevel(CalendarView::CalendarContentLevel::Year);

    showWindow(window);

    const QPoint monthWheelPoint = monthView->contentRect().center();
    FLUENT_MAKE_WHEEL_EVENT(monthWheel, monthWheelPoint.x(), monthWheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(monthView, &monthWheel);
    processEvents();

    EXPECT_EQ(monthView->visibleMonth(), QDate(2027, 5, 1));
    EXPECT_TRUE(monthWheel.isAccepted());
    EXPECT_EQ(monthView->property("previousVisibleMonth").toDate(), QDate(2026, 5, 1));
    EXPECT_EQ(monthView->property("transitionVisibleMonth").toDate(), QDate(2027, 5, 1));
    EXPECT_EQ(monthView->property("monthTransitionDirection").toInt(), 1);
    EXPECT_LT(monthView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(monthView->property("contentTransitionDirection").toInt(), 0);

    const QPoint yearWheelPoint = yearView->contentRect().center();
    FLUENT_MAKE_WHEEL_EVENT(yearWheel, yearWheelPoint.x(), yearWheelPoint.y(), -120, Qt::NoModifier);
    QApplication::sendEvent(yearView, &yearWheel);
    processEvents();

    EXPECT_EQ(yearView->visibleMonth(), QDate(2038, 5, 1));
    EXPECT_TRUE(yearWheel.isAccepted());
    EXPECT_EQ(yearView->property("previousVisibleMonth").toDate(), QDate(2026, 5, 1));
    EXPECT_EQ(yearView->property("transitionVisibleMonth").toDate(), QDate(2038, 5, 1));
    EXPECT_EQ(yearView->property("monthTransitionDirection").toInt(), 1);
    EXPECT_LT(yearView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(yearView->property("contentTransitionDirection").toInt(), 0);
}

TEST_F(CalendarViewTest, TitleCyclesContentLevelsWithTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->titleButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(calendarView->property("previousContentLevel").value<CalendarView::CalendarContentLevel>(), CalendarView::CalendarContentLevel::Day);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 1);
    EXPECT_LT(calendarView->property("contentTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(calendarView->property("titleText").toString(), QStringLiteral("2026"));

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->titleButtonRect().center());
    processEvents();
    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Year);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 1);
    EXPECT_EQ(calendarView->property("titleText").toString(), QStringLiteral("2016 - 2027"));

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->titleButtonRect().center());
    processEvents();
    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Day);
}

TEST_F(CalendarViewTest, LevelSwitchCancelsMonthScrollTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->nextButtonRect().center());
    ASSERT_TRUE(calendarView->property("previousVisibleMonth").toDate().isValid());

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, calendarView->titleButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("monthTransitionProgress").toReal(), 1.0);
}

TEST_F(CalendarViewTest, YearAndMonthContentsDrillDown)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    calendarView->setContentLevel(CalendarView::CalendarContentLevel::Year);
    showWindow(window);

    const QVariantMap contentCells = calendarView->property("contentCellRects").toMap();
    const QPoint year2027 = contentCells.value(QString::number(12)).toRect().center();
    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, year2027);
    processEvents();

    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), -1);
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2027, 5, 1));

    const QPoint february = contentCells.value(QString::number(2)).toRect().center();
    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier, february);
    processEvents();

    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Day);
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2027, 2, 1));
}

TEST_F(CalendarViewTest, MouseActivationUpdatesSelection)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    calendarView->setSelectedDate(QDate(2026, 5, 21));
    QSignalSpy selectedSpy(calendarView, &CalendarView::selectedDateChanged);
    QSignalSpy activatedSpy(calendarView, &CalendarView::dateActivated);
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 5, 22)));
    processEvents();

    EXPECT_EQ(calendarView->selectedDate(), QDate(2026, 5, 22));
    ASSERT_EQ(selectedSpy.count(), 1);
    ASSERT_EQ(activatedSpy.count(), 1);
    EXPECT_EQ(activatedSpy.at(0).at(0).toDate(), QDate(2026, 5, 22));
}

TEST_F(CalendarViewTest, OutOfRangeCellsAreDisabled)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setDateRange(QDate(2026, 5, 10), QDate(2026, 5, 20));
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    calendarView->setSelectedDate(QDate(2026, 5, 15));
    QSignalSpy activatedSpy(calendarView, &CalendarView::dateActivated);
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 5, 9)));
    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      cellCenterForDate(calendarView, QDate(2026, 5, 21)));
    processEvents();

    EXPECT_EQ(calendarView->selectedDate(), QDate(2026, 5, 15));
    EXPECT_EQ(activatedSpy.count(), 0);
}

TEST_F(CalendarViewTest, OutOfRangeMonthAndYearCellsDoNotDrillDown)
{
    auto* monthView = new CalendarView(window);
    monthView->setGeometry(32, 32, monthView->sizeHint().width(), monthView->sizeHint().height());
    monthView->setDateRange(QDate(2026, 5, 10), QDate(2026, 6, 20));
    monthView->setVisibleMonth(QDate(2026, 5, 1));
    monthView->setContentLevel(CalendarView::CalendarContentLevel::Month);

    auto* yearView = new CalendarView(window);
    yearView->setGeometry(440, 32, yearView->sizeHint().width(), yearView->sizeHint().height());
    yearView->setDateRange(QDate(2026, 1, 1), QDate(2027, 12, 31));
    yearView->setVisibleMonth(QDate(2026, 5, 1));
    yearView->setContentLevel(CalendarView::CalendarContentLevel::Year);

    showWindow(window);

    const QVariantMap monthCells = monthView->property("contentCellRects").toMap();
    QTest::mouseClick(monthView, Qt::LeftButton, Qt::NoModifier,
                      monthCells.value(QString::number(4)).toRect().center());
    processEvents();
    EXPECT_EQ(monthView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(monthView->visibleMonth(), QDate(2026, 5, 1));

    QTest::mouseClick(monthView, Qt::LeftButton, Qt::NoModifier,
                      monthCells.value(QString::number(6)).toRect().center());
    processEvents();
    EXPECT_EQ(monthView->contentLevel(), CalendarView::CalendarContentLevel::Day);
    EXPECT_EQ(monthView->visibleMonth(), QDate(2026, 6, 1));

    const QVariantMap yearCells = yearView->property("contentCellRects").toMap();
    QTest::mouseClick(yearView, Qt::LeftButton, Qt::NoModifier,
                      yearCells.value(QString::number(10)).toRect().center());
    processEvents();
    EXPECT_EQ(yearView->contentLevel(), CalendarView::CalendarContentLevel::Year);
    EXPECT_EQ(yearView->visibleMonth(), QDate(2026, 5, 1));

    QTest::mouseClick(yearView, Qt::LeftButton, Qt::NoModifier,
                      yearCells.value(QString::number(12)).toRect().center());
    processEvents();
    EXPECT_EQ(yearView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(yearView->visibleMonth(), QDate(2027, 5, 1));
}

TEST_F(CalendarViewTest, KeyboardNavigationActivatesFocusedDate)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    calendarView->setSelectedDate(QDate(2026, 5, 21));
    QSignalSpy activatedSpy(calendarView, &CalendarView::dateActivated);
    showWindow(window);

    calendarView->setFocus();
    QTest::keyClick(calendarView, Qt::Key_Right);
    QTest::keyClick(calendarView, Qt::Key_Return);
    processEvents();

    EXPECT_EQ(calendarView->selectedDate(), QDate(2026, 5, 22));
    ASSERT_EQ(activatedSpy.count(), 1);
    EXPECT_EQ(activatedSpy.at(0).at(0).toDate(), QDate(2026, 5, 22));
}

TEST_F(CalendarViewTest, ThemeUpdateRefreshesVisibleControl)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(), calendarView->sizeHint().height());
    showWindow(window);

    FluentElement::setTheme(FluentElement::Dark);
    processEvents();
    EXPECT_EQ(FluentElement::currentTheme(), FluentElement::Dark);
    EXPECT_TRUE(calendarView->isVisible());
}

TEST_F(CalendarViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(1120, 560);
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("CalendarView"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(title);

    auto* basic = new CalendarView(window);
    basic->setVisibleMonth(QDate(2026, 5, 1));
    basic->setSelectedDate(QDate(2026, 5, 21));
    basic->anchors()->top = {title, Edge::Bottom, 28};
    basic->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(basic);

    auto* monthLevel = new CalendarView(window);
    monthLevel->setVisibleMonth(QDate(2026, 5, 1));
    monthLevel->setSelectedDate(QDate(2026, 5, 21));
    monthLevel->setContentLevel(CalendarView::CalendarContentLevel::Month);
    monthLevel->anchors()->top = {title, Edge::Bottom, 28};
    monthLevel->anchors()->left = {basic, Edge::Right, 32};
    layout->addWidget(monthLevel);

    auto* yearLevel = new CalendarView(window);
    yearLevel->setVisibleMonth(QDate(2026, 5, 1));
    yearLevel->setSelectedDate(QDate(2026, 5, 21));
    yearLevel->setContentLevel(CalendarView::CalendarContentLevel::Year);
    yearLevel->anchors()->top = {title, Edge::Bottom, 28};
    yearLevel->anchors()->left = {monthLevel, Edge::Right, 32};
    layout->addWidget(yearLevel);

    auto* themeButton = new Button(QStringLiteral("Dark"), window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setFixedSize(96, 32);
    themeButton->anchors()->top = {title, Edge::Top, 4};
    themeButton->anchors()->right = {title, Edge::Right, 0};
    layout->addWidget(themeButton);
    QObject::connect(themeButton, &Button::clicked, themeButton, [themeButton]() {
        const bool dark = FluentElement::currentTheme() == FluentElement::Dark;
        FluentElement::setTheme(dark ? FluentElement::Light : FluentElement::Dark);
        themeButton->setText(dark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
}
