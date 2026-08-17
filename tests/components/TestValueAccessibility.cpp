#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QTest>
#include <QVariant>
#include <QVector>

#include <cmath>

#include "components/basicinput/RatingControl.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/status_info/ProgressBar.h"
#include "components/status_info/ProgressRing.h"
#include "components/textfields/NumberBox.h"

using fluent::basicinput::RatingControl;
using fluent::basicinput::ToggleSwitch;
using fluent::status_info::ProgressBar;
using fluent::status_info::ProgressRing;
using fluent::textfields::NumberBox;

namespace {

void showAndProcess(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    widget.show();
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

#endif

} // namespace

TEST(ValueAccessibilityTest, Contract_AccessibilityToggleSwitchIsCheckableAndToggleable)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    ToggleSwitch toggle;
    toggle.setAccessibleName(QStringLiteral("Wi-Fi"));
    toggle.setOnContent(QStringLiteral("On"));
    toggle.setOffContent(QStringLiteral("Off"));
    toggle.setAccessibleDescription(QStringLiteral("Network state"));
    showAndProcess(toggle, toggle.sizeHint());

    QAccessibleInterface* root = accessible(&toggle);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::CheckBox);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Wi-Fi"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Network state"));
    EXPECT_TRUE(root->state().checkable);
    EXPECT_FALSE(root->state().checked);

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::toggleAction()));
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::toggleAction()),
              QStringList{QStringLiteral("Space")});

    actions->doAction(QAccessibleActionInterface::toggleAction());
    EXPECT_TRUE(toggle.isOn());
    EXPECT_TRUE(root->state().checked);
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Network state"));

    toggle.setEnabled(false);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::toggleAction()));
#endif
}

TEST(ValueAccessibilityTest, Contract_AccessibilityRatingExposesBoundedValueAndActions)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    RatingControl rating;
    rating.setAccessibleName(QStringLiteral("Experience"));
    rating.setCaption(QStringLiteral("Optional rating"));
    showAndProcess(rating, rating.sizeHint());

    QAccessibleInterface* root = accessible(&rating);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Slider);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Experience"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Optional rating"));
    EXPECT_EQ(root->text(QAccessible::Value),
              QStringLiteral("No rating"));

    QAccessibleValueInterface* value = root->valueInterface();
    ASSERT_NE(value, nullptr);
    EXPECT_DOUBLE_EQ(value->currentValue().toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(value->minimumValue().toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(value->maximumValue().toDouble(), 5.0);
    EXPECT_DOUBLE_EQ(value->minimumStepSize().toDouble(), 0.5);

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    actions->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_DOUBLE_EQ(rating.value(), 0.5);
    actions->doAction(QAccessibleActionInterface::decreaseAction());
    EXPECT_DOUBLE_EQ(rating.value(), -1.0);
    EXPECT_DOUBLE_EQ(value->currentValue().toDouble(), 0.0);

    rating.setAccessibleDescription(QStringLiteral("Caller description"));
    rating.setCaption(QStringLiteral("Changed caption"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Caller description"));

    rating.setIsReadOnly(true);
    EXPECT_TRUE(root->state().readOnly);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::increaseAction()));
    value->setCurrentValue(4.0);
    EXPECT_DOUBLE_EQ(rating.value(), -1.0);
#endif
}

TEST(ValueAccessibilityTest, Contract_AccessibilityNumberBoxRetainsTextAndAddsNumericValue)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    NumberBox box;
    box.setHeader(QStringLiteral("Quantity"));
    box.setRange(-10.0, 10.0);
    box.setSmallChange(0.5);
    box.setValue(2.5);
    showAndProcess(box, QSize(220, box.sizeHint().height()));

    QAccessibleInterface* root = accessible(&box);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::SpinBox);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Quantity"));
    EXPECT_EQ(root->text(QAccessible::Value), QStringLiteral("2.5"));

    QAccessibleValueInterface* value = root->valueInterface();
    QAccessibleTextInterface* text = root->textInterface();
    QAccessibleEditableTextInterface* editable =
        root->editableTextInterface();
    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(value, nullptr);
    ASSERT_NE(text, nullptr);
    ASSERT_NE(editable, nullptr);
    ASSERT_NE(actions, nullptr);
    EXPECT_DOUBLE_EQ(value->currentValue().toDouble(), 2.5);
    EXPECT_DOUBLE_EQ(value->minimumValue().toDouble(), -10.0);
    EXPECT_DOUBLE_EQ(value->maximumValue().toDouble(), 10.0);
    EXPECT_DOUBLE_EQ(value->minimumStepSize().toDouble(), 0.5);
    EXPECT_EQ(text->text(0, text->characterCount()),
              QStringLiteral("2.5"));

    text->addSelection(0, 1);
    int start = -1;
    int end = -1;
    text->selection(0, &start, &end);
    EXPECT_EQ(start, 0);
    EXPECT_EQ(end, 1);

    actions->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_DOUBLE_EQ(box.value(), 3.0);
    value->setCurrentValue(4.0);
    EXPECT_DOUBLE_EQ(box.value(), 4.0);

    box.setReadOnly(true);
    EXPECT_TRUE(root->state().readOnly);
    EXPECT_FALSE(root->state().editable);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::increaseAction()));
    editable->replaceText(0, box.text().size(), QStringLiteral("7"));
    EXPECT_DOUBLE_EQ(box.value(), 4.0);

    box.setReadOnly(false);
    box.setFocus(Qt::OtherFocusReason);
    box.setText(QStringLiteral("not-a-number"));
    QTest::keyClick(&box, Qt::Key_Return);
    QApplication::processEvents();
    EXPECT_TRUE(std::isnan(box.value()));
    EXPECT_TRUE(root->state().invalid);
    EXPECT_FALSE(value->currentValue().isValid());
