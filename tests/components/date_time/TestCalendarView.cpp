#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QElapsedTimer>
#include <QImage>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/date_time/CalendarView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "QtTestEnvironment.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::date_time::CalendarView;
using fluent::textfields::Label;

namespace {

using Edge = AnchorLayout::Edge;

#if QT_CONFIG(accessibility)

struct AccessibleEventRecord {
    QAccessible::Event type = QAccessible::InvalidEvent;
    QAccessibleTableModelChangeEvent::ModelChangeType modelChangeType =
        QAccessibleTableModelChangeEvent::ModelReset;
};

QVector<AccessibleEventRecord> g_accessibleEvents;

void captureAccessibleEvent(QAccessibleEvent* event)
{
    if (!event)
        return;
    AccessibleEventRecord record;
    record.type = event->type();
    if (event->type() == QAccessible::TableModelChanged) {
        record.modelChangeType =
            static_cast<QAccessibleTableModelChangeEvent*>(event)->modelChangeType();
    }
    g_accessibleEvents.append(record);
}

struct ScopedAccessibleEventCapture {
    ScopedAccessibleEventCapture()
    {
        previous = QAccessible::installUpdateHandler(captureAccessibleEvent);
        g_accessibleEvents.clear();
    }

    ~ScopedAccessibleEventCapture()
    {
        QAccessible::installUpdateHandler(previous);
        g_accessibleEvents.clear();
    }

    int count(QAccessible::Event type) const
    {
        int result = 0;
        for (const AccessibleEventRecord& record : g_accessibleEvents) {
            if (record.type == type)
                ++result;
        }
        return result;
    }

    QAccessible::UpdateHandler previous = nullptr;
};

#endif

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

bool sendCalendarWheel(CalendarView* view, const QPoint& point, const QPoint& pixelDelta,
                       const QPoint& angleDelta, Qt::ScrollPhase phase = Qt::NoScrollPhase,
                       int waitMs = 0)
{
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, point, point, pixelDelta, angleDelta, Qt::NoButton,
                                       Qt::NoModifier, phase, false);
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
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        window = new CalendarViewTestWindow();
        window->resize(820, 520);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
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

TEST_F(CalendarViewTest, Contract_AccessibilityExposesLogicalCalendarTable)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    auto* view = new CalendarView(window);
    view->setGeometry(32, 32, view->sizeHint().width(), view->sizeHint().height());
    view->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    view->setVisibleMonth(QDate(2026, 5, 1));
    view->setSelectedDate(QDate(2026, 5, 21));
    view->setAccessibleName(QStringLiteral("Release calendar"));
    view->setAccessibleDescription(QStringLiteral("Choose a release date"));
    showWindow(window);

    QAccessibleInterface* root = QAccessible::queryAccessibleInterface(view);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Table);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Release calendar"));
    EXPECT_EQ(root->text(QAccessible::Description), QStringLiteral("Choose a release date"));
    EXPECT_EQ(root->text(QAccessible::Value),
              view->locale().toString(QDate(2026, 5, 21), QLocale::LongFormat));
    EXPECT_EQ(root->childCount(), 45);

    ASSERT_NE(root->child(0), nullptr);
    ASSERT_NE(root->child(1), nullptr);
    ASSERT_NE(root->child(2), nullptr);
    EXPECT_EQ(root->child(0)->role(), QAccessible::Button);
    EXPECT_EQ(root->child(0)->text(QAccessible::Name), QStringLiteral("Previous page"));
    EXPECT_EQ(root->child(1)->text(QAccessible::Name), QStringLiteral("May 2026"));
    EXPECT_EQ(root->child(2)->text(QAccessible::Name), QStringLiteral("Next page"));

    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 6);
    EXPECT_EQ(table->columnCount(), 7);
    EXPECT_EQ(table->columnDescription(0),
              view->locale().standaloneDayName(view->firstDayOfWeek(), QLocale::LongFormat));

    const int offset = view->gridStartDate().daysTo(QDate(2026, 5, 21));
    ASSERT_GE(offset, 0);
    QAccessibleInterface* selected = table->cellAt(offset / 7, offset % 7);
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->role(), QAccessible::Cell);
    EXPECT_EQ(selected->text(QAccessible::Name),
              view->locale().toString(QDate(2026, 5, 21), QLocale::LongFormat));
    EXPECT_TRUE(selected->state().selectable);
    EXPECT_TRUE(selected->state().selected);
    ASSERT_NE(selected->tableCellInterface(), nullptr);
    EXPECT_EQ(selected->tableCellInterface()->rowIndex(), offset / 7);
    EXPECT_EQ(selected->tableCellInterface()->columnIndex(), offset % 7);
    EXPECT_EQ(table->selectedCellCount(), 1);

    view->setFocus(Qt::OtherFocusReason);
    processEvents();
    ASSERT_NE(root->focusChild(), nullptr);
    EXPECT_EQ(root->focusChild(), selected);
    EXPECT_TRUE(selected->state().focused);

    view->setVisibleMonth(QDate(2026, 6, 1));
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Release calendar"));
    EXPECT_EQ(root->text(QAccessible::Description), QStringLiteral("Choose a release date"));
