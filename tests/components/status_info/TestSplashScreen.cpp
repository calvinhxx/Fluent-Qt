#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QShortcut>
#include <QSignalSpy>
#include <QTest>
#include <limits>

#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/status_info/SplashScreen.h"
#include "components/status_info/ProgressRing.h"
#include "components/status_info/ProgressBar.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "QtTestEnvironment.h"

using fluent::status_info::SplashScreen;
using fluent::status_info::ProgressRing;
using fluent::textfields::Label;
using fluent::MotionPolicy;

class SplashScreenTest : public ::testing::Test {
protected:
    MotionPolicy::Mode savedMode = MotionPolicy::instance().mode();
    fluent::FluentElement::Theme savedTheme = fluent::FluentElement::currentTheme();
    void TearDown() override
    {
        MotionPolicy::instance().setMode(savedMode);
        fluent::FluentElement::setTheme(savedTheme);
    }
};

TEST_F(SplashScreenTest, Contract_ProgressNormalizationAndNoOpSignals)
{
    static_assert(std::is_base_of<fluent::FluentElement, SplashScreen>::value);
    static_assert(std::is_base_of<fluent::QMLPlus, SplashScreen>::value);
    SplashScreen splash;
    EXPECT_TRUE(splash.isIndeterminate());
    EXPECT_EQ(splash.progress(), 0);
    EXPECT_TRUE(splash.text().isEmpty());
    EXPECT_TRUE(splash.icon().isNull());
    QSignalSpy values(&splash, &SplashScreen::progressChanged);
    QSignalSpy modes(&splash, &SplashScreen::indeterminateChanged);
    splash.setProgress(1, 4);
    splash.setProgress(25, 100);
    EXPECT_EQ(splash.progress(), 25);
    EXPECT_EQ(values.count(), 1);
    EXPECT_EQ(modes.count(), 1);
    auto* ring = splash.findChild<ProgressRing*>();
    ASSERT_NE(ring, nullptr);
    EXPECT_FALSE(ring->isIndeterminate());
    EXPECT_EQ(ring->value(), 25);
    splash.setProgress(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    EXPECT_EQ(splash.progress(), 100);
    splash.setProgress(std::numeric_limits<int>::min(), 1);
    EXPECT_EQ(splash.progress(), 0);
    splash.setProgress(10, 0);
    EXPECT_TRUE(splash.isIndeterminate());
    EXPECT_TRUE(splash.findChild<Label*>("splashPercentage")->isHidden());
    splash.setIndeterminate(false);
    EXPECT_FALSE(splash.isIndeterminate());
}

TEST_F(SplashScreenTest, Contract_PropertySignalsAndSmallLayout)
{
    QWidget host;
    host.resize(180, 160);
    SplashScreen splash(&host);
    splash.setPresentation(SplashScreen::Presentation::Simple);
    QSignalSpy sizes(&splash, &SplashScreen::iconSizeChanged);
    splash.setIconSize(QSize(-5, 0));
    splash.setIconSize(QSize(0, 0));
    EXPECT_EQ(splash.iconSize(), QSize(1, 1));
    EXPECT_EQ(sizes.count(), 1);
    splash.setIconSize(QSize(96, 96));
    QPixmap artwork(96, 96);
    artwork.fill(Qt::red);
    QIcon icon(artwork);
    QSignalSpy icons(&splash, &SplashScreen::iconChanged);
    splash.setIcon(icon);
    splash.setIcon(icon);
    EXPECT_EQ(icons.count(), 1);
    const QString text =
        QStringLiteral("Loading a very long application status without losing accessible text");
    QSignalSpy texts(&splash, &SplashScreen::textChanged);
    splash.setText(text);
    splash.setText(text);
    EXPECT_EQ(texts.count(), 1);
    splash.setProgress(60, 100);
    host.show();
    splash.show();
    QApplication::processEvents();
    EXPECT_EQ(splash.geometry(), host.rect());
    for (auto* child : splash.findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!child->isHidden())
            EXPECT_TRUE(splash.rect().contains(child->geometry()))
                << child->objectName().toStdString();
    }
    auto* label = splash.findChild<Label*>("splashStatusText");
    EXPECT_EQ(label->text(), text);
    EXPECT_TRUE(label->isTextElided());
    host.resize(800, 600);
    QApplication::processEvents();
    EXPECT_EQ(splash.geometry(), host.rect());
    EXPECT_EQ(splash.findChild<ProgressRing*>()->geometry().center().y(), 443);
}

