#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QBoxLayout>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QTimeZone>
#include <type_traits>

#include <FluentQt/FluentQt.h>

using fluent::date_time::DateTimeRangePicker;
using fluent::date_time::TimePicker;

namespace {
QDateTime at(int day, int hour = 0, int minute = 0)
{
    return QDateTime::fromString(QStringLiteral("2025-10-%1T%2:%3:00Z")
                                     .arg(day, 2, 10, QLatin1Char('0'))
                                     .arg(hour, 2, 10, QLatin1Char('0'))
                                     .arg(minute, 2, 10, QLatin1Char('0')),
                                 Qt::ISODate);
}

void settle()
{
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QApplication::processEvents();
    QApplication::processEvents();
}

bool isHitTarget(QWidget* host, QWidget* target, const QPoint& position)
{
    auto* hit = host->childAt(target->mapTo(host, position));
    return hit && (hit == target || target->isAncestorOf(hit));
}

void clickVisible(QWidget* host, QWidget* target, const QPoint& position)
{
    ASSERT_TRUE(isHitTarget(host, target, position)) << target->objectName().toStdString();
    QTest::mouseClick(target, Qt::LeftButton, Qt::NoModifier, position);
    settle();
}

qreal dateContentCenterY(QWidget* input)
{
    // Render at 2x so even a half-logical-pixel press nudge is observable.
    // zh_CN: 以 2 倍比例绘制，检测按下时半个逻辑像素的文字位移。
    QImage image(input->size() * 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    input->render(&painter);
    painter.end();
    const QColor background = image.pixelColor(8, image.height() / 2);
    qreal weightedY = 0;
    qreal totalWeight = 0;
    // Exclude the border and the independently interactive trailing clear action.
    // zh_CN: 排除边框以及独立交互的尾部清除按钮。
    for (int y = 8; y < image.height() - 8; ++y) {
        for (int x = 16; x < image.width() - 72; ++x) {
            const QColor pixel = image.pixelColor(x, y);
            const int weight = qAbs(pixel.red() - background.red()) +
                               qAbs(pixel.green() - background.green()) +
                               qAbs(pixel.blue() - background.blue());
            weightedY += y * weight;
            totalWeight += weight;
        }
    }
    return totalWeight > 0 ? weightedY / totalWeight / 2 : -1;
}
} // namespace

TEST(DateTimeRangePickerTest, Contract_DefaultsAndOwnedInputs)
{
    static_assert(std::is_base_of<QWidget, DateTimeRangePicker>::value);
    static_assert(std::is_base_of<fluent::FluentElement, DateTimeRangePicker>::value);
    static_assert(std::is_base_of<fluent::QMLPlus, DateTimeRangePicker>::value);
    DateTimeRangePicker picker;
    EXPECT_FALSE(picker.startDateTime().isValid());
    EXPECT_FALSE(picker.endDateTime().isValid());
    EXPECT_TRUE(picker.isRangeValid());
    EXPECT_TRUE(picker.isTimeEnabled());
    EXPECT_TRUE(picker.startLabelText().isEmpty());
    EXPECT_TRUE(picker.endLabelText().isEmpty());
    EXPECT_TRUE(picker.isAncestorOf(picker.startDatePicker()));
    EXPECT_TRUE(picker.isAncestorOf(picker.endTimePicker()));
    EXPECT_FALSE(picker.startTimePicker()->isEnabled());
    EXPECT_FALSE(picker.endClearButton()->isEnabled());
    EXPECT_EQ(picker.focusProxy(), picker.entryButton());
    EXPECT_FALSE(picker.isPickerOpen());
    EXPECT_TRUE(picker.isPresetsVisible());
    EXPECT_EQ(picker.displayText(), picker.placeholderText());
}

TEST(DateTimeRangePickerTest, Contract_AtomicMinutePrecisionAndNoOpSignals)
{
    DateTimeRangePicker picker;
    QSignalSpy start(&picker, &DateTimeRangePicker::startDateTimeChanged);
    QSignalSpy end(&picker, &DateTimeRangePicker::endDateTimeChanged);
    QSignalSpy range(&picker, &DateTimeRangePicker::rangeChanged);
    QObject::connect(&picker, &DateTimeRangePicker::startDateTimeChanged, &picker,
                     [&](const QDateTime&) { EXPECT_EQ(picker.endDateTime(), at(3, 15, 10)); });
    auto beginning = at(1, 10, 10);
    beginning.setTime(QTime(10, 10, 58, 900));
    picker.setRange(beginning, at(3, 15, 10));
    EXPECT_EQ(picker.startDateTime(), at(1, 10, 10));
    EXPECT_EQ(picker.startDateTime().timeSpec(), Qt::UTC);
    EXPECT_EQ(picker.startTimePicker()->selectedTime(), QTime(10, 10));
    EXPECT_EQ(start.count(), 1);
    EXPECT_EQ(end.count(), 1);
    EXPECT_EQ(range.count(), 1);
    picker.setRange(beginning, at(3, 15, 10));
    EXPECT_EQ(range.count(), 1);
}

TEST(DateTimeRangePickerTest, Contract_IndependentOpenEndpointsAndClear)
{
    DateTimeRangePicker picker;
    QSignalSpy range(&picker, &DateTimeRangePicker::rangeChanged);
    picker.setStartDateTime(at(1));
    EXPECT_TRUE(picker.isRangeValid());
    EXPECT_FALSE(picker.endDateTime().isValid());
    picker.setEndDateTime(at(3));
    picker.clearStart();
    EXPECT_EQ(picker.endDateTime(), at(3));
    EXPECT_FALSE(picker.startDateTime().isValid());
    EXPECT_FALSE(picker.startDatePicker()->date().isValid());
    EXPECT_FALSE(picker.startTimePicker()->selectedTime().isValid());
    picker.clearEnd();
    EXPECT_TRUE(picker.isRangeValid());
    EXPECT_EQ(range.count(), 4);
    picker.clear();
    EXPECT_EQ(range.count(), 4);
    picker.setRange(at(1), at(3));
    picker.clear();
    EXPECT_EQ(range.count(), 6);
}

TEST(DateTimeRangePickerTest, Contract_ReversedRangePreservesValuesAndReportsValidity)
{
    DateTimeRangePicker picker;
    QSignalSpy validity(&picker, &DateTimeRangePicker::rangeValidityChanged);
    picker.setRange(at(3), at(1));
    EXPECT_FALSE(picker.isRangeValid());
    EXPECT_EQ(picker.startDateTime(), at(3));
    EXPECT_EQ(picker.endDateTime(), at(1));
    EXPECT_EQ(validity.count(), 1);
    picker.setStartDateTime(at(4));
    EXPECT_EQ(validity.count(), 1);
    picker.clearEnd();
    EXPECT_TRUE(picker.isRangeValid());
    EXPECT_EQ(validity.count(), 2);
    picker.setRange(at(3), at(3));
    EXPECT_TRUE(picker.isRangeValid());
}

TEST(DateTimeRangePickerTest, Contract_DateOnlyMidnightAndModeSignals)
{
    DateTimeRangePicker picker;
    picker.setRange(at(1, 15, 10), at(1, 10, 10));
    QSignalSpy mode(&picker, &DateTimeRangePicker::timeEnabledChanged);
    picker.setTimeEnabled(false);
    EXPECT_EQ(picker.startDateTime(), at(1));
    EXPECT_EQ(picker.endDateTime(), at(1));
    EXPECT_TRUE(picker.isRangeValid());
    EXPECT_TRUE(picker.startTimePicker()->isHidden());
    picker.setTimeEnabled(false);
    EXPECT_EQ(mode.count(), 1);
    picker.setTimeEnabled(true);
    EXPECT_FALSE(picker.startTimePicker()->isHidden());
    EXPECT_EQ(picker.startDateTime(), at(1));
}

TEST(DateTimeRangePickerTest, Contract_EditingDateAndTimeAndPreservingTimeZone)
{
    DateTimeRangePicker picker;
    picker.startDatePicker()->setDate(QDate(2025, 10, 1));
    EXPECT_EQ(picker.startDateTime().time(), QTime(0, 0));
    EXPECT_TRUE(picker.startTimePicker()->isEnabled());
    picker.startTimePicker()->setSelectedTime(QTime(10, 10));
    EXPECT_EQ(picker.startDateTime().time(), QTime(10, 10));
    const QTimeZone zone(5 * 3600 + 30 * 60);
    picker.setStartDateTime(QDateTime(QDate(2025, 10, 1), QTime(10, 10), zone));
    picker.startDatePicker()->setDate(QDate(2025, 10, 3));
    EXPECT_EQ(picker.startDateTime().timeZone(), zone);
    EXPECT_EQ(picker.startDateTime().time(), QTime(10, 10));
    picker.startDatePicker()->clearDate();
    picker.startTimePicker()->setSelectedTime(QTime(9, 30));
    EXPECT_FALSE(picker.startDateTime().isValid());
    EXPECT_FALSE(picker.startTimePicker()->selectedTime().isValid());
}

TEST(DateTimeRangePickerTest, Contract_InputBoundsAndMinuteStepStayAligned)
{
    DateTimeRangePicker picker;
    picker.startDatePicker()->setDateRange(QDate(2025, 10, 2), QDate(2025, 10, 5));
    picker.startTimePicker()->setMinuteIncrement(5);
    picker.setRange(at(1, 9, 33), at(8));
    EXPECT_EQ(picker.startDateTime(), at(2, 9, 35));
    EXPECT_EQ(picker.startDatePicker()->date(), QDate(2025, 10, 2));
    EXPECT_EQ(picker.startTimePicker()->selectedTime(), QTime(9, 35));
    picker.startTimePicker()->setMinuteIncrement(10);
    EXPECT_EQ(picker.startDateTime(), at(2, 9, 40));
    picker.startDatePicker()->setMinDate(QDate(2025, 10, 4));
    EXPECT_EQ(picker.startDateTime(), at(4, 9, 40));
}

TEST(DateTimeRangePickerTest, Contract_ReentrantChangeAndDeletion)
{
    DateTimeRangePicker picker;
    QSignalSpy ranges(&picker, &DateTimeRangePicker::rangeChanged);
    QObject::connect(&picker, &DateTimeRangePicker::startDateTimeChanged, &picker, [&] {
        if (picker.startDateTime().isValid())
            picker.clearStart();
    });
    picker.setRange(at(1), at(3));
    EXPECT_FALSE(picker.startDateTime().isValid());
    EXPECT_EQ(picker.endDateTime(), at(3));
    ASSERT_EQ(ranges.count(), 1);
    EXPECT_FALSE(ranges.at(0).at(0).toDateTime().isValid());
    auto* disposable = new DateTimeRangePicker;
    QObject::connect(disposable, &DateTimeRangePicker::startDateTimeChanged, disposable,
                     [disposable] { delete disposable; });
    disposable->setRange(at(1), at(3));
}

TEST(DateTimeRangePickerTest, Contract_ResponsiveAndRtlLayout)
{
    QWidget host;
    host.resize(900, 720);
    DateTimeRangePicker picker(&host);
    picker.setStartLabelText(QStringLiteral("Start"));
    picker.setEndLabelText(QStringLiteral("End"));
    picker.setRange(at(12, 10, 10), at(16, 15, 10));
    picker.resize(480, picker.sizeHint().height());
    host.show();
    picker.openPicker();
    settle();
    auto* popup = host.findChild<fluent::dialogs_flyouts::Flyout*>(
        QStringLiteral("DateTimeRangePickerPopup"));
    ASSERT_NE(popup, nullptr);
    EXPECT_TRUE(picker.isPickerOpen());
    const QRect card(popup->mapTo(&host, popup->contentsRect().topLeft()),
                     popup->contentsRect().size());
    EXPECT_TRUE(host.rect().contains(card));
    picker.setTimeEnabled(false);
    picker.setRange(at(20), at(5));
    settle();
    picker.setRange(at(5), at(20));
    settle();
    auto* inputs = host.findChild<QWidget*>(QStringLiteral("rangeEndpointInputs"));
    ASSERT_NE(inputs, nullptr);
    EXPECT_LE(inputs->height(), inputs->sizeHint().height() + 1);
    picker.setTimeEnabled(true);
    picker.closePicker();
    host.setLayoutDirection(Qt::RightToLeft);
    host.resize(380, 680);
    picker.resize(300, picker.sizeHint().height());
    picker.openPicker();
    settle();
    const QRect narrowCard(popup->mapTo(&host, popup->contentsRect().topLeft()),
                           popup->contentsRect().size());
    EXPECT_TRUE(host.rect().contains(narrowCard))
        << narrowCard.x() << "," << narrowCard.y() << " " << narrowCard.width() << "x"
        << narrowCard.height() << " host " << host.width() << "x" << host.height();
    EXPECT_EQ(picker.calendarView()->layoutDirection(), Qt::RightToLeft);
    EXPECT_TRUE(picker.calendarView()->isVisible());
    host.resize(320, 600);
    settle();
    EXPECT_TRUE(host.rect().contains(
        QRect(popup->mapTo(&host, popup->contentsRect().topLeft()), popup->contentsRect().size())));
    EXPECT_LT(picker.height(), 100);
    EXPECT_TRUE(picker.rect().contains(
        QRect(picker.entryButton()->mapTo(&picker, QPoint()), picker.entryButton()->size())));
}

TEST(DateTimeRangePickerTest, Contract_EndpointAlignmentAndSharedValidation)
{
    QWidget host;
    host.resize(900, 740);
    DateTimeRangePicker picker(&host);
    picker.setStartLabelText(QStringLiteral("Start"));
    picker.setEndLabelText(QStringLiteral("End"));
    picker.setRange(at(12, 10, 10), at(16, 15, 10));
    host.show();
    picker.openPicker();
    settle();
    auto* inputs = host.findChild<QWidget*>(QStringLiteral("rangeEndpointInputs"));
    auto* field = host.findChild<fluent::layout::Field*>(QStringLiteral("rangeEndpointField"));
    ASSERT_NE(inputs, nullptr);
    ASSERT_NE(field, nullptr);
    const auto rectInInputs = [inputs](QWidget* child) {
        return QRect(child->mapTo(inputs, QPoint()), child->size());
    };
    for (bool withTime : {true, false}) {
        SCOPED_TRACE(withTime);
        picker.setTimeEnabled(withTime);
        picker.setRange(at(12), at(16));
        settle();
        const QRect start = rectInInputs(picker.startDatePicker());
        const QRect end = rectInInputs(picker.endDatePicker());
        EXPECT_EQ(start.top(), end.top());
        EXPECT_LE(qAbs(start.width() - end.width()), 1);
        if (withTime) {
            EXPECT_EQ(picker.startDatePicker()->size(), picker.startTimePicker()->size());
            EXPECT_EQ(picker.endDatePicker()->size(), picker.endTimePicker()->size());
        }
        picker.setEndDateTime(at(5));
        settle();
        EXPECT_EQ(rectInInputs(picker.startDatePicker()), start);
        EXPECT_EQ(rectInInputs(picker.endDatePicker()), end);
        EXPECT_EQ(field->validationState(), fluent::layout::Field::ValidationState::Error);
        EXPECT_EQ(field->validationMessage(), picker.invalidRangeText());
        auto* status = field->findChild<QWidget*>(QStringLiteral("fluentFieldStatusRow"));
        ASSERT_NE(status, nullptr);
        EXPECT_TRUE(status->isVisible());
        EXPECT_GT(status->mapTo(field, QPoint()).y(),
                  inputs->mapTo(field, inputs->rect().bottomLeft()).y());
        EXPECT_EQ(status->width(), inputs->width());
        picker.setEndDateTime(at(16));
        settle();
        EXPECT_FALSE(status->isVisible());
        EXPECT_EQ(rectInInputs(picker.startDatePicker()), start);
    }
    picker.setInvalidRangeText(QStringLiteral(
        "Choose an end date on or after the start date. You can also clear either bound."));
    picker.setEndDateTime(at(5));
    settle();
    auto* message =
        field->findChild<fluent::textfields::Label*>(QStringLiteral("fluentFieldStatus"));
    ASSERT_NE(message, nullptr);
    EXPECT_GE(message->height(), message->heightForWidth(message->width()));
    EXPECT_TRUE(field->rect().contains(QRect(message->mapTo(field, QPoint()), message->size())));
}

TEST(DateTimeRangePickerTest, Contract_EmbeddedClearActionAndEmptyPresentation)
{
    QWidget host;
    host.resize(900, 720);
    DateTimeRangePicker picker(&host);
    picker.setRange(at(1, 10, 10), at(3, 15, 10));
    host.show();
    picker.openPicker();
    settle();
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        host.setLayoutDirection(direction);
        picker.setRange(at(1, 10, 10), at(3, 15, 10));
        settle();
        auto* date = picker.startDatePicker();
        auto* clear = picker.startClearButton();
        const QRect clearRect(clear->mapTo(date, QPoint()), clear->size());
        EXPECT_TRUE(date->rect().contains(clearRect));
        EXPECT_EQ(clear->width(), 28);
        EXPECT_EQ(clear->height(), 28);
        EXPECT_EQ(clearRect.center().y(), date->rect().center().y());
        if (direction == Qt::LeftToRight)
            EXPECT_GT(clearRect.left(), date->width() / 2);
        else
            EXPECT_LT(clearRect.right(), date->width() / 2);
#if QT_CONFIG(accessibility)
        auto* accessible = QAccessible::queryAccessibleInterface(date);
        auto* accessibleClear = QAccessible::queryAccessibleInterface(clear);
        ASSERT_NE(accessible, nullptr);
        ASSERT_NE(accessibleClear, nullptr);
        EXPECT_GE(accessible->indexOfChild(accessibleClear), 0);
#endif
        clickVisible(&host, date, date->rect().center());
        EXPECT_TRUE(date->isCalendarOpen());
        EXPECT_EQ(date->fluentStyle(), fluent::basicinput::Button::Standard);
        clickVisible(&host, clear, clear->rect().center());
        EXPECT_FALSE(picker.startDateTime().isValid());
        EXPECT_EQ(picker.endDateTime(), at(3, 15, 10));
        EXPECT_TRUE(picker.isPickerOpen());
        EXPECT_FALSE(date->isCalendarOpen());
        EXPECT_EQ(date->fluentStyle(), fluent::basicinput::Button::Standard);
        EXPECT_TRUE(clear->isHidden());
        EXPECT_FALSE(picker.startTimePicker()->isEnabled());
        EXPECT_EQ(picker.startDatePicker()->width(), picker.startTimePicker()->width());
    }
}

