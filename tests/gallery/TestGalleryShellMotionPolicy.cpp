#include <gtest/gtest.h>

#include <QAbstractAnimation>
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include "QtTestEnvironment.h"

#include "components/basicinput/Button.h"
#include "components/collections/TreeView.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/layout/ParticleBackdrop.h"
#include "components/navigation/StackContentHost.h"
#include "components/status_info/ProgressBar.h"
#include "components/status_info/ProgressRing.h"
#include "components/status_info/Shimmer.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "model/GalleryNavigationItem.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/SettingsPage.h"
#include "view/shell/GalleryIntroTour.h"
#include "view/shell/GalleryContentPresenter.h"
#include "view/shell/GalleryNavigationPane.h"
#include "view/shell/GallerySplashScreen.h"
#include "view/shell/GalleryTitleBarController.h"
#include "view/shell/GalleryTopNavigationPane.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryMotion.h"
#include "view/widgets/GallerySampleCatalog.h"

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

    auto* fade = splash->findChild<QPropertyAnimation*>(QStringLiteral("splashDismissAnimation"));
    ASSERT_NE(fade, nullptr);
    EXPECT_EQ(fade->state(), QAbstractAnimation::Running);
    EXPECT_GT(fade->duration(), 0);
    EXPECT_LE(fade->duration(), 50);
    ASSERT_TRUE(QTest::qWaitFor([&] { return splashGuard.isNull(); }, 500));
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

    auto* fade = splash->findChild<QPropertyAnimation*>(QStringLiteral("splashDismissAnimation"));
    if (fade)
        EXPECT_EQ(fade->state(), QAbstractAnimation::Stopped);
    processDeferredDeletes();
    EXPECT_TRUE(splashGuard.isNull());
}

TEST_F(GalleryShellMotionPolicyTest, StartupPrewarmCompletesHiddenPageLayoutBeforeHandoff)
{
    fluent::navigation::StackContentHost host;
    host.resize(960, 680);
    fluent::gallery::GalleryNavigationViewModel model;
    fluent::gallery::GalleryContentPresenter presenter(&host, model);
    ASSERT_TRUE(presenter.presentRoute(QStringLiteral("home")));
    showAndProcess(host);
    QSignalSpy finished(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmFinished);
    presenter.prewarmRoutes(
        {QStringLiteral("button"), QStringLiteral("slider"), QStringLiteral("tab-view")});
    ASSERT_TRUE(QTest::qWaitFor([&] { return (finished.count()) == (1); }, 4000));
    EXPECT_EQ(presenter.currentRouteId(), QStringLiteral("home"));

    class ResizeWatch final : public QObject {
    public:
        int count = 0;
        bool eventFilter(QObject*, QEvent* event) override
        {
            if (event->type() == QEvent::Resize)
                ++count;
            return false;
        }
    } watch;
    int warmedPages = 0;
    for (auto* page : host.findChildren<fluent::gallery::GalleryContentPage*>()) {
        if (page == presenter.currentPage())
            continue;
        EXPECT_TRUE(page->isHidden());
        page->installEventFilter(&watch);
        ++warmedPages;
    }
    ASSERT_EQ(warmedPages, 3);
    // Opacity effects render a QWidget subtree. That must not deliver the warmed pages'
    // first resize in the middle of the logo's flight.
    host.grab();
    EXPECT_EQ(watch.count, 0);
}

