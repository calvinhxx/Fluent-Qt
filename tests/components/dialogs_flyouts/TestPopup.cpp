
#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <cstdlib>

#include <gtest/gtest.h>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/textfields/Label.h"
#include <QImage>

using namespace fluent::dialogs_flyouts;
using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::textfields::Label;

// ── FluentTestWindow ─────────────────────────────────────────────────────────
class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class PressProbe : public QWidget {
public:
    int presses = 0;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        ++presses;
        QWidget::mousePressEvent(event);
    }
};

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

// ── Fixture ──────────────────────────────────────────────────────────────────
class PopupTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        fluentRegisterMetaTypeNames<fluent::dialogs_flyouts::Popup::CloseReason>(
            "fluent::dialogs_flyouts::Popup::CloseReason", "CloseReason");
    }

    void SetUp() override
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        window = new FluentTestWindow();
        window->setFixedSize(800, 600);
        window->setWindowTitle("Popup Test");
        window->onThemeUpdated();
        window->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
    }
    void TearDown() override
    {
        delete window;
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    }

    FluentTestWindow* window = nullptr;
};

// ══════════════════════════════════════════════════════════════════════════════
// 1. 默认属性
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, DefaultProperties)
{
    Popup p(window);
    EXPECT_FALSE(p.isOpen());
    EXPECT_FALSE(p.isModal());
    EXPECT_FALSE(p.isDim());
    EXPECT_TRUE(p.isAnimationEnabled());
    EXPECT_TRUE(p.closePolicy().testFlag(Popup::CloseOnPressOutside));
    EXPECT_TRUE(p.closePolicy().testFlag(Popup::CloseOnEscape));
    // 默认尺寸 320+32 × 160+32
    EXPECT_EQ(p.width(), 352);
    EXPECT_EQ(p.height(), 192);
    const QColor publishedSurface = p.property("fluentSurfaceColor").value<QColor>();
    EXPECT_TRUE(publishedSurface.isValid());
    EXPECT_EQ(publishedSurface, p.themeColorsRef().bgLayer);
}

// ══════════════════════════════════════════════════════════════════════════════
// 2. open/close 信号顺序
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, OpenCloseSignals_AnimationDisabled)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy aboutToShow(&p, &Popup::aboutToShow);
    QSignalSpy opened(&p, &Popup::opened);
    QSignalSpy openedChanged(&p, &Popup::isOpenChanged);

    p.open();

    EXPECT_EQ(aboutToShow.count(), 1);
    EXPECT_EQ(opened.count(), 1);
    EXPECT_EQ(openedChanged.count(), 1);
    EXPECT_TRUE(p.isOpen());

    QSignalSpy aboutToHide(&p, &Popup::aboutToHide);
    QSignalSpy closed(&p, &Popup::closed);

    p.close();

    EXPECT_EQ(aboutToHide.count(), 1);
    EXPECT_EQ(closed.count(), 1);
    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, ReentrantOpenDuringAboutToShowIsIgnored)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy aboutToShow(&p, &Popup::aboutToShow);
    QSignalSpy opened(&p, &Popup::opened);
    QSignalSpy openedChanged(&p, &Popup::isOpenChanged);

    bool reentrantOpenAttempted = false;
    QObject::connect(&p, &Popup::aboutToShow, &p, [&]() {
        if (reentrantOpenAttempted)
            return;
        reentrantOpenAttempted = true;
        p.open();
    });

    p.open();

    EXPECT_TRUE(reentrantOpenAttempted);
    EXPECT_EQ(aboutToShow.count(), 1);
    EXPECT_EQ(opened.count(), 1);
    EXPECT_EQ(openedChanged.count(), 1);
    EXPECT_TRUE(p.isOpen());
    p.close();
}

TEST_F(PopupTest, AboutToShowHandlerCanSynchronouslyDeletePopup)
{
    auto* popup = new Popup(window);
    popup->setAnimationEnabled(false);
    QPointer<Popup> guard(popup);
    QObject::connect(popup, &Popup::aboutToShow, window, [popup] { delete popup; });

    popup->open();

    EXPECT_TRUE(guard.isNull());
}

TEST_F(PopupTest, OpenStateHandlerCanSynchronouslyDeletePopup)
{
    auto* popup = new Popup(window);
    popup->setAnimationEnabled(false);
    QPointer<Popup> guard(popup);
    QObject::connect(popup, &Popup::isOpenChanged, window, [popup](bool open) {
        if (open)
            delete popup;
    });

    popup->open();

    EXPECT_TRUE(guard.isNull());
}

TEST_F(PopupTest, ProgressHandlerCanSynchronouslyDeletePopup)
{
    auto* popup = new Popup(window);
    popup->setAnimationEnabled(false);
    QPointer<Popup> guard(popup);
    QObject::connect(popup, &Popup::popupProgressChanged, window,
                     [popup](double) { delete popup; });

    popup->open();

    EXPECT_TRUE(guard.isNull());
}