TEST_F(SplashScreenTest, Contract_DismissHidesAndCanBeReused)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    QWidget host;
    SplashScreen splash(&host);
    QSignalSpy finished(&splash, &SplashScreen::dismissed);
    splash.dismiss();
    EXPECT_EQ(finished.count(), 0);
    host.show();
    splash.show();
    splash.setProgress(100, 100);
    EXPECT_TRUE(splash.isVisible());
    splash.dismiss();
    EXPECT_TRUE(splash.isHidden());
    EXPECT_EQ(finished.count(), 1);
    splash.dismiss();
    EXPECT_EQ(finished.count(), 1);
    splash.show();
    EXPECT_DOUBLE_EQ(static_cast<QGraphicsOpacityEffect*>(splash.graphicsEffect())->opacity(), 1.0);
    EXPECT_TRUE(splash.findChild<ProgressRing*>()->isHidden());
    EXPECT_TRUE(splash.findChild<fluent::status_info::ProgressBar*>()->isVisible());
    splash.dismiss();
    EXPECT_EQ(finished.count(), 2);
}

TEST_F(SplashScreenTest, Contract_AnimationCancellationAndDynamicMotion)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    QWidget host;
    SplashScreen splash(&host);
    host.show();
    splash.show();
    QSignalSpy finished(&splash, &SplashScreen::dismissed);
    splash.dismiss();
    splash.dismiss();
    auto* fade = splash.findChild<QPropertyAnimation*>();
    ASSERT_NE(fade, nullptr);
    EXPECT_TRUE(splash.isVisible());
    splash.hide();
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(fade->state(), QAbstractAnimation::Stopped);
    splash.show();
    splash.dismiss();
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    EXPECT_LE(fade->duration() - fade->currentTime(), 50);
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_TRUE(splash.isHidden());
}

TEST_F(SplashScreenTest, Contract_DeletionAndReentrantCompletion)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    QWidget host;
    host.show();
    QPointer<SplashScreen> splash = new SplashScreen(&host);
    splash->show();
    QObject::connect(splash, &SplashScreen::dismissed, splash, [splash]() { delete splash; });
    splash->dismiss();
    EXPECT_TRUE(splash.isNull());
    splash = new SplashScreen(&host);
    QObject::connect(splash, &SplashScreen::indeterminateChanged, splash,
                     [splash]() { delete splash; });
    splash->setProgress(50, 100);
    EXPECT_TRUE(splash.isNull());
    SplashScreen reusable(&host);
    reusable.show();
    QObject::connect(&reusable, &SplashScreen::dismissed, &reusable, [&]() { reusable.show(); });
    reusable.dismiss();
    EXPECT_TRUE(reusable.isVisible());
}

TEST_F(SplashScreenTest, Contract_InputIsScopedAndRestored)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    QWidget window;
    window.resize(640, 480);
    QWidget host(&window);
    host.setGeometry(0, 50, 640, 430);
    fluent::basicinput::Button beneath("Underlying action", &host);
    fluent::basicinput::Button chrome("Outside host", &window);
    chrome.move(0, 0);
    window.show();
    window.activateWindow();
    beneath.setFocus();
    QApplication::processEvents();
    SplashScreen splash(&host);
    splash.show();
    QSignalSpy underlying(&beneath, &QAbstractButton::clicked);
    QSignalSpy outside(&chrome, &QAbstractButton::clicked);
    QTest::mouseClick(&beneath, Qt::LeftButton);
    EXPECT_EQ(underlying.count(), 0);
    QTest::mouseClick(&chrome, Qt::LeftButton);
    EXPECT_EQ(outside.count(), 1);
    splash.setFocus();
    QTest::keyClick(&splash, Qt::Key_Tab);
    EXPECT_FALSE(beneath.hasFocus());
    QTest::keyClick(&splash, Qt::Key_Escape);
    EXPECT_TRUE(splash.isVisible());
    splash.dismiss();
    QTest::mouseClick(&beneath, Qt::LeftButton);
    EXPECT_EQ(underlying.count(), 1);
}