TEST_F(GalleryShellMotionPolicyTest, SplashHandoffCachesContentAndRestoresLivePainting)
{
    class PaintCounter final : public fluent::textfields::Label {
    public:
        using Label::Label;
        int paints = 0;
        void paintEvent(QPaintEvent* event) override
        {
            ++paints;
            Label::paintEvent(event);
        }
    };
    QWidget host;
    host.resize(640, 480);
    PaintCounter content(QStringLiteral("Ready"), &host);
    content.setGeometry(host.rect());
    content.setAutoFillBackground(true);
    auto* splash = new GallerySplashScreen(&host);
    showAndProcess(host);
    splash->show();
    const QImage liveFrame = content.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    splash->cacheDismissalContent(&content);
    ASSERT_NE(content.graphicsEffect(), nullptr);
    EXPECT_EQ(content.grab().toImage().convertToFormat(QImage::Format_ARGB32), liveFrame);
    splash->dismiss();
    auto* fade = splash->findChild<QPropertyAnimation*>("splashDismissAnimation");
    ASSERT_NE(fade, nullptr);
    fade->pause();
    fade->setCurrentTime(fade->duration() / 4);
    host.grab();
    const int capturedPaints = content.paints;
    ASSERT_GT(capturedPaints, 0);
    for (int percent : {35, 50, 65}) {
        content.update(); // A live background may request a new frame throughout the handoff.
        fade->setCurrentTime(fade->duration() * percent / 100);
        host.grab();
        EXPECT_EQ(content.paints, capturedPaints);
    }
    content.resize(600, 440);
    host.grab();
    EXPECT_GT(content.paints, capturedPaints);
    const int resizedPaints = content.paints;
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    host.grab();
    EXPECT_GT(content.paints, resizedPaints);

    fade->setCurrentTime(fade->duration());
    EXPECT_TRUE(splash->isHidden());
    EXPECT_EQ(content.graphicsEffect(), nullptr);
    const int beforeLivePaint = content.paints;
    host.grab();
    EXPECT_GT(content.paints, beforeLivePaint);
}

TEST_F(GalleryShellMotionPolicyTest, SplashContentCacheRespectsEffectsCancellationAndMotion)
{
    QWidget host;
    host.resize(640, 480);
    QWidget content(&host);
    content.setGeometry(host.rect());
    fluent::layout::ParticleBackdrop background(&content);
    background.setGeometry(content.rect());
    fluent::layout::ParticleBackdrop disabledBackground(&content);
    disabledBackground.setAnimationEnabled(false);
    fluent::layout::ParticleBackdrop hiddenBackground(&content);
    hiddenBackground.hide();
    auto* splash = new GallerySplashScreen(&host);
    showAndProcess(host);
    splash->show();
    auto* existing = new QGraphicsOpacityEffect(&content);
    content.setGraphicsEffect(existing);
    splash->cacheDismissalContent(&content);
    EXPECT_EQ(content.graphicsEffect(), existing);
    splash->clearDismissalContent();
    EXPECT_EQ(content.graphicsEffect(), existing);
    content.setGraphicsEffect(nullptr);
    splash->cacheDismissalContent(&host);
    EXPECT_EQ(host.graphicsEffect(), nullptr);

    splash->cacheDismissalContent(&content);
    ASSERT_NE(content.graphicsEffect(), nullptr);
    EXPECT_FALSE(background.isAnimationEnabled());
    EXPECT_FALSE(disabledBackground.isAnimationEnabled());
    EXPECT_TRUE(hiddenBackground.isAnimationEnabled());
    splash->dismiss();
    splash->hide();
    EXPECT_EQ(content.graphicsEffect(), nullptr);
    EXPECT_TRUE(background.isAnimationEnabled());
    EXPECT_FALSE(disabledBackground.isAnimationEnabled());
    splash->show();
    for (auto mode : {fluent::MotionPolicy::Mode::Reduced, fluent::MotionPolicy::Mode::Disabled}) {
        fluent::MotionPolicy::instance().setMode(mode);
        splash->cacheDismissalContent(&content);
        EXPECT_EQ(content.graphicsEffect(), nullptr);
    }
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    splash->cacheDismissalContent(&content);
    ASSERT_NE(content.graphicsEffect(), nullptr);
    delete splash;
    EXPECT_EQ(content.graphicsEffect(), nullptr);
    EXPECT_TRUE(background.isAnimationEnabled());
}

