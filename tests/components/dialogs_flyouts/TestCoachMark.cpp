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
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

#include <QImage>

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
    EXPECT_EQ(coach.surfaceMode(), CoachMark::TopLevelSurface);
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

TEST_F(CoachMarkTest, SameWindowSurfaceCentersInsideOwnerAndTracksResize) {
    window->setMinimumSize(0, 0);
    window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    CoachMark coach(window, CoachMark::SameWindowSurface);
    coach.setCardSize(QSize(260, 128));
    coach.open();
    QApplication::processEvents();

    EXPECT_TRUE(coach.isOpen());
    EXPECT_EQ(coach.surfaceMode(), CoachMark::SameWindowSurface);
    EXPECT_EQ(coach.parentWidget(), window);
    EXPECT_EQ(coach.windowType(), Qt::Widget);
    EXPECT_NEAR(coach.geometry().center().x(), window->rect().center().x(), 2);
    EXPECT_NEAR(coach.geometry().center().y(), window->rect().center().y(), 2);

    window->resize(760, 560);
    const bool recentered = QTest::qWaitFor(
        [&]() {
            return std::abs(coach.geometry().center().x() - window->rect().center().x()) <= 2
                && std::abs(coach.geometry().center().y() - window->rect().center().y()) <= 2;
        },
        1000);
    EXPECT_TRUE(recentered);
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
TEST_F(CoachMarkTest, TracksMovingTargetAncestorAndClosesWhenClipped) {
    auto* scrollingContent = new QWidget(window);
    scrollingContent->setGeometry(0, 0, window->width(), 900);
    scrollingContent->show();

    auto* target = new Button("Target", scrollingContent);
    target->setGeometry(260, 220, 120, 32);
    target->show();

    CoachMark coach(window);
    coach.setCardSize(QSize(240, 120));
    coach.setPlacement(CoachMark::Bottom);
    coach.setTarget(target);
    coach.open();
    QTest::qWaitForWindowExposed(&coach);
    const QPoint initialPosition = coach.pos();

    scrollingContent->move(0, -64);
    QTRY_COMPARE_WITH_TIMEOUT(coach.pos(), initialPosition - QPoint(0, 64), 1000);

    scrollingContent->move(0, -500);
    QTRY_VERIFY_WITH_TIMEOUT(!coach.isOpen(), 1000);
}

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
TEST_F(CoachMarkTest, OpenInheritsThemeOverrideFromTarget) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    host->setGeometry(120, 120, 260, 180);
    host->show();

    auto* target = new Button("Target", host);
    target->setGeometry(24, 24, 120, 32);
    target->show();

    CoachMark coach(window);
    coach.setTarget(target);
    coach.open();
    QTest::qWaitForWindowExposed(&coach);

    EXPECT_TRUE(coach.isOpen());
    EXPECT_EQ(coach.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(coach.themeColors().bgLayer, QColor("#2C2C2C"));
    coach.close();
}

TEST_F(CoachMarkTest, PlacementEnumIsRegistered) {
    const int index = CoachMark::staticMetaObject.indexOfEnumerator("Placement");
    ASSERT_GE(index, 0);
    const QMetaEnum meta = CoachMark::staticMetaObject.enumerator(index);
    EXPECT_EQ(meta.keyToValue("Bottom"), static_cast<int>(CoachMark::Bottom));
    EXPECT_EQ(meta.keyToValue("Right"), static_cast<int>(CoachMark::Right));

    const int surfaceIndex = CoachMark::staticMetaObject.indexOfEnumerator("SurfaceMode");
    ASSERT_GE(surfaceIndex, 0);
    const QMetaEnum surfaceMeta = CoachMark::staticMetaObject.enumerator(surfaceIndex);
    EXPECT_EQ(surfaceMeta.keyToValue("TopLevelSurface"), static_cast<int>(CoachMark::TopLevelSurface));
    EXPECT_EQ(surfaceMeta.keyToValue("SameWindowSurface"), static_cast<int>(CoachMark::SameWindowSurface));
}

// ══════════════════════════════════════════════════════════════════════════════
//  Design-language × theme sweep — CoachMark outline per design language
// ──────────────────────────────────────────────────────────────────────────────
//
// CoachMark::paintEvent now branches on the design language for the OUTLINE STROKE only (Material 3
// → borderless QPen(Qt::NoPen), elevation via shadow; macOS → 1px strokeStrong hairline; Fluent →
// unchanged strokeDefault). The fill stays bgLayer everywhere and the card/tail/shadow geometry is
// shared. For every language (Fluent / Material 3 / macOS) crossed with every theme (Light / Dark)
// the card must paint a valid, non-empty image with content and must NOT render an opaque near-black
// surface at the card centre (the invalid-QColor trap: a default-constructed QColor is INVALID yet
// QColor::alpha() == 255, so a bare alpha()>0 guard + setBrush(invalidColor) paints SOLID OPAQUE
// BLACK). Design language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: CoachMark::paintEvent 现仅按设计语言分支绘制「外轮廓描边」(Material 3 → 无边框
// QPen(Qt::NoPen),高度靠阴影;macOS → 1px strokeStrong 发丝边;Fluent → 不变的 strokeDefault)。填充各
// 语言均为 bgLayer,且 card/tail/shadow 几何全部共享。三种语言(Fluent/Material 3/macOS)× 两种主题
//(Light/Dark)下,卡片都必须绘制出有效、非空且有内容的图像,且卡片中心不得呈现不透明近黑表面(无效
// QColor 陷阱:默认构造的 QColor 无效却返回 alpha==255,裸 alpha()>0 + setBrush(无效色) 会涂成不透明
// 纯黑)。设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。
class CoachMarkDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }
};

// No fixture window needed: a target-less CoachMark falls back to centring and the card centre is the
// image centre. open()+processEvents realizes the overlay so paintEvent runs; no exec().
// zh_CN: 无需夹具窗口:无 target 的 CoachMark 回退到居中,卡片中心即图像中心。open()+processEvents 即可实现
// 浮层并触发 paintEvent;不用 exec()。
TEST_F(CoachMarkDesignLanguageTest, AllLanguagesAndThemesPaintWithoutOpaqueBlackSurface) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            CoachMark coach;
            coach.setCardSize(QSize(300, 160));
            coach.move(-2000, -2000);  // offscreen so it never steals focus / flashes. zh_CN: 离屏,避免抢焦点/闪烁。
            coach.open();
            QApplication::processEvents();
            const QImage img = coach.grab().toImage();
            coach.close();
            QApplication::processEvents();

            // 1. The card paints a valid, non-empty image with content. zh_CN: 卡片绘制出有效、非空且有内容的图像。
            ASSERT_FALSE(img.isNull()) << ctx;
            EXPECT_GT(img.width(), 0) << ctx;
            EXPECT_GT(img.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(img)) << "CoachMark painted nothing: " << ctx;

            // 2. The card centre (bgLayer surface) must not be opaque near-black. zh_CN: 卡片中心(bgLayer 表面)
            // 不得为不透明近黑。
            const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "CoachMark painted an opaque black surface at the card centre: " << ctx
                << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
        }
    }
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
