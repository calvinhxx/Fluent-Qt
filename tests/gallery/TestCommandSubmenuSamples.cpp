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

TEST(GalleryCommandSubmenuTest, MenuPageRetainsThreeLevelsAndExclusiveChoices)
{
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("menu"));
    const auto found = std::find_if(samples.cbegin(), samples.cend(), [](const auto& sample) {
        return sample.id == QStringLiteral("menu-cascading-selection");
    });
    ASSERT_NE(found, samples.cend());
    EXPECT_TRUE(found->codeSnippet.contains(QStringLiteral("sortMenu->addMenu(orderMenu)")));
    QWidget host;
    auto* root = found->createPreview(&host);
    root->adjustSize();
    host.resize(800, 500);
    host.show();
    QApplication::processEvents();
    auto* menu = root->findChild<fluent::menus_toolbars::FluentMenu*>();
    ASSERT_NE(menu, nullptr);
    menu->popup(host.mapToGlobal(QPoint(20, 40)));
    auto* sort = menu->actions().first()->menu();
    ASSERT_NE(sort, nullptr);
    menu->setActiveAction(sort->menuAction());
    QTest::keyClick(menu, Qt::Key_Right);
    ASSERT_TRUE(sort->isVisible());
    auto* order = sort->actions().last()->menu();
    ASSERT_NE(order, nullptr);
    sort->setActiveAction(order->menuAction());
    QTest::keyClick(sort, Qt::Key_Right);
    ASSERT_TRUE(order->isVisible());
    QAction* descending = order->actions().last();
    order->setActiveAction(descending);
    QTest::keyClick(order, Qt::Key_Return);
    EXPECT_TRUE(descending->isChecked());
    EXPECT_FALSE(order->actions().first()->isChecked());
    bool statusUpdated = false;
    for (auto* label : root->findChildren<fluent::textfields::Label*>())
        statusUpdated |= label->text() == QStringLiteral("Order: Descending");
    EXPECT_TRUE(statusUpdated);
}

TEST(GalleryCommandSubmenuTest, PreviewTitleAndCaptionStayCenteredAsAGroup)
{
    int groups = 0;
    for (const auto& route : {"menu", "menu-bar", "command-palette", "command-bar", "command-bar-flyout"}) {
        for (const auto& sample : fluent::gallery::gallerySamplesForRoute(QString::fromLatin1(route))) {
            QWidget host;
            host.resize(900, 600);
            auto* root = sample.createPreview(&host);
            root->adjustSize();
            host.show();
            QApplication::processEvents();
            for (auto* text : root->findChildren<QWidget*>(QStringLiteral("Gallery.CommandPreview.Text"))) {
                SCOPED_TRACE(sample.id.toStdString());
                ++groups;
                const qreal center = text->y() + text->height() / 2.0;
                EXPECT_NEAR(center, text->parentWidget()->height() / 2.0, 1.0);
                EXPECT_LE(text->height(), text->sizeHint().height() + 1);
            }
        }
    }
    EXPECT_EQ(groups, 5);
}