#endif
}

TEST_F(CalendarViewTest, Contract_AccessibilityActionsTrackRangeAndContentLevels)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    auto* view = new CalendarView(window);
    view->setGeometry(32, 32, view->sizeHint().width(), view->sizeHint().height());
    view->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    view->setDateRange(QDate(2026, 5, 10), QDate(2026, 5, 20));
    showWindow(window);

    QAccessibleInterface* root = QAccessible::queryAccessibleInterface(view);
    ASSERT_NE(root, nullptr);
    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);

    const int disabledOffset = view->gridStartDate().daysTo(QDate(2026, 5, 9));
    QAccessibleInterface* disabled = table->cellAt(disabledOffset / 7, disabledOffset % 7);
    ASSERT_NE(disabled, nullptr);
    EXPECT_TRUE(disabled->state().disabled);
    ASSERT_NE(disabled->actionInterface(), nullptr);
    EXPECT_TRUE(disabled->actionInterface()->actionNames().isEmpty());

    const int enabledOffset = view->gridStartDate().daysTo(QDate(2026, 5, 15));
    QAccessibleInterface* enabled = table->cellAt(enabledOffset / 7, enabledOffset % 7);
    ASSERT_NE(enabled, nullptr);
    ASSERT_NE(enabled->actionInterface(), nullptr);
    EXPECT_FALSE(enabled->state().disabled);
    QSignalSpy activatedSpy(view, &CalendarView::dateActivated);
    enabled->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(view->selectedDate(), QDate(2026, 5, 15));
    EXPECT_EQ(activatedSpy.count(), 1);

    EXPECT_TRUE(root->child(0)->state().disabled);
    EXPECT_TRUE(root->child(2)->state().disabled);

    QAccessibleInterface* title = root->child(1);
    ASSERT_NE(title, nullptr);
    ASSERT_NE(title->actionInterface(), nullptr);
    title->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(view->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(table->rowCount(), 4);
    EXPECT_EQ(table->columnCount(), 3);
    EXPECT_EQ(root->childCount(), 15);

    QAccessibleInterface* january = table->cellAt(0, 0);
    QAccessibleInterface* may = table->cellAt(1, 1);
    ASSERT_NE(january, nullptr);
    ASSERT_NE(may, nullptr);
    EXPECT_EQ(january->text(QAccessible::Name), QStringLiteral("January"));
    EXPECT_TRUE(january->state().disabled);
    EXPECT_EQ(may->text(QAccessible::Name), QStringLiteral("May"));
    EXPECT_TRUE(may->state().selected);
    may->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(view->contentLevel(), CalendarView::CalendarContentLevel::Day);

    title = root->child(1);
    title->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    title->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(view->contentLevel(), CalendarView::CalendarContentLevel::Year);
    EXPECT_EQ(table->cellAt(0, 0)->text(QAccessible::Name), QStringLiteral("2016"));
    EXPECT_EQ(table->cellAt(2, 2)->text(QAccessible::Name), QStringLiteral("2024"));
#endif
}