TEST_F(PopupTest, SetIsOpen_DelegatesToOpenClose)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy opened(&p, &Popup::opened);
    p.setIsOpen(true);
    EXPECT_EQ(opened.count(), 1);
    EXPECT_TRUE(p.isOpen());

    QSignalSpy closed(&p, &Popup::closed);
    p.setIsOpen(false);
    EXPECT_EQ(closed.count(), 1);
    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, Contract_IsOpenIsLogicalRequestedState)
{
    Popup p(window);
    QSignalSpy opened(&p, &Popup::opened);
    QSignalSpy closed(&p, &Popup::closed);
    QSignalSpy openChanged(&p, &Popup::isOpenChanged);

    p.open();
    EXPECT_TRUE(p.isOpen());
    EXPECT_TRUE(p.isVisible());
    EXPECT_EQ(opened.count(), 0);
    EXPECT_EQ(openChanged.count(), 1);
    EXPECT_LT(p.popupProgress(), 1.0);

    ASSERT_TRUE(QTest::qWaitFor([&]() { return opened.count() == 1; }, 1000));
    EXPECT_TRUE(p.isOpen());
    EXPECT_TRUE(p.isVisible());
    EXPECT_DOUBLE_EQ(p.popupProgress(), 1.0);

    p.close();
    EXPECT_FALSE(p.isOpen());
    EXPECT_TRUE(p.isVisible());
    EXPECT_EQ(closed.count(), 0);
    EXPECT_EQ(openChanged.count(), 2);

    ASSERT_TRUE(QTest::qWaitFor([&]() { return closed.count() == 1; }, 1000));
    EXPECT_FALSE(p.isOpen());
    EXPECT_FALSE(p.isVisible());
}

TEST_F(PopupTest, Contract_AnimationDisabledSyncSettlesOpenState)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QStringList order;
    QObject::connect(&p, &Popup::opening, [&] {
        EXPECT_FALSE(p.isOpen());
        EXPECT_FALSE(p.isVisible());
        order << QStringLiteral("opening");
    });
    QObject::connect(&p, &Popup::aboutToShow, [&] {
        EXPECT_FALSE(p.isOpen());
        EXPECT_FALSE(p.isVisible());
        order << QStringLiteral("aboutToShow");
    });
    QObject::connect(&p, &Popup::isOpenChanged, [&](bool open) {
        EXPECT_EQ(p.isOpen(), open);
        EXPECT_EQ(p.isVisible(), !open);
        order << (open ? QStringLiteral("isOpenChanged(true)")
                       : QStringLiteral("isOpenChanged(false)"));
    });
    QObject::connect(&p, &Popup::opened, [&] {
        EXPECT_TRUE(p.isOpen());
        EXPECT_TRUE(p.isVisible());
        order << QStringLiteral("opened");
    });
    QObject::connect(&p, &Popup::closing, [&] {
        EXPECT_TRUE(p.isOpen());
        EXPECT_TRUE(p.isVisible());
        order << QStringLiteral("closing");
    });
    QObject::connect(&p, &Popup::aboutToHide, [&] { order << QStringLiteral("aboutToHide"); });
    QObject::connect(&p, &Popup::closed, [&] {
        EXPECT_FALSE(p.isOpen());
        EXPECT_FALSE(p.isVisible());
        order << QStringLiteral("closed");
    });

    p.open();
    EXPECT_TRUE(p.isOpen());
    EXPECT_TRUE(p.isVisible());
    EXPECT_DOUBLE_EQ(p.popupProgress(), 1.0);
    EXPECT_EQ(order, (QStringList{
                         QStringLiteral("opening"),
                         QStringLiteral("aboutToShow"),
                         QStringLiteral("isOpenChanged(true)"),
                         QStringLiteral("opened"),
                     }));

    p.close();
    EXPECT_FALSE(p.isOpen());
    EXPECT_FALSE(p.isVisible());
    EXPECT_DOUBLE_EQ(p.popupProgress(), 0.0);
    EXPECT_EQ(order, (QStringList{
                         QStringLiteral("opening"),
                         QStringLiteral("aboutToShow"),
                         QStringLiteral("isOpenChanged(true)"),
                         QStringLiteral("opened"),
                         QStringLiteral("closing"),
                         QStringLiteral("aboutToHide"),
                         QStringLiteral("isOpenChanged(false)"),
                         QStringLiteral("closed"),
                     }));
}

TEST_F(PopupTest, Contract_CloseWhileOpeningCancelsEntrance)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QStringList order;
    QObject::connect(&p, &Popup::opening, [&] { order << QStringLiteral("opening"); });
    QObject::connect(&p, &Popup::aboutToShow, [&] { order << QStringLiteral("aboutToShow"); });
    QObject::connect(&p, &Popup::opened, [&] { order << QStringLiteral("opened"); });
    QObject::connect(&p, &Popup::closing, [&] { order << QStringLiteral("closing"); });
    QObject::connect(&p, &Popup::aboutToHide, [&] { order << QStringLiteral("aboutToHide"); });
    QObject::connect(&p, &Popup::closed, [&] { order << QStringLiteral("closed"); });
    bool cancelNextOpen = true;
    QObject::connect(&p, &Popup::opening, &p, [&]() {
        if (cancelNextOpen)
            p.close();
    });

    p.open();
    EXPECT_FALSE(p.isOpen());
    EXPECT_FALSE(p.isVisible());
    EXPECT_EQ(order, (QStringList{
                         QStringLiteral("opening"),
                         QStringLiteral("closing"),
                         QStringLiteral("aboutToHide"),
                         QStringLiteral("closed"),
                     }));

    cancelNextOpen = false;
    p.open();
    EXPECT_TRUE(p.isOpen());
    p.close();
    EXPECT_FALSE(p.isOpen());

    Popup animated(window);
    QSignalSpy opened(&animated, &Popup::opened);
    QSignalSpy closed(&animated, &Popup::closed);
    animated.open();
    EXPECT_TRUE(animated.isOpen());
    EXPECT_EQ(opened.count(), 0);

    animated.close();
    EXPECT_FALSE(animated.isOpen());
    EXPECT_TRUE(animated.isVisible());
    ASSERT_TRUE(QTest::qWaitFor([&]() { return closed.count() == 1; }, 1000));
    EXPECT_FALSE(animated.isVisible());
    EXPECT_EQ(opened.count(), 0);
}

