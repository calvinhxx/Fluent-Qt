#include <gtest/gtest.h>
#include <QApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

#include <cstdlib>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/CoachMark.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using namespace fluent::dialogs_flyouts;
using fluent::basicinput::Button;
using fluent::textfields::Label;

// Shadow margin baked into the outer window size by CoachMark (see OverlayGeometry).
static constexpr int kShadowMargin = ::fluent::overlay::defaultShadowMargin();  // 16

// ── FluentTestWindow ─────────────────────────────────────────────────────────
class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── Fixture ──────────────────────────────────────────────────────────────────
class CoachMarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Centre the host on screen so a coach mark fits on every side without clamping.
        // zh_CN: 把宿主窗口放在屏幕中央，使 coach mark 在任意方向都放得下、不被裁剪。
        const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        window = new FluentTestWindow();
        window->setFixedSize(640, 480);
        window->move(avail.center() - QPoint(320, 240));
        window->setWindowTitle("CoachMark Test");
        window->onThemeUpdated();
        window->show();
        QTest::qWaitForWindowExposed(window);
    }

    void TearDown() override {
        delete window;
        window = nullptr;
    }

    Button* makeTarget(const QPoint& pos, const QSize& size = QSize(120, 32)) {
        auto* btn = new Button("Target", window);
        btn->setFixedSize(size);
        btn->move(pos);
        btn->show();
        return btn;
    }

    QRect targetGlobalRect(QWidget* target) const {
        return QRect(target->mapToGlobal(QPoint(0, 0)), target->size());
    }

    // Visible card rect of the coach mark in global coordinates (window minus shadow margin).
    QRect cardGlobalRect(CoachMark* coach) const {
        return ::fluent::overlay::visibleCardGeometry(coach->geometry());
    }

    FluentTestWindow* window = nullptr;
};

// ── 1. Default property values match the documented contract ─────────────────
TEST_F(CoachMarkTest, DefaultProperties) {
    CoachMark coach(window);

    EXPECT_FALSE(coach.isOpen());
    EXPECT_EQ(coach.target(), nullptr);
    EXPECT_EQ(coach.placement(), CoachMark::Auto);
    EXPECT_EQ(coach.cardSize(), QSize(330, 168));
    EXPECT_NE(coach.contentHost(), nullptr);

    // Outer window already sized for the default card + shadow margin on each side.
    EXPECT_EQ(coach.size(), QSize(330 + 2 * kShadowMargin, 168 + 2 * kShadowMargin));
}

// ── 2. cardSize resizes the outer window and is a no-op when unchanged ────────
TEST_F(CoachMarkTest, CardSizeResizesOuterWindowAndIsIdempotent) {
    CoachMark coach(window);

    coach.setCardSize(QSize(300, 160));
    EXPECT_EQ(coach.cardSize(), QSize(300, 160));
    EXPECT_EQ(coach.size(), QSize(300 + 2 * kShadowMargin, 160 + 2 * kShadowMargin));

    // Re-setting the same value must keep state stable (convention: no-op setter).
    coach.setCardSize(QSize(300, 160));
    EXPECT_EQ(coach.cardSize(), QSize(300, 160));
    EXPECT_EQ(coach.size(), QSize(300 + 2 * kShadowMargin, 160 + 2 * kShadowMargin));

    // contentHost fills the visible card area.
    EXPECT_EQ(coach.contentHost()->size(), QSize(300, 160));
}

// ── 3. open()/close() flip isOpen and emit the state signals once ────────────
TEST_F(CoachMarkTest, OpenCloseToggleStateAndEmitOnce) {
    auto* target = makeTarget(QPoint(260, 220));

    CoachMark coach(window);
    coach.setTarget(target);

    QSignalSpy openChangedSpy(&coach, &CoachMark::openChanged);
    QSignalSpy openedSpy(&coach, &CoachMark::opened);
    QSignalSpy closedSpy(&coach, &CoachMark::closed);

    coach.open();
    EXPECT_TRUE(coach.isOpen());
    ASSERT_EQ(openedSpy.count(), 1);
    ASSERT_EQ(openChangedSpy.count(), 1);
    EXPECT_TRUE(openChangedSpy.last().at(0).toBool());

    // Opening again while open is a no-op — no duplicate signals.
    coach.open();
    EXPECT_EQ(openedSpy.count(), 1);
    EXPECT_EQ(openChangedSpy.count(), 1);

    coach.close();
    EXPECT_FALSE(coach.isOpen());
    ASSERT_EQ(closedSpy.count(), 1);
    ASSERT_EQ(openChangedSpy.count(), 2);
    EXPECT_FALSE(openChangedSpy.last().at(0).toBool());

    // Closing again while closed is a no-op.
    coach.close();
    EXPECT_EQ(closedSpy.count(), 1);
    EXPECT_EQ(openChangedSpy.count(), 2);
}

// ── 4. setOpen() delegates to open()/close() ─────────────────────────────────
TEST_F(CoachMarkTest, SetOpenDelegates) {
    auto* target = makeTarget(QPoint(260, 220));

    CoachMark coach(window);
    coach.setTarget(target);

    coach.setOpen(true);
    EXPECT_TRUE(coach.isOpen());
    coach.setOpen(false);
    EXPECT_FALSE(coach.isOpen());
}

// ── 5. Bottom placement: card sits below the target, horizontally centred ────
TEST_F(CoachMarkTest, BottomPlacementSitsBelowTarget) {
    auto* target = makeTarget(QPoint(260, 180));

    CoachMark coach(window);
    coach.setCardSize(QSize(300, 140));
    coach.setPlacement(CoachMark::Bottom);
    coach.setTarget(target);
    coach.open();
    QTest::qWaitForWindowExposed(&coach);

    const QRect tgt = targetGlobalRect(target);
    const QRect card = cardGlobalRect(&coach);

    EXPECT_GT(card.top(), tgt.bottom());
    EXPECT_NEAR(card.center().x(), tgt.center().x(), 2);
}