TEST_F(CalendarViewTest, Contract_AccessibilityEventsFollowRealChangesAndNoOps)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    CalendarView view;
    view.setVisibleMonth(QDate(2026, 5, 1));
    ASSERT_NE(QAccessible::queryAccessibleInterface(&view), nullptr);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibleEventCapture capture;
    view.setSelectedDate(QDate(2026, 5, 12));
    EXPECT_EQ(capture.count(QAccessible::Selection), 1);
    EXPECT_EQ(capture.count(QAccessible::ValueChanged), 1);

    view.setSelectedDate(QDate(2026, 5, 12));
    EXPECT_EQ(capture.count(QAccessible::Selection), 1);
    EXPECT_EQ(capture.count(QAccessible::ValueChanged), 1);

    view.setVisibleMonth(QDate(2026, 6, 1));
    EXPECT_EQ(capture.count(QAccessible::TableModelChanged), 1);
    view.setVisibleMonth(QDate(2026, 6, 1));
    EXPECT_EQ(capture.count(QAccessible::TableModelChanged), 1);

    view.setContentLevel(CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(capture.count(QAccessible::TableModelChanged), 2);
    view.setContentLevel(CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(capture.count(QAccessible::TableModelChanged), 2);
    ASSERT_GE(g_accessibleEvents.size(), 4);
    EXPECT_EQ(g_accessibleEvents.last().modelChangeType,
              QAccessibleTableModelChangeEvent::ModelReset);
#endif
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

TEST_F(CalendarViewTest, DateRangeCommitsAtomically)
{
    CalendarView view;
    view.setSelectedDate(QDate(2026, 5, 15));
    QSignalSpy minSpy(&view, &CalendarView::minDateChanged);
    QSignalSpy maxSpy(&view, &CalendarView::maxDateChanged);
    QSignalSpy selectedSpy(&view, &CalendarView::selectedDateChanged);
    bool observersSawCompleteRange = true;
    QObject::connect(&view, &CalendarView::minDateChanged, &view,
                     [&view, &observersSawCompleteRange](const QDate&) {
                         observersSawCompleteRange =
                             observersSawCompleteRange && view.maxDate() == QDate(2026, 5, 20);
                     });

    view.setDateRange(QDate(2026, 5, 20), QDate(2026, 5, 10));

    EXPECT_EQ(view.minDate(), QDate(2026, 5, 20));
    EXPECT_EQ(view.maxDate(), QDate(2026, 5, 20));
    EXPECT_EQ(view.selectedDate(), QDate(2026, 5, 20));
    EXPECT_EQ(minSpy.count(), 1);
    EXPECT_EQ(maxSpy.count(), 1);
    EXPECT_EQ(selectedSpy.count(), 1);
    EXPECT_TRUE(observersSawCompleteRange);
}

TEST_F(CalendarViewTest, LocaleFollowsQWidgetAndFirstDayCanBeOverridden)
{
    CalendarView view;
    QSignalSpy localeSpy(&view, &CalendarView::localeChanged);
    const QLocale us(QLocale::English, QLocale::UnitedStates);
    const QLocale german(QLocale::German, QLocale::Germany);
    const QLocale firstLocale = view.locale() == us ? german : us;
    const QLocale secondLocale = firstLocale == us ? german : us;

    QWidget* base = &view;
    base->setLocale(firstLocale);
    EXPECT_EQ(view.locale(), firstLocale);
    EXPECT_EQ(view.firstDayOfWeek(), firstLocale.firstDayOfWeek());

    view.setFirstDayOfWeek(Qt::Thursday);
    base->setLocale(secondLocale);
    EXPECT_EQ(view.locale(), secondLocale);
    EXPECT_EQ(view.firstDayOfWeek(), Qt::Thursday);

    view.resetFirstDayOfWeek();
    EXPECT_EQ(view.firstDayOfWeek(), secondLocale.firstDayOfWeek());
    EXPECT_EQ(localeSpy.count(), 2);
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

TEST_F(CalendarViewTest, MonthAndYearIndicatorsAreCircularAndCentered)
{
    CalendarView view;
    const QVariantMap cells = view.property("contentCellRects").toMap();
    const QVariantMap indicators = view.property("contentIndicatorRects").toMap();

    ASSERT_EQ(cells.size(), 12);
    ASSERT_EQ(indicators.size(), 12);
    for (int index = 1; index <= 12; ++index) {
        const QString key = QString::number(index);
        const QRectF cell = QRectF(cells.value(key).toRect()).adjusted(8.0, 8.0, -8.0, -8.0);
        const QRectF indicator = indicators.value(key).toRectF();
        EXPECT_FALSE(indicator.isEmpty());
        EXPECT_NEAR(indicator.width(), indicator.height(), 0.01);
        EXPECT_NEAR(indicator.center().x(), cell.center().x(), 0.5);
        EXPECT_NEAR(indicator.center().y(), cell.center().y(), 0.5);
        EXPECT_TRUE(cell.contains(indicator));
        EXPECT_LT(indicator.width(), cell.width());
    }
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
    EXPECT_EQ(todaySelectedStates.value(today.toString(Qt::ISODate)).toString(),
              QStringLiteral("current"));
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    calendarView->setSelectedDate(QDate(2026, 5, 21));
    QSignalSpy activatedSpy(calendarView, &CalendarView::dateActivated);
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(calendarView->selectedDate(), QDate(2026, 5, 21));
    EXPECT_EQ(activatedSpy.count(), 0);
}

TEST_F(CalendarViewTest, MonthNavigationStartsVerticalTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(calendarView->property("previousVisibleMonth").toDate(), QDate(2026, 5, 1));
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 1);
    EXPECT_LT(calendarView->property("monthTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("contentTransitionProgress").toReal(), 1.0);
}

TEST_F(CalendarViewTest, MotionPolicyDisabledSettlesMonthTransitionSynchronously)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());
    EXPECT_FALSE(calendarView->property("transitionVisibleMonth").toDate().isValid());
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("monthTransitionProgress").toReal(), 1.0);
}

TEST_F(CalendarViewTest, MonthNavigationDoesNotDefaultFocusFirstDay)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2027, 1, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2027, 2, 1));
    EXPECT_FALSE(calendarView->property("focusedDate").toDate().isValid());
    EXPECT_FALSE(calendarView->selectedDate().isValid());
}

