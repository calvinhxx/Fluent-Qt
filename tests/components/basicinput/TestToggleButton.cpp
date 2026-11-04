#include "components/basicinput/ToggleButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include <QCoreApplication>
#include <QEvent>
#include <QButtonGroup>
#include <QImage>
#include <QPointer>
#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace fluent;
using namespace fluent::basicinput;

TEST(ToggleButtonTest, Contract_DefaultsAndPropertySignalsAreNoOpSafe)
{
    ToggleButton toggle(QStringLiteral("Toggle"));
    EXPECT_FALSE(toggle.isThreeState());
    EXPECT_EQ(toggle.checkState(), Qt::Unchecked);

    QSignalSpy threeStateSpy(&toggle, &ToggleButton::threeStateChanged);
    QSignalSpy checkStateSpy(&toggle, &ToggleButton::checkStateChanged);

    toggle.setThreeState(true);
    toggle.setThreeState(true);
    toggle.setCheckState(Qt::Checked);
    toggle.setCheckState(Qt::Checked);

    EXPECT_TRUE(toggle.isThreeState());
    EXPECT_EQ(toggle.checkState(), Qt::Checked);
    EXPECT_EQ(threeStateSpy.count(), 1);
    EXPECT_EQ(checkStateSpy.count(), 1);
}

TEST(ToggleButtonTest, Contract_ProgrammaticPartialCheckStateIsPreserved)
{
    ToggleButton toggle(QStringLiteral("Three-state"));
    toggle.setThreeState(true);

    int toggledCount = 0;
    bool lastChecked = false;
    QObject::connect(&toggle, &QPushButton::toggled, [&toggledCount, &lastChecked](bool checked) {
        ++toggledCount;
        lastChecked = checked;
    });

    toggle.setCheckState(Qt::PartiallyChecked);

    EXPECT_EQ(toggle.checkState(), Qt::PartiallyChecked);
    EXPECT_TRUE(toggle.isChecked());
    EXPECT_EQ(toggledCount, 1);
    EXPECT_TRUE(lastChecked);
}

TEST(ToggleButtonTest, Contract_ToggledCallbackCanDeleteButton)
{
    QObject receiver;
    auto* toggle = new ToggleButton(QStringLiteral("Toggle"));
    QPointer<ToggleButton> guard(toggle);
    int checkStateNotifications = 0;
    QObject::connect(toggle, &ToggleButton::checkStateChanged, &receiver, [&] { ++checkStateNotifications; });
    QObject::connect(toggle, &QPushButton::toggled, &receiver, [toggle] { delete toggle; });

    toggle->setCheckState(Qt::Checked);

    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(checkStateNotifications, 0);
}

TEST(ToggleButtonTest, Contract_ToggledCallbackCanReplaceCheckState)
{
    ToggleButton toggle(QStringLiteral("Toggle"));
    toggle.setThreeState(true);
    QSignalSpy checkStateSpy(&toggle, &ToggleButton::checkStateChanged);
    QObject::connect(&toggle, &QPushButton::toggled, &toggle, [&](bool checked) {
        if (checked)
            toggle.setCheckState(Qt::PartiallyChecked);
    });

    toggle.setCheckState(Qt::Checked);

    EXPECT_TRUE(toggle.isChecked());
    EXPECT_EQ(toggle.checkState(), Qt::PartiallyChecked);
    ASSERT_EQ(checkStateSpy.count(), 1);
    EXPECT_EQ(checkStateSpy.at(0).at(0).toInt(), Qt::PartiallyChecked);

    toggle.setChecked(false);
    EXPECT_EQ(toggle.checkState(), Qt::Unchecked);
    EXPECT_EQ(checkStateSpy.count(), 2);
}

TEST(ToggleButtonTest, Contract_ClickToggledCallbackCanDeleteButton)
{
    QObject receiver;
    for (const bool threeState : {false, true}) {
        SCOPED_TRACE(threeState);
        auto* toggle = new ToggleButton(QStringLiteral("Toggle"));
        toggle->setThreeState(threeState);
        QPointer<ToggleButton> guard(toggle);
        int notifications = 0;
        QObject::connect(toggle, &QPushButton::toggled, &receiver, [toggle, &notifications](bool checked) {
            EXPECT_TRUE(checked);
            ++notifications;
            delete toggle;
        });

        toggle->click();

        EXPECT_TRUE(guard.isNull());
        EXPECT_EQ(notifications, 1);
    }
}

TEST(ToggleButtonTest, Contract_CheckStateCallbackCanDeleteButton)
{
    QObject receiver;
    auto* toggle = new ToggleButton(QStringLiteral("Toggle"));
    QPointer<ToggleButton> guard(toggle);
    QObject::connect(toggle, &ToggleButton::checkStateChanged, &receiver, [toggle](Qt::CheckState) { delete toggle; });

    toggle->setCheckState(Qt::Checked);

    EXPECT_TRUE(guard.isNull());
}

TEST(ToggleButtonTest, Contract_BlockedCheckStateUpdatesRemainSilent)
{
    ToggleButton toggle(QStringLiteral("Toggle"));
    QSignalSpy toggledSpy(&toggle, &QPushButton::toggled);
    QSignalSpy stateSpy(&toggle, &ToggleButton::checkStateChanged);
    toggle.blockSignals(true);

    toggle.setCheckState(Qt::PartiallyChecked);

    EXPECT_TRUE(toggle.signalsBlocked());
    EXPECT_TRUE(toggle.isChecked());
    EXPECT_EQ(toggle.checkState(), Qt::PartiallyChecked);
    EXPECT_TRUE(toggledSpy.isEmpty());
    EXPECT_TRUE(stateSpy.isEmpty());
    toggle.blockSignals(false);
    toggle.setCheckState(Qt::Unchecked);
    ASSERT_EQ(toggledSpy.count(), 1);
    ASSERT_EQ(stateSpy.count(), 1);
    EXPECT_EQ(stateSpy.first().first().value<Qt::CheckState>(), Qt::Unchecked);
}

