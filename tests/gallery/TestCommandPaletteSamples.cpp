#include <gtest/gtest.h>
#include <algorithm>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QTest>
#include <QTreeView>

#include "components/basicinput/CheckBox.h"
#include "components/basicinput/Slider.h"

#include "components/collections/ListView.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandPalette.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/shell/GalleryWindow.h"
#include "viewmodel/GallerySettings.h"
#include "view/pages/GalleryContentPage.h"

TEST(GalleryCommandPaletteTest, Contract_SharedActionsAndEditingSamples)
{
    using fluent::menus_toolbars::CommandPalette;
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("command-palette"));
    ASSERT_EQ(samples.size(), 3);
    for (const auto& sample : samples) {
        SCOPED_TRACE(sample.id.toStdString());
        EXPECT_TRUE(sample.codeSnippet.contains(QStringLiteral("palette->addCommand")));
        QWidget host;
        host.resize(900, 700);
        QWidget* root = sample.createPreview(&host);
        root->setGeometry(20, 20, 700, 400);
        auto* palette = root->findChild<CommandPalette*>();
        ASSERT_NE(palette, nullptr);
        palette->setAnimationEnabled(false);
        host.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
        host.activateWindow();
        ASSERT_TRUE(QTest::qWaitForWindowActive(&host));
        if (sample.id == QStringLiteral("command-palette-basic")) {
            EXPECT_EQ(palette->commands().size(), 4);
            root->findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandPalette.OpenBasic"))->click();
            palette->setQuery(QStringLiteral("inbox"));
            ASSERT_TRUE(palette->triggerCurrent());
            EXPECT_EQ(
                root->findChild<fluent::textfields::Label*>(QStringLiteral("Gallery.CommandPalette.BasicLocation"))
                    ->text(),
                QStringLiteral("Current page: Inbox"));
        } else if (sample.id.endsWith(QStringLiteral("shared-actions"))) {
            EXPECT_EQ(palette->commands().size(), 8);
            EXPECT_DOUBLE_EQ(palette->backdropBlurRadius(), 8.0);
            EXPECT_DOUBLE_EQ(palette->dimOpacity(), 0.12);
            auto* bar = root->findChild<fluent::menus_toolbars::CommandBar*>();
            auto* menu = root->findChild<fluent::menus_toolbars::FluentMenu*>();
            ASSERT_NE(bar, nullptr);
            ASSERT_NE(menu, nullptr);
            QAction* details = bar->primaryActions().last();
            EXPECT_TRUE(menu->actions().contains(details));
            EXPECT_TRUE(palette->commands().contains(details));
            root->findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandPalette.Open"))->click();
            palette->setQuery(QStringLiteral("properties"));
            EXPECT_EQ(palette->currentAction(), details);
            ASSERT_TRUE(palette->triggerCurrent());
            EXPECT_TRUE(details->isChecked());
            auto* list = root->findChild<fluent::collections::ListView*>(
                QStringLiteral("Gallery.CommandPalette.Notes"));
            ASSERT_NE(list, nullptr);
            EXPECT_EQ(list->model()->rowCount(), 0);
            palette->open();
            palette->setQuery(QStringLiteral("new"));
            ASSERT_TRUE(palette->triggerCurrent());
            EXPECT_EQ(list->model()->rowCount(), 1);
            EXPECT_EQ(
                root->findChild<fluent::textfields::Label*>(QStringLiteral("Gallery.CommandPalette.Status"))->text(),
                QStringLiteral("1 note"));
            palette->setQuery(QStringLiteral("clear"));
            ASSERT_NE(palette->currentAction(), nullptr);
            EXPECT_TRUE(palette->currentAction()->isEnabled());
            palette->open();
            ASSERT_TRUE(palette->triggerCurrent());
            EXPECT_EQ(list->model()->rowCount(), 0);
            EXPECT_EQ(palette->currentAction(), nullptr);
        } else {
            root->findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandPalette.OpenEditing"))->click();
            auto* editor = root->findChild<fluent::textfields::LineEdit*>(
                QStringLiteral("Gallery.CommandPalette.Editor"));
            auto* search = palette->findChild<fluent::textfields::LineEdit*>();
            ASSERT_NE(editor, nullptr);
            ASSERT_NE(search, nullptr);
            QTest::keyClicks(search, "copy");
            QTest::keyClick(search, Qt::Key_Return);
            EXPECT_FALSE(palette->isOpen());
            EXPECT_EQ(QApplication::clipboard()->text(), QStringLiteral("Review"));
            EXPECT_EQ(editor->selectedText(), QStringLiteral("Review"));
            EXPECT_TRUE(editor->hasFocus());
        }
    }
}

