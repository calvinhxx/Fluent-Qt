#include <gtest/gtest.h>

#include <QAbstractAnimation>
#include <QApplication>
#include <QCoreApplication>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <QVariantAnimation>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "components/collections/TreeView.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/windowing/TitleBar.h"
#include "model/GalleryNavigationItem.h"
#include "view/shell/GalleryIntroTour.h"
#include "view/shell/GalleryNavigationPane.h"
#include "view/shell/GallerySplashScreen.h"
#include "view/shell/GalleryTitleBarController.h"
#include "view/shell/GalleryTopNavigationPane.h"
#include "view/support/GalleryMotion.h"

namespace {

using fluent::basicinput::Button;
using fluent::collections::TreeView;
using fluent::dialogs_flyouts::Popup;
using fluent::gallery::GalleryIntroTour;
using fluent::gallery::GalleryNavigationItem;
using fluent::gallery::GalleryNavigationPane;
using fluent::gallery::GallerySplashScreen;
using fluent::gallery::GalleryTitleBarController;
using fluent::gallery::GalleryTopNavigationPane;
using fluent::overlay::OverlayScrim;
using fluent::windowing::TitleBar;

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

void processDeferredDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

GalleryNavigationItem navigationItem(const QString& id, const QString& title,
                                     GalleryNavigationItem::Kind kind,
                                     const QString& parentId = QString())
{
    GalleryNavigationItem item;
    item.id = id;
    item.title = title;
    item.parentId = parentId;
    item.kind = kind;
    return item;
}

QRect targetRectInScrim(QWidget* target, QWidget* window, OverlayScrim* scrim)
{
    return QRect(target->mapTo(window, QPoint(0, 0)), target->size())
        .translated(-scrim->geometry().topLeft());
}

class GalleryShellMotionPolicyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    }

    void TearDown() override
    {
        processDeferredDeletes();
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(GalleryShellMotionPolicyTest, ReducedMotionCapsSplashDismissAndKeepsCleanup)
{
    QWidget host;
    host.resize(640, 480);
    showAndProcess(host);

    auto* splash = new GallerySplashScreen(&host);
    splash->setGeometry(host.rect());
    splash->show();
    QPointer<GallerySplashScreen> splashGuard = splash;
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);

    splash->dismiss();

    auto* fade =
        splash->findChild<QPropertyAnimation*>(QStringLiteral("gallerySplashDismissAnimation"));
    ASSERT_NE(fade, nullptr);
    EXPECT_EQ(fade->state(), QAbstractAnimation::Running);
    EXPECT_GT(fade->duration(), 0);
    EXPECT_LE(fade->duration(), 50);
    QTRY_VERIFY_WITH_TIMEOUT(splashGuard.isNull(), 500);
}

TEST_F(GalleryShellMotionPolicyTest, DisabledMotionSettlesSplashDismissSynchronously)
{
    QWidget host;
    host.resize(640, 480);
    showAndProcess(host);

    auto* splash = new GallerySplashScreen(&host);
    splash->setGeometry(host.rect());
    splash->show();
    QPointer<GallerySplashScreen> splashGuard = splash;
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    splash->dismiss();

    auto* fade =
        splash->findChild<QPropertyAnimation*>(QStringLiteral("gallerySplashDismissAnimation"));
    if (fade)
        EXPECT_EQ(fade->state(), QAbstractAnimation::Stopped);
    processDeferredDeletes();
    EXPECT_TRUE(splashGuard.isNull());
}

