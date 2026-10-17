#include <gtest/gtest.h>

#include <QRegion>
#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QStyle>
#include <QProxyStyle>
#include <QScopeGuard>
#include <QScreen>
#include <QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/MotionPolicy.h"
#include "components/menus_toolbars/Menu.h"
#include "compatibility/private/RuntimePlatformCapabilities_p.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "QtFontComparison.h"
#include "QtTestEnvironment.h"
#include "compatibility/QtCompat.h"

using fluent::ThemeRegistry;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuItem;

namespace {

class ResetSubmenuOnEnterStyle final : public QProxyStyle {
public:
    explicit ResetSubmenuOnEnterStyle(bool resetOnEnter = true, int closeTimeout = 1000)
        : QProxyStyle(QStringLiteral("Fusion")), m_resetOnEnter(resetOnEnter), m_closeTimeout(closeTimeout)
    {}

    int styleHint(StyleHint hint,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr,
        QStyleHintReturn* data = nullptr) const override
    {
        if (hint == SH_Menu_SubMenuResetWhenReenteringParent)
            return m_resetOnEnter;
        if (hint == SH_Menu_SubMenuSloppyCloseTimeout)
            return m_closeTimeout;
        return QProxyStyle::styleHint(hint, option, widget, data);
    }

private:
    bool m_resetOnEnter;
    int m_closeTimeout;
};

class MenuStyleScope final {
public:
    explicit MenuStyleScope(bool resetOnEnter, int closeTimeout = 1000) : m_previous(QApplication::style())
    {
        // QMenu caches these style hints in its constructor. Keep the original
        // style alive and restore the exact object, including unnamed styles.
        m_previous->setParent(nullptr);
        QApplication::setStyle(new ResetSubmenuOnEnterStyle(resetOnEnter, closeTimeout));
    }

    ~MenuStyleScope() { QApplication::setStyle(m_previous); }

private:
    QStyle* m_previous;
};

class RuntimeCapabilitiesScope final {
public:
    explicit RuntimeCapabilitiesScope(const compatibility::detail::RuntimePlatformCapabilities& capabilities)
        : m_previous(compatibility::detail::runtimePlatformCapabilities())
    {
        compatibility::detail::setRuntimePlatformCapabilities(capabilities);
    }

    ~RuntimeCapabilitiesScope() { compatibility::detail::setRuntimePlatformCapabilities(m_previous); }

private:
    compatibility::detail::RuntimePlatformCapabilities m_previous;
};

} // namespace

class MenuTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        ThemeRegistry::instance().resetToDefaults();
    }
};

TEST_F(MenuTest, FontStylePropertiesNotifyOnlyOnChange)
{
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
    EXPECT_EQ(tests::support::normalizedFontFamilies(menu.font()),
        tests::support::normalizedFontFamilies(menu.themeFont(Typography::FontRole::BodyStrong).toQFont()));
    EXPECT_EQ(tests::support::normalizedFontFamilies(item.font()),
        tests::support::normalizedFontFamilies(item.themeFont(Typography::FontRole::Caption).toQFont()));
}

TEST_F(MenuTest, MenuItemRetainsQActionTriggerSemantics)
{
    FluentMenu menu(QStringLiteral("Actions"));
    auto* item = new FluentMenuItem(QStringLiteral("Open"), &menu);
    menu.addAction(item);
    QSignalSpy triggerSpy(item, &QAction::triggered);

    item->trigger();

    EXPECT_EQ(triggerSpy.count(), 1);
    ASSERT_EQ(menu.actions().size(), 1);
    EXPECT_EQ(menu.actions().constFirst(), item);
}

TEST_F(MenuTest, MotionPolicyDisabledSettlesEntranceTransition)
{
    auto capabilities = compatibility::detail::runtimePlatformCapabilities();
    capabilities.translucentPopupSurfaces = true;
    RuntimeCapabilitiesScope capabilityScope(capabilities);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("Open"));
    menu.popup(QPoint(100, 100));

    auto* animation = menu.findChild<QVariantAnimation*>(QStringLiteral("fluentMenuEntranceAnimation"),
        Qt::FindDirectChildrenOnly);
    ASSERT_NE(animation, nullptr);
    EXPECT_EQ(animation->duration(), 0);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    menu.hide();
}