TEST_F(SplashScreenTest, Contract_KeyboardShortcutAndFocusRestoration)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    QWidget host;
    fluent::basicinput::Button beneath("Underlying action", &host);
    QShortcut shortcut(QKeySequence(Qt::CTRL | Qt::Key_K), &host);
    QSignalSpy activation(&shortcut, &QShortcut::activated);
    host.show();
    host.activateWindow();
    beneath.setFocus();
    QApplication::processEvents();
    SplashScreen splash(&host);
    splash.show();
    QApplication::processEvents();
    ASSERT_TRUE(splash.hasFocus());
    QTest::keyClick(&splash, Qt::Key_K, Qt::ControlModifier);
    EXPECT_EQ(activation.count(), 0);
    beneath.setFocus();
    EXPECT_TRUE(splash.hasFocus());
    splash.dismiss();
    EXPECT_TRUE(beneath.hasFocus());
    QTest::keyClick(&beneath, Qt::Key_K, Qt::ControlModifier);
    EXPECT_EQ(activation.count(), 1);
}

TEST_F(SplashScreenTest, Contract_NewSiblingsStayBehindCoverAndHostDeletionStopsAnimation)
{
    auto* host = new QWidget;
    host->resize(300, 200);
    auto* splash = new SplashScreen(host);
    host->show();
    splash->show();
    auto* added = new fluent::basicinput::Button("Late content", host);
    added->setGeometry(10, 10, 120, 32);
    added->show();
    added->raise();
    QApplication::processEvents();
    EXPECT_NE(host->childAt(20, 20), added);
    QPointer<SplashScreen> guard(splash);
    splash->dismiss();
    delete host;
    EXPECT_TRUE(guard.isNull());
    QApplication::processEvents();
}

TEST_F(SplashScreenTest, Contract_OverlappingCoversReplaceWithoutFocusOrStackingRecursion)
{
    QWidget host;
    host.resize(640, 480);
    SplashScreen first(&host);
    SplashScreen second(&host);
    QWidget nested(&host);
    nested.resize(200, 160);
    SplashScreen third(&nested);
    host.show();
    first.show();
    QSignalSpy dismissed(&first, &SplashScreen::dismissed);
    QSignalSpy replaced(&first, &SplashScreen::replaced);
    second.show();
    QApplication::processEvents();
    EXPECT_TRUE(first.isHidden());
    EXPECT_TRUE(second.isVisible());
    EXPECT_EQ(dismissed.count(), 0);
    EXPECT_EQ(replaced.count(), 1);
    third.show();
    QApplication::processEvents();
    EXPECT_TRUE(second.isHidden());
    EXPECT_TRUE(third.isVisible());
    first.show();
    EXPECT_TRUE(third.isHidden());
    EXPECT_TRUE(first.isVisible());
    first.hide();
    EXPECT_EQ(replaced.count(), 1);
}

TEST_F(SplashScreenTest, Contract_ReplacementCallbacksMayDeleteOrShowAnotherCover)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    QWidget host;
    host.resize(640, 480);
    host.show();
    QPointer<SplashScreen> first = new SplashScreen(&host);
    SplashScreen second(&host);
    SplashScreen third(&host);
    first->show();
    QObject::connect(first, &SplashScreen::replaced, &host, [&]() {
        EXPECT_TRUE(first->isHidden());
        EXPECT_TRUE(second.isVisible());
        delete first;
        third.show();
    });
    QSignalSpy secondReplaced(&second, &SplashScreen::replaced);
    second.show();
    EXPECT_TRUE(first.isNull());
    EXPECT_TRUE(second.isHidden());
    EXPECT_TRUE(third.isVisible());
    EXPECT_EQ(secondReplaced.count(), 1);
    third.dismiss();
    second.show();
    EXPECT_TRUE(second.isVisible());
}