TEST(DateTimeRangePickerTest, Contract_PopupExclusivityAndCloseOnHideDisable)
{
    QWidget host;
    host.resize(900, 720);
    DateTimeRangePicker picker(&host);
    picker.setRange(at(1), at(3));
    picker.resize(picker.sizeHint());
    host.show();
    picker.openPicker();
    settle();
    picker.startDatePicker()->openCalendar();
    EXPECT_TRUE(picker.startDatePicker()->isOpen());
    EXPECT_TRUE(picker.isPickerOpen());
    picker.endDatePicker()->openCalendar();
    EXPECT_FALSE(picker.startDatePicker()->isOpen());
    picker.endTimePicker()->openPicker();
    EXPECT_FALSE(picker.endDatePicker()->isOpen());
    EXPECT_TRUE(picker.endTimePicker()->isOpen());
    settle();
    auto* timeFlyout =
        host.findChild<fluent::dialogs_flyouts::Flyout*>(QStringLiteral("TimePickerFlyout"));
    ASSERT_NE(timeFlyout, nullptr);
    QTest::mouseClick(timeFlyout, Qt::LeftButton, Qt::NoModifier,
                      timeFlyout->contentsRect().bottomRight() - QPoint(2, 2));
    EXPECT_TRUE(picker.isPickerOpen());
    EXPECT_TRUE(picker.endTimePicker()->isOpen());
    picker.setTimeEnabled(false);
    EXPECT_FALSE(picker.endTimePicker()->isOpen());
    picker.startDatePicker()->openCalendar();
    picker.hide();
    EXPECT_FALSE(picker.isPickerOpen());
    EXPECT_FALSE(picker.startDatePicker()->isOpen());
    picker.show();
    picker.openPicker();
    picker.endDatePicker()->openCalendar();
    picker.setEnabled(false);
    EXPECT_FALSE(picker.isPickerOpen());
    EXPECT_FALSE(picker.endDatePicker()->isOpen());
}