TEST_F(PopupTest, Contract_OpenWhileOpeningIsIgnored)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy opening(&p, &Popup::opening);
    QSignalSpy opened(&p, &Popup::opened);
    QObject::connect(&p, &Popup::opening, &p, [&]() { p.open(); });

    p.open();
    EXPECT_EQ(opening.count(), 1);
    EXPECT_EQ(opened.count(), 1);
    EXPECT_TRUE(p.isOpen());
    p.close();
}

TEST_F(PopupTest, Contract_CloseWhileClosingIsIgnored)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy closing(&p, &Popup::closing);
    QSignalSpy closed(&p, &Popup::closed);
    QObject::connect(&p, &Popup::closing, &p, [&]() { p.close(); });

    p.open();
    p.close();
    EXPECT_EQ(closing.count(), 1);
    EXPECT_EQ(closed.count(), 1);
    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, Contract_OpenWhileClosingReversesToOpen)
{
    Popup p(window);
    QSignalSpy opened(&p, &Popup::opened);
    p.open();
    ASSERT_TRUE(QTest::qWaitFor([&]() { return opened.count() == 1; }, 1000));

    p.close();
    EXPECT_FALSE(p.isOpen());
    EXPECT_TRUE(p.isVisible());

    p.open();
    EXPECT_TRUE(p.isOpen());
    ASSERT_TRUE(QTest::qWaitFor([&]() { return opened.count() == 2; }, 1000));
    EXPECT_TRUE(p.isVisible());
    EXPECT_DOUBLE_EQ(p.popupProgress(), 1.0);
    p.close();
    ASSERT_TRUE(QTest::qWaitFor([&]() { return !p.isVisible(); }, 1000));
}

TEST_F(PopupTest, Contract_CloseReasonsAndNotifyNoOps)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy closing(&p, &Popup::closing);
    QSignalSpy modalChanged(&p, &Popup::modalChanged);
    QSignalSpy dimChanged(&p, &Popup::dimChanged);
    QSignalSpy policyChanged(&p, &Popup::closePolicyChanged);
    QSignalSpy animationChanged(&p, &Popup::animationEnabledChanged);
    QSignalSpy openChanged(&p, &Popup::isOpenChanged);

    p.setModal(true);
    p.setModal(true);
    p.setDim(true);
    p.setDim(true);
    p.setClosePolicy(Popup::CloseOnEscape);
    p.setClosePolicy(Popup::CloseOnEscape);
    p.setAnimationEnabled(false);
    p.setAnimationEnabled(false);
    EXPECT_EQ(modalChanged.count(), 1);
    EXPECT_EQ(dimChanged.count(), 1);
    EXPECT_EQ(policyChanged.count(), 1);
    EXPECT_EQ(animationChanged.count(), 0);

    p.open();
    p.close();
    ASSERT_EQ(closing.count(), 1);
    EXPECT_EQ(closing.at(0).at(0).toInt(), static_cast<int>(Popup::Programmatic));

    p.open();
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&p, &ev);
    ASSERT_EQ(closing.count(), 2);
    EXPECT_EQ(closing.at(1).at(0).toInt(), static_cast<int>(Popup::Escape));

    p.setClosePolicy(Popup::ClosePolicy(Popup::CloseOnPressOutside | Popup::CloseOnEscape));
    p.open();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    processEvents();
    ASSERT_EQ(closing.count(), 3);
    EXPECT_EQ(closing.at(2).at(0).toInt(), static_cast<int>(Popup::LightDismiss));

    p.setIsOpen(true);
    const int openSignals = openChanged.count();
    p.setIsOpen(true);
    EXPECT_EQ(openChanged.count(), openSignals);
    p.setIsOpen(false);
}

TEST_F(PopupTest, Contract_ModalDimClosePolicyAreOrthogonal)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setClosePolicy(Popup::NoAutoClose);

    p.setModal(false);
    p.setDim(false);
    p.open();
    EXPECT_EQ(window->findChild<fluent::overlay::OverlayScrim*>(QStringLiteral("PopupScrim"),
                                                                Qt::FindDirectChildrenOnly),
              nullptr);
    p.close();

    p.setModal(true);
    p.setDim(false);
    p.open();
    auto* modalScrim = window->findChild<fluent::overlay::OverlayScrim*>(
        QStringLiteral("PopupScrim"), Qt::FindDirectChildrenOnly);
    ASSERT_NE(modalScrim, nullptr);
    EXPECT_FALSE(modalScrim->testAttribute(Qt::WA_TransparentForMouseEvents));
    p.close();
    processEvents();

    p.setModal(false);
    p.setDim(true);
    p.open();
    auto* dimScrim = window->findChild<fluent::overlay::OverlayScrim*>(QStringLiteral("PopupScrim"),
                                                                       Qt::FindDirectChildrenOnly);
    ASSERT_NE(dimScrim, nullptr);
    EXPECT_TRUE(dimScrim->testAttribute(Qt::WA_TransparentForMouseEvents));
    p.close();
    processEvents();

    p.setModal(true);
    p.setDim(true);
    p.open();
    auto* both = window->findChild<fluent::overlay::OverlayScrim*>(QStringLiteral("PopupScrim"),
                                                                   Qt::FindDirectChildrenOnly);
    ASSERT_NE(both, nullptr);
    EXPECT_FALSE(both->testAttribute(Qt::WA_TransparentForMouseEvents));
    p.close();
}