TEST_F(SplashScreenTest, Contract_WindowMinimizePreservesDismissal)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(320, 240);
    SplashScreen splash(&host);
    host.show();
    splash.show();
    QApplication::processEvents();
    QSignalSpy completed(&splash, &SplashScreen::dismissed);
    splash.dismiss();
    host.showMinimized();
    QApplication::processEvents();
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    EXPECT_EQ(completed.count(), 1);
    EXPECT_TRUE(splash.isHidden());
    host.showNormal();
    QApplication::processEvents();
    EXPECT_FALSE(splash.isVisible());
}

TEST_F(SplashScreenTest, Contract_AccessibilityStatusAndProgress)
{
    QWidget host;
    SplashScreen splash(&host);
    splash.setAccessibleName("Startup");
    splash.setAccessibleDescription("Application-owned description");
    splash.setText("Loading workspace");
    splash.setProgress(2, 5);
    host.show();
    splash.show();
    auto* root = QAccessible::queryAccessibleInterface(&splash);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Pane);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Startup"));
    EXPECT_TRUE(root->state().busy);
    auto* ring = QAccessible::queryAccessibleInterface(
        splash.findChild<fluent::status_info::ProgressBar*>());
    ASSERT_NE(ring, nullptr);
    ASSERT_NE(ring->valueInterface(), nullptr);
    EXPECT_EQ(ring->valueInterface()->currentValue().toInt(), 40);
    auto* status =
        QAccessible::queryAccessibleInterface(splash.findChild<Label*>("splashStatusText"));
    EXPECT_EQ(status->text(QAccessible::Name), QStringLiteral("Loading workspace"));
    splash.hide();
    EXPECT_FALSE(root->state().busy);
}

TEST_F(SplashScreenTest, Contract_ThemeAndLargeCaptionLayout)
{
    SplashScreen splash;
    splash.resize(220, 180);
    splash.setText("Loading resources");
    splash.setProgress(1, 2);
    splash.setThemeOverrides({{"font", QJsonObject{{"scale", 2.0}}},
                              {"light", QJsonObject{{"textSecondary", "#804020"}}},
                              {"dark", QJsonObject{{"textSecondary", "#80E0D0"}}}});
    for (auto theme : {fluent::FluentElement::Theme::Light, fluent::FluentElement::Theme::Dark}) {
        fluent::FluentElement::setTheme(theme);
        splash.show();
        QApplication::processEvents();
        auto* label = splash.findChild<Label*>("splashStatusText");
        EXPECT_EQ(label->font().pixelSize(),
                  splash.themeFont(Typography::FontRole::Caption).toQFont().pixelSize());
        EXPECT_TRUE(splash.rect().contains(label->geometry()));
        EXPECT_EQ(label->themeColors().textSecondary, splash.themeColors().textSecondary);
        EXPECT_TRUE(
            splash.rect().contains(splash.findChild<Label*>("splashPercentage")->geometry()));
    }
}

TEST_F(SplashScreenTest, Contract_BrandedDefaultAndSimpleCompatibility)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(640, 480);
    SplashScreen splash(&host);
    EXPECT_EQ(splash.presentation(), SplashScreen::Presentation::Branded);
    QSignalSpy presentations(&splash, &SplashScreen::presentationChanged);
    QSignalSpy titles(&splash, &SplashScreen::titleChanged);
    QSignalSpy subtitles(&splash, &SplashScreen::subtitleChanged);
    splash.setTitle("Workspace");
    splash.setTitle("Workspace");
    splash.setSubtitle("Ready for your next idea");
    splash.setSubtitle("Ready for your next idea");
    EXPECT_EQ(titles.count(), 1);
    EXPECT_EQ(subtitles.count(), 1);
    splash.setProgress(40, 100);
    host.show();
    splash.show();
    auto* intro = splash.findChild<QVariantAnimation*>("splashRevealAnimation");
    ASSERT_NE(intro, nullptr);
    EXPECT_EQ(intro->state(), QAbstractAnimation::Running);
    EXPECT_TRUE(splash.findChild<ProgressRing*>()->isHidden());
    EXPECT_TRUE(splash.findChild<fluent::status_info::ProgressBar*>()->isVisible());
    splash.setPresentation(SplashScreen::Presentation::Simple);
    splash.setPresentation(SplashScreen::Presentation::Simple);
    splash.setPresentation(static_cast<SplashScreen::Presentation>(123));
    EXPECT_EQ(presentations.count(), 1);
    EXPECT_EQ(splash.progress(), 40);
    EXPECT_EQ(splash.title(), QStringLiteral("Workspace"));
    EXPECT_EQ(intro->state(), QAbstractAnimation::Stopped);
    EXPECT_TRUE(splash.findChild<ProgressRing*>()->isVisible());
    EXPECT_TRUE(splash.findChild<fluent::status_info::ProgressBar*>()->isHidden());
    EXPECT_TRUE(splash.findChild<QWidget*>("splashBrandText")->isHidden());
    splash.setPresentation(SplashScreen::Presentation::Branded);
    EXPECT_TRUE(splash.findChild<QWidget*>("splashBrandText")->isVisible());
    EXPECT_EQ(splash.progress(), 40);
}