TEST(DateTimeRangePickerTest, Contract_LabelsAndLocaleNoOps)
{
    DateTimeRangePicker picker;
    QSignalSpy labels(&picker, &DateTimeRangePicker::startLabelTextChanged);
    picker.setStartLabelText(QStringLiteral("Since"));
    picker.setStartLabelText(QStringLiteral("Since"));
    EXPECT_EQ(labels.count(), 1);
    QSignalSpy locales(&picker, &DateTimeRangePicker::localeChanged);
    const QLocale target(QLocale::German, QLocale::Germany);
    picker.setLocale(target);
    picker.setLocale(target);
    EXPECT_EQ(locales.count(), 1);
    EXPECT_EQ(picker.startDatePicker()->locale(), target);
    EXPECT_EQ(picker.endTimePicker()->locale(), target);
}

TEST(DateTimeRangePickerTest, Contract_AccessibilityRangeValuesAndActions)
{
#if QT_CONFIG(accessibility)
    DateTimeRangePicker picker;
    picker.setAccessibleName(QStringLiteral("Order time filter"));
    picker.setStartLabelText(QStringLiteral("Since"));
    auto* root = QAccessible::queryAccessibleInterface(&picker);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ComboBox);
    EXPECT_TRUE(root->state().hasPopup);
    EXPECT_TRUE(root->state().collapsed);
    EXPECT_TRUE(root->actionInterface()->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Order time filter"));
    EXPECT_TRUE(root->text(QAccessible::Value).contains(QStringLiteral("Unbounded")));
    EXPECT_GT(root->childCount(), 0);
    picker.setRange(at(3), at(1));
    EXPECT_EQ(root->text(QAccessible::Description), picker.invalidRangeText());
    EXPECT_FALSE(root->state().invalid);
    picker.clearEnd();
    EXPECT_TRUE(root->text(QAccessible::Description).isEmpty());
    EXPECT_TRUE(root->text(QAccessible::Value).contains(QStringLiteral("Since")));
    auto* clear = QAccessible::queryAccessibleInterface(picker.startClearButton());
    ASSERT_NE(clear, nullptr);
    ASSERT_NE(clear->actionInterface(), nullptr);
    EXPECT_FALSE(clear->text(QAccessible::Name).isEmpty());
    QSignalSpy cleared(&picker, &DateTimeRangePicker::startDateTimeChanged);
    clear->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    if (cleared.isEmpty())
        EXPECT_TRUE(cleared.wait(1000));
    EXPECT_FALSE(picker.startDateTime().isValid());
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Order time filter"));
#endif
}

