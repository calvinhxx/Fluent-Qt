#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QImage>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/basicinput/Button.h"
#include "components/date_time/CalendarView.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::date_time::CalendarView;
using fluent::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

class CalendarViewTestWindow : public QWidget, public fluent::FluentElement {
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
        qRegisterMetaType<fluent::date_time::CalendarView::CalendarContentLevel>(
            "fluent::date_time::CalendarView::CalendarContentLevel");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new CalendarViewTestWindow();
        window->resize(820, 520);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
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
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&view), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&view), nullptr);
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

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    EXPECT_TRUE(calendarView->isVisible());
}

// ─── Design-language × theme day-indicator compatibility ────────────────────
//
// CalendarView's per-day selected/today/hover indicator must paint under each design language
// (Fluent / Material 3 / macOS) crossed with each App theme (Light / Dark). Fluent is unchanged
// from the original WinUI treatment; Material 3 fills the selected day with a `primary` circle
// (on-primary glyph) + a `primary` outline ring for today + a circular state layer on hover;
// macOS fills the selected day with an accent circle + white glyph + an accent ring for today.
// Design language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: CalendarView 每日的选中/今天/悬停指示器必须在三种设计语言(Fluent/Material 3/macOS)× 两种主题
//(Light/Dark)下都能绘制。Fluent 与原 WinUI 一致;Material 3 用 primary 圆(on-primary 字)填充选中、
// primary 描边环标记今天、悬停用圆形 state layer;macOS 用 accent 圆 + 白字填充选中、accent 环标记今天。
// 设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。
class CalendarViewDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a CalendarView pinned to a fixed visible month, optionally with a selected day in that
    // month, sized like a realistic popup calendar, and grab it as an image. Animations/zoom
    // transitions don't advance inside a grab, so a static selected state is what gets painted.
    // zh_CN: 构建固定可见月份的 CalendarView(可选在该月设置选中日),设定与真实弹出日历一致的尺寸并抓取为
    // 图像。grab 内动画/缩放过渡不会推进,故绘制的是静态选中态。
    static QImage grabCalendar(bool withSelection)
    {
        CalendarView view;
        view.resize(300, 340);
        view.setVisibleMonth(QDate(2026, 5, 1));
        if (withSelection)
            view.setSelectedDate(QDate(2026, 5, 21));
        return view.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img)
    {
        if (img.isNull())
            return false;
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

TEST_F(CalendarViewDesignLanguageTest, AllLanguagesAndThemesPaintSelectedIndicator)
{
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    QImage fluentSelected; // captured to prove Material differs from Fluent. zh_CN: 留存以证明 Material 与 Fluent 不同。

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const QImage plain = grabCalendar(false);
            const QImage selected = grabCalendar(true);

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // 1. Both states paint a valid, non-empty image with content. zh_CN: 两态都绘制出有效、非空且有内容的图像。
            ASSERT_FALSE(plain.isNull()) << ctx << "/plain";
            ASSERT_FALSE(selected.isNull()) << ctx << "/selected";
            EXPECT_GT(selected.width(), 0) << ctx;
            EXPECT_GT(selected.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(plain)) << "plain calendar painted nothing: " << ctx;
            EXPECT_TRUE(hasPaintedContent(selected)) << "selected calendar painted nothing: " << ctx;

            // 2. The selected-day indicator MUST paint — a calendar WITH a selection differs from one
            // withOUT. zh_CN: 选中日指示器必须绘制——带选中与不带选中的日历必须不同。
            const int diff = differingPixels(plain, selected);
            ASSERT_GE(diff, 0) << ctx << " (size mismatch)";
            EXPECT_GT(diff, 0)
                << "selected-day indicator did not paint (selected == plain): " << ctx;

            if (lang.lang == fluent::FluentElement::DesignFluent && th.theme == fluent::FluentElement::Light)
                fluentSelected = selected;

            // 3. The Material/Cupertino branch must visibly differ from Fluent in the same theme,
            // proving the brand branch is exercised (not a silent fall-through). zh_CN: 同主题下
            // Material/Cupertino 分支必须与 Fluent 明显不同,证明品牌分支被走到(而非静默回退)。
            if (lang.lang != fluent::FluentElement::DesignFluent &&
                th.theme == fluent::FluentElement::Light && !fluentSelected.isNull()) {
                const int brandDiff = differingPixels(fluentSelected, selected);
                ASSERT_GE(brandDiff, 0) << ctx << " (size mismatch vs Fluent)";
                EXPECT_GT(brandDiff, 0)
                    << lang.name << " selected calendar is identical to Fluent (branch not exercised)";
            }
        }
    }
}

// Regression for the invalid-QColor trap (a default-constructed QColor is INVALID yet
// QColor::alpha() returns 255, so a bare alpha()>0 guard + setBrush(invalidColor) paints SOLID
// OPAQUE BLACK). The day indicator / hover fills must never render an opaque near-#000 block on a
// plain (non-text) background pixel under any design language or theme. zh_CN: 无效 QColor 陷阱回归
//(默认构造的 QColor 无效却返回 alpha==255,裸 alpha()>0 + setBrush(无效色) 会涂成不透明纯黑)。
// 日期指示器/悬停填充在任何设计语言或主题下都不得在纯背景像素上呈现不透明近黑块。
TEST_F(CalendarViewDesignLanguageTest, NoOpaqueBlackFillAtPlainBackground)
{
    const fluent::FluentElement::DesignLanguage langs[] = {
        fluent::FluentElement::DesignFluent,
        fluent::FluentElement::DesignMaterial,
        fluent::FluentElement::DesignCupertino,
    };
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    for (auto lang : langs) {
        for (auto theme : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang);
            fluent::FluentElement::setTheme(theme);

            const QImage img = grabCalendar(true);
            ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme;

            // Sample a plain background pixel inside the card but clear of the day grid / header text:
            // the top-left interior just below the rounded corner. A spurious solid-black fill would
            // bleed across the whole surface, so any near-#000 opaque sample here flags the trap.
            // zh_CN: 采样卡片内、避开日期网格/标题文字的纯背景像素:圆角下方左上内侧。一旦出现伪纯黑填充会
            // 漫过整个表面,故此处任何近黑不透明采样即触发陷阱告警。
            const QColor c = img.pixelColor(6, img.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "CalendarView painted an opaque black fill at a plain pixel: lang=" << lang
                << " theme=" << theme << " rgba=(" << c.red() << "," << c.green() << ","
                << c.blue() << "," << c.alpha() << ")";
        }
    }
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
        const bool dark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
        fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Light : fluent::FluentElement::Dark);
        themeButton->setText(dark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
}