TEST_F(SplashScreenTest, Contract_BrandTextRendersDuringRevealAndFade)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QWidget host;
    host.setStyleSheet("background: #101010;");
    host.resize(640, 480);
    SplashScreen splash(&host);
    splash.setTitle("Workspace");
    host.show();
    splash.show();
    auto* title = splash.findChild<Label*>("splashTitle");
    auto* intro = splash.findChild<QVariantAnimation*>("splashRevealAnimation");
    ASSERT_NE(title, nullptr);
    ASSERT_NE(intro, nullptr);
    const auto brightTitlePixels = [&]() {
        const QPixmap frame = host.grab();
        const qreal scale = frame.devicePixelRatio();
        const QPoint origin = title->mapTo(&host, QPoint());
        const QImage region =
            frame.toImage().copy(qRound(origin.x() * scale), qRound(origin.y() * scale),
                                 qRound(title->width() * scale), qRound(title->height() * scale));
        int count = 0;
        for (int y = 0; y < region.height(); ++y) {
            for (int x = 0; x < region.width(); ++x) {
                const QColor pixel = region.pixelColor(x, y);
                if (qMin(pixel.red(), qMin(pixel.green(), pixel.blue())) > 160)
                    ++count;
            }
        }
        return count;
    };
    intro->setCurrentTime(0);
    EXPECT_EQ(brightTitlePixels(), 0);
    const auto overrides = title->themeOverrides();
    const auto style = title->styleSheet();
    const auto geometry = title->geometry();
    for (int percent : {25, 40, 60, 80}) {
        intro->setCurrentTime(intro->duration() * percent / 100);
        EXPECT_EQ(title->themeOverrides(), overrides);
        EXPECT_EQ(title->styleSheet(), style);
        EXPECT_EQ(title->geometry(), geometry);
    }
    intro->setCurrentTime(intro->duration());
    EXPECT_GT(brightTitlePixels(), 50);
    splash.dismiss();
    auto* fade = splash.findChild<QPropertyAnimation*>("splashDismissAnimation");
    fade->setCurrentTime(fade->duration() * 3 / 4);
    EXPECT_EQ(brightTitlePixels(), 0);
    fade->setCurrentTime(fade->duration());
    EXPECT_TRUE(splash.isHidden());
}