TEST(GalleryCommandPaletteTest, Contract_FullGallerySurfaceAndEditingRecovery)
{
    const QVariant previousAutomated = qApp->property("fluentqtGalleryAutomated");
    struct RestoreProperty {
        QVariant value;
        ~RestoreProperty() { qApp->setProperty("fluentqtGalleryAutomated", value); }
    } restore{previousAutomated};
    qApp->setProperty("fluentqtGalleryAutomated", true);
    fluent::gallery::GallerySettings::instance().setIntroCompleted(true);
    fluent::gallery::GalleryWindow window;
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Mica);
    window.resize(1144, 760);
    window.show();
    window.activateWindow();
    ASSERT_TRUE(
        QTest::qWaitFor([&]() { return !window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")); }, 60000));
    ASSERT_TRUE(window.selectRoute(QStringLiteral("command-palette")));
    QAbstractButton* open = nullptr;
    ASSERT_TRUE(QTest::qWaitFor([&]() {
        open = window.findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandPalette.OpenEditing"));
        return open && open->isVisible() && window.currentContentPage()
            && window.currentContentPage()->routeId() == QStringLiteral("command-palette");
    }));
    auto* editor = window.findChild<fluent::textfields::LineEdit*>(QStringLiteral("Gallery.CommandPalette.Editor"));
    auto* palette = window.findChild<fluent::menus_toolbars::CommandPalette*>(
        QStringLiteral("Gallery.CommandPalette.Editing"));
    ASSERT_NE(editor, nullptr);
    ASSERT_NE(palette, nullptr);
    palette->setAnimationEnabled(false);
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        palette->setTheme(theme);
        open->click();
        auto* search = palette->findChild<fluent::textfields::LineEdit*>(QStringLiteral("FluentCommandPalette.Search"));
        auto* view = palette->findChild<fluent::collections::ListView*>();
        ASSERT_TRUE(palette->isOpen());
        EXPECT_LT(palette->height(), 340);
        EXPECT_EQ(palette->parentWidget(), &window);
        ASSERT_NE(palette->currentAction(), nullptr);
        EXPECT_TRUE(palette->currentAction()->isEnabled());
        QApplication::processEvents();
        const QPixmap snapshot = palette->grab();
        const QPoint sample = view->viewport()->mapTo(palette, QPoint(1, view->viewport()->height() - 2));
        EXPECT_EQ(snapshot.toImage().pixelColor(sample * snapshot.devicePixelRatio()),
            palette->themeColorsRef().bgLayer);
        QTest::keyClicks(search, "copy");
        QTest::keyClick(search, Qt::Key_Return);
        EXPECT_FALSE(palette->isOpen());
        EXPECT_EQ(QApplication::clipboard()->text(), QStringLiteral("Review"));
        EXPECT_TRUE(editor->hasFocus());
    }
    editor->clear();
    EXPECT_FALSE(open->isEnabled());
    window.findChild<QAbstractButton*>(QStringLiteral("Gallery.CommandPalette.ResetEditing"))->click();
    EXPECT_EQ(editor->text(), QStringLiteral("Review the release notes"));
    EXPECT_TRUE(open->isEnabled());
    auto* workspace = window.findChild<fluent::menus_toolbars::CommandPalette*>(
        QStringLiteral("Gallery.CommandPalette.Shared"));
    ASSERT_NE(workspace, nullptr);
    workspace->setAnimationEnabled(false);
    auto* navigation = window.findChild<QTreeView*>(QStringLiteral("galleryFooterNavigationTreeView"));
    ASSERT_NE(navigation, nullptr);
    const QModelIndex settings = navigation->model()->index(0, 0);
    ASSERT_TRUE(settings.isValid());
    ASSERT_EQ(settings.data().toString(), QStringLiteral("Settings"));
    ASSERT_TRUE(navigation->isVisible());
    const auto target = [&] {
        return fluent::overlay::presentedPointInTopLevel(navigation->viewport(),
            navigation->visualRect(settings).center());
    };
    for (const auto policy :
        {fluent::dialogs_flyouts::Popup::CloseOnPressOutside, fluent::dialogs_flyouts::Popup::NoAutoClose}) {
        workspace->setClosePolicy(policy);
        workspace->open();
        // Deliver through QWindow: a direct QWidget click misses scrim removal
        // before Qt chooses the background receiver.
        // zh_CN: 经 QWindow 分发，覆盖 Qt 选择接收控件前遮罩被移除的路径。
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, target());
        EXPECT_EQ(window.currentRouteId(), QStringLiteral("command-palette"));
        EXPECT_EQ(workspace->isOpen(), policy == fluent::dialogs_flyouts::Popup::NoAutoClose);
        workspace->close();
    }
    QApplication::processEvents();
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, target());
    ASSERT_TRUE(QTest::qWaitFor([&] { return window.currentRouteId() == QStringLiteral("settings"); }));
}