TEST_F(PopupTest, Contract_ThemeChangeDoesNotMutateOpenState)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.open();

    QSignalSpy openChanged(&p, &Popup::isOpenChanged);
    QSignalSpy opened(&p, &Popup::opened);
    QSignalSpy closed(&p, &Popup::closed);

    const auto previous = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(previous == fluent::FluentElement::Light
                                        ? fluent::FluentElement::Dark
                                        : fluent::FluentElement::Light);
    p.onThemeUpdated();

    EXPECT_TRUE(p.isOpen());
    EXPECT_EQ(openChanged.count(), 0);
    EXPECT_EQ(opened.count(), 0);
    EXPECT_EQ(closed.count(), 0);

    fluent::FluentElement::setTheme(previous);
    p.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 3. 挂载到 topLevelWidget
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, OpenReparentsToTopLevelWidget)
{
    auto* mid = new QWidget(window);
    auto* leaf = new QWidget(mid);

    Popup p(leaf);
    p.setAnimationEnabled(false);
    p.open();

    EXPECT_EQ(p.parentWidget(), window);
    EXPECT_FALSE(p.isWindow());
    EXPECT_NE(p.windowType(), Qt::Window);
    EXPECT_NE(p.windowType(), Qt::Dialog);
    EXPECT_TRUE(p.isVisible());

    p.close();
}

TEST_F(PopupTest, CloseRestoresFocusWhenItRemainsInsidePopup)
{
    auto* trigger = new Button("Open", window);
    trigger->setGeometry(24, 24, 100, 36);
    trigger->show();
    window->activateWindow();
    trigger->setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), trigger, 1000);

    Popup p(window);
    p.setAnimationEnabled(false);
    p.open();
    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), &p, 1000);

    p.close();

    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), trigger, 1000);
}

TEST_F(PopupTest, CloseDoesNotStealFocusMovedOutsidePopup)
{
    auto* trigger = new Button("Open", window);
    trigger->setGeometry(24, 24, 100, 36);
    trigger->show();
    auto* destination = new Button("Destination", window);
    destination->setGeometry(160, 24, 120, 36);
    destination->show();
    window->activateWindow();
    trigger->setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), trigger, 1000);

    Popup p(window);
    p.setAnimationEnabled(false);
    p.open();
    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), &p, 1000);
    destination->setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), destination, 1000);

    p.close();

    QTRY_COMPARE_WITH_TIMEOUT(QApplication::focusWidget(), destination, 1000);
}

// ══════════════════════════════════════════════════════════════════════════════
// 4. x / y 定位
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, ExplicitPosition_RespectsXY)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.resize(120, 60);
    p.setPosition(window, QPoint(50, 80));
    p.open();

    // setPosition 接收可见卡片坐标，widget 实际 pos = (50-16, 80-16)
    EXPECT_EQ(p.pos(), QPoint(34, 64));
    p.close();
}

TEST_F(PopupTest, RelativePositionTracksMovingAncestorAndClosesWhenClipped)
{
    auto* scrollingContent = new QWidget(window);
    scrollingContent->setGeometry(0, 0, window->width(), 1000);
    scrollingContent->show();

    auto* trigger = new QWidget(scrollingContent);
    trigger->setGeometry(120, 280, 80, 36);
    trigger->show();

    Popup p(window);
    p.setAnimationEnabled(false);
    p.setPosition(trigger, QPoint(0, trigger->height() + 8));
    p.open();
    ASSERT_TRUE(p.isOpen());
    const QPoint initialPosition = p.pos();

    scrollingContent->move(0, -64);
    QTRY_COMPARE_WITH_TIMEOUT(p.pos(), initialPosition - QPoint(0, 64), 1000);

    scrollingContent->move(0, -500);
    QTRY_VERIFY_WITH_TIMEOUT(!p.isOpen(), 1000);
}

TEST_F(PopupTest, DefaultPosition_CentersInParent)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.resize(200, 100);
    // 不调用 move()，应当居中
    p.open();

    QPoint expected((window->width() - 200) / 2, (window->height() - 100) / 2);
    EXPECT_EQ(p.pos(), expected);
    p.close();
}

TEST_F(PopupTest, VisibleCardPressKeepsPopupOpen)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setPosition(window, QPoint(120, 140));
    p.open();
    ASSERT_TRUE(p.isOpen());

    const int shadow = fluent::overlay::defaultShadowMargin();
    QTest::mouseClick(&p, Qt::LeftButton, Qt::NoModifier, QPoint(shadow + 8, shadow + 8));
    processEvents();

    EXPECT_TRUE(p.isOpen());
    p.close();
}