TEST_F(CalendarViewTest, MouseMonthNavigationDoesNotShowFocusIndicator)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    const QDate today = QDate::currentDate();
    calendarView->setVisibleMonth(QDate(today.year(), today.month(), 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());
    processEvents();

    EXPECT_FALSE(calendarView->property("focusIndicatorVisible").toBool());
    EXPECT_FALSE(calendarView->selectedDate().isValid());
}

TEST_F(CalendarViewTest, ContentWheelStepCommitsOnePageWithTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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

TEST_F(CalendarViewTest, NoPhasePixelSameDirectionTailAfterAnimationUsesExtendedGap)
{
    constexpr int kDefaultClusterGapMs = 120;
    constexpr int kCommittedTailGapMs = 220;
    constexpr int kProbeGapMs = kDefaultClusterGapMs + 10;
    constexpr int kProbeHeadroomMs = 10;

    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(190);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));

    QElapsedTimer probeTimer;
    probeTimer.start();
    QTest::qWait(kProbeGapMs);
    processEvents();
    const qint64 actualProbeGapMs = probeTimer.elapsed();
    ASSERT_GT(actualProbeGapMs, kDefaultClusterGapMs);
    if (actualProbeGapMs >= kCommittedTailGapMs - kProbeHeadroomMs) {
        GTEST_SKIP() << "Runner overslept the committed-tail probe window: " << actualProbeGapMs
                     << " ms";
    }
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -120), QPoint()));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, NoPhasePixelCanPageAfterTailGap)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(360);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -120), QPoint()));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 7, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 2);
}

TEST_F(CalendarViewTest, NoPhasePixelCanReverseAfterTailGap)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(360);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, 60), QPoint()));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, 60), QPoint()));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 2);
}