#endif
}

TEST(ValueAccessibilityTest, Contract_AccessibilityProgressExposesDeterminateAndBusyStates)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    ProgressBar bar;
    bar.setAccessibleName(QStringLiteral("Download"));
    bar.setRange(10.0, 60.0);
    bar.setValue(35.0);
    showAndProcess(bar, bar.sizeHint());

    QAccessibleInterface* barRoot = accessible(&bar);
    ASSERT_NE(barRoot, nullptr);
    EXPECT_EQ(barRoot->role(), QAccessible::ProgressBar);
    EXPECT_TRUE(barRoot->state().readOnly);
    QAccessibleValueInterface* barValue = barRoot->valueInterface();
    ASSERT_NE(barValue, nullptr);
    EXPECT_DOUBLE_EQ(barValue->currentValue().toDouble(), 35.0);
    EXPECT_DOUBLE_EQ(barValue->minimumValue().toDouble(), 10.0);
    EXPECT_DOUBLE_EQ(barValue->maximumValue().toDouble(), 60.0);
    EXPECT_FALSE(barRoot->actionInterface()->actionNames().contains(
        QAccessibleActionInterface::increaseAction()));

    bar.setIsIndeterminate(true);
    QApplication::processEvents();
    EXPECT_FALSE(barValue->currentValue().isValid());
    EXPECT_TRUE(barRoot->state().busy);
    EXPECT_TRUE(barRoot->state().animated);
    EXPECT_EQ(barRoot->text(QAccessible::Description),
              QStringLiteral("In progress"));
    bar.setShowPaused(true);
    EXPECT_FALSE(barRoot->state().busy);
    EXPECT_FALSE(barRoot->state().animated);
    EXPECT_EQ(barRoot->text(QAccessible::Description),
              QStringLiteral("Paused"));

    ProgressRing ring;
    ring.setAccessibleDescription(QStringLiteral("Sync status"));
    ring.setIsActive(true);
    showAndProcess(ring, ring.sizeHint());
    QAccessibleInterface* ringRoot = accessible(&ring);
    ASSERT_NE(ringRoot, nullptr);
    EXPECT_EQ(ringRoot->role(), QAccessible::ProgressBar);
    EXPECT_TRUE(ringRoot->state().readOnly);
    EXPECT_TRUE(ringRoot->state().busy);
    EXPECT_TRUE(ringRoot->state().animated);
    EXPECT_EQ(ringRoot->text(QAccessible::Description),
              QStringLiteral("Sync status"));
    QAccessibleValueInterface* ringValue = ringRoot->valueInterface();
    ASSERT_NE(ringValue, nullptr);
    EXPECT_FALSE(ringValue->currentValue().isValid());

    ring.setIsIndeterminate(false);
    ring.setRange(0, 20);
    ring.setValue(8);
    EXPECT_FALSE(ringRoot->state().busy);
    EXPECT_FALSE(ringRoot->state().animated);
    EXPECT_EQ(ringValue->currentValue().toInt(), 8);
#endif
}

TEST(ValueAccessibilityTest, Contract_AccessibilityValueEventsFollowEffectiveChangesAndNoOps)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    ToggleSwitch toggle;
    RatingControl rating;
    NumberBox number;
    ProgressBar progress;
    ASSERT_NE(accessible(&toggle), nullptr);
    ASSERT_NE(accessible(&rating), nullptr);
    ASSERT_NE(accessible(&number), nullptr);
    ASSERT_NE(accessible(&progress), nullptr);

    ScopedAccessibilityEventCapture capture;

    toggle.setIsOn(true);
    EXPECT_EQ(capture.count(&toggle, QAccessible::StateChanged), 1);
    toggle.setIsOn(true);
    EXPECT_EQ(capture.count(&toggle, QAccessible::StateChanged), 1);

    rating.setValue(2.0);
    EXPECT_EQ(capture.count(&rating, QAccessible::ValueChanged), 1);
    rating.setValue(2.0);
    EXPECT_EQ(capture.count(&rating, QAccessible::ValueChanged), 1);

    number.setValue(3.0);
    EXPECT_EQ(capture.count(&number, QAccessible::ValueChanged), 1);
    number.setValue(3.0);
    EXPECT_EQ(capture.count(&number, QAccessible::ValueChanged), 1);

    progress.setValue(30.0);
    EXPECT_EQ(capture.count(&progress, QAccessible::ValueChanged), 1);
    progress.setValue(30.0);
    EXPECT_EQ(capture.count(&progress, QAccessible::ValueChanged), 1);

    capture.clear();
    progress.setIsIndeterminate(true);
    EXPECT_EQ(capture.count(&progress, QAccessible::StateChanged), 1);
    EXPECT_EQ(capture.count(&progress, QAccessible::ValueChanged), 1);
    progress.setIsIndeterminate(true);
    EXPECT_EQ(capture.count(&progress, QAccessible::StateChanged), 1);
    EXPECT_EQ(capture.count(&progress, QAccessible::ValueChanged), 1);
#endif
}