TEST(DateTimeRangePickerTest, Contract_TwoClickRangeAndAccessibleCalendar)
{
    QWidget host;
    host.resize(900, 740);
    DateTimeRangePicker picker(&host);
    picker.setTimeEnabled(false);
    picker.setRange(at(1), at(3));
    picker.resize(picker.sizeHint());
    host.show();
    picker.openPicker();
    picker.clear();
    auto* calendar = picker.calendarView();
    settle();
    QTest::mouseClick(calendar, Qt::LeftButton, Qt::NoModifier,
                      calendar->dateCellRect(QDate(2025, 10, 16)).center());
    EXPECT_EQ(picker.startDateTime().date(), QDate(2025, 10, 16));
    EXPECT_FALSE(picker.endDateTime().isValid());
    EXPECT_TRUE(picker.isPickerOpen());
    QTest::mouseClick(calendar, Qt::LeftButton, Qt::NoModifier,
                      calendar->dateCellRect(QDate(2025, 10, 12)).center());
    EXPECT_EQ(picker.startDateTime().date(), QDate(2025, 10, 12));
    EXPECT_EQ(picker.endDateTime().date(), QDate(2025, 10, 16));
    EXPECT_TRUE(picker.isPickerOpen());
    const auto states = calendar->property("dateVisualStates").toMap();
    EXPECT_EQ(states.value(QStringLiteral("2025-10-12")).toString(), QStringLiteral("range-start"));
    EXPECT_EQ(states.value(QStringLiteral("2025-10-14")).toString(),
              QStringLiteral("range-interior"));
    EXPECT_EQ(states.value(QStringLiteral("2025-10-16")).toString(), QStringLiteral("range-end"));
#if QT_CONFIG(accessibility)
    auto* accessible = QAccessible::queryAccessibleInterface(calendar);
    ASSERT_NE(accessible, nullptr);
    ASSERT_NE(accessible->tableInterface(), nullptr);
    EXPECT_TRUE(accessible->state().multiSelectable);
    EXPECT_EQ(accessible->tableInterface()->selectedCellCount(), 5);
#endif
    auto* preset = host.findChild<fluent::basicinput::Button*>(QStringLiteral("rangePreset2"));
    ASSERT_NE(preset, nullptr);
    clickVisible(&host, preset, preset->rect().center());
    EXPECT_TRUE(picker.isPickerOpen());
    EXPECT_EQ(picker.startDateTime().date().daysTo(picker.endDateTime().date()), 6);
    auto* done = host.findChild<fluent::basicinput::Button*>(QStringLiteral("rangeDone"));
    ASSERT_NE(done, nullptr);
    clickVisible(&host, done, done->rect().center());
    EXPECT_FALSE(picker.isPickerOpen());
}