TEST_F(GalleryShellMotionPolicyTest, StartupReplacementCleansUpWithoutStartingTour)
{
    fluent::gallery::GalleryWindow window;
    auto* presenter = window.findChild<fluent::gallery::GalleryContentPresenter*>();
    ASSERT_NE(presenter, nullptr);
    presenter->setPrewarmPaused(true);
    QPointer<GallerySplashScreen> original = window.findChild<GallerySplashScreen*>();
    ASSERT_TRUE(original);
    showAndProcess(window);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    fluent::status_info::SplashScreen replacement(window.contentHost());
    replacement.show();
    processDeferredDeletes();
    EXPECT_TRUE(original.isNull());
    EXPECT_TRUE(replacement.isVisible());
    EXPECT_EQ(window.findChild<GalleryIntroTour*>(), nullptr);
    const int readyPages = window.findChildren<fluent::gallery::GalleryComponentPage*>().size();
    QSignalSpy progress(presenter, &fluent::gallery::GalleryContentPresenter::prewarmProgress);
    QTest::qWait(350);
    EXPECT_TRUE(progress.isEmpty());
    EXPECT_EQ(window.findChildren<fluent::gallery::GalleryComponentPage*>().size(), readyPages)
        << "Replacing the startup splash must cancel its queued and staged page work";
    replacement.dismiss();
    EXPECT_TRUE(replacement.isHidden());
}

TEST_F(GalleryShellMotionPolicyTest, FastStartupPreservesPresentationFromWindowShow)
{
    fluent::gallery::GalleryWindow window;
    window.resize(960, 680);
    auto* presenter = window.findChild<fluent::gallery::GalleryContentPresenter*>();
    ASSERT_NE(presenter, nullptr);
    presenter->setPrewarmPaused(true);
    QPointer<GallerySplashScreen> splash = window.findChild<GallerySplashScreen*>();
    ASSERT_TRUE(splash);
    QSignalSpy dismissed(splash, &GallerySplashScreen::dismissed);
    // Model content finishing before the top-level window has appeared.
    presenter->prewarmFinished();
    QTest::qWait(300);
    EXPECT_EQ(dismissed.count(), 0);

    QElapsedTimer visible;
    visible.start();
    showAndProcess(window);
    QTest::qWait(350);
    ASSERT_TRUE(splash);
    EXPECT_TRUE(splash->isVisible());
    EXPECT_EQ(window.findChild<QWidget*>("splashLogoTransition"), nullptr);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return window.findChild<QWidget*>("splashLogoTransition"); }, 2000));
    EXPECT_GE(visible.elapsed(), 1400);
    // The first-run tour must also wait for the complete connected transition.
    QTest::qWait(500);
    EXPECT_EQ(window.findChild<GalleryIntroTour*>(), nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return (dismissed.count()) == (1); }, 1200));
    EXPECT_GE(visible.elapsed(), 2000);
}

TEST_F(GalleryShellMotionPolicyTest, ReducedPreferenceReleasesPendingStartupPresentation)
{
    for (auto mode : {fluent::MotionPolicy::Mode::Reduced, fluent::MotionPolicy::Mode::Disabled}) {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::gallery::GalleryWindow window;
        auto* presenter = window.findChild<fluent::gallery::GalleryContentPresenter*>();
        ASSERT_NE(presenter, nullptr);
        presenter->setPrewarmPaused(true);
        QPointer<GallerySplashScreen> splash = window.findChild<GallerySplashScreen*>();
        ASSERT_TRUE(splash);
        QSignalSpy dismissed(splash, &GallerySplashScreen::dismissed);
        presenter->prewarmFinished();
        showAndProcess(window);
        QTest::qWait(200);
        EXPECT_EQ(dismissed.count(), 0);
        fluent::MotionPolicy::instance().setMode(mode);
        ASSERT_TRUE(QTest::qWaitFor([&] { return (dismissed.count()) == (1); }, 400));
        EXPECT_EQ(window.findChild<QWidget*>("splashLogoTransition"), nullptr);
    }
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return (animation.state()) == (QAbstractAnimation::Stopped); }, 250));
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

class GalleryStartupTest : public GalleryShellMotionPolicyTest {
protected:
    bool oldSpatial = false;
    bool oldIntro = false;
    bool oldParticles = false;
    fluent::gallery::GallerySettings::MotionMode oldMotion;
    fluent::gallery::GallerySettings::ThemeMode oldTheme;
    fluent::windowing::BackdropEffect oldEffect;

    void SetUp() override
    {
        GalleryShellMotionPolicyTest::SetUp();
        auto& settings = fluent::gallery::GallerySettings::instance();
        oldSpatial = settings.spatialModeEnabled();
        oldIntro = settings.introCompleted();
        oldParticles = settings.homeParticlesEnabled();
        oldMotion = settings.motionMode();
        oldTheme = settings.themeMode();
        oldEffect = settings.windowEffect();
        settings.setSpatialModeEnabled(false);
        settings.setIntroCompleted(true);
        settings.setHomeParticlesEnabled(false);
        settings.setMotionMode(fluent::gallery::GallerySettings::MotionMode::Full);
    }