// ── 6. Right placement: card sits to the right of the target, vert. centred ──
TEST_F(CoachMarkTest, RightPlacementSitsRightOfTarget) {
    auto* target = makeTarget(QPoint(180, 200));

    CoachMark coach(window);
    coach.setCardSize(QSize(280, 130));
    coach.setPlacement(CoachMark::Right);
    coach.setTarget(target);
    coach.open();
    QTest::qWaitForWindowExposed(&coach);

    const QRect tgt = targetGlobalRect(target);
    const QRect card = cardGlobalRect(&coach);

    EXPECT_GT(card.left(), tgt.right());
    EXPECT_NEAR(card.center().y(), tgt.center().y(), 2);
}

// ── 7. No target → centred over the owner window ─────────────────────────────
TEST_F(CoachMarkTest, NoTargetCentersOverOwner) {
    CoachMark coach(window);
    coach.setCardSize(QSize(300, 140));
    coach.open();
    QTest::qWaitForWindowExposed(&coach);

    EXPECT_NEAR(coach.geometry().center().x(), window->frameGeometry().center().x(), 2);
    EXPECT_NEAR(coach.geometry().center().y(), window->frameGeometry().center().y(), 2);
}

// ── 8. Retargeting while open keeps it open and glides to the new target ─────
TEST_F(CoachMarkTest, RetargetWhileOpenGlidesToNewTarget) {
    auto* first = makeTarget(QPoint(160, 200));
    auto* second = makeTarget(QPoint(420, 200));

    CoachMark coach(window);
    coach.setCardSize(QSize(240, 120));
    coach.setPlacement(CoachMark::Bottom);
    coach.setTarget(first);
    coach.open();
    QTest::qWaitForWindowExposed(&coach);

    coach.setTarget(second);
    EXPECT_TRUE(coach.isOpen());
    EXPECT_EQ(coach.target(), second);

    // The move is animated; wait for the card to settle under the new target.
    const QRect secondGlobal = targetGlobalRect(second);
    const bool glided = QTest::qWaitFor(
        [&]() {
            const QRect card = cardGlobalRect(&coach);
            return std::abs(card.center().x() - secondGlobal.center().x()) <= 2;
        },
        1500);
    EXPECT_TRUE(glided);
}

// ── 9. Destroyed target is handled safely (QPointer auto-clears) ─────────────
TEST_F(CoachMarkTest, TargetDestroyedClearsPointer) {
    auto* target = makeTarget(QPoint(260, 220));

    CoachMark coach(window);
    coach.setTarget(target);
    EXPECT_EQ(coach.target(), target);

    delete target;
    EXPECT_EQ(coach.target(), nullptr);

    // Opening with a cleared target falls back to centring — must not crash.
    coach.open();
    EXPECT_TRUE(coach.isOpen());
    coach.close();
}

// ── 10. The Placement enum is registered for the meta-object / QML ───────────
TEST_F(CoachMarkTest, PlacementEnumIsRegistered) {
    const int index = CoachMark::staticMetaObject.indexOfEnumerator("Placement");
    ASSERT_GE(index, 0);
    const QMetaEnum meta = CoachMark::staticMetaObject.enumerator(index);
    EXPECT_EQ(meta.keyToValue("Bottom"), static_cast<int>(CoachMark::Bottom));
    EXPECT_EQ(meta.keyToValue("Right"), static_cast<int>(CoachMark::Right));
}

// ── VisualCheck — interactive demo of the placements + glide ─────────────────
TEST_F(CoachMarkTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->hide();

    auto* visual = new FluentTestWindow();
    visual->setFixedSize(720, 520);
    visual->setWindowTitle("CoachMark VisualCheck — click a target to point the coach mark at it");
    visual->onThemeUpdated();

    auto* coach = new CoachMark(visual);
    coach->setCardSize(QSize(320, 150));
    {
        auto* host = coach->contentHost();
        auto* layout = new QVBoxLayout(host);
        layout->setContentsMargins(18, 14, 14, 14);
        layout->setSpacing(8);
        auto* title = new Label("Bottom placement", host);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        layout->addWidget(title);
        auto* body = new Label("The tail points back at the control you clicked. "
                               "Pick another target to watch it glide.", host);
        body->setFluentTypography(Typography::FontRole::Body);
        body->setWordWrap(true);
        layout->addWidget(body);
        layout->addStretch(1);
        auto* gotIt = new Button("Got it", host);
        gotIt->setFluentStyle(Button::Accent);
        QObject::connect(gotIt, &Button::clicked, coach, [coach]() { coach->close(); });
        layout->addWidget(gotIt, 0, Qt::AlignRight);

        auto addTarget = [&](const QString& label, CoachMark::Placement placement,
                             int x, int y) {
            auto* btn = new Button(label, visual);
            btn->setFixedSize(140, 32);
            btn->move(x, y);
            btn->show();
            QObject::connect(btn, &Button::clicked, coach, [coach, btn, title, placement, label]() {
                title->setText(label + QStringLiteral(" placement"));
                coach->setPlacement(placement);
                coach->setTarget(btn);
                coach->open();
            });
        };

        addTarget("Bottom", CoachMark::Bottom, 290, 120);
        addTarget("Top", CoachMark::Top, 290, 380);
        addTarget("Right", CoachMark::Right, 120, 250);
        addTarget("Left", CoachMark::Left, 460, 250);
    }

    visual->show();
    qApp->exec();
    delete visual;
}