TEST(DateTimeRangePickerTest, Contract_CompactEndpointFlyouts)
{
    QWidget host;
    host.resize(1000, 740);
    DateTimeRangePicker picker(&host);
    picker.setStartLabelText(QStringLiteral("Start"));
    picker.setEndLabelText(QStringLiteral("End"));
    picker.setRange(at(1, 10, 10), at(3, 15, 10));
    picker.setGeometry(40, 40, 600, picker.sizeHint().height());
    host.show();
    settle();
    clickVisible(&host, picker.entryButton(), picker.entryButton()->rect().center());
    for (auto* input : {picker.startDatePicker(), picker.endDatePicker()}) {
        clickVisible(&host, input, input->rect().center());
        ASSERT_TRUE(input->isCalendarOpen());
        EXPECT_FALSE(input->isCheckable());
        EXPECT_TRUE(picker.isPickerOpen());
        auto* child = input->calendarView();
        ASSERT_NE(child, nullptr);
        EXPECT_LT(child->width(), picker.calendarView()->width());
        EXPECT_LT(child->height(), 300);
        EXPECT_GE(child->dateCellRect(QDate(2025, 10, 2)).height(), 28);
        EXPECT_TRUE(isHitTarget(&host, child, child->dateCellRect(QDate(2025, 10, 2)).center()));
    }
    EXPECT_FALSE(picker.startDatePicker()->isCalendarOpen());
    clickVisible(&host, picker.endDatePicker(), picker.endDatePicker()->rect().center());
    EXPECT_FALSE(picker.endDatePicker()->isCalendarOpen());
    EXPECT_FALSE(picker.endDatePicker()->isChecked());
    QTest::keyClick(picker.endDatePicker(), Qt::Key_Return);
    auto* end = picker.endDatePicker()->calendarView();
    clickVisible(&host, end, end->dateCellRect(QDate(2025, 10, 5)).center());
    EXPECT_EQ(picker.startDateTime(), at(1, 10, 10));
    EXPECT_EQ(picker.endDateTime(), at(5, 15, 10));
    EXPECT_FALSE(picker.endDatePicker()->isCalendarOpen());
    EXPECT_EQ(picker.endDatePicker()->fluentStyle(), fluent::basicinput::Button::Standard);
    EXPECT_TRUE(picker.isPickerOpen());
    picker.startDatePicker()->setDateRange(QDate(2025, 10, 2), QDate(2025, 10, 20));
    clickVisible(&host, picker.startDatePicker(), picker.startDatePicker()->rect().center());
    auto* start = picker.startDatePicker()->calendarView();
    EXPECT_FALSE(start->isDateSelectable(QDate(2025, 10, 1)));
    clickVisible(&host, start, start->dateCellRect(QDate(2025, 10, 3)).center());
    EXPECT_EQ(picker.startDateTime(), at(3, 10, 10));
    EXPECT_EQ(picker.endDateTime(), at(5, 15, 10));
    EXPECT_FALSE(picker.startDatePicker()->isCalendarOpen());
    EXPECT_EQ(picker.startDatePicker()->fluentStyle(), fluent::basicinput::Button::Standard);
    EXPECT_TRUE(picker.isPickerOpen());
}