TEST_F(PopupTest, ShadowMarginPressDismissesAsOutsideVisibleCard)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setPosition(window, QPoint(120, 140));
    p.open();
    ASSERT_TRUE(p.isOpen());

    const int shadow = fluent::overlay::defaultShadowMargin();
    QTest::mouseClick(&p, Qt::LeftButton, Qt::NoModifier, QPoint(shadow / 2, shadow + 8));
    processEvents();

    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, RelativePosition_MapsWidgetLocalCoordinates)
{
    auto* trigger = new QWidget(window);
    trigger->setGeometry(120, 220, 80, 36);

    Popup p(window);
    p.setAnimationEnabled(false);

    p.setPosition(trigger, QPoint(12, 18));
    p.open();

    const QPoint expected = trigger->mapTo(window, QPoint(12, 18)) - QPoint(16, 16);
    EXPECT_EQ(p.pos(), expected);
    p.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 5. ClosePolicy
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, ExplicitPositionInheritsThemeOverrideFromRelativeWidget)
{
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    host->setGeometry(80, 120, 240, 160);
    host->show();

    auto* trigger = new QWidget(host);
    trigger->setGeometry(24, 24, 80, 32);
    trigger->show();

    Popup p(trigger);
    p.setAnimationEnabled(false);
    p.setPosition(trigger, QPoint(0, trigger->height() + 8));
    p.open();

    EXPECT_EQ(p.parentWidget(), window);
    EXPECT_EQ(p.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(p.themeColors().bgLayer, QColor("#2C2C2C"));
    p.close();
}

TEST_F(PopupTest, ThemeSourceInheritsOverrideWithoutAnchor)
{
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Light));
    host->setGeometry(80, 120, 240, 160);
    host->show();

    auto* trigger = new QWidget(host);
    trigger->setGeometry(24, 24, 80, 32);
    trigger->show();

    Popup p(window);
    p.setAnimationEnabled(false);
    p.setThemeSource(trigger);
    p.open();

    EXPECT_EQ(p.effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(p.themeColors().bgLayer, QColor("#FFFFFF"));
    p.close();
}

TEST_F(PopupTest, NoAutoClose_PressOutsideKeepsOpen)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setClosePolicy(Popup::NoAutoClose);
    p.open();
    ASSERT_TRUE(p.isOpen());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QApplication::processEvents();

    EXPECT_TRUE(p.isOpen());
    p.close();
}

TEST_F(PopupTest, EscapeClosesFromOwningTopLevelContext)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.open();
    ASSERT_TRUE(p.isOpen());

    QTest::keyClick(window, Qt::Key_Escape);
    processEvents();

    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, NonModalOutsidePressClosesAndContinuesToBackgroundTarget)
{
    auto* background = new PressProbe();
    background->setParent(window);
    background->setGeometry(window->rect());
    background->show();

    Popup p(window);
    p.setAnimationEnabled(false);
    p.setModal(false);
    p.setPosition(window, QPoint(300, 220));
    p.open();
    ASSERT_TRUE(p.isOpen());

    QTest::mouseClick(background, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    processEvents();

    EXPECT_EQ(background->presses, 1);
    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, EscapeClosesPopupWhenPolicySet)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.open();
    ASSERT_TRUE(p.isOpen());

    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&p, &ev);

    EXPECT_FALSE(p.isOpen());
}

TEST_F(PopupTest, EscapeIgnoredWhenPolicyOmitsCloseOnEscape)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setClosePolicy(Popup::NoAutoClose);
    p.open();
    ASSERT_TRUE(p.isOpen());

    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&p, &ev);

    EXPECT_TRUE(p.isOpen());
    p.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 6. Modal — Scrim
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, Modal_CreatesScrimOverTopLevel)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setModal(true);
    p.setDim(true);
    p.open();

    bool foundScrim = false;
    for (auto* child : window->findChildren<QWidget*>()) {
        if (child == &p)
            continue;
        if (child->parent() == window && child->geometry() == window->rect() &&
            child->isVisible()) {
            foundScrim = true;
            break;
        }
    }
    EXPECT_TRUE(foundScrim);

    p.close();
    QApplication::processEvents();
}

TEST_F(PopupTest, ModalScrimBlocksBackgroundInput)
{
    auto* background = new PressProbe();
    background->setParent(window);
    background->setGeometry(window->rect());
    background->show();

    Popup p(window);
    p.setAnimationEnabled(false);
    p.setModal(true);
    p.setDim(true);
    p.setPosition(window, QPoint(300, 220));
    p.open();
    ASSERT_TRUE(p.isOpen());

    const QPoint outsidePopupPoint(10, 10);
    QWidget* blocker = window->childAt(outsidePopupPoint);
    ASSERT_NE(blocker, nullptr);
    EXPECT_NE(blocker, background);
    EXPECT_NE(blocker, &p);

    QTest::mouseClick(blocker, Qt::LeftButton, Qt::NoModifier,
                      blocker->mapFrom(window, outsidePopupPoint));
    processEvents();

    EXPECT_EQ(background->presses, 0);
    p.close();
}

