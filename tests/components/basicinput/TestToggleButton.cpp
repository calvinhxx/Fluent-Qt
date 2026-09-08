#include "components/basicinput/ToggleButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPointer>
#include <QSignalSpy>
#include <QVBoxLayout>

#include <gtest/gtest.h>

using namespace fluent;
using namespace fluent::basicinput;

class ToggleButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ToggleButtonTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new ToggleButtonTestWindow();
        window->setFixedSize(600, 600);
        window->setWindowTitle("Fluent ToggleButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic ToggleButton
        layout->addWidget(new QLabel("1. Basic ToggleButton:", window));
        auto* hLayout1 = new QHBoxLayout();
        auto* toggle1 = new ToggleButton("ToggleButton", window);
        auto* label1 = new QLabel("Output: Off", window);

        QObject::connect(toggle1, &ToggleButton::toggled, [label1](bool checked) {
            label1->setText(QString("Output: %1").arg(checked ? "On" : "Off"));
        });

        hLayout1->addWidget(toggle1);
        hLayout1->addWidget(label1);
        hLayout1->addStretch();
        layout->addLayout(hLayout1);

        // 2. Disabled ToggleButton
        layout->addWidget(new QLabel("2. Disabled ToggleButton:", window));
        auto* hLayout2 = new QHBoxLayout();
        auto* toggle2 = new ToggleButton("Disabled Off", window);
        toggle2->setEnabled(false);
        auto* toggle3 = new ToggleButton("Disabled On", window);
        toggle3->setChecked(true);
        toggle3->setEnabled(false);
        hLayout2->addWidget(toggle2);
        hLayout2->addWidget(toggle3);
        hLayout2->addStretch();
        layout->addLayout(hLayout2);

        // 3. Different Sizes
        layout->addWidget(new QLabel("3. Different Sizes:", window));
        auto* hLayout3 = new QHBoxLayout();
        auto* small = new ToggleButton("Small", window);
        small->setFluentSize(Button::Small);
        auto* normal = new ToggleButton("Standard", window);
        normal->setFluentSize(Button::StandardSize);
        auto* large = new ToggleButton("Large", window);
        large->setFluentSize(Button::Large);
        hLayout3->addWidget(small);
        hLayout3->addWidget(normal);
        hLayout3->addWidget(large);
        hLayout3->addStretch();
        layout->addLayout(hLayout3);

        // 4. ThreeState ToggleButton
        layout->addWidget(new QLabel(
            "4. ThreeState ToggleButton (Unchecked -> Checked -> Indeterminate):", window));
        auto* hLayout4 = new QHBoxLayout();
        auto* toggle4 = new ToggleButton("ThreeState", window);
        toggle4->setThreeState(true);
        auto* label4 = new QLabel("State: Unchecked", window);

        QObject::connect(toggle4, &ToggleButton::checkStateChanged, [label4](Qt::CheckState state) {
            QString stateStr = "Unchecked";
            if (state == Qt::Checked)
                stateStr = "Checked";
            else if (state == Qt::PartiallyChecked)
                stateStr = "Indeterminate";
            label4->setText(QString("State: %1").arg(stateStr));
        });

        hLayout4->addWidget(toggle4);
        hLayout4->addWidget(label4);
        hLayout4->addStretch();
        layout->addLayout(hLayout4);

        layout->addStretch();

        // Theme switch button
        auto* themeBtn = new QPushButton("Switch Theme", window);
        layout->addWidget(themeBtn);
        QObject::connect(themeBtn, &QPushButton::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() ==
                                                    fluent::FluentElement::Light
                                                ? fluent::FluentElement::Dark
                                                : fluent::FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    ToggleButtonTestWindow* window = nullptr;
};

TEST_F(ToggleButtonTest, Contract_DefaultsAndPropertySignalsAreNoOpSafe)
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

TEST_F(ToggleButtonTest, Contract_ProgrammaticPartialCheckStateIsPreserved)
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

TEST_F(ToggleButtonTest, Contract_ToggledCallbackCanDeleteButton)
{
    auto* toggle = new ToggleButton(QStringLiteral("Toggle"));
    QPointer<ToggleButton> guard(toggle);
    int checkStateNotifications = 0;
    QObject::connect(toggle, &ToggleButton::checkStateChanged, window,
                     [&] { ++checkStateNotifications; });
    QObject::connect(toggle, &QPushButton::toggled, window, [toggle] { delete toggle; });

    toggle->setCheckState(Qt::Checked);

    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(checkStateNotifications, 0);
}

TEST_F(ToggleButtonTest, Contract_ToggledCallbackCanReplaceCheckState)
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

TEST_F(ToggleButtonTest, Contract_ClickToggledCallbackCanDeleteButton)
{
    for (const bool threeState : {false, true}) {
        SCOPED_TRACE(threeState);
        auto* toggle = new ToggleButton(QStringLiteral("Toggle"));
        toggle->setThreeState(threeState);
        QPointer<ToggleButton> guard(toggle);
        int notifications = 0;
        QObject::connect(toggle, &QPushButton::toggled, window,
                         [toggle, &notifications](bool checked) {
                             EXPECT_TRUE(checked);
                             ++notifications;
                             delete toggle;
                         });

        toggle->click();

        EXPECT_TRUE(guard.isNull());
        EXPECT_EQ(notifications, 1);
    }
}

TEST_F(ToggleButtonTest, Contract_CheckStateCallbackCanDeleteButton)
{
    auto* toggle = new ToggleButton(QStringLiteral("Toggle"));
    QPointer<ToggleButton> guard(toggle);
    QObject::connect(toggle, &ToggleButton::checkStateChanged, window,
                     [toggle](Qt::CheckState) { delete toggle; });

    toggle->setCheckState(Qt::Checked);

    EXPECT_TRUE(guard.isNull());
}

TEST_F(ToggleButtonTest, Contract_BlockedCheckStateUpdatesRemainSilent)
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

TEST_F(ToggleButtonTest, Contract_CheckStateSupportsQueuedConnections)
{
    ToggleButton toggle(QStringLiteral("Toggle"));
    Qt::CheckState received = Qt::Unchecked;
    int notifications = 0;
    QObject::connect(
        &toggle, &ToggleButton::checkStateChanged, window,
        [&](Qt::CheckState state) {
            received = state;
            ++notifications;
        },
        Qt::QueuedConnection);

    toggle.setCheckState(Qt::PartiallyChecked);

    EXPECT_EQ(notifications, 0);
    QCoreApplication::sendPostedEvents(window, QEvent::MetaCall);
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(received, Qt::PartiallyChecked);
}

TEST_F(ToggleButtonTest, Contract_GroupNotificationOrderAndReentryArePreserved)
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
    EXPECT_EQ(events, (QStringList{QStringLiteral("toggled:on"), QStringLiteral("group:on"),
                                   QStringLiteral("state:1")}));
    events.clear();
    toggle.setCheckState(Qt::Unchecked);
    EXPECT_EQ(events, (QStringList{QStringLiteral("toggled:off"), QStringLiteral("group:off"),
                                   QStringLiteral("state:0")}));
}

TEST_F(ToggleButtonTest, Contract_ExclusiveButtonsKeepQtSelectionRules)
{
    for (const bool useGroup : {false, true}) {
        SCOPED_TRACE(useGroup);
        ToggleButton first(QStringLiteral("First"), window);
        ToggleButton second(QStringLiteral("Second"), window);
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

TEST_F(ToggleButtonTest, Contract_ToggledCallbackCanReverseCheckedState)
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

TEST_F(ToggleButtonTest, Contract_LightAndDarkCheckedStatePaintsDistinctly)
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

TEST_F(ToggleButtonTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
