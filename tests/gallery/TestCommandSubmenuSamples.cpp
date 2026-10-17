#include <gtest/gtest.h>

#include <algorithm>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/Label.h"
#include "view/widgets/GallerySampleCatalog.h"

TEST(GalleryCommandSubmenuTest, Contract_CommandSubmenuSamplesExecuteTheirSharedChoices)
{
    for (const QString& route : {QStringLiteral("command-bar"), QStringLiteral("command-bar-flyout")}) {
        SCOPED_TRACE(route.toStdString());
        const QString id = route + QStringLiteral("-submenus");
        const auto samples = fluent::gallery::gallerySamplesForRoute(route);
        const auto found = std::find_if(samples.cbegin(), samples.cend(), [&](const auto& sample) {
            return sample.id == id;
        });
        ASSERT_NE(found, samples.cend());
        EXPECT_NE(found, samples.cbegin());
        EXPECT_EQ(samples.constFirst().id,
            route == QStringLiteral("command-bar")
                ? QStringLiteral("command-bar-responsive-overflow")
                : QStringLiteral("command-bar-flyout-show-modes"));
        QWidget defaultHost;
        auto* defaultPreview = samples.constFirst().createPreview(&defaultHost);
        ASSERT_NE(defaultPreview, nullptr);
        auto* defaultBar = defaultPreview->findChild<fluent::menus_toolbars::CommandBar*>();
        auto* defaultFlyout = defaultPreview->findChild<fluent::menus_toolbars::CommandBarFlyout*>();
        ASSERT_TRUE(defaultBar || defaultFlyout);
        const auto defaults = defaultBar
            ? defaultBar->primaryActions() + defaultBar->secondaryActions()
            : defaultFlyout->primaryActions() + defaultFlyout->secondaryActions();
        ASSERT_FALSE(defaults.isEmpty());
        for (QAction* action : defaults)
            EXPECT_EQ(action->menu(), nullptr);
        EXPECT_TRUE(found->codeSnippet.contains(QStringLiteral("view->menuAction()")));
        EXPECT_TRUE(found->codeSnippet.contains(QStringLiteral("sort->addMenu(order)")));
        QWidget window;
        window.resize(900, 650);
        auto* preview = found->createPreview(&window);
        ASSERT_NE(preview, nullptr);
        preview->setGeometry(30, 30, 700, 250);
        window.show();
        QApplication::processEvents();
        auto* flyout = preview->findChild<fluent::menus_toolbars::CommandBarFlyout*>();
        auto* bar = preview->findChild<fluent::menus_toolbars::CommandBar*>();
        if (flyout) {
            flyout->setAnimationEnabled(false);
            auto* open = preview->findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandSubmenus.Open"));
            ASSERT_NE(open, nullptr);
            open->click();
        }
        auto* menu = preview->findChild<fluent::menus_toolbars::FluentMenu*>(
            QStringLiteral("Gallery.CommandSubmenus.View"));
        ASSERT_NE(menu, nullptr);
        QAction* branch = menu->menuAction();
        QAbstractButton* presenter = nullptr;
        for (auto* button : window.findChildren<QAbstractButton*>()) {
            if (button->property("commandAction").value<QObject*>() == branch && button->isVisible())
                presenter = button;
        }
        ASSERT_NE(presenter, nullptr);
        presenter->click();
        ASSERT_TRUE(menu->isVisible());
        auto* second = menu->actions().first()->menu();
        ASSERT_NE(second, nullptr);
        menu->setActiveAction(second->menuAction());
        QTest::keyClick(menu, Qt::Key_Right);
        ASSERT_TRUE(second->isVisible());
        auto* third = second->actions().first()->menu();
        ASSERT_NE(third, nullptr);
        second->setActiveAction(third->menuAction());
        QTest::keyClick(second, Qt::Key_Right);
        ASSERT_TRUE(third->isVisible());
        QAction* descending = third->actions().last();
        QSignalSpy triggered(descending, &QAction::triggered);
        third->setActiveAction(descending);
        QTest::keyClick(third, Qt::Key_Return);
        QApplication::processEvents();
        EXPECT_EQ(triggered.count(), 1);
        EXPECT_TRUE(descending->isChecked());
        auto* status = preview->findChild<fluent::textfields::Label*>(QStringLiteral("Gallery.CommandSubmenus.Status"));
        ASSERT_NE(status, nullptr);
        EXPECT_EQ(status->text(), QStringLiteral("Order: Descending"));
        EXPECT_FALSE(menu->isVisible());
        if (flyout)
            EXPECT_FALSE(flyout->isOpen());
        if (bar) {
            auto* compact = preview->findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandSubmenus.Compact"));
            ASSERT_NE(compact, nullptr);
            const QSize buttonSize = compact->size();
            const QSize statusSize = status->size();
            compact->click();
            QApplication::processEvents();
            EXPECT_TRUE(bar->overflowedPrimaryActions().contains(branch));
            EXPECT_EQ(bar->width(), 80);
            EXPECT_EQ(compact->size(), buttonSize);
            EXPECT_GE(compact->width(), compact->sizeHint().width());
            EXPECT_EQ(status->size(), statusSize);
            EXPECT_GE(status->width(), status->fontMetrics().horizontalAdvance(status->text()));
            compact->click();
            QApplication::processEvents();
            EXPECT_EQ(bar->width(), 480);
            EXPECT_EQ(compact->size(), buttonSize);
            EXPECT_EQ(status->size(), statusSize);
        }
    }
}