TEST_F(SplashScreenTest, Contract_ConnectedDismissalBorrowsTargetAcrossHostBoundary)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    QWidget window;
    window.resize(640, 480);
    QWidget host(&window);
    host.setGeometry(0, 56, 640, 424);
    QLabel target(&window);
    target.setGeometry(24, 14, 28, 28);
    auto* effect = new QGraphicsOpacityEffect(&target);
    effect->setOpacity(0.6);
    target.setGraphicsEffect(effect);
    SplashScreen splash(&host);
    QPixmap artwork(96, 96);
    artwork.fill(Qt::blue);
    splash.setIcon(QIcon(artwork));
    splash.setTransitionTarget(&target);
    window.show();
    splash.show();
    QApplication::processEvents();
    QSignalSpy finished(&splash, &SplashScreen::dismissed);
    splash.dismiss();
    QPointer<QWidget> travelling = window.findChild<QWidget*>("splashLogoTransition");
    ASSERT_TRUE(travelling);
    EXPECT_EQ(travelling->parentWidget(), &window);
    EXPECT_TRUE(travelling->testAttribute(Qt::WA_TransparentForMouseEvents));
    EXPECT_EQ(travelling->focusPolicy(), Qt::NoFocus);
    auto* fade = splash.findChild<QPropertyAnimation*>("splashDismissAnimation");
    EXPECT_GE(fade->duration(), 600);
    EXPECT_LE(fade->duration(), 1000);
    const QPoint initialCenter = travelling->geometry().center();
    fade->setCurrentTime(fade->duration() / 4);
    const qreal departure = qreal(initialCenter.y() - travelling->geometry().center().y()) /
                            (initialCenter.y() - target.geometry().center().y());
    EXPECT_GT(departure, 0.05);
    EXPECT_LT(departure, 0.25); // A gentle start, instead of jumping most of the distance.
    fade->setCurrentTime(fade->duration() * 9 / 10);
    EXPECT_LT(travelling->geometry().center().y(), host.y());
    EXPECT_LT(qAbs(travelling->width() - target.width()), 8);
    target.move(140, 14);
    fade->setCurrentTime(fade->duration() * 99 / 100);
    EXPECT_LE(qAbs(travelling->geometry().center().x() - target.geometry().center().x()), 2);
    fade->setCurrentTime(fade->duration());
    EXPECT_TRUE(!travelling || !travelling->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(travelling.isNull());
    EXPECT_EQ(finished.count(), 1);
    EXPECT_TRUE(splash.isHidden());
    EXPECT_EQ(target.graphicsEffect(), effect);
    EXPECT_DOUBLE_EQ(effect->opacity(), 0.6);
    EXPECT_EQ(target.parentWidget(), &window);
    EXPECT_TRUE(target.isVisible());
}

TEST_F(SplashScreenTest, Contract_TargetDeletionAndCancelledTransitionAreSafe)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(640, 480);
    auto* target = new QWidget(&host);
    target->setGeometry(20, 20, 24, 24);
    SplashScreen splash(&host);
    QPixmap artwork(96, 96);
    artwork.fill(Qt::red);
    splash.setIcon(QIcon(artwork));
    QSignalSpy targets(&splash, &SplashScreen::transitionTargetChanged);
    splash.setTransitionTarget(target);
    splash.setTransitionTarget(target);
    splash.setTransitionTarget(&splash);
    EXPECT_EQ(targets.count(), 1);
    host.show();
    splash.show();
    splash.dismiss();
    QPointer<QWidget> travelling = host.findChild<QWidget*>("splashLogoTransition");
    ASSERT_TRUE(travelling);
    delete target;
    EXPECT_EQ(splash.transitionTarget(), nullptr);
    EXPECT_EQ(targets.count(), 2);
    auto* fade = splash.findChild<QPropertyAnimation*>("splashDismissAnimation");
    fade->setCurrentTime(fade->duration() / 2);
    QSignalSpy finished(&splash, &SplashScreen::dismissed);
    splash.hide();
    EXPECT_TRUE(!travelling || !travelling->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(travelling.isNull());
    EXPECT_EQ(finished.count(), 0);
    splash.show();
    EXPECT_TRUE(splash.isVisible());
    EXPECT_EQ(splash.findChild<QVariantAnimation*>("splashRevealAnimation")->state(),
              QAbstractAnimation::Running);
    splash.dismiss();
    fade->setCurrentTime(fade->duration());
    EXPECT_EQ(finished.count(), 1);
}