    void TearDown() override
    {
        auto& settings = fluent::gallery::GallerySettings::instance();
        settings.setSpatialModeEnabled(oldSpatial);
        settings.setIntroCompleted(oldIntro);
        settings.setHomeParticlesEnabled(oldParticles);
        settings.setMotionMode(oldMotion);
        settings.setThemeMode(oldTheme);
        settings.setWindowEffect(oldEffect);
        GalleryShellMotionPolicyTest::TearDown();
    }
};

TEST_F(GalleryStartupTest, SlowPrewarmYieldsPerSampleAndNeverExpiresWhilePaused)
{
    fluent::navigation::StackContentHost host;
    host.resize(960, 680);
    fluent::gallery::GalleryNavigationViewModel model;
    fluent::gallery::GalleryContentPresenter presenter(&host, model);
    ASSERT_TRUE(presenter.presentRoute(QStringLiteral("home")));
    QSignalSpy progress(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmProgress);
    QSignalSpy finished(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmFinished);
    presenter.prewarmRoutes(
        {QStringLiteral("button"), QStringLiteral("combobox"), QStringLiteral("tab-view")});
    QPointer<fluent::gallery::GalleryComponentPage> staged;
    bool inspectedFirstTurn = false;
    QTimer::singleShot(0, &host, [&] {
        const auto pages = host.findChildren<fluent::gallery::GalleryComponentPage*>();
        if (!pages.isEmpty())
            staged = pages.front();
        presenter.setPrewarmPaused(true);
        inspectedFirstTurn = true;
    });
    ASSERT_TRUE(QTest::qWaitFor([&] { return inspectedFirstTurn; }, 1000));
    ASSERT_TRUE(staged);
    EXPECT_TRUE(staged->isHidden());
    EXPECT_TRUE(staged->hasPendingSamples());
    EXPECT_EQ(staged->sampleCount(), 0)
        << "A staged page shell must yield before constructing its first live example";
    ASSERT_EQ(progress.count(), 1);
    EXPECT_EQ(progress.last().at(0).toInt(), 0);
    EXPECT_EQ(progress.last().at(1).toInt(), 3);

    // This deliberately crosses the former 3-second startup cutoff. Paused time
    // must neither report false completion nor discard the unbuilt catalog tail.
    QTest::qWait(3100);
    EXPECT_EQ(progress.count(), 1);
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(staged->sampleCount(), 0);
    presenter.setPrewarmPaused(false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 15000));
    ASSERT_EQ(progress.count(), 4);
    for (int index = 0; index != progress.count(); ++index) {
        EXPECT_EQ(progress.at(index).at(0).toInt(), index);
        EXPECT_EQ(progress.at(index).at(1).toInt(), 3);
    }
    EXPECT_FALSE(staged->hasPendingSamples());
    EXPECT_EQ(staged->sampleCount(), fluent::gallery::gallerySamplesForRoute("button").size());
    ASSERT_TRUE(presenter.presentRoute(QStringLiteral("button")));
    EXPECT_EQ(presenter.currentPage(), staged.data());
}

TEST_F(GalleryStartupTest, FailedPrewarmReportsTheRouteWithoutCountingItReady)
{
    fluent::navigation::StackContentHost host;
    fluent::gallery::GalleryNavigationViewModel model;
    fluent::gallery::GalleryContentPresenter presenter(&host, model);
    ASSERT_TRUE(presenter.presentRoute(QStringLiteral("home")));
    QSignalSpy progress(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmProgress);
    QSignalSpy failed(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmFailed);
    QSignalSpy finished(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmFinished);
    presenter.prewarmRoutes({QStringLiteral("button"), QStringLiteral("missing-startup-route")});
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 10000));
    ASSERT_EQ(failed.count(), 1);
    EXPECT_EQ(failed.first().first().toString(), QStringLiteral("missing-startup-route"));
    ASSERT_FALSE(progress.isEmpty());
    EXPECT_EQ(progress.last().at(0).toInt(), 1);
    EXPECT_EQ(progress.last().at(1).toInt(), 2);
    EXPECT_FALSE(presenter.presentRoute(QStringLiteral("missing-startup-route")));
    EXPECT_TRUE(presenter.presentRoute(QStringLiteral("button")));
}