TEST_F(GalleryShellMotionPolicyTest, DisabledMotionSettlesIntroTourSpotlightAndCleanup)
{
    QWidget host;
    host.resize(800, 600);
    QWidget firstTarget(&host);
    firstTarget.setGeometry(80, 90, 180, 40);
    QWidget secondTarget(&host);
    secondTarget.setGeometry(500, 390, 190, 44);
    firstTarget.show();
    secondTarget.show();
    showAndProcess(host);

    GalleryIntroTour tour(&host);
    GalleryIntroTour::Step first;
    first.target = &firstTarget;
    first.title = QStringLiteral("First target");
    first.body = QStringLiteral("First step");
    GalleryIntroTour::Step second;
    second.target = &secondTarget;
    second.title = QStringLiteral("Second target");
    second.body = QStringLiteral("Second step");
    tour.setSteps({first, second});
    QSignalSpy finishedSpy(&tour, &GalleryIntroTour::finished);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    tour.start();

    auto* scrim = host.findChild<OverlayScrim*>(QStringLiteral("GalleryIntroTour.Scrim"));
    auto* dimAnimation =
        tour.findChild<QPropertyAnimation*>(QStringLiteral("galleryIntroTourDimAnimation"));
    auto* spotAnimation =
        tour.findChild<QPropertyAnimation*>(QStringLiteral("galleryIntroTourSpotlightAnimation"));
    auto* nextButton = host.findChild<Button*>(QStringLiteral("GalleryIntroTour.NextButton"));
    ASSERT_NE(scrim, nullptr);
    ASSERT_NE(dimAnimation, nullptr);
    ASSERT_NE(spotAnimation, nullptr);
    ASSERT_NE(nextButton, nullptr);
    EXPECT_DOUBLE_EQ(scrim->progress(), 1.0);
    EXPECT_EQ(dimAnimation->state(), QAbstractAnimation::Stopped);

    QTest::mouseClick(nextButton, Qt::LeftButton);

    EXPECT_EQ(spotAnimation->state(), QAbstractAnimation::Stopped);
    EXPECT_TRUE(scrim->spotlightRect().contains(targetRectInScrim(&secondTarget, &host, scrim)));

    QPointer<OverlayScrim> scrimGuard = scrim;
    QTest::mouseClick(nextButton, Qt::LeftButton);

    EXPECT_EQ(finishedSpy.count(), 1);
    EXPECT_DOUBLE_EQ(scrim->progress(), 0.0);
    EXPECT_EQ(dimAnimation->state(), QAbstractAnimation::Stopped);
    processDeferredDeletes();
    EXPECT_TRUE(scrimGuard.isNull());
}

TEST_F(GalleryShellMotionPolicyTest, DisabledMotionSettlesNavigationPaneTransitions)
{
    QWidget host;
    host.resize(520, 520);
    const QVector<GalleryNavigationItem> items = {
        navigationItem(QString(), QStringLiteral("Controls"),
                       GalleryNavigationItem::Kind::SectionHeader),
        navigationItem(QStringLiteral("foundation"), QStringLiteral("Foundation"),
                       GalleryNavigationItem::Kind::CategoryRoute),
        navigationItem(QStringLiteral("button"), QStringLiteral("Button"),
                       GalleryNavigationItem::Kind::ComponentRoute, QStringLiteral("foundation"))};
    GalleryNavigationPane pane(items, &host);
    pane.setGeometry(0, 0, 260, host.height());

    const QVector<GalleryNavigationItem> footerItems = {
        navigationItem(QStringLiteral("settings"), QStringLiteral("Settings"),
                       GalleryNavigationItem::Kind::FooterRoute)};
    GalleryNavigationPane footer(footerItems, &host);
    footer.setGeometry(270, 0, 240, 64);
    showAndProcess(host);
    pane.show();
    footer.show();
    QApplication::processEvents();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    pane.setCompact(true);

    auto* compactAnimation = pane.findChild<QPropertyAnimation*>(
        QStringLiteral("galleryNavigationCompactVisualAnimation"));
    ASSERT_NE(compactAnimation, nullptr);
    EXPECT_EQ(compactAnimation->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(pane.compactVisualProgress(), 1.0);

    auto* footerTree = footer.findChild<TreeView*>();
    ASSERT_NE(footerTree, nullptr);
    const QModelIndex settingsIndex = footer.indexForRouteId(QStringLiteral("settings"));
    const QRect settingsRect = footerTree->visualRect(settingsIndex);
    ASSERT_FALSE(settingsRect.isEmpty());
    QTest::mousePress(footerTree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      settingsRect.center());

    auto* settingsAnimation = footer.findChild<QPropertyAnimation*>(
        QStringLiteral("gallerySettingsIconRotationAnimation"));
    ASSERT_NE(settingsAnimation, nullptr);
    EXPECT_EQ(settingsAnimation->state(), QAbstractAnimation::Stopped);
    EXPECT_NEAR(footer.settingsIconRotation(), 0.0, 0.001);
    QTest::mouseRelease(footerTree->viewport(), Qt::LeftButton, Qt::NoModifier,
                        settingsRect.center());

    auto* tree = pane.findChild<TreeView*>();
    ASSERT_NE(tree, nullptr);
    const QModelIndex categoryIndex = pane.indexForRouteId(QStringLiteral("foundation"));
    const QRect categoryRect = tree->visualRect(categoryIndex);
    ASSERT_FALSE(categoryRect.isEmpty());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, categoryRect.center());

    auto* flyout = host.findChild<Popup*>(QStringLiteral("galleryCompactNavigationFlyout"));
    ASSERT_NE(flyout, nullptr);
    EXPECT_TRUE(flyout->isVisible());
    auto* entrance = flyout->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryCompactNavigationFlyoutEntranceAnimation"));
    if (entrance) {
        EXPECT_EQ(entrance->state(), QAbstractAnimation::Stopped);
        EXPECT_EQ(flyout->pos(), entrance->endValue().toPoint());
    }
    const QPoint settledPosition = flyout->pos();
    processDeferredDeletes();
    EXPECT_EQ(flyout->pos(), settledPosition);
    EXPECT_EQ(flyout->findChild<QPropertyAnimation*>(
                  QStringLiteral("galleryCompactNavigationFlyoutEntranceAnimation")),
              nullptr);

    pane.setCompact(false);
    EXPECT_DOUBLE_EQ(pane.compactVisualProgress(), 0.0);
    EXPECT_EQ(compactAnimation->state(), QAbstractAnimation::Stopped);
}

