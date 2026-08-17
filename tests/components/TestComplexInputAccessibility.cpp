#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QKeyEvent>
#include <QListView>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include "components/basicinput/Button.h"
#include "components/basicinput/ColorPicker.h"
#include "components/date_time/DatePicker.h"
#include "components/date_time/TimePicker.h"
#include "components/scrolling/ScrollView.h"
#include "components/scrolling/AnnotatedScrollBar.h"
#include "components/textfields/AutoSuggestBox.h"
#include "QtTestEnvironment.h"

using fluent::basicinput::Button;
using fluent::basicinput::ColorPicker;
using fluent::date_time::DatePicker;
using fluent::date_time::TimePicker;
using fluent::scrolling::AnnotatedScrollBar;
using fluent::scrolling::AnnotatedScrollBarLabel;
using fluent::textfields::AutoSuggestBox;

namespace {

void showAndProcess(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    widget.show();
    QApplication::processEvents();
}

void sendKey(QWidget* widget, Qt::Key key,
             Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &release);
    QApplication::processEvents();
}

#if QT_CONFIG(accessibility)

struct AccessibleEventRecord {
    QObject* object = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
};

QVector<AccessibleEventRecord> g_accessibilityEvents;

void captureAccessibilityEvent(QAccessibleEvent* event)
{
    if (event) {
        g_accessibilityEvents.append(
            AccessibleEventRecord{event->object(), event->type()});
    }
}

class ScopedAccessibilityEventCapture {
public:
    ScopedAccessibilityEventCapture()
        : m_previous(
              QAccessible::installUpdateHandler(captureAccessibilityEvent))
    {
        g_accessibilityEvents.clear();
    }

    ~ScopedAccessibilityEventCapture()
    {
        QAccessible::installUpdateHandler(m_previous);
        g_accessibilityEvents.clear();
    }

    int count(QObject* object, QAccessible::Event type) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object && event.type == type)
                ++result;
        }
        return result;
    }

    void clear() { g_accessibilityEvents.clear(); }

private:
    QAccessible::UpdateHandler m_previous = nullptr;
};

QAccessibleInterface* accessible(QWidget* widget)
{
    return widget ? QAccessible::queryAccessibleInterface(widget) : nullptr;
}

bool hasControllerRelation(QAccessibleInterface* root, QWidget* target)
{
    if (!root || !target)
        return false;
    QAccessibleInterface* expected = accessible(target);
    for (const auto& relation : root->relations(QAccessible::Controller)) {
        if (relation.first == expected
            && relation.second.testFlag(QAccessible::Controller)) {
            return true;
        }
    }
    return false;
}

#endif

} // namespace