TEST_F(GalleryStartupTest, ProgressObserversCanPauseOrCancelWithoutStartingAnotherPage)
{
    for (const bool cancel : {false, true}) {
        SCOPED_TRACE(::testing::Message() << "cancel=" << cancel);
        fluent::navigation::StackContentHost host;
        fluent::gallery::GalleryNavigationViewModel model;
        fluent::gallery::GalleryContentPresenter presenter(&host, model);
        ASSERT_TRUE(presenter.presentRoute(QStringLiteral("home")));
        QSignalSpy finished(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmFinished);
        bool stopped = false;
        QObject::connect(&presenter, &fluent::gallery::GalleryContentPresenter::prewarmProgress,
                         &host, [&](int done, int) {
                             if (done != 1 || stopped)
                                 return;
                             stopped = true;
                             if (cancel)
                                 presenter.cancelPrewarm();
                             else
                                 presenter.setPrewarmPaused(true);
                         });
        presenter.prewarmRoutes({QStringLiteral("button"), QStringLiteral("combobox")});
        ASSERT_TRUE(QTest::qWaitFor([&] { return stopped; }, 10000));
        QTest::qWait(80);
        const auto pages = host.findChildren<fluent::gallery::GalleryComponentPage*>();
        ASSERT_EQ(pages.size(), 1)
            << "A synchronous progress observer must stop this tick before the next page shell";
        EXPECT_EQ(pages.first()->routeId(), QStringLiteral("button"));
        EXPECT_EQ(finished.count(), 0)
            << "Cancellation is not successful completion of the remaining queue";
        if (!cancel) {
            presenter.setPrewarmPaused(false);
            ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 10000));
            EXPECT_EQ(host.findChildren<fluent::gallery::GalleryComponentPage*>().size(), 2);
        }
    }
}

TEST_F(GalleryStartupTest, HiddenAndClosedWindowsStopPendingPrewarmAndReleaseOwnedPages)
{
    auto* window = new fluent::gallery::GalleryWindow;
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->resize(960, 680);
    QPointer<fluent::gallery::GalleryWindow> windowGuard = window;
    QPointer<fluent::gallery::GalleryContentPresenter> presenter =
        window->findChild<fluent::gallery::GalleryContentPresenter*>();
    ASSERT_TRUE(presenter);
    QSignalSpy progress(presenter, &fluent::gallery::GalleryContentPresenter::prewarmProgress);
    QTest::qWait(80);
    EXPECT_TRUE(window->findChildren<fluent::gallery::GalleryComponentPage*>().isEmpty())
        << "No component page may be built before the splash has painted";
    EXPECT_EQ(progress.count(), 0);
    // Exercise a pending multi-page queue explicitly; normal startup now warms
    // only Home and Settings instead of eagerly constructing every component.
    presenter->prewarmRoutes({QStringLiteral("button"), QStringLiteral("tab-view")});
    bool hidAfterReadyPage = false;
    QObject::connect(
        presenter, &fluent::gallery::GalleryContentPresenter::prewarmProgress, window,
        [&](int done, int) {
            if (done > 0 && !hidAfterReadyPage &&
                !window->findChildren<fluent::gallery::GalleryComponentPage*>().isEmpty()) {
                hidAfterReadyPage = true;
                window->hide();
            }
        });
    window->show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return hidAfterReadyPage; }, 15000));
    ASSERT_TRUE(windowGuard);
    EXPECT_FALSE(window->isVisible());
    const int pausedProgressCount = progress.count();
    QVector<QPointer<fluent::gallery::GalleryComponentPage>> ownedPages;
    for (auto* page : window->findChildren<fluent::gallery::GalleryComponentPage*>())
        ownedPages.append(page);
    ASSERT_FALSE(ownedPages.isEmpty());
    QTest::qWait(350);
    EXPECT_EQ(progress.count(), pausedProgressCount);
    EXPECT_EQ(window->findChildren<fluent::gallery::GalleryComponentPage*>().size(),
              ownedPages.size());
    window->show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return progress.count() > pausedProgressCount; }, 15000));
    window->close();
    processDeferredDeletes();
    EXPECT_TRUE(windowGuard.isNull());
    EXPECT_TRUE(presenter.isNull());
    for (const auto& page : ownedPages)
        EXPECT_TRUE(page.isNull());
    // A queued zero-timer must not dereference its old presenter after close.
    QTest::qWait(80);
}