TEST(DateTimeRangePickerTest, Contract_DateTriggerPressAndPopupStatesStayStable)
{
    QWidget host;
    host.resize(1000, 740);
    DateTimeRangePicker picker(&host);
    picker.setRange(at(1, 10, 10), at(3, 15, 10));
    picker.setGeometry(40, 40, 600, picker.sizeHint().height());
    host.show();
    picker.openPicker();
    settle();
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        SCOPED_TRACE(theme);
        for (auto* input : {picker.startDatePicker(), picker.endDatePicker()}) {
            input->setProperty("fluentThemeOverride", static_cast<int>(theme));
            static_cast<fluent::FluentElement*>(input)->onThemeUpdated();
            QSignalSpy styles(input, &fluent::basicinput::Button::fluentStyleChanged);
            QSignalSpy toggles(input, &QAbstractButton::toggled);
            QSignalSpy openStates(input,
                                  &fluent::date_time::CalendarDatePicker::calendarOpenChanged);
            const QRect geometry = input->geometry();
            QTest::mouseMove(input, input->rect().center());
            settle();
            const qreal restingCenter = dateContentCenterY(input);
            ASSERT_GT(restingCenter, 0);
            for (int click = 0; click < 6; ++click) {
                SCOPED_TRACE(click);
                QTest::mousePress(input, Qt::LeftButton, Qt::NoModifier, input->rect().center());
                EXPECT_NEAR(dateContentCenterY(input), restingCenter, 0.1);
                QTest::mouseRelease(input, Qt::LeftButton, Qt::NoModifier, input->rect().center());
                settle();
                EXPECT_EQ(input->isCalendarOpen(), click % 2 == 0);
                EXPECT_EQ(input->geometry(), geometry);
                EXPECT_EQ(input->fluentStyle(), fluent::basicinput::Button::Standard);
                EXPECT_NEAR(dateContentCenterY(input), restingCenter, 0.1);
            }
            EXPECT_EQ(openStates.count(), 6);
            EXPECT_EQ(styles.count(), 0);
            EXPECT_EQ(toggles.count(), 0);
            EXPECT_TRUE(picker.isPickerOpen());
        }
    }
}