TEST(ToggleButtonTest, Contract_CheckStateSupportsQueuedConnections)
{
    QObject receiver;
    ToggleButton toggle(QStringLiteral("Toggle"));
    Qt::CheckState received = Qt::Unchecked;
    int notifications = 0;
    QObject::connect(
        &toggle, &ToggleButton::checkStateChanged, &receiver,
        [&](Qt::CheckState state) {
            received = state;
            ++notifications;
        },
        Qt::QueuedConnection);

    toggle.setCheckState(Qt::PartiallyChecked);

    EXPECT_EQ(notifications, 0);
    QCoreApplication::sendPostedEvents(&receiver, QEvent::MetaCall);
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(received, Qt::PartiallyChecked);
}

TEST(ToggleButtonTest, Contract_GroupNotificationOrderAndReentryArePreserved)
{
    ToggleButton toggle(QStringLiteral("Grouped"));
    QButtonGroup group;
    group.setExclusive(false);
    group.addButton(&toggle, 7);
    QStringList events;
    QObject::connect(&toggle, &QPushButton::toggled, &toggle, [&](bool checked) {
        events.append(checked ? QStringLiteral("toggled:on") : QStringLiteral("toggled:off"));
    });
    QObject::connect(&group, &QButtonGroup::idToggled, &toggle, [&](int id, bool checked) {
        EXPECT_EQ(id, 7);
        events.append(checked ? QStringLiteral("group:on") : QStringLiteral("group:off"));
        if (checked)
            toggle.setCheckState(Qt::PartiallyChecked);
    });
    QObject::connect(&toggle, &ToggleButton::checkStateChanged, &toggle, [&](Qt::CheckState state) {
        events.append(QStringLiteral("state:%1").arg(state));
    });

    toggle.setCheckState(Qt::Checked);

    EXPECT_EQ(toggle.checkState(), Qt::PartiallyChecked);
    EXPECT_EQ(events,
        (QStringList{QStringLiteral("toggled:on"), QStringLiteral("group:on"), QStringLiteral("state:1")}));
    events.clear();
    toggle.setCheckState(Qt::Unchecked);
    EXPECT_EQ(events,
        (QStringList{QStringLiteral("toggled:off"), QStringLiteral("group:off"), QStringLiteral("state:0")}));
}

TEST(ToggleButtonTest, Contract_ExclusiveButtonsKeepQtSelectionRules)
{
    for (const bool useGroup : {false, true}) {
        SCOPED_TRACE(useGroup);
        QWidget parent;
        ToggleButton first(QStringLiteral("First"), &parent);
        ToggleButton second(QStringLiteral("Second"), &parent);
        QButtonGroup group;
        if (useGroup) {
            group.addButton(&first);
            group.addButton(&second);
        } else {
            first.setAutoExclusive(true);
            second.setAutoExclusive(true);
        }
        first.setCheckState(Qt::Checked);
        first.click();
        EXPECT_TRUE(first.isChecked());
        EXPECT_EQ(first.checkState(), Qt::Checked);

        second.click();
        EXPECT_FALSE(first.isChecked());
        EXPECT_TRUE(second.isChecked());
        EXPECT_EQ(first.checkState(), Qt::Unchecked);
        EXPECT_EQ(second.checkState(), Qt::Checked);
    }
}

TEST(ToggleButtonTest, Contract_ToggledCallbackCanReverseCheckedState)
{
    for (const bool replaceTriState : {false, true}) {
        SCOPED_TRACE(replaceTriState);
        ToggleButton toggle(QStringLiteral("Toggle"));
        QSignalSpy checkStateSpy(&toggle, &ToggleButton::checkStateChanged);
        QObject::connect(&toggle, &QPushButton::toggled, &toggle, [&](bool checked) {
            if (checked) {
                if (replaceTriState)
                    toggle.setCheckState(Qt::PartiallyChecked);
                toggle.setChecked(false);
            }
        });

        toggle.setCheckState(Qt::Checked);

        EXPECT_FALSE(toggle.isChecked());
        EXPECT_EQ(toggle.checkState(), Qt::Unchecked);
        ASSERT_EQ(checkStateSpy.count(), replaceTriState ? 2 : 1);
        EXPECT_EQ(checkStateSpy.last().at(0).toInt(), Qt::Unchecked);
    }
}

TEST(ToggleButtonTest, Contract_LightAndDarkCheckedStatePaintsDistinctly)
{
    const FluentElement::Theme themes[]{FluentElement::Light, FluentElement::Dark};
    for (const auto theme : themes) {
        FluentElement::setTheme(theme);

        auto grabState = [](bool checked) {
            ToggleButton toggle(QStringLiteral("Toggle"));
            toggle.setChecked(checked);
            toggle.resize(140, 36);
            return toggle.grab().toImage();
        };

        const QImage unchecked = grabState(false);
        const QImage checked = grabState(true);
        ASSERT_FALSE(unchecked.isNull()) << "theme=" << theme;
        ASSERT_EQ(checked.size(), unchecked.size()) << "theme=" << theme;
        EXPECT_NE(checked, unchecked) << "theme=" << theme;
    }

    ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}