TEST_F(GalleryShellMotionPolicyTest, DisabledMotionSettlesTopNavigationTransitions)
{
    QWidget host;
    host.resize(760, 420);
    const QVector<GalleryNavigationItem> items = {
        navigationItem(QStringLiteral("foundation"), QStringLiteral("Foundation"),
                       GalleryNavigationItem::Kind::CategoryRoute),
        navigationItem(QStringLiteral("button"), QStringLiteral("Button"),
                       GalleryNavigationItem::Kind::ComponentRoute, QStringLiteral("foundation")),
        navigationItem(QStringLiteral("settings"), QStringLiteral("Settings"),
                       GalleryNavigationItem::Kind::FooterRoute)};
    GalleryTopNavigationPane pane(items, &host);
    pane.setGeometry(0, 0, pane.sizeHint().width(), pane.sizeHint().height());
    showAndProcess(host);
    pane.show();
    QApplication::processEvents();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    auto* settingsButton =
        pane.findChild<Button*>(QStringLiteral("galleryTopNavigationButton_settings"));
    auto* categoryButton =
        pane.findChild<Button*>(QStringLiteral("galleryTopNavigationButton_foundation"));
    ASSERT_NE(settingsButton, nullptr);
    ASSERT_NE(categoryButton, nullptr);

    QTest::mouseClick(settingsButton, Qt::LeftButton);
    auto* rotation = settingsButton->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTopSettingsIconRotationAnimation"), Qt::FindDirectChildrenOnly);
    ASSERT_NE(rotation, nullptr);
    EXPECT_EQ(rotation->state(), QAbstractAnimation::Stopped);
    EXPECT_NEAR(settingsButton->iconRotation(), 0.0, 0.001);

    QTest::mouseClick(categoryButton, Qt::LeftButton);
    auto* flyout = host.findChild<Popup*>(QStringLiteral("galleryTopNavigationFlyout"));
    ASSERT_NE(flyout, nullptr);
    EXPECT_TRUE(flyout->isVisible());
    auto* entrance = flyout->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTopNavigationFlyoutEntranceAnimation"));
    if (entrance) {
        EXPECT_EQ(entrance->state(), QAbstractAnimation::Stopped);
        EXPECT_EQ(flyout->pos(), entrance->endValue().toPoint());
    }
    const QPoint settledPosition = flyout->pos();
    processDeferredDeletes();
    EXPECT_EQ(flyout->pos(), settledPosition);
    EXPECT_EQ(flyout->findChild<QPropertyAnimation*>(
                  QStringLiteral("galleryTopNavigationFlyoutEntranceAnimation")),
              nullptr);
}

