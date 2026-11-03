#include <gtest/gtest.h>
#include <QAction>
#include <QApplication>
#include <QTest>
#include <QWindow>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/textfields/Label.h"
#include "platform/GalleryPlatform.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryWindowPlacement.h"
#include "viewmodel/GallerySettings.h"

TEST(GalleryZoomRestartTest, PendingZoomKeepsCurrentGeometryAndRemainsDiscoverable)
{
    using namespace fluent::gallery;
    ASSERT_TRUE(platform::supportsInterfaceZoom());
    ASSERT_FALSE(platform::supportsLiveInterfaceZoom());
    auto& settings = GallerySettings::instance();
    settings.setInterfaceZoomPercent(100);
    settings.setSpatialModeEnabled(false);
    settings.setMotionMode(GallerySettings::MotionMode::Disabled);
    GalleryWindow window;
    GalleryWindowPlacement placement(&window, &settings);
    window.resize(700, 600);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<QWidget*>("gallerySplashScreen"); }, 5000));
    const QSize size = window.size();
    const QSize minimum = window.minimumSize();
    const qreal dpr = window.windowHandle()->devicePixelRatio();
    auto* zoomIn = window.findChild<QAction*>("galleryZoomInAction");
    ASSERT_NE(zoomIn, nullptr);
    zoomIn->trigger();
    EXPECT_EQ(settings.interfaceZoomPercent(), 110);
    EXPECT_DOUBLE_EQ(platform::appliedInterfaceZoom(), 1.0);
    EXPECT_EQ(window.size(), size);
    EXPECT_EQ(window.minimumSize(), minimum);
    EXPECT_DOUBLE_EQ(window.windowHandle()->devicePixelRatio(), dpr);
    auto* bubble = window.findChild<fluent::dialogs_flyouts::Popup*>("galleryInterfaceZoomBubble");
    ASSERT_NE(bubble, nullptr);
    EXPECT_TRUE(bubble->isOpen());
    auto* level = bubble->findChild<fluent::textfields::Label*>("galleryZoomLevelLabel");
    auto* hint = bubble->findChild<fluent::textfields::Label*>("galleryZoomRestartHint");
    ASSERT_NE(level, nullptr);
    ASSERT_NE(hint, nullptr);
    EXPECT_EQ(level->text(), QStringLiteral("110%"));
    EXPECT_FALSE(hint->isHidden());
    placement.saveNow();
    EXPECT_EQ(settings.windowGeometryZoomPercent(), 100);
    EXPECT_EQ(settings.windowNormalGeometry().size(), size);
    window.findChild<QAction*>("galleryActualSizeAction")->trigger();
    EXPECT_EQ(level->text(), QStringLiteral("100%"));
    EXPECT_TRUE(hint->isHidden());
    EXPECT_FALSE(bubble->findChild<fluent::basicinput::Button*>("galleryZoomResetButton")->isEnabled());
}