TEST_F(SplashScreenTest, Contract_WindowCloseDuringConnectedDismissal)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    QWidget window;
    window.resize(640, 480);
    QWidget host(&window);
    host.setGeometry(0, 48, 640, 432);
    QWidget target(&window);
    target.setGeometry(20, 12, 24, 24);
    SplashScreen splash(&host);
    QPixmap artwork(96, 96);
    artwork.fill(Qt::blue);
    splash.setIcon(QIcon(artwork));
    splash.setTransitionTarget(&target);
    for (int run = 0; run < 8; ++run) {
        window.show();
        splash.show();
        splash.dismiss();
        QPointer<QWidget> travelling = window.findChild<QWidget*>("splashLogoTransition");
        ASSERT_TRUE(travelling);
        QWidget laterSibling(&window);
        laterSibling.show();
        window.close();
        EXPECT_FALSE(window.isVisible());
        EXPECT_TRUE(!travelling || !travelling->isVisible());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        EXPECT_TRUE(travelling.isNull());
    }
}

TEST_F(SplashScreenTest, Contract_EntranceNeverDelaysDismissalAndRespectsMotionPolicy)
{
    QWidget host;
    host.resize(640, 480);
    QWidget target(&host);
    target.setGeometry(20, 20, 24, 24);
    SplashScreen splash(&host);
    QPixmap artwork(96, 96);
    artwork.fill(Qt::blue);
    splash.setIcon(QIcon(artwork));
    splash.setTransitionTarget(&target);
    host.show();
    for (auto mode :
         {MotionPolicy::Mode::Reduced, MotionPolicy::Mode::Disabled, MotionPolicy::Mode::Full}) {
        MotionPolicy::instance().setMode(mode);
        splash.show();
        auto* intro = splash.findChild<QVariantAnimation*>("splashRevealAnimation");
        EXPECT_EQ(intro->state(), mode == MotionPolicy::Mode::Full ? QAbstractAnimation::Running
                                                                   : QAbstractAnimation::Stopped);
        splash.dismiss();
        EXPECT_EQ(intro->state(), QAbstractAnimation::Stopped);
        auto* fade = splash.findChild<QPropertyAnimation*>("splashDismissAnimation");
        if (mode != MotionPolicy::Mode::Full) {
            EXPECT_EQ(host.findChild<QWidget*>("splashLogoTransition"), nullptr);
            EXPECT_LE(fade->duration(), 50);
        }
        if (fade->state() == QAbstractAnimation::Running)
            fade->setCurrentTime(fade->duration());
        EXPECT_TRUE(splash.isHidden());
    }
}

TEST_F(SplashScreenTest, Contract_BrandedNarrowLayoutAndHighContrast)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    SplashScreen splash;
    splash.setTitle(QString(120, QLatin1Char('W')));
    splash.setSubtitle(QString(160, QLatin1Char('S')));
    splash.setText(QString(200, QLatin1Char('L')));
    splash.setProgress(30, 100);
    splash.setThemeOverrides({{"font", QJsonObject{{"scale", 2.0}}}});
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark,
                       fluent::FluentElement::HighContrast}) {
        fluent::FluentElement::setTheme(theme);
        for (const auto size : {QSize(640, 480), QSize(220, 180), QSize(160, 160)}) {
            splash.resize(size);
            splash.show();
            QApplication::processEvents();
            for (auto* child : splash.findChildren<QWidget*>()) {
                if (!child->isVisible() || child->size().isEmpty())
                    continue;
                EXPECT_TRUE(
                    splash.rect().contains(QRect(child->mapTo(&splash, QPoint()), child->size())))
                    << child->objectName().toStdString();
            }
            if (theme == fluent::FluentElement::HighContrast)
                EXPECT_EQ(splash.findChild<QVariantAnimation*>("splashRevealAnimation")->state(),
                          QAbstractAnimation::Stopped);
        }
        splash.hide();
    }
}