TEST_F(PopupTest, HostResizeSynchronizesScrimAndCenteredPlacement)
{
    window->setMinimumSize(0, 0);
    window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    Popup p(window);
    p.setAnimationEnabled(false);
    p.setModal(true);
    p.setDim(true);
    p.resize(200, 100);
    p.open();

    auto* scrim = window->findChild<fluent::overlay::OverlayScrim*>(QStringLiteral("PopupScrim"),
                                                                    Qt::FindDirectChildrenOnly);
    ASSERT_NE(scrim, nullptr);
    EXPECT_EQ(scrim->geometry(), fluent::overlay::overlaySurfaceRect(window));

    window->resize(920, 680);

    QTRY_COMPARE_WITH_TIMEOUT(scrim->geometry(), fluent::overlay::overlaySurfaceRect(window), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(
        p.pos(), QPoint((window->width() - p.width()) / 2, (window->height() - p.height()) / 2),
        1000);
    EXPECT_TRUE(scrim->isVisible());
    EXPECT_TRUE(p.isVisible());
    p.close();
}

TEST_F(PopupTest, NonModal_CreatesNoScrim)
{
    Popup p(window);
    p.setAnimationEnabled(false);
    p.setModal(false);
    p.open();

    int candidateScrims = 0;
    for (auto* child : window->findChildren<QWidget*>()) {
        if (child == &p)
            continue;
        if (child->parent() == window && child->geometry() == window->rect() &&
            child->isVisible()) {
            ++candidateScrims;
        }
    }
    EXPECT_EQ(candidateScrims, 0);

    p.close();
}

TEST_F(PopupTest, ParentDestructionDeletesOwnedPopupSafely)
{
    auto* top = new FluentTestWindow();
    top->setFixedSize(360, 240);
    top->onThemeUpdated();
    top->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(top));

    auto* popup = new Popup(top);
    QPointer<Popup> popupPointer = popup;
    popup->setAnimationEnabled(false);
    popup->open();
    ASSERT_TRUE(popup->isOpen());

    delete top;
    processEvents();

    EXPECT_TRUE(popupPointer.isNull());
}