TEST_F(MenuTest, MultipleVisibleActionsContributeIndependentRowsToSizeHint)
{
    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("First action"));
    menu.addAction(QStringLiteral("Second action"));
    menu.addAction(QStringLiteral("Third action"));

    const QMargins margins = menu.contentsMargins();
    const int minimumRowsHeight = 3 * Spacing::ControlHeight::Small + margins.top() + margins.bottom();
    EXPECT_GE(menu.sizeHint().height(), minimumRowsHeight)
        << "A multi-action popup must never collapse to a single WASM row";
}

TEST_F(MenuTest, Contract_CascadeRowsAlignWithoutAccumulatingShadowOffsets)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        QWidget host;
        FluentMenu root(QStringLiteral("View"), &host);
        FluentMenu second(QStringLiteral("Sort by"), &host);
        FluentMenu third(QStringLiteral("Order"), &host);
        root.setLayoutDirection(direction);
        root.addAction(QStringLiteral("First"));
        root.addMenu(&second);
        second.addMenu(&third);
        QAction* leaf = third.addAction(QStringLiteral("Ascending"));
        third.addAction(QStringLiteral("Descending"));
        const int forward = direction == Qt::RightToLeft ? Qt::Key_Left : Qt::Key_Right;
        const int x = direction == Qt::RightToLeft ? screen.right() - root.sizeHint().width() - 40 : screen.left() + 40;
        root.popup(QPoint(x, screen.top() + 100));
        root.setActiveAction(second.menuAction());
        QTest::keyClick(&root, static_cast<Qt::Key>(forward));
        ASSERT_TRUE(QTest::qWaitFor([&] { return second.isVisible(); }));
        second.setActiveAction(third.menuAction());
        QTest::keyClick(&second, static_cast<Qt::Key>(forward));
        ASSERT_TRUE(QTest::qWaitFor([&] { return third.isVisible(); }));
        QTest::qWait(20);
        const int rootCenter = root.mapToGlobal(root.actionGeometry(second.menuAction()).center()).y();
        const int secondCenter = second.mapToGlobal(second.actionGeometry(third.menuAction()).center()).y();
        const int thirdCenter = third.mapToGlobal(third.actionGeometry(leaf).center()).y();
        EXPECT_NEAR(rootCenter, secondCenter, 1);
        EXPECT_NEAR(secondCenter, thirdCenter, 1);
        EXPECT_EQ(second.layoutDirection(), direction);
        EXPECT_EQ(third.layoutDirection(), direction);
        EXPECT_GE(second.width() - second.contentsMargins().left() * 2, 160);
        EXPECT_GE(third.width() - third.contentsMargins().left() * 2, 160);
        root.hide();
        second.hide();
        third.hide();

        root.popup(screen.bottomRight() - QPoint(8, 8));
        root.setActiveAction(second.menuAction());
        QTest::keyClick(&root, static_cast<Qt::Key>(forward));
        ASSERT_TRUE(QTest::qWaitFor([&] { return second.isVisible(); }));
        QTest::qWait(20);
        const int shadow = second.contentsMargins().left();
        EXPECT_TRUE(screen.contains(second.geometry().adjusted(shadow, shadow, -shadow, -shadow)));
        root.hide();
        second.hide();
    }
}

TEST_F(MenuTest, Contract_CheckedItemKeepsSurfaceColorUntilHighlighted)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        FluentMenu menu(QStringLiteral("Order"));
        QAction* checked = menu.addAction(QStringLiteral("Ascending"));
        checked->setCheckable(true);
        checked->setChecked(true);
        menu.addAction(QStringLiteral("Descending"));
        menu.popup(QPoint(100, 100));
        menu.setActiveAction(nullptr);
        QApplication::processEvents();
        const QPoint point(menu.width() - menu.contentsMargins().left() - 12,
            menu.actionGeometry(checked).center().y());
        const auto pixel = [&] {
            const QPixmap image = menu.grab();
            return image.toImage().pixelColor(point * image.devicePixelRatioF());
        };
        EXPECT_EQ(pixel(), menu.themeColorsRef().bgLayer);
        menu.setActiveAction(checked);
        EXPECT_NE(pixel(), menu.themeColorsRef().bgLayer);
        EXPECT_TRUE(checked->isChecked());
        menu.hide();
    }
}

TEST_F(MenuTest, OpaquePopupSurfaceUsesRoundedWindowMask)
{
    auto capabilities = compatibility::detail::runtimePlatformCapabilities();
    capabilities.translucentPopupSurfaces = false;
    RuntimeCapabilitiesScope capabilityScope(capabilities);

    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("Confirm selection"));
    menu.addAction(QStringLiteral("Review changes"));
    menu.popup(QPoint(100, 100));
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }, 5000));
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