TEST_F(SplashScreenTest, Contract_LoadingAnimationRepaintsOnlyChangingRegions)
{
    class PaintProbe final : public SplashScreen {
    public:
        using SplashScreen::SplashScreen;
        QRegion painted;

    protected:
        void paintEvent(QPaintEvent* event) override
        {
            painted += event->region();
            SplashScreen::paintEvent(event);
        }
    };

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    QWidget host;
    host.resize(1280, 800);
    PaintProbe splash(&host);
    QPixmap artwork(112, 112);
    artwork.fill(Qt::blue);
    splash.setIcon(QIcon(artwork));
    splash.setIconSize(artwork.size());
    splash.setTitle("Workspace");
    splash.setProgress(0, 100);
    host.show();
    splash.show();
    auto* intro = splash.findChild<QVariantAnimation*>("splashRevealAnimation");
    ASSERT_NE(intro, nullptr);
    intro->pause();
    intro->setCurrentTime(intro->duration() / 3);
    QApplication::processEvents();
    QApplication::processEvents();
    splash.painted = QRegion();
    intro->setCurrentTime(intro->duration() / 2);
    QApplication::processEvents();
    EXPECT_FALSE(splash.painted.isEmpty());
    EXPECT_FALSE(splash.painted.contains(QPoint(4, 4)));
    EXPECT_FALSE(splash.painted.contains(QPoint(host.width() - 4, 4)));

    intro->setCurrentTime(intro->duration());
    QApplication::processEvents();
    splash.painted = QRegion();
    splash.setProgress(50, 100);
    QApplication::processEvents();
    EXPECT_FALSE(splash.painted.contains(host.rect().center()));
    EXPECT_FALSE(splash.painted.contains(QPoint(4, 4)));
    EXPECT_EQ(splash.findChild<Label*>("splashPercentage")->text(), "50%");
}

TEST_F(SplashScreenTest, Contract_CachedBackgroundTracksThemeSizeAndPresentation)
{
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    SplashScreen splash;
    splash.setProgress(40, 100);
    splash.resize(640, 480);
    splash.show();
    splash.grab(); // Populate the cache before changing any of its inputs.

    for (auto theme : {fluent::FluentElement::Dark, fluent::FluentElement::HighContrast,
                       fluent::FluentElement::Light}) {
        fluent::FluentElement::setTheme(theme);
        for (auto presentation :
             {SplashScreen::Presentation::Simple, SplashScreen::Presentation::Branded}) {
            splash.setPresentation(presentation);
            for (const QSize size : {QSize(320, 240), QSize(900, 600)}) {
                splash.resize(size);
                SplashScreen fresh;
                fresh.setPresentation(presentation);
                fresh.setProgress(40, 100);
                fresh.resize(size);
                fresh.show();
                QApplication::processEvents();
                const QImage actual = splash.grab().toImage();
                const QImage expected = fresh.grab().toImage();
                ASSERT_EQ(actual.size(), expected.size());
                const qreal dpr = actual.devicePixelRatio();
                for (const QPoint point :
                     {QPoint(4, 4), QPoint(size.width() / 2, size.height() / 2),
                      QPoint(size.width() - 5, size.height() - 5)}) {
                    const QPoint pixel(qRound(point.x() * dpr), qRound(point.y() * dpr));
                    EXPECT_EQ(actual.pixelColor(pixel), expected.pixelColor(pixel));
                }
            }
        }
    }
}

TEST_F(SplashScreenTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP();
    SplashScreen splash;
    QPixmap artwork(96, 96);
    artwork.fill(QColor("#0078D4"));
    splash.setIcon(QIcon(artwork));
    splash.setText("Loading workspace resources");
    splash.setProgress(3, 5);
    const bool narrow = qEnvironmentVariableIsSet("SPLASH_NARROW");
    const bool dark = qEnvironmentVariableIsSet("SPLASH_DARK");
    fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Theme::Dark
                                         : fluent::FluentElement::Theme::Light);
    splash.resize(narrow ? QSize(220, 180) : QSize(640, 480));
    splash.show();
    if (tests::support::isVisualSnapshotMode()) {
        tests::support::VisualSnapshotOptions options;
        options.windowSize = splash.size();
        options.variant = QStringLiteral("splash-%1-%2")
                              .arg(dark ? "dark" : "light", narrow ? "narrow" : "normal");
        options.theme = dark ? tests::support::VisualSnapshotTheme::Dark
                             : tests::support::VisualSnapshotTheme::Light;
        EXPECT_TRUE(tests::support::captureVisualSnapshot(&splash, options));
        return;
    }
    qApp->exec();
}