TEST(GalleryCommandPaletteTest, Contract_PopupOwnsBackdropControls)
{
    QWidget host;
    host.resize(900, 700);
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("popup"));
    const auto sample = std::find_if(samples.begin(), samples.end(), [](const auto& item) {
        return item.id == QStringLiteral("popup-modal-dim");
    });
    ASSERT_NE(sample, samples.end());
    QWidget* root = sample->createPreview(&host);
    root->setGeometry(20, 20, 700, 180);
    host.show();
    QApplication::processEvents();
    root->findChild<QAbstractButton*>(QStringLiteral("Gallery.Popup.OpenEffects"))->click();
    auto* popup = host.findChild<fluent::dialogs_flyouts::Popup*>(QStringLiteral("Gallery.Popup.Effects"));
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->isModal());
    auto* blur = popup->findChild<fluent::basicinput::Slider*>(QStringLiteral("Gallery.Popup.Blur"));
    auto* dim = popup->findChild<fluent::basicinput::Slider*>(QStringLiteral("Gallery.Popup.Dim"));
    ASSERT_NE(blur, nullptr);
    ASSERT_NE(dim, nullptr);
    blur->setValue(12);
    dim->setValue(25);
    EXPECT_DOUBLE_EQ(popup->backdropBlurRadius(), 12.0);
    EXPECT_DOUBLE_EQ(popup->dimOpacity(), 0.25);
    auto* counter = root->findChild<QAbstractButton*>(QStringLiteral("Gallery.Popup.Background"));
    auto* status = root->findChild<fluent::textfields::Label*>(QStringLiteral("Gallery.Popup.Status"));
    const QPoint point = counter->mapTo(&host, counter->rect().center());
    QTest::mouseClick(host.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(status->text(), QStringLiteral("Background clicks: 0"));
    EXPECT_TRUE(popup->isOpen());
    popup->findChild<fluent::basicinput::CheckBox*>()->setChecked(false);
    QTest::mouseClick(host.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(status->text(), QStringLiteral("Background clicks: 1"));
    EXPECT_TRUE(popup->isOpen());
    popup->setAnimationEnabled(false);
    popup->close();
}