TEST_F(MenuTest, RepeatedPopupKeepsStableHeight)
{
    FluentMenu menu(QStringLiteral("Actions"));
    menu.addAction(QStringLiteral("Confirm selection"));
    menu.addAction(QStringLiteral("Review changes"));

    menu.popup(QPoint(100, 100));
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }, 5000));
    QTest::qWait(20);
    const int firstHeight = menu.height();
    const int firstHintHeight = menu.sizeHint().height();
    const QList<QAction*> firstActions = menu.actions();
    const int firstRowHeight = menu.actionGeometry(firstActions.constFirst()).height();

    menu.hide();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible(); }, 5000));

    menu.popup(QPoint(100, 100));
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }, 5000));
    QTest::qWait(20);
    const int secondHeight = menu.height();
    const int secondHintHeight = menu.sizeHint().height();
    const QList<QAction*> secondActions = menu.actions();
    const int secondRowHeight = menu.actionGeometry(secondActions.constFirst()).height();
    menu.hide();

    EXPECT_EQ(firstHeight, firstHintHeight) << "The first popup must discard any transient platform-window excess";
    EXPECT_EQ(secondHeight, secondHintHeight) << "Later popups must continue to follow the settled size hint";
    EXPECT_EQ(firstRowHeight, secondRowHeight) << "Repeated popup must not change the action layout";
    EXPECT_EQ(firstHeight, secondHeight)
        << "The first popup must use the same settled geometry as later opens"
        << "; first hint=" << firstHintHeight << ", second hint=" << secondHintHeight
        << ", first row=" << firstRowHeight << ", second row=" << secondRowHeight;
}

TEST_F(MenuTest, PointerCanCrossIntoSubmenuAndActivateLeaf)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native popup mouse routing.";

    const QPoint previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([previousCursor] { QCursor::setPos(previousCursor); });
    MenuStyleScope styleScope(true);

    QWidget host;
    host.resize(800, 600);
    host.show();
    host.raise();
    host.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
    ASSERT_TRUE(QTest::qWaitForWindowActive(&host));

    FluentMenu menu(QStringLiteral("View options"), &host);
    auto* submenu = new FluentMenu(QStringLiteral("Sort by"), &menu);
    QAction* first = submenu->addAction(QStringLiteral("Name"));
    QAction* second = submenu->addAction(QStringLiteral("Date modified"));
    QAction* third = submenu->addAction(QStringLiteral("Type"));
    QAction* branch = menu.addMenu(submenu);
    menu.addSeparator();
    menu.addAction(QStringLiteral("Compact list"));
    menu.addAction(QStringLiteral("Comfortable list"));
    menu.addAction(QStringLiteral("Large icons"));
    menu.addAction(QStringLiteral("Show hidden files"));

    for (QAction* leaf : {first, second, third}) {
        SCOPED_TRACE(leaf->text().toStdString());
        QTest::mouseMove(&host, QPoint(5, 5));
        menu.popup(host.mapToGlobal(QPoint(40, 80)));
        ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
        QTest::qWait(100);
        const QPoint source = menu.actionGeometry(branch).center();
        QTest::mouseMove(&menu, source - QPoint(24, 0));
        QTest::qWait(20);
        QTest::mouseMove(&menu, source);
        ASSERT_TRUE(QTest::qWaitFor([&] { return submenu->isVisible(); }, 2000));
        const QPoint start = menu.mapToGlobal(source);
        const QPoint finish = submenu->mapToGlobal(submenu->actionGeometry(leaf).center());
        for (int step = 1; step <= 60; ++step) {
            const QPoint point = start + (finish - start) * step / 60;
            QMenu* target = submenu->geometry().contains(point) ? submenu : &menu;
            QTest::mouseMove(target, target->mapFromGlobal(point), 0);
            QTest::qWait(10);
            ASSERT_TRUE(submenu->isVisible()) << "Pointer at " << point.x() << ',' << point.y();
        }
        QTest::qWait(1200);
        ASSERT_TRUE(submenu->isVisible());
        QSignalSpy activated(leaf, &QAction::triggered);
        QTest::mouseClick(submenu, Qt::LeftButton, Qt::NoModifier, submenu->actionGeometry(leaf).center());
        EXPECT_EQ(activated.count(), 1);
        EXPECT_FALSE(submenu->isVisible());
        EXPECT_FALSE(menu.isVisible());
    }
}

