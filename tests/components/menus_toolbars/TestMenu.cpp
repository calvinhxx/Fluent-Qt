#include <gtest/gtest.h>

#include <QtTest/QSignalSpy>

#include "components/foundation/ThemeRegistry.h"
#include "components/menus_toolbars/Menu.h"
#include "design/Spacing.h"
#include "design/Typography.h"

using fluent::ThemeRegistry;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuItem;

class MenuTest : public ::testing::Test {
protected:
    void TearDown() override {
        ThemeRegistry::instance().resetToDefaults();
    }
};

TEST_F(MenuTest, FontStylePropertiesNotifyOnlyOnChange) {
    FluentMenu menu(QStringLiteral("Actions"));
    FluentMenuItem item(QStringLiteral("Open"), &menu);
    QSignalSpy menuSpy(&menu, &FluentMenu::fontStyleChanged);
    QSignalSpy itemSpy(&item, &FluentMenuItem::fontStyleChanged);

    EXPECT_EQ(menu.fontStyle(), Typography::FontRole::Body);
    EXPECT_EQ(item.fontStyle(), Typography::FontRole::Body);

    menu.setFontStyle(Typography::FontRole::BodyStrong);
    item.setFontStyle(Typography::FontRole::Caption);
    EXPECT_EQ(menu.fontStyle(), Typography::FontRole::BodyStrong);
    EXPECT_EQ(item.fontStyle(), Typography::FontRole::Caption);
    EXPECT_EQ(menuSpy.count(), 1);
    EXPECT_EQ(itemSpy.count(), 1);

    menu.setFontStyle(Typography::FontRole::BodyStrong);
    item.setFontStyle(Typography::FontRole::Caption);
    EXPECT_EQ(menuSpy.count(), 1);
    EXPECT_EQ(itemSpy.count(), 1);
    EXPECT_EQ(menu.font(), menu.themeFont(Typography::FontRole::BodyStrong).toQFont());
    EXPECT_EQ(item.font(), item.themeFont(Typography::FontRole::Caption).toQFont());
}

TEST_F(MenuTest, MenuItemRetainsQActionTriggerSemantics) {
    FluentMenu menu(QStringLiteral("Actions"));
    auto* item = new FluentMenuItem(QStringLiteral("Open"), &menu);
    menu.addAction(item);
    QSignalSpy triggerSpy(item, &QAction::triggered);

    item->trigger();

    EXPECT_EQ(triggerSpy.count(), 1);
    ASSERT_EQ(menu.actions().size(), 1);
    EXPECT_EQ(menu.actions().constFirst(), item);
}

TEST_F(MenuTest, MultipleVisibleActionsContributeIndependentRowsToSizeHint) {
    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("First action"));
    menu.addAction(QStringLiteral("Second action"));
    menu.addAction(QStringLiteral("Third action"));

    const QMargins margins = menu.contentsMargins();
    const int minimumRowsHeight =
        3 * Spacing::ControlHeight::Small
        + margins.top() + margins.bottom();
    EXPECT_GE(menu.sizeHint().height(), minimumRowsHeight)
        << "A multi-action popup must never collapse to a single WASM row";
}
