#include <gtest/gtest.h>
#include <QApplication>
#include <QAbstractButton>
#include <QTest>
#include <QTreeView>
#include <algorithm>
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/Slider.h"
#include "components/collections/DrawerView.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/textfields/Label.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "view/pages/GalleryContentPage.h"
#include "view/shell/GalleryWindow.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "viewmodel/GallerySettings.h"

namespace {
struct Example {
    QString route, sample, prefix;
};
const QList<Example> examples{
    {"popup", "popup-modal-dim", "Gallery.Popup"},
    {"dialog", "dialog-backdrop-effects", "Gallery.Dialog"},
    {"drawer-view", "drawer-view-backdrop-effects", "Gallery.Drawer"},
};

template <class Surface> void verifyControls(QWidget& host, QWidget* root, Surface* overlay, const QString& prefix)
{
    ASSERT_NE(overlay, nullptr);
    overlay->setAnimationEnabled(false);
    EXPECT_TRUE(overlay->isModal());
    auto* blur = overlay->template findChild<fluent::basicinput::Slider*>(prefix + ".Blur");
    auto* dim = overlay->template findChild<fluent::basicinput::Slider*>(prefix + ".Dim");
    ASSERT_NE(blur, nullptr);
    ASSERT_NE(dim, nullptr);
    blur->setValue(12);
    dim->setValue(25);
    EXPECT_EQ(overlay->backdropBlurRadius(), 12.0);
    EXPECT_EQ(overlay->dimOpacity(), 0.25);
    overlay->template findChild<fluent::basicinput::CheckBox*>(prefix + ".DimEnabled")->setChecked(false);
    EXPECT_FALSE(overlay->isDim());
    EXPECT_TRUE(overlay->isModal());
    auto* counter = root->findChild<QAbstractButton*>(prefix + ".Background");
    auto* status = root->findChild<fluent::textfields::Label*>(prefix + ".Status");
    const QPoint target = counter->mapTo(&host, counter->rect().center());
    QTest::mouseClick(host.windowHandle(), Qt::LeftButton, Qt::NoModifier, target);
    EXPECT_TRUE(overlay->isOpen());
    EXPECT_EQ(status->text(), QStringLiteral("Background clicks: 0"));
    auto* modal = overlay->template findChild<fluent::basicinput::CheckBox*>(prefix + ".Modal");
    modal->setChecked(false);
    QTest::mouseClick(host.windowHandle(), Qt::LeftButton, Qt::NoModifier, target);
    EXPECT_TRUE(overlay->isOpen());
    EXPECT_EQ(status->text(), QStringLiteral("Background clicks: 1"));
    modal->setChecked(true);
    overlay->template findChild<fluent::basicinput::CheckBox*>(prefix + ".CloseOutside")->setChecked(true);
    EXPECT_TRUE(overlay->closePolicy().testFlag(Surface::CloseOnPressOutside));
    const QPointer<Surface> guard(overlay);
    QTest::mouseClick(host.windowHandle(), Qt::LeftButton, Qt::NoModifier, target);
    EXPECT_TRUE(!guard || !guard->isOpen());
    EXPECT_EQ(status->text(), QStringLiteral("Background clicks: 1"));
}
} // namespace

TEST(GalleryOverlayTest, Contract_ThreeExamplesShareLiveControlsAndModalInput)
{
    for (const auto& example : examples) {
        SCOPED_TRACE(example.route.toStdString());
        QWidget host;
        host.resize(900, 700);
        const auto samples = fluent::gallery::gallerySamplesForRoute(example.route);
        const auto sample = std::find_if(samples.begin(), samples.end(), [&](const auto& item) {
            return item.id == example.sample;
        });
        ASSERT_NE(sample, samples.end());
        auto* root = sample->createPreview(&host);
        root->setGeometry(20, 20, 700, 180);
        host.show();
        host.activateWindow();
        QApplication::processEvents();
        auto* opener = root->findChild<QAbstractButton*>(example.prefix + ".OpenEffects");
        ASSERT_NE(opener, nullptr);
        opener->click();
        QWidget* overlay = host.findChild<QWidget*>(example.prefix + ".Effects");
        if (auto* popup = qobject_cast<fluent::dialogs_flyouts::Popup*>(overlay))
            verifyControls(host, root, popup, example.prefix);
        else if (auto* dialog = qobject_cast<fluent::dialogs_flyouts::Dialog*>(overlay))
            verifyControls(host, root, dialog, example.prefix);
        else
            verifyControls(host, root, qobject_cast<fluent::collections::DrawerView*>(overlay), example.prefix);
    }
}

TEST(GalleryOverlayTest, Contract_ModalSurfacesBlockFullGalleryNavigation)
{
    const auto previousAutomated = qApp->property("fluentqtGalleryAutomated");
    struct Restore {
        QVariant value;
        ~Restore() { qApp->setProperty("fluentqtGalleryAutomated", value); }
    } restore{previousAutomated};
    qApp->setProperty("fluentqtGalleryAutomated", true);
    fluent::gallery::GallerySettings::instance().setIntroCompleted(true);
    fluent::gallery::GalleryWindow window;
    window.resize(1144, 760);
    window.show();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<QWidget*>("gallerySplashScreen"); }, 60000));
    auto* navigation = window.findChild<QTreeView*>("galleryFooterNavigationTreeView");
    ASSERT_NE(navigation, nullptr);
    const auto settings = navigation->model()->index(0, 0);
    ASSERT_EQ(settings.data().toString(), QStringLiteral("Settings"));
    for (const auto& example : examples) {
        SCOPED_TRACE(example.route.toStdString());
        ASSERT_TRUE(window.selectRoute(example.route));
        QAbstractButton* opener = nullptr;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            opener = window.findChild<QAbstractButton*>(example.prefix + ".OpenEffects");
            return opener && opener->isVisible() && window.currentRouteId() == example.route;
        }));
        opener->click();
        auto* overlay = window.findChild<QWidget*>(example.prefix + ".Effects");
        ASSERT_NE(overlay, nullptr);
        ASSERT_TRUE(overlay->isVisible());
        auto* outside = overlay->findChild<fluent::basicinput::CheckBox*>(example.prefix + ".CloseOutside");
        ASSERT_NE(outside, nullptr);
        const auto target = [&] {
            return fluent::overlay::presentedPointInTopLevel(navigation->viewport(),
                navigation->visualRect(settings).center());
        };
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, target());
        EXPECT_EQ(window.currentRouteId(), example.route);
        EXPECT_TRUE(overlay->isVisible());
        outside->setChecked(true);
        QPointer<QWidget> guard(overlay);
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, target());
        ASSERT_TRUE(QTest::qWaitFor([&] { return !guard || !guard->isVisible(); }));
        EXPECT_EQ(window.currentRouteId(), example.route);
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, target());
        ASSERT_TRUE(QTest::qWaitFor([&] { return window.currentRouteId() == "settings"; }));
    }
}