TEST_F(MenuTest, ParentEnterAtSubmenuOverlapPreservesCascade)
{
    for (bool resetOnEnter : {false, true}) {
        SCOPED_TRACE(resetOnEnter);
        MenuStyleScope styleScope(resetOnEnter, 80);
        auto capabilities = compatibility::detail::runtimePlatformCapabilities();
        capabilities.translucentPopupSurfaces = true;
        RuntimeCapabilitiesScope capabilityScope(capabilities);
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

        QWidget host;
        host.resize(800, 600);
        host.show();
        if (!tests::support::isHeadlessPlatform()) {
            host.raise();
            host.activateWindow();
            ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
            ASSERT_TRUE(QTest::qWaitForWindowActive(&host));
        }
        FluentMenu menu(QStringLiteral("View"), &host);
        FluentMenu submenu(QStringLiteral("Sort by"), &menu);
        submenu.addAction(QStringLiteral("Name"));
        QAction* leaf = submenu.addAction(QStringLiteral("Date modified"));
        QAction* branch = menu.addMenu(&submenu);
        QAction* sibling = menu.addAction(QStringLiteral("Compact list"));
        menu.popup(host.mapToGlobal(QPoint(40, 80)));
        menu.setActiveAction(branch);
        ASSERT_TRUE(QTest::qWaitFor([&] { return submenu.isVisible(); }));
        if (!tests::support::isHeadlessPlatform()) {
            // Drain native popup exposure events before injecting the overlap sequence.
            ASSERT_TRUE(QTest::qWaitForWindowExposed(&menu));
            ASSERT_TRUE(QTest::qWaitForWindowExposed(&submenu));
            QGuiApplication::sync();
        }
        const QPoint crossing = submenu.mapToGlobal(QPoint(1, submenu.actionGeometry(leaf).center().y()));
        ASSERT_TRUE(menu.frameGeometry().contains(crossing));
        const QPoint childLocal = submenu.mapFromGlobal(crossing);
        QSignalSpy hidden(&submenu, &QMenu::aboutToHide);
        QEnterEvent childEnter(childLocal, childLocal, crossing);
        QApplication::sendEvent(&submenu, &childEnter);
        QEvent childLeave(QEvent::Leave);
        QApplication::sendEvent(&submenu, &childLeave);
        const QPoint parentLocal = menu.mapFromGlobal(crossing);
        QEnterEvent parentEnter(parentLocal, parentLocal, crossing);
        QApplication::sendEvent(&menu, &parentEnter);
        EXPECT_TRUE(submenu.isVisible()) << "Crossing overlapping popup shadows must not close the child";
        EXPECT_EQ(menu.activeAction(), branch);

        for (int step = 0; step < 3; ++step) {
            const QPoint local = menu.mapFromGlobal(crossing + QPoint(0, step));
            FLUENT_MAKE_MOUSE_EVENT(move, QEvent::MouseMove, &menu, local, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&menu, &move);
        }
        QTest::qWait(160);
        EXPECT_TRUE(submenu.isVisible()) << "Follow-up mouse moves and the close timer must keep the child open";
        EXPECT_EQ(menu.activeAction(), branch);
        EXPECT_EQ(hidden.count(), 0);

        const QPoint leafPosition = submenu.actionGeometry(leaf).center();
        for (int step = 0; step < 8; ++step) {
            const QPoint local = menu.mapFromGlobal(submenu.mapToGlobal(leafPosition));
            FLUENT_MAKE_MOUSE_EVENT(move, QEvent::MouseMove, &menu, local, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&menu, &move);
        }
        EXPECT_EQ(submenu.activeAction(), leaf);

        const QPoint back = menu.mapToGlobal(menu.actionGeometry(sibling).center());
        ASSERT_FALSE(submenu.frameGeometry().contains(back));
        const QPoint backLocal = menu.mapFromGlobal(back);
        QEnterEvent backEnter(backLocal, backLocal, back);
        QApplication::sendEvent(&menu, &backEnter);
        EXPECT_EQ(submenu.isVisible(), !resetOnEnter) << "Real parent reentry must retain the native style policy";
        for (int step = 0; step < 8; ++step) {
            FLUENT_MAKE_MOUSE_EVENT(move, QEvent::MouseMove, &menu, backLocal, Qt::NoButton, Qt::NoButton,
                Qt::NoModifier);
            QApplication::sendEvent(&menu, &move);
        }
        QTest::qWait(160);
        EXPECT_FALSE(submenu.isVisible());
        EXPECT_EQ(menu.activeAction(), sibling);
        EXPECT_TRUE(menu.isVisible());
        menu.hide();
    }
}