TEST(ComplexInputAccessibilityTest, Contract_AccessibilityColorPickerExposesColorAndAdjustableRegions)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    ColorPicker picker;
    picker.setAccessibleName(QStringLiteral("Brand color"));
    showAndProcess(picker, QSize(440, 420));

    QAccessibleInterface* root = accessible(&picker);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ColorChooser);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Brand color"));
    EXPECT_EQ(root->text(QAccessible::Value),
              QStringLiteral("#FFFFFFFF"));

    QWidget* spectrum = picker.findChild<QWidget*>(
        QStringLiteral("ColorPicker.Spectrum"));
    QWidget* hue = picker.findChild<QWidget*>(
        QStringLiteral("ColorPicker.HueBar"));
    QWidget* preview = picker.findChild<QWidget*>(
        QStringLiteral("ColorPicker.PreviewPane"));
    ASSERT_NE(spectrum, nullptr);
    ASSERT_NE(hue, nullptr);
    ASSERT_NE(preview, nullptr);

    QAccessibleInterface* spectrumInterface = accessible(spectrum);
    QAccessibleInterface* hueInterface = accessible(hue);
    QAccessibleInterface* previewInterface = accessible(preview);
    ASSERT_NE(spectrumInterface, nullptr);
    ASSERT_NE(hueInterface, nullptr);
    ASSERT_NE(previewInterface, nullptr);
    EXPECT_EQ(spectrumInterface->role(), QAccessible::ColorChooser);
    EXPECT_EQ(hueInterface->role(), QAccessible::Slider);
    EXPECT_EQ(previewInterface->role(), QAccessible::Graphic);
    EXPECT_TRUE(spectrumInterface->state().focusable);
    EXPECT_TRUE(hueInterface->state().focusable);
    EXPECT_FALSE(spectrumInterface->text(QAccessible::Value).isEmpty());
    EXPECT_EQ(previewInterface->text(QAccessible::Value),
              root->text(QAccessible::Value));

    QAccessibleValueInterface* hueValue = hueInterface->valueInterface();
    ASSERT_NE(hueValue, nullptr);
    EXPECT_EQ(hueValue->minimumValue().toInt(), 0);
    EXPECT_EQ(hueValue->maximumValue().toInt(), 359);
    hueValue->setCurrentValue(180);
    EXPECT_NEAR(picker.hue(), 180.0 / 359.0, 0.01);

    const qreal oldSaturation = picker.saturation();
    sendKey(spectrum, Qt::Key_Right);
    EXPECT_GT(picker.saturation(), oldSaturation);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture capture;
    picker.setColor(picker.color());
    EXPECT_EQ(capture.count(&picker, QAccessible::ValueChanged), 0);
    picker.setColor(QColor(12, 34, 56, 78));
    EXPECT_EQ(capture.count(&picker, QAccessible::ValueChanged), 1);
    EXPECT_EQ(root->text(QAccessible::Value),
              QStringLiteral("#0C22384E"));
#endif
}

