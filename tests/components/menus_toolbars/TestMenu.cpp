#include <gtest/gtest.h>

#include <QRegion>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "components/foundation/ThemeRegistry.h"
#include "components/menus_toolbars/Menu.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"

using fluent::ThemeRegistry;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuItem;

namespace {

class RuntimeCapabilitiesScope final {
public:
    explicit RuntimeCapabilitiesScope(
        const compatibility::detail::RuntimePlatformCapabilities& capabilities)
        : m_previous(compatibility::detail::runtimePlatformCapabilities())
    {
        compatibility::detail::setRuntimePlatformCapabilities(capabilities);
    }

    ~RuntimeCapabilitiesScope()
    {
        compatibility::detail::setRuntimePlatformCapabilities(m_previous);
    }

private:
    compatibility::detail::RuntimePlatformCapabilities m_previous;
};

} // namespace

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

TEST_F(MenuTest, OpaquePopupSurfaceUsesRoundedWindowMask) {
    auto capabilities =
        compatibility::detail::runtimePlatformCapabilities();
    capabilities.translucentPopupSurfaces = false;
    RuntimeCapabilitiesScope capabilityScope(capabilities);

    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("Confirm selection"));
    menu.addAction(QStringLiteral("Review changes"));
    menu.popup(QPoint(100, 100));
    QTRY_VERIFY(menu.isVisible());
    QTest::qWait(20);

    const QRegion surfaceMask = menu.mask();
    ASSERT_FALSE(surfaceMask.isEmpty());
    EXPECT_FALSE(surfaceMask.contains(menu.rect().topLeft()));
    EXPECT_FALSE(surfaceMask.contains(menu.rect().topRight()));
    EXPECT_FALSE(surfaceMask.contains(menu.rect().bottomRight()));
    EXPECT_FALSE(surfaceMask.contains(menu.rect().bottomLeft()));
    EXPECT_TRUE(surfaceMask.contains(menu.rect().center()));
    menu.hide();
}

TEST_F(MenuTest, RepeatedPopupKeepsStableHeight) {
    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("Confirm selection"));
    menu.addAction(QStringLiteral("Review changes"));

    menu.popup(QPoint(100, 100));
    QTRY_VERIFY(menu.isVisible());
    QTest::qWait(20);
    const int firstHeight = menu.height();
    const int firstHintHeight = menu.sizeHint().height();
    const QList<QAction*> firstActions = menu.actions();
    const int firstRowHeight = menu.actionGeometry(firstActions.constFirst()).height();

    menu.hide();
    QTRY_VERIFY(!menu.isVisible());

    menu.popup(QPoint(100, 100));
    QTRY_VERIFY(menu.isVisible());
    QTest::qWait(20);
    const int secondHeight = menu.height();
    const int secondHintHeight = menu.sizeHint().height();
    const QList<QAction*> secondActions = menu.actions();
    const int secondRowHeight = menu.actionGeometry(secondActions.constFirst()).height();
    menu.hide();

    EXPECT_EQ(firstHeight, firstHintHeight)
        << "The first popup must discard any transient platform-window excess";
    EXPECT_EQ(secondHeight, secondHintHeight)
        << "Later popups must continue to follow the settled size hint";
    EXPECT_EQ(firstRowHeight, secondRowHeight)
        << "Repeated popup must not change the action layout";
    EXPECT_EQ(firstHeight, secondHeight)
        << "The first popup must use the same settled geometry as later opens"
        << "; first hint=" << firstHintHeight
        << ", second hint=" << secondHintHeight
        << ", first row=" << firstRowHeight
        << ", second row=" << secondRowHeight;
}