TEST_F(CalendarViewTest, NoPhasePixelReverseTailDuringAnimationDoesNotPoisonReversePage)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint()));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(190);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, 30), QPoint()));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));

    QTest::qWait(150);
    processEvents();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, QPoint(0, 120), QPoint()));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 5, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 2);
}

TEST_F(CalendarViewTest, PhaseBasedTouchpadGesturePagesOnce)
{
    if (!fluentWheelEventSupportsPhase())
        GTEST_SKIP() << fluentWheelEventPhaseSkipReason();

    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);
    QSignalSpy visibleMonthSpy(calendarView, &CalendarView::visibleMonthChanged);

    const QPoint wheelPoint = calendarView->gridRect().center();
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 0, Qt::ScrollBegin));
    EXPECT_TRUE(
        sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint(), Qt::ScrollUpdate));
    EXPECT_TRUE(
        sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -60), QPoint(), Qt::ScrollUpdate));
    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    QTest::qWait(300);
    EXPECT_TRUE(
        sendCalendarWheel(calendarView, wheelPoint, QPoint(0, -120), QPoint(), Qt::ScrollUpdate));
    EXPECT_TRUE(sendCalendarWheel(calendarView, wheelPoint, 0, Qt::ScrollEnd));

    EXPECT_EQ(calendarView->visibleMonth(), QDate(2026, 6, 1));
    EXPECT_EQ(visibleMonthSpy.count(), 1);
}

TEST_F(CalendarViewTest, OppositeDirectionTailDuringAnimationDoesNotNavigateBack)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());
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
    FLUENT_MAKE_WHEEL_EVENT(monthWheel, monthWheelPoint.x(), monthWheelPoint.y(), -120,
                            Qt::NoModifier);
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
    FLUENT_MAKE_WHEEL_EVENT(yearWheel, yearWheelPoint.x(), yearWheelPoint.y(), -120,
                            Qt::NoModifier);
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->titleButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_EQ(
        calendarView->property("previousContentLevel").value<CalendarView::CalendarContentLevel>(),
        CalendarView::CalendarContentLevel::Day);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 1);
    EXPECT_LT(calendarView->property("contentTransitionProgress").toReal(), 1.0);
    EXPECT_EQ(calendarView->property("titleText").toString(), QStringLiteral("2026"));

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->titleButtonRect().center());
    processEvents();
    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Year);
    EXPECT_EQ(calendarView->property("contentTransitionDirection").toInt(), 1);
    EXPECT_EQ(calendarView->property("titleText").toString(), QStringLiteral("2016 - 2027"));

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->titleButtonRect().center());
    processEvents();
    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Day);
}

TEST_F(CalendarViewTest, LevelSwitchCancelsMonthScrollTransition)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    calendarView->setVisibleMonth(QDate(2026, 5, 1));
    showWindow(window);

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->nextButtonRect().center());
    ASSERT_TRUE(calendarView->property("previousVisibleMonth").toDate().isValid());

    QTest::mouseClick(calendarView, Qt::LeftButton, Qt::NoModifier,
                      calendarView->titleButtonRect().center());
    processEvents();

    EXPECT_EQ(calendarView->contentLevel(), CalendarView::CalendarContentLevel::Month);
    EXPECT_FALSE(calendarView->property("previousVisibleMonth").toDate().isValid());
    EXPECT_EQ(calendarView->property("monthTransitionDirection").toInt(), 0);
    EXPECT_EQ(calendarView->property("monthTransitionProgress").toReal(), 1.0);
}

TEST_F(CalendarViewTest, YearAndMonthContentsDrillDown)
{
    auto* calendarView = new CalendarView(window);
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
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
    calendarView->setGeometry(32, 32, calendarView->sizeHint().width(),
                              calendarView->sizeHint().height());
    showWindow(window);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
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
    title->setFluentTypography(Typography::FontRole::Title);
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
        fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Light
                                             : fluent::FluentElement::Dark);
        themeButton->setText(dark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    });

    window->onThemeUpdated();
    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        tests::support::VisualSnapshotOptions light;
        light.windowSize = QSize(1120, 560);
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