TEST(ComplexInputAccessibilityTest, Contract_AccessibilityDateAndTimePickersSeparatePendingAndCommittedValues)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    DatePicker date;
    date.setSelectedDate(QDate(2026, 8, 16));
    showAndProcess(date, QSize(240, date.sizeHint().height()));

    QAccessibleInterface* dateRoot = accessible(&date);
    ASSERT_NE(dateRoot, nullptr);
    EXPECT_EQ(dateRoot->role(), QAccessible::ButtonMenu);
    EXPECT_TRUE(dateRoot->state().hasPopup);
    EXPECT_TRUE(dateRoot->state().collapsed);
    const QString committedDate = dateRoot->text(QAccessible::Value);
    EXPECT_FALSE(committedDate.isEmpty());

    QAccessibleActionInterface* dateActions = dateRoot->actionInterface();
    ASSERT_NE(dateActions, nullptr);
    EXPECT_TRUE(dateActions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
    dateActions->doAction(QAccessibleActionInterface::showMenuAction());
    QApplication::processEvents();
    EXPECT_TRUE(date.isDropDownOpen());
    EXPECT_TRUE(dateRoot->state().expanded);

    QWidget* dateFlyout = date.findChild<QWidget*>(
        QStringLiteral("DatePickerFlyout"));
    QWidget* month = date.findChild<QWidget*>(
        QStringLiteral("DatePickerMonthColumn"));
    ASSERT_NE(dateFlyout, nullptr);
    ASSERT_NE(month, nullptr);
    EXPECT_TRUE(hasControllerRelation(dateRoot, dateFlyout));
    QAccessibleInterface* monthInterface = accessible(month);
    ASSERT_NE(monthInterface, nullptr);
    EXPECT_EQ(monthInterface->role(), QAccessible::SpinBox);
    EXPECT_EQ(monthInterface->text(QAccessible::Name),
              QStringLiteral("Month"));
    ASSERT_NE(monthInterface->valueInterface(), nullptr);
    QAccessibleActionInterface* monthActions =
        monthInterface->actionInterface();
    ASSERT_NE(monthActions, nullptr);

    const QDate selectedBefore = date.selectedDate();
    monthActions->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_EQ(date.selectedDate(), selectedBefore);
    EXPECT_EQ(dateRoot->text(QAccessible::Value), committedDate);

    Button* dateConfirm = date.findChild<Button*>(
        QStringLiteral("DatePickerConfirmButton"));
    Button* dateCancel = date.findChild<Button*>(
        QStringLiteral("DatePickerCancelButton"));
    ASSERT_NE(dateConfirm, nullptr);
    ASSERT_NE(dateCancel, nullptr);
    EXPECT_EQ(dateConfirm->accessibleName(), QStringLiteral("Confirm date"));
    EXPECT_EQ(dateCancel->accessibleName(), QStringLiteral("Cancel"));
    date.closePicker();
    QApplication::processEvents();

    TimePicker time;
    time.setSelectedTime(QTime(9, 30));
    time.setMinuteIncrement(15);
    showAndProcess(time, QSize(220, time.sizeHint().height()));
    QAccessibleInterface* timeRoot = accessible(&time);
    ASSERT_NE(timeRoot, nullptr);
    EXPECT_EQ(timeRoot->role(), QAccessible::ButtonMenu);
    const QString committedTime = timeRoot->text(QAccessible::Value);
    timeRoot->actionInterface()->doAction(
        QAccessibleActionInterface::showMenuAction());
    QApplication::processEvents();

    QWidget* timeFlyout = time.findChild<QWidget*>(
        QStringLiteral("TimePickerFlyout"));
    QWidget* minute = time.findChild<QWidget*>(
        QStringLiteral("TimePickerMinuteColumn"));
    ASSERT_NE(timeFlyout, nullptr);
    ASSERT_NE(minute, nullptr);
    EXPECT_TRUE(hasControllerRelation(timeRoot, timeFlyout));
    QAccessibleInterface* minuteInterface = accessible(minute);
    ASSERT_NE(minuteInterface, nullptr);
    QAccessibleValueInterface* minuteValue =
        minuteInterface->valueInterface();
    ASSERT_NE(minuteValue, nullptr);
    EXPECT_EQ(minuteValue->minimumStepSize().toInt(), 15);
    const QTime selectedTimeBefore = time.selectedTime();
    minuteInterface->actionInterface()->doAction(
        QAccessibleActionInterface::increaseAction());
    EXPECT_EQ(time.selectedTime(), selectedTimeBefore);
    EXPECT_EQ(timeRoot->text(QAccessible::Value), committedTime);

    Button* timeConfirm = time.findChild<Button*>(
        QStringLiteral("TimePickerConfirmButton"));
    ASSERT_NE(timeConfirm, nullptr);
    EXPECT_EQ(timeConfirm->accessibleName(), QStringLiteral("Confirm time"));
    time.closePicker();
    QApplication::processEvents();

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture capture;
    date.setSelectedDate(date.selectedDate());
    EXPECT_EQ(capture.count(&date, QAccessible::ValueChanged), 0);
    date.setSelectedDate(QDate(2026, 8, 17));
    EXPECT_EQ(capture.count(&date, QAccessible::ValueChanged), 1);
#endif
}

TEST(ComplexInputAccessibilityTest, Contract_AccessibilityAnnotatedScrollBarKeepsFilteredLabelsOperable)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    AnnotatedScrollBar bar;
    bar.setAccessibleName(QStringLiteral("Document sections"));
    bar.setRange(0, 100);
    bar.setPageStep(20);
    bar.setLabels({
        AnnotatedScrollBarLabel(QStringLiteral("Intro"), 0,
                                QStringLiteral("Opening")),
        AnnotatedScrollBarLabel(QStringLiteral("Methods"), 25),
        AnnotatedScrollBarLabel(QStringLiteral("Results"), 50),
        AnnotatedScrollBarLabel(QStringLiteral("Discussion"), 75),
        AnnotatedScrollBarLabel(QStringLiteral("Appendix"), 100)});
    showAndProcess(bar, QSize(110, 90));

    QAccessibleInterface* root = accessible(&bar);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ScrollBar);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Document sections"));
    ASSERT_NE(root->valueInterface(), nullptr);
    EXPECT_EQ(root->valueInterface()->minimumValue().toInt(), 0);
    EXPECT_EQ(root->valueInterface()->maximumValue().toInt(), 100);
    EXPECT_EQ(root->childCount(), 5);

    bool foundOffscreen = false;
    for (int index = 0; index < root->childCount(); ++index) {
        QAccessibleInterface* label = root->child(index);
        ASSERT_NE(label, nullptr);
        EXPECT_EQ(label->role(), QAccessible::Link);
        EXPECT_TRUE(label->state().linked);
        EXPECT_FALSE(label->text(QAccessible::Name).isEmpty());
        foundOffscreen = foundOffscreen || label->state().offscreen;
    }
    EXPECT_TRUE(foundOffscreen)
        << "Collision-filtered labels must remain as offscreen logical links";

    QSignalSpy activated(&bar, &AnnotatedScrollBar::labelActivated);
    QAccessibleInterface* results = root->child(2);
    ASSERT_NE(results, nullptr);
    results->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(bar.value(), 50);
    EXPECT_EQ(activated.count(), 1);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture capture;
    bar.setValue(50);
    EXPECT_EQ(capture.count(&bar, QAccessible::ValueChanged), 0);
    bar.setValue(60);
    EXPECT_EQ(capture.count(&bar, QAccessible::ValueChanged), 1);

    capture.clear();
    bar.setLabels(bar.labels());
    EXPECT_EQ(capture.count(&bar, QAccessible::ObjectReorder), 0);
    bar.addLabel(AnnotatedScrollBarLabel(QStringLiteral("Notes"), 90));
    EXPECT_EQ(capture.count(&bar, QAccessible::ObjectReorder), 1);