TEST_F(GalleryStartupTest, BoundedStartupAndCancellableColdPagesPreserveWarmState)
{
    fluent::gallery::GallerySettings::instance().setHomeParticlesEnabled(true);
    QElapsedTimer clock;
    clock.start();
    fluent::gallery::GalleryWindow window;
    window.resize(1100, 820);
    auto* presenter = window.findChild<fluent::gallery::GalleryContentPresenter*>();
    ASSERT_NE(presenter, nullptr);
    QPointer<GallerySplashScreen> splash = window.findChild<GallerySplashScreen*>();
    auto* particles = window.findChild<fluent::layout::ParticleBackdrop*>("galleryHomeParticles");
    ASSERT_NE(particles, nullptr);
    particles->setPauseWhenInactive(false);
    EXPECT_FALSE(particles->isAnimating());
    QSignalSpy finished(presenter, &fluent::gallery::GalleryContentPresenter::prewarmFinished);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 10000));
    EXPECT_TRUE(window.findChildren<fluent::gallery::GalleryComponentPage*>().isEmpty())
        << "Unvisited component demos must not extend startup";
    EXPECT_FALSE(particles->isAnimating());
    ASSERT_TRUE(QTest::qWaitFor([&] { return splash.isNull(); }, 6000));
    const qint64 handoffMs = clock.elapsed();
    RecordProperty("startupHandoffMs", int(handoffMs));
    EXPECT_TRUE(particles->isAnimating());
    ASSERT_TRUE(window.selectRoute(QStringLiteral("settings")));
    auto* settings = window.currentSettingsPage();
    ASSERT_NE(settings, nullptr);

    // Cancellation destroys an incomplete cold page, never publishes it.
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    QPointer<fluent::gallery::GalleryComponentPage> staged;
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const auto pages = window.findChildren<fluent::gallery::GalleryComponentPage*>();
            if (!pages.isEmpty())
                staged = pages.front();
            return !staged.isNull();
        },
        3000));
    ASSERT_TRUE(staged->hasPendingSamples());
    EXPECT_TRUE(staged->isHidden());
    ASSERT_TRUE(window.selectRoute(QStringLiteral("settings")));
    EXPECT_TRUE(staged.isNull());
    EXPECT_EQ(window.currentSettingsPage(), settings);

    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const auto* page = window.currentContentPage();
            return page && page->routeId() == QStringLiteral("button");
        },
        15000));
    auto* page = qobject_cast<fluent::gallery::GalleryComponentPage*>(window.currentContentPage());
    ASSERT_NE(page, nullptr);
    EXPECT_FALSE(page->hasPendingSamples());
    EXPECT_EQ(page->sampleCount(), fluent::gallery::gallerySamplesForRoute("button").size());
    ASSERT_TRUE(window.selectRoute(QStringLiteral("settings")));
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    EXPECT_EQ(window.currentContentPage(), page);
    qInfo("Bounded Gallery startup: handoffMs=%lld completeChecksMs=%lld", handoffMs,
          clock.elapsed());
}

TEST_F(GalleryStartupTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Gallery startup and mode switching require native desktop review";
    auto& settings = fluent::gallery::GallerySettings::instance();
    settings.setThemeMode(fluent::gallery::GallerySettings::ThemeMode::Dark);
    settings.setWindowEffect(fluent::windowing::BackdropEffect::Mica);
    settings.setHomeParticlesEnabled(true);
    fluent::gallery::GalleryWindow window;
    window.resize(1180, 820);
    window.show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(
            QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 60000));
        tests::support::VisualSnapshotOptions options;
        options.theme = tests::support::VisualSnapshotTheme::Dark;
        options.variant = QStringLiteral("startup-ready");
        ASSERT_TRUE(tests::support::captureVisualSnapshot(&window, options));
        return;
    }
    qApp->exec();
}

} // namespace