// ══════════════════════════════════════════════════════════════════════════════
// 7. 动画
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(PopupTest, AnimationDisabled_OpenedEmittedSynchronously)
{
    Popup p(window);
    p.setAnimationEnabled(false);

    QSignalSpy spy(&p, &Popup::opened);
    p.open();
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(PopupTest, MotionPolicyDisabledSettlesOpenAndCloseSynchronously)
{
    Popup popup(window);
    QSignalSpy openedSpy(&popup, &Popup::opened);
    QSignalSpy closedSpy(&popup, &Popup::closed);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    popup.open();

    EXPECT_TRUE(popup.isOpen());
    EXPECT_TRUE(popup.isVisible());
    EXPECT_DOUBLE_EQ(popup.popupProgress(), 1.0);
    EXPECT_EQ(openedSpy.count(), 1);

    popup.close();

    EXPECT_FALSE(popup.isOpen());
    EXPECT_FALSE(popup.isVisible());
    EXPECT_DOUBLE_EQ(popup.popupProgress(), 0.0);
    EXPECT_EQ(closedSpy.count(), 1);
}

TEST_F(PopupTest, DisablingLocalAnimationSettlesActiveEntrance)
{
    Popup popup(window);
    QSignalSpy openedSpy(&popup, &Popup::opened);
    popup.open();
    popup.setAnimationEnabled(false);

    EXPECT_TRUE(popup.isOpen());
    EXPECT_TRUE(popup.isVisible());
    EXPECT_DOUBLE_EQ(popup.popupProgress(), 1.0);
    EXPECT_EQ(openedSpy.count(), 1);

    popup.close();
}

TEST_F(PopupTest, DisablingExitAnimationSettlesActiveExitThroughClosedCleanup)
{
    Popup popup(window);
    QSignalSpy openedSpy(&popup, &Popup::opened);
    QSignalSpy closedSpy(&popup, &Popup::closed);
    QStringList order;
    QObject::connect(&popup, &Popup::closing, &popup,
                     [&](Popup::CloseReason) { order << QStringLiteral("closing"); });
    QObject::connect(&popup, &Popup::aboutToHide, &popup,
                     [&] { order << QStringLiteral("aboutToHide"); });
    QObject::connect(&popup, &Popup::isOpenChanged, &popup, [&](bool open) {
        if (!open)
            order << QStringLiteral("isOpenChanged(false)");
    });
    QObject::connect(&popup, &Popup::closed, &popup, [&] { order << QStringLiteral("closed"); });

    popup.open();
    ASSERT_TRUE(QTest::qWaitFor([&] { return openedSpy.count() == 1; }, 1000));

    popup.close();
    EXPECT_FALSE(popup.isOpen());
    EXPECT_TRUE(popup.isVisible());
    EXPECT_EQ(closedSpy.count(), 0);
    EXPECT_EQ(order, QStringList({QStringLiteral("closing"), QStringLiteral("aboutToHide"),
                                  QStringLiteral("isOpenChanged(false)")}));

    popup.setExitAnimationEnabled(false);

    EXPECT_FALSE(popup.isVisible());
    EXPECT_DOUBLE_EQ(popup.popupProgress(), 0.0);
    EXPECT_EQ(closedSpy.count(), 1);
    EXPECT_EQ(order,
              QStringList({QStringLiteral("closing"), QStringLiteral("aboutToHide"),
                           QStringLiteral("isOpenChanged(false)"), QStringLiteral("closed")}));
}

TEST_F(PopupTest, AnimationProgress_DrivesUpdates)
{
    Popup p(window);

    QSignalSpy spy(&p, &Popup::popupProgressChanged);
    QSignalSpy opened(&p, &Popup::opened);
    p.open();
    EXPECT_TRUE(p.isOpen());
    EXPECT_TRUE(p.isVisible());

    ASSERT_TRUE(QTest::qWaitFor([&]() { return opened.count() == 1; }, 1000));
    EXPECT_GT(spy.count(), 1);
    EXPECT_DOUBLE_EQ(p.popupProgress(), 1.0);

    p.close();
}

TEST_F(PopupTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    // VisualCheck 不复用 fixture 的 window —— 关掉它，避免遮挡/拦截事件
    window->hide();

    using Edge = AnchorLayout::Edge;

    auto* visual = new FluentTestWindow();
    visual->setFixedSize(1000, 750);
    visual->setWindowTitle("Popup VisualCheck — click buttons to open popups");
    visual->onThemeUpdated();

    auto* layout = new AnchorLayout(visual);
    visual->setLayout(layout);

    // ── Theme toggle ─────────────────────────────────────────────
    auto* themeBtn = new Button("Toggle Theme", visual);
    themeBtn->setFixedSize(160, 32);
    themeBtn->anchors()->top = {visual, Edge::Top, 16};
    themeBtn->anchors()->right = {visual, Edge::Right, -16};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, [visual]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() ==
                                                fluent::FluentElement::Light
                                            ? fluent::FluentElement::Dark
                                            : fluent::FluentElement::Light);
        visual->onThemeUpdated();
    });

    // ── 1. Info Popup — positioned (60, 130) ─────────────────────
    auto* btn1 = new Button("Info Popup", visual);
    btn1->setFixedSize(160, 36);
    btn1->anchors()->top = {visual, Edge::Top, 80};
    btn1->anchors()->left = {visual, Edge::Left, 60};
    layout->addWidget(btn1);

    {
        auto* p = new Popup(visual);
        p->setPosition(visual, QPoint(60, 130));

        auto* pl = new AnchorLayout(p);
        p->setLayout(pl);

        auto* title = new Label("Information", p);
        title->setFluentTypography(Typography::FontRole::Subtitle);
        title->anchors()->top = {p, Edge::Top, 24};
        title->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(title);

        auto* caption = new Label("Last updated: just now", p);
        caption->setFluentTypography(Typography::FontRole::Caption);
        caption->anchors()->top = {title, Edge::Bottom, 4};
        caption->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(caption);

        auto* body = new Label("This popup demonstrates x/y positioning.\n"
                               "It appears at a fixed location relative\n"
                               "to the parent window.",
                               p);
        body->anchors()->top = {caption, Edge::Bottom, 12};
        body->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(body);

        auto* gotItBtn = new Button("Got it", p);
        gotItBtn->setFixedSize(80, 32);
        gotItBtn->anchors()->top = {body, Edge::Bottom, 16};
        gotItBtn->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(gotItBtn);

        QObject::connect(gotItBtn, &Button::clicked, p, &Popup::close);
        QObject::connect(btn1, &Button::clicked, p, &Popup::open);
    }

    // ── 2. Center Popup — default center position ────────────────
    auto* btn2 = new Button("Center Popup", visual);
    btn2->setFixedSize(160, 36);
    btn2->anchors()->horizontalCenter = {visual, Edge::HCenter, 0};
    btn2->anchors()->top = {visual, Edge::Top, 80};
    layout->addWidget(btn2);

    {
        auto* p = new Popup(visual);
        // no setPosition — defaults to center

        auto* pl = new AnchorLayout(p);
        p->setLayout(pl);

        auto* title = new Label("Quick Actions", p);
        title->setFluentTypography(Typography::FontRole::Subtitle);
        title->anchors()->top = {p, Edge::Top, 24};
        title->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(title);

        auto* desc = new Label("Choose an action to perform:", p);
        desc->anchors()->top = {title, Edge::Bottom, 8};
        desc->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(desc);

        auto* actionA = new Button("Action A", p);
        actionA->setFixedSize(120, 32);
        actionA->anchors()->top = {desc, Edge::Bottom, 16};
        actionA->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(actionA);

        auto* actionB = new Button("Action B", p);
        actionB->setFixedSize(120, 32);
        actionB->anchors()->top = {desc, Edge::Bottom, 16};
        actionB->anchors()->left = {actionA, Edge::Right, 8};
        pl->addWidget(actionB);

        auto* hint = new Label("Press Escape to dismiss", p);
        hint->setFluentTypography(Typography::FontRole::Caption);
        hint->anchors()->top = {actionA, Edge::Bottom, 12};
        hint->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(hint);

        QObject::connect(actionA, &Button::clicked, p, &Popup::close);
        QObject::connect(actionB, &Button::clicked, p, &Popup::close);
        QObject::connect(btn2, &Button::clicked, p, &Popup::open);
    }

    // ── 3. Modal + Dim Popup — confirm dialog style ──────────────
    auto* btn3 = new Button("Modal + Dim", visual);
    btn3->setFixedSize(160, 36);
    btn3->anchors()->horizontalCenter = {visual, Edge::HCenter, 0};
    btn3->anchors()->verticalCenter = {visual, Edge::VCenter, 0};
    layout->addWidget(btn3);

    {
        auto* p = new Popup(visual);
        p->setModal(true);
        p->setDim(true);
        p->setClosePolicy(Popup::CloseOnEscape); // 只能通过按钮或 Escape 关闭

        auto* pl = new AnchorLayout(p);
        p->setLayout(pl);

        auto* title = new Label("Confirm Delete", p);
        title->setFluentTypography(Typography::FontRole::Subtitle);
        title->anchors()->top = {p, Edge::Top, 24};
        title->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(title);

        auto* body = new Label("Are you sure you want to delete this item?\n"
                               "This action cannot be undone.",
                               p);
        body->anchors()->top = {title, Edge::Bottom, 12};
        body->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(body);

        auto* deleteBtn = new Button("Delete", p);
        deleteBtn->setFixedSize(100, 32);
        deleteBtn->anchors()->top = {body, Edge::Bottom, 20};
        deleteBtn->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(deleteBtn);

        auto* cancelBtn = new Button("Cancel", p);
        cancelBtn->setFixedSize(100, 32);
        cancelBtn->anchors()->top = {body, Edge::Bottom, 20};
        cancelBtn->anchors()->left = {deleteBtn, Edge::Right, 8};
        pl->addWidget(cancelBtn);

        QObject::connect(deleteBtn, &Button::clicked, p, &Popup::close);
        QObject::connect(cancelBtn, &Button::clicked, p, &Popup::close);
        QObject::connect(btn3, &Button::clicked, p, &Popup::open);
    }

    // ── 4. Notification Popup — bottom-right positioned ──────────
    auto* btn4 = new Button("Notification", visual);
    btn4->setFixedSize(160, 36);
    btn4->anchors()->bottom = {visual, Edge::Bottom, -60};
    btn4->anchors()->right = {visual, Edge::Right, -60};
    layout->addWidget(btn4);

    {
        auto* p = new Popup(visual);
        p->setPosition(visual, QPoint(600, 480));

        auto* pl = new AnchorLayout(p);
        p->setLayout(pl);

        auto* title = new Label("New Messages", p);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        title->anchors()->top = {p, Edge::Top, 24};
        title->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(title);

        auto* body = new Label("You have 3 unread messages\n"
                               "from your team members.",
                               p);
        body->anchors()->top = {title, Edge::Bottom, 8};
        body->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(body);

        auto* timestamp = new Label("2 minutes ago", p);
        timestamp->setFluentTypography(Typography::FontRole::Caption);
        timestamp->anchors()->top = {body, Edge::Bottom, 8};
        timestamp->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(timestamp);

        auto* dismissBtn = new Button("Dismiss", p);
        dismissBtn->setFixedSize(80, 32);
        dismissBtn->anchors()->top = {timestamp, Edge::Bottom, 12};
        dismissBtn->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(dismissBtn);

        QObject::connect(dismissBtn, &Button::clicked, p, &Popup::close);
        QObject::connect(btn4, &Button::clicked, p, &Popup::open);
    }

    // ── 5. Sticky Popup — NoAutoClose ────────────────────────────
    auto* btn5 = new Button("Sticky Popup", visual);
    btn5->setFixedSize(160, 36);
    btn5->anchors()->top = {visual, Edge::Top, 80};
    btn5->anchors()->right = {visual, Edge::Right, -60};
    layout->addWidget(btn5);

    {
        auto* p = new Popup(visual);
        p->setPosition(visual, QPoint(620, 130));
        p->setClosePolicy(Popup::NoAutoClose);

        auto* pl = new AnchorLayout(p);
        p->setLayout(pl);

        auto* title = new Label("Sticky Note", p);
        title->setFluentTypography(Typography::FontRole::Subtitle);
        title->anchors()->top = {p, Edge::Top, 24};
        title->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(title);

        auto* body = new Label("This popup won't close on outside\n"
                               "click or Escape key press.\n"
                               "You must click Close explicitly.",
                               p);
        body->anchors()->top = {title, Edge::Bottom, 8};
        body->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(body);

        auto* closeBtn = new Button("Close", p);
        closeBtn->setFixedSize(80, 32);
        closeBtn->anchors()->top = {body, Edge::Bottom, 16};
        closeBtn->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(closeBtn);

        QObject::connect(closeBtn, &Button::clicked, p, &Popup::close);
        QObject::connect(btn5, &Button::clicked, p, &Popup::open);
    }

    // ── 6. Relative Position — popup anchored below its trigger button ──────
    // 演示 setPosition(QWidget* relativeTo, localPos) 用法：
    // 每次点击时动态计算相对坐标，popup 始终出现在按钮正下方
    auto* btn6 = new Button("Relative Pos", visual);
    btn6->setFixedSize(160, 36);
    btn6->anchors()->horizontalCenter = {visual, Edge::HCenter, 0};
    btn6->anchors()->bottom = {visual, Edge::Bottom, -60};
    layout->addWidget(btn6);

    {
        auto* p = new Popup(visual);
        p->resize(260, 160);

        auto* pl = new AnchorLayout(p);
        p->setLayout(pl);

        auto* title = new Label("Relative Position", p);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        title->anchors()->top = {p, Edge::Top, 24};
        title->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(title);

        auto* body = new Label("This popup is positioned\n"
                               "relative to its trigger button\n"
                               "via setPosition(widget, localPos).",
                               p);
        body->anchors()->top = {title, Edge::Bottom, 8};
        body->anchors()->left = {p, Edge::Left, 28};
        pl->addWidget(body);

        // Popup 保持 QML 风格的 x/y 语义；先激活 layout 拿到最终可见高度，再算相对坐标。
        QObject::connect(btn6, &Button::clicked, p, [p, btn6]() {
            p->ensurePolished();
            if (auto* popupLayout = p->layout()) {
                popupLayout->activate();
                const QSize hint = popupLayout->totalSizeHint();
                if (hint.isValid() && !hint.isEmpty())
                    p->resize(hint);
            }

            p->setPosition(btn6, QPoint(0, -p->contentsRect().height()));
            p->open();
        });
    }

    visual->show();
    qApp->exec();
    delete visual;
}