#endif
}

TEST(ComplexInputAccessibilityTest, Contract_AccessibilityAutoSuggestRetainsTextAndControlsSuggestionList)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    AutoSuggestBox box;
    box.setHeader(QStringLiteral("Search files"));
    box.setSuggestions({QStringLiteral("Alpha"), QStringLiteral("Beta")});
    box.setText(QStringLiteral("a"));
    showAndProcess(box, QSize(260, box.sizeHint().height()));
    box.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();

    QAccessibleInterface* root = accessible(&box);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::EditableText);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Search files"));
    EXPECT_EQ(root->text(QAccessible::Value), QStringLiteral("a"));
    EXPECT_TRUE(root->state().supportsAutoCompletion);
    EXPECT_TRUE(root->state().hasPopup);
    EXPECT_TRUE(root->state().expandable);
    ASSERT_NE(root->textInterface(), nullptr);
    ASSERT_NE(root->editableTextInterface(), nullptr);
    EXPECT_EQ(root->textInterface()->text(
                  0, root->textInterface()->characterCount()),
              QStringLiteral("a"));

    QWidget* list = box.findChild<QWidget*>(
        QStringLiteral("AutoSuggestBoxSuggestionList"));
    ASSERT_NE(list, nullptr);
    EXPECT_TRUE(hasControllerRelation(root, list));

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
    actions->doAction(QAccessibleActionInterface::showMenuAction());
    QApplication::processEvents();
    EXPECT_TRUE(box.isSuggestionListOpen());
    EXPECT_TRUE(root->state().expanded);
    EXPECT_EQ(QApplication::focusWidget(), &box);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture capture;
    sendKey(&box, Qt::Key_Down);
    EXPECT_EQ(capture.count(
                  &box, QAccessible::ActiveDescendantChanged), 1);

    capture.clear();
    box.setSuggestions(box.suggestions());
    EXPECT_EQ(capture.count(&box, QAccessible::StateChanged), 0);
    box.setSuggestions({QStringLiteral("Gamma")});
    EXPECT_EQ(capture.count(&box, QAccessible::StateChanged), 1);

    auto* queryButton = box.findChild<Button*>(
        QStringLiteral("AutoSuggestBoxQueryButton"));
    auto* clearButton = box.findChild<Button*>(
        QStringLiteral("AutoSuggestBoxClearButton"));
    ASSERT_NE(queryButton, nullptr);
    ASSERT_NE(clearButton, nullptr);
    EXPECT_EQ(queryButton->accessibleName(), QStringLiteral("Submit query"));
    EXPECT_EQ(clearButton->accessibleName(), QStringLiteral("Clear text"));

    box.setAccessibleName(QStringLiteral("Project search"));
    box.setHeader(QStringLiteral("Changed header"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Project search"));
#endif
}