TEST(DateTimeRangePickerTest, Contract_CompactFlyoutNestedDismissalAndResize)
{
    QWidget host;
    host.resize(900, 720);
    DateTimeRangePicker picker(&host);
    picker.setRange(at(1), at(3));
    picker.setGeometry(40, 40, 500, picker.sizeHint().height());
    host.show();
    picker.openPicker();
    settle();
    clickVisible(&host, picker.startDatePicker(), picker.startDatePicker()->rect().center());
    auto* child = picker.startDatePicker()->calendarView();
    auto* popup = qobject_cast<fluent::dialogs_flyouts::Flyout*>(child->parentWidget());
    ASSERT_NE(popup, nullptr);
    EXPECT_FALSE(popup->isModal());
    EXPECT_FALSE(popup->isDim());
    host.resize(380, 660);
    settle();
    EXPECT_TRUE(host.rect().contains(
        QRect(popup->mapTo(&host, popup->contentsRect().topLeft()), popup->contentsRect().size())));
    EXPECT_TRUE(isHitTarget(&host, child, child->titleButtonRect().center()));
    QTest::keyClick(child, Qt::Key_Escape);
    settle();
    EXPECT_FALSE(picker.startDatePicker()->isCalendarOpen());
    EXPECT_TRUE(picker.isPickerOpen());
    clickVisible(&host, picker.endDatePicker(), picker.endDatePicker()->rect().center());
    auto* done = host.findChild<fluent::basicinput::Button*>(QStringLiteral("rangeDone"));
    ASSERT_NE(done, nullptr);
    clickVisible(&host, done, done->rect().center());
    EXPECT_FALSE(picker.endDatePicker()->isCalendarOpen());
    EXPECT_FALSE(picker.isPickerOpen());
    picker.openPicker();
    settle();
    clickVisible(&host, picker.startDatePicker(), picker.startDatePicker()->rect().center());
    QTest::mouseClick(&host, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    settle();
    EXPECT_FALSE(picker.startDatePicker()->isCalendarOpen());
    EXPECT_FALSE(picker.isPickerOpen());
}

TEST(DateTimeRangePickerTest, Contract_InlineTimeAndDismissal)
{
    QWidget host;
    host.resize(900, 720);
    DateTimeRangePicker picker(&host);
    picker.setRange(at(1, 10, 10), at(3, 15, 10));
    picker.setGeometry(40, 40, 500, picker.sizeHint().height());
    host.show();
    picker.openPicker();
    settle();
    auto* edit =
        picker.startTimePicker()->findChild<QLineEdit*>(QStringLiteral("rangeStartTimeEdit"));
    ASSERT_NE(edit, nullptr);
#if QT_CONFIG(accessibility)
    auto* timeAccessible = QAccessible::queryAccessibleInterface(picker.startTimePicker());
    ASSERT_NE(timeAccessible, nullptr);
    EXPECT_FALSE(timeAccessible->state().hasPopup);
    EXPECT_EQ(timeAccessible->role(), QAccessible::Grouping);
    auto* editAccessible = QAccessible::queryAccessibleInterface(edit);
    ASSERT_NE(editAccessible, nullptr);
    EXPECT_EQ(editAccessible->role(), QAccessible::EditableText);
    EXPECT_EQ(editAccessible->text(QAccessible::Name), QStringLiteral("Start time"));
#endif
    clickVisible(&host, edit, edit->rect().center());
    edit->selectAll();
    QTest::keyClicks(edit, "11:35");
    QTest::keyClick(edit, Qt::Key_Return);
    EXPECT_EQ(picker.startDateTime(), at(1, 11, 35));
    EXPECT_EQ(picker.startDateTime().timeSpec(), Qt::UTC);
    EXPECT_FALSE(picker.startTimePicker()->isOpen());
    EXPECT_TRUE(picker.isPickerOpen());
    picker.startTimePicker()->setMinuteIncrement(5);
    edit->setCursorPosition(4);
    QTest::keyClick(edit, Qt::Key_Up);
    EXPECT_EQ(picker.startDateTime(), at(1, 11, 40));
    edit->selectAll();
    QTest::keyClicks(edit, "25:99");
    QTest::keyClick(edit, Qt::Key_Return);
    EXPECT_EQ(picker.startDateTime(), at(1, 11, 40));
    EXPECT_EQ(edit->text(), QStringLiteral("11:40"));
    EXPECT_EQ(host.findChildren<fluent::dialogs_flyouts::Flyout*>().size(), 1);
    picker.startTimePicker()->setClockIdentifier(TimePicker::ClockIdentifier::TwelveHourClock);
    edit->selectAll();
    QTest::keyClicks(edit, "1:25 PM");
    QTest::keyClick(edit, Qt::Key_Return);
    EXPECT_EQ(picker.startDateTime(), at(1, 13, 25));
    auto* done = host.findChild<fluent::basicinput::Button*>(QStringLiteral("rangeDone"));
    ASSERT_NE(done, nullptr);
    // Done also commits an in-progress time edit before dismissing.
    edit->selectAll();
    QTest::keyClicks(edit, "2:30 PM");
    clickVisible(&host, done, done->rect().center());
    EXPECT_EQ(picker.startDateTime(), at(1, 14, 30));
    EXPECT_FALSE(picker.isPickerOpen());
    picker.openPicker();
    settle();
    QTest::keyClick(picker.calendarView(), Qt::Key_Escape);
    settle();
    EXPECT_FALSE(picker.isPickerOpen());
    picker.openPicker();
    settle();
    QTest::mouseClick(&host, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    settle();
    EXPECT_FALSE(picker.isPickerOpen());
}

TEST(DateTimeRangePickerTest, Contract_InlinePresetsAndNarrowHost)
{
    QWidget host;
    host.resize(380, 660);
    DateTimeRangePicker picker(&host);
    picker.setTimeEnabled(false);
    picker.setGeometry(20, 20, 300, picker.sizeHint().height());
    host.show();
    picker.openPicker();
    settle();
    auto* presets = host.findChild<QWidget*>(QStringLiteral("rangePresets"));
    ASSERT_NE(presets, nullptr);
    EXPECT_TRUE(presets->isVisible());
    auto* preset = host.findChild<fluent::basicinput::Button*>(QStringLiteral("rangePreset5"));
    ASSERT_NE(preset, nullptr);
    clickVisible(&host, preset, preset->rect().center());
    EXPECT_TRUE(presets->isVisible());
    EXPECT_TRUE(picker.isPickerOpen());
    EXPECT_TRUE(picker.startDateTime().isValid());
    EXPECT_EQ(host.findChildren<fluent::dialogs_flyouts::Flyout*>().size(), 1);
    picker.setPresetsVisible(false);
    EXPECT_FALSE(presets->isVisible());
}

TEST(DateTimeRangePickerTest, Contract_WholeDayPresetsAndReferenceTimeZone)
{
    DateTimeRangePicker picker;
    const QDateTime reference =
        QDateTime::fromString(QStringLiteral("2024-03-31T13:45:19+05:30"), Qt::ISODate);
    const QList<QDate> starts{QDate(2024, 3, 31), QDate(2024, 3, 30), QDate(2024, 3, 25),
                              QDate(2024, 3, 1),  QDate(2023, 10, 1), QDate(2023, 4, 1)};
    for (int index = 0; index < starts.size(); ++index) {
        picker.applyPreset(static_cast<DateTimeRangePicker::Preset>(index), reference);
        EXPECT_EQ(picker.startDateTime().date(), starts.at(index));
        EXPECT_EQ(picker.endDateTime().date(),
                  index == 1 ? QDate(2024, 3, 30) : QDate(2024, 3, 31));
        EXPECT_EQ(picker.startDateTime().time(), QTime(0, 0));
        EXPECT_EQ(picker.endDateTime().time(), QTime(23, 59));
        EXPECT_EQ(picker.endDateTime().offsetFromUtc(), reference.offsetFromUtc());
    }
    QSignalSpy changed(&picker, &DateTimeRangePicker::rangeChanged);
    picker.applyPreset(DateTimeRangePicker::Preset::PastYear, reference);
    picker.applyPreset(DateTimeRangePicker::Preset::Today, QDateTime());
    EXPECT_EQ(changed.count(), 0);
    picker.setTimeEnabled(false);
    picker.applyPreset(DateTimeRangePicker::Preset::PastWeek, reference);
    EXPECT_EQ(picker.endDateTime().time(), QTime(0, 0));
    EXPECT_EQ(picker.startDateTime().date().daysTo(picker.endDateTime().date()), 6);
}

TEST(DateTimeRangePickerTest, Contract_ReverseCompletionPreservesTimeAndZone)
{
    QList<QDateTime> starts{
        at(20, 10, 30),
        QDateTime::fromString(QStringLiteral("2025-10-20T10:30:00+05:30"), Qt::ISODate)};
    const QTimeZone zone("America/New_York");
    if (zone.isValid())
        starts.append(QDateTime(QDate(2025, 10, 20), QTime(10, 30), zone));
    for (const auto& original : starts) {
        QWidget host;
        host.resize(900, 720);
        DateTimeRangePicker picker(&host);
        picker.setRange(original, QDateTime());
        picker.setGeometry(40, 40, 500, picker.sizeHint().height());
        host.show();
        picker.openPicker();
        settle();
        auto expectedStart = original;
        expectedStart.setDate(QDate(2025, 10, 5));
        QSignalSpy changed(&picker, &DateTimeRangePicker::rangeChanged);
        auto* calendar = picker.calendarView();
        clickVisible(&host, calendar, calendar->dateCellRect(expectedStart.date()).center());
        EXPECT_EQ(picker.startDateTime(), expectedStart);
        EXPECT_EQ(picker.endDateTime(), original);
        EXPECT_EQ(picker.endDateTime().time(), original.time());
        EXPECT_EQ(picker.endDateTime().timeSpec(), original.timeSpec());
        EXPECT_EQ(picker.endDateTime().timeZone(), original.timeZone());
        EXPECT_EQ(changed.count(), 1);
        EXPECT_TRUE(picker.isPickerOpen());
    }
}

TEST(DateTimeRangePickerTest, Contract_WholeDayPresetsRemainWholeAtCoarseMinuteSteps)
{
    QWidget host;
    host.resize(900, 740);
    DateTimeRangePicker picker(&host);
    picker.setGeometry(40, 40, 500, picker.sizeHint().height());
    const auto reference =
        QDateTime::fromString(QStringLiteral("2025-10-20T10:30:00+05:30"), Qt::ISODate);
    host.show();
    for (int step : {5, 15, 30, 59}) {
        picker.startTimePicker()->setMinuteIncrement(step);
        picker.endTimePicker()->setMinuteIncrement(step);
        for (int preset = 0; preset < 6; ++preset) {
            picker.applyPreset(static_cast<DateTimeRangePicker::Preset>(preset), reference);
            EXPECT_EQ(picker.startDateTime().time(), QTime(0, 0));
            EXPECT_EQ(picker.endDateTime().time(), QTime(23, 59));
            EXPECT_EQ(picker.endTimePicker()->selectedTime(), QTime(23, 59));
            EXPECT_EQ(picker.endDateTime().offsetFromUtc(), reference.offsetFromUtc());
            EXPECT_EQ(picker.endTimePicker()->minuteIncrement(), step);
            picker.openPicker();
            settle();
            auto* edit =
                picker.endTimePicker()->findChild<QLineEdit*>(QStringLiteral("rangeEndTimeEdit"));
            ASSERT_NE(edit, nullptr);
            EXPECT_EQ(edit->text(), QStringLiteral("23:59"));
            picker.closePicker();
            EXPECT_EQ(picker.endDateTime().time(), QTime(23, 59));
        }
    }
    TimePicker ordinary;
    ordinary.setMinuteIncrement(15);
    ordinary.setSelectedTime(QTime(23, 59));
    EXPECT_EQ(ordinary.selectedTime(), QTime(23, 45));
    picker.endTimePicker()->setMinuteIncrement(15);
    picker.endTimePicker()->setSelectedTime(QTime(10, 16));
    EXPECT_EQ(picker.endDateTime().time(), QTime(10, 15));
}

TEST(DateTimeRangePickerTest, Contract_CustomPresetAndOpenPropertyNoOps)
{
    QWidget host;
    host.resize(900, 720);
    DateTimeRangePicker picker(&host);
    picker.resize(picker.sizeHint());
    host.show();
    QSignalSpy open(&picker, &DateTimeRangePicker::pickerOpenChanged);
    QTest::keyClick(picker.entryButton(), Qt::Key_F4);
    picker.openPicker();
    EXPECT_TRUE(picker.isPickerOpen());
    EXPECT_EQ(open.count(), 1);
    picker.setAutoApplyPresetsEnabled(false);
    int requests = 0;
    QObject::connect(&picker, &DateTimeRangePicker::presetRequested, &picker,
                     [&](DateTimeRangePicker::Preset preset) {
                         EXPECT_EQ(preset, DateTimeRangePicker::Preset::Today);
                         ++requests;
                         picker.setRange(at(1, 10), at(3, 15));
                     });
    auto* button = host.findChild<fluent::basicinput::Button*>(QStringLiteral("rangePreset0"));
    ASSERT_NE(button, nullptr);
    button->click();
    EXPECT_EQ(requests, 1);
    EXPECT_EQ(picker.startDateTime(), at(1, 10));
    picker.closePicker();
    picker.closePicker();
    EXPECT_EQ(open.count(), 2);
    QSignalSpy visible(&picker, &DateTimeRangePicker::presetsVisibleChanged);
    picker.setPresetsVisible(false);
    picker.setPresetsVisible(false);
    EXPECT_EQ(visible.count(), 1);
}

TEST(DateTimeRangePickerTest, Contract_PopupTeardownAfterReparenting)
{
    QWidget host;
    host.resize(900, 720);
    auto* picker = new DateTimeRangePicker(&host);
    picker->resize(picker->sizeHint());
    host.show();
    picker->openPicker();
    QPointer<QWidget> calendar = picker->calendarView();
    QPointer<QWidget> popup = host.findChild<QWidget*>(QStringLiteral("DateTimeRangePickerPopup"));
    ASSERT_FALSE(popup.isNull());
    delete picker;
    EXPECT_TRUE(popup.isNull());
    EXPECT_TRUE(calendar.isNull());

    auto* disposable = new DateTimeRangePicker(&host);
    disposable->show();
    QPointer<DateTimeRangePicker> guard = disposable;
    QObject::connect(disposable, &DateTimeRangePicker::pickerOpenChanged, disposable,
                     [disposable](bool open) {
                         if (open)
                             delete disposable;
                     });
    disposable->openPicker();
    EXPECT_TRUE(guard.isNull());
}