TEST_F(GalleryShellMotionPolicyTest, DisabledMotionSettlesTitleBarTransitions)
{
    QWidget host;
    host.resize(900, 100);
    auto* titleBar = new TitleBar(&host);
    titleBar->setGeometry(0, 0, host.width(), 48);
    GalleryTitleBarController::Callbacks callbacks;
    auto* controller = new GalleryTitleBarController(titleBar, {}, std::move(callbacks), &host);
    showAndProcess(host);
    titleBar->show();
    QApplication::processEvents();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    controller->setBackAvailable(true);
    auto* backButton = titleBar->findChild<Button*>(QStringLiteral("GalleryTitleBar.BackButton"));
    auto* menuButton = titleBar->findChild<Button*>(QStringLiteral("GalleryTitleBar.MenuButton"));
    auto* backReveal = controller->findChild<QVariantAnimation*>(
        QStringLiteral("galleryTitleBarBackRevealAnimation"));
    ASSERT_NE(backButton, nullptr);
    ASSERT_NE(menuButton, nullptr);
    ASSERT_NE(backReveal, nullptr);
    EXPECT_EQ(backReveal->state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(backButton->width(), 24);
    EXPECT_DOUBLE_EQ(backButton->contentOpacity(), 1.0);

    controller->setChromeVisible(false);
    controller->setChromeVisible(true, true);
    auto* chromeReveal = controller->findChild<QVariantAnimation*>(
        QStringLiteral("galleryTitleBarChromeRevealAnimation"));
    ASSERT_NE(chromeReveal, nullptr);
    EXPECT_EQ(chromeReveal->state(), QAbstractAnimation::Stopped);
    EXPECT_TRUE(backButton->isVisible());

    controller->setMenuEnabled(true);
    QTest::mousePress(menuButton, Qt::LeftButton, Qt::NoModifier, menuButton->rect().center());
    auto* pressAnimation = menuButton->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTitleBarButtonPressAnimation"));
    if (pressAnimation)
        EXPECT_EQ(pressAnimation->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(menuButton->iconScale(), 1.0);
    QTest::mouseRelease(menuButton, Qt::LeftButton, Qt::NoModifier, menuButton->rect().center());
    processDeferredDeletes();
    EXPECT_EQ(menuButton->findChild<QPropertyAnimation*>(
                  QStringLiteral("galleryTitleBarButtonPressAnimation")),
              nullptr);
}

TEST_F(GalleryShellMotionPolicyTest, RunningGalleryTransitionConvergesWhenMotionIsReduced)
{
    QVariantAnimation animation;
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);
    QSignalSpy finishedSpy(&animation, &QVariantAnimation::finished);

    fluent::gallery::motion::startFiniteTransition(&animation, 1000);
    ASSERT_EQ(animation.state(), QAbstractAnimation::Running);
    animation.setCurrentTime(200);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);

    EXPECT_EQ(animation.state(), QAbstractAnimation::Running);
    EXPECT_LE(animation.duration() - animation.currentTime(), 50);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    EXPECT_EQ(animation.duration(), 1000);
    EXPECT_EQ(animation.state(), QAbstractAnimation::Running);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    EXPECT_LE(animation.duration() - animation.currentTime(), 50);
    QTRY_COMPARE_WITH_TIMEOUT(animation.state(), QAbstractAnimation::Stopped, 250);
    EXPECT_EQ(finishedSpy.count(), 1);
    EXPECT_DOUBLE_EQ(animation.currentValue().toDouble(), 1.0);
}

TEST_F(GalleryShellMotionPolicyTest, RunningTransientGalleryTransitionDeletesWhenMotionIsDisabled)
{
    auto* animation = new QVariantAnimation;
    QPointer<QVariantAnimation> guard(animation);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    QSignalSpy finishedSpy(animation, &QVariantAnimation::finished);

    fluent::gallery::motion::startFiniteTransition(animation, 1000, true,
                                                   QAbstractAnimation::DeleteWhenStopped);
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    if (guard)
        EXPECT_EQ(guard->state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(finishedSpy.count(), 1);
    processDeferredDeletes();
    EXPECT_TRUE(guard.isNull());
}

} // namespace
