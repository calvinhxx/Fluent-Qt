#include "compatibility/WidgetCompat.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "SelectionIndicatorTestUtils.h"
#include <QApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <gtest/gtest.h>

using namespace fluent;
using namespace fluent::basicinput;

TEST(CheckBoxTest, Contract_DefaultsAndPropertySignalsAreNoOpSafe)
{
    CheckBox checkBox(QStringLiteral("Option"));
    EXPECT_EQ(checkBox.boxSize(), 20);
    EXPECT_EQ(checkBox.boxMargin(), 8);
    EXPECT_EQ(checkBox.textGap(), 8);
    EXPECT_FALSE(checkBox.hoverBackgroundEnabled());

    QSignalSpy boxSizeSpy(&checkBox, &CheckBox::boxSizeChanged);
    QSignalSpy boxMarginSpy(&checkBox, &CheckBox::boxMarginChanged);
    QSignalSpy textGapSpy(&checkBox, &CheckBox::textGapChanged);
    QSignalSpy hoverSpy(&checkBox, &CheckBox::hoverBackgroundEnabledChanged);

    checkBox.setBoxSize(24);
    checkBox.setBoxSize(24);
    checkBox.setBoxMargin(12);
    checkBox.setBoxMargin(12);
    checkBox.setTextGap(10);
    checkBox.setTextGap(10);
    checkBox.setHoverBackgroundEnabled(true);
    checkBox.setHoverBackgroundEnabled(true);

    EXPECT_EQ(checkBox.boxSize(), 24);
    EXPECT_EQ(checkBox.boxMargin(), 12);
    EXPECT_EQ(checkBox.textGap(), 10);
    EXPECT_TRUE(checkBox.hoverBackgroundEnabled());
    EXPECT_EQ(boxSizeSpy.count(), 1);
    EXPECT_EQ(boxMarginSpy.count(), 1);
    EXPECT_EQ(textGapSpy.count(), 1);
    EXPECT_EQ(hoverSpy.count(), 1);
}

TEST(CheckBoxTest, Contract_ProgrammaticTriStateIsPreserved)
{
    CheckBox checkBox(QStringLiteral("Three-state"));
    checkBox.setTristate(true);

    int changeCount = 0;
    Qt::CheckState lastState = Qt::Unchecked;
    fluentConnectCheckStateChanged(&checkBox, &checkBox, [&changeCount, &lastState](Qt::CheckState state) {
        ++changeCount;
        lastState = state;
    });

    checkBox.setCheckState(Qt::PartiallyChecked);

    EXPECT_EQ(checkBox.checkState(), Qt::PartiallyChecked);
    EXPECT_EQ(changeCount, 1);
    EXPECT_EQ(lastState, Qt::PartiallyChecked);
}

TEST(CheckBoxTest, Contract_LightAndDarkCheckStatesPaintDistinctly)
{
    const FluentElement::Theme themes[]{FluentElement::Light, FluentElement::Dark};
    for (const auto theme : themes) {
        FluentElement::setTheme(theme);

        auto grabState = [](Qt::CheckState state) {
            CheckBox checkBox(QStringLiteral("Sample"));
            checkBox.setTristate(true);
            checkBox.setCheckState(state);
            checkBox.resize(checkBox.sizeHint().expandedTo(QSize(200, 32)));
            return checkBox.grab().toImage();
        };

        const QImage unchecked = grabState(Qt::Unchecked);
        const QImage checked = grabState(Qt::Checked);
        const QImage partial = grabState(Qt::PartiallyChecked);
        ASSERT_FALSE(unchecked.isNull()) << "theme=" << theme;
        ASSERT_EQ(checked.size(), unchecked.size()) << "theme=" << theme;
        ASSERT_EQ(partial.size(), unchecked.size()) << "theme=" << theme;
        EXPECT_NE(checked, unchecked) << "theme=" << theme;
        EXPECT_NE(partial, unchecked) << "theme=" << theme;
    }

    ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}

TEST(CheckBoxTest, Contract_UncheckedOutlineRemainsVisibleWithoutHover)
{
    for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        SCOPED_TRACE(::testing::Message() << "theme=" << theme);
        CheckBox checkBox(QStringLiteral(""));
        checkBox.setProperty("fluentThemeOverride", static_cast<int>(theme));
        checkBox.resize(48, 32);
        const QRect indicator(checkBox.boxMargin(), (checkBox.height() - checkBox.boxSize()) / 2, checkBox.boxSize(),
            checkBox.boxSize());
        testutils::expectVisibleUncheckedOutline(checkBox, indicator, checkBox.themeColors().bgLayer,
            theme == FluentElement::HighContrast);
    }
}

TEST(CheckBoxTest, Contract_KeyboardFocusHasOneFluentCueAndMouseClearsIt)
{
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* before = new Button(QStringLiteral("Before"), &host);
    auto* check = new CheckBox(QStringLiteral("Option"), &host);
    layout->addWidget(before);
    layout->addWidget(check);
    host.show();
    QApplication::setActiveWindow(&host);
    before->setFocus();
    ASSERT_TRUE(QTest::qWaitFor([before] { return before->hasFocus(); }, 1000));
    QTest::keyClick(before, Qt::Key_Tab);
    ASSERT_TRUE(check->hasFocus());
#ifdef Q_OS_MAC
    EXPECT_FALSE(check->testAttribute(Qt::WA_MacShowFocusRect));
#endif
    const QImage focused = check->grab().toImage();

    QTest::keyClick(check, Qt::Key_Space);
    EXPECT_EQ(check->checkState(), Qt::Checked);
    QTest::mouseClick(check, Qt::LeftButton, Qt::NoModifier, check->rect().center());
    EXPECT_EQ(check->checkState(), Qt::Unchecked);
    const QImage mouse = check->grab().toImage();
    const qreal dpr = focused.devicePixelRatio();
    const QRect topEdge(qRound(4 * dpr), 0, qRound((check->width() - 8) * dpr), qRound(3 * dpr));
    EXPECT_NE(focused.copy(topEdge), mouse.copy(topEdge));
}

TEST(CheckBoxTest, Contract_ZeroMarginLayoutContainsCustomPaintedBounds)
{
    QWidget host;
    QVBoxLayout layout(&host);
    layout.setContentsMargins(0, 0, 0, 0);
    CheckBox check(QStringLiteral("Option"));
    layout.addWidget(&check);
    host.resize(240, 80);
    host.show();
    QApplication::processEvents();
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        host.setLayoutDirection(direction);
        layout.activate();
        EXPECT_TRUE(host.contentsRect().contains(check.geometry())) << check.geometry().x();
    }
}
