
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>
#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "QtTestEnvironment.h"

using namespace fluent::dialogs_flyouts;
using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::textfields::Label;

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class TeachingTipTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        fluentRegisterMetaTypeNames<fluent::dialogs_flyouts::TeachingTip::CloseReason>(
            "fluent::dialogs_flyouts::TeachingTip::CloseReason",
            "CloseReason");
    }

    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(900, 680);
        window->setWindowTitle("TeachingTip Test");
        window->onThemeUpdated();
        window->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
    }

    void TearDown() override {
        delete window;
        window = nullptr;
    }

    Button* makeAnchor(const QPoint& pos, const QSize& size = QSize(120, 32)) {
        auto* btn = new Button("Anchor", window);
        btn->setFixedSize(size);
        btn->move(pos);
        btn->show();
        return btn;
    }

    // contentHost rect mapped to window coordinates
    QRect contentInWindow(TeachingTip* tip) const {
        auto* host = tip->contentHost();
        return QRect(host->mapTo(window, QPoint(0, 0)), host->size());
    }

    TeachingTip::CloseReason lastCloseReason(const QSignalSpy& spy) const {
        return static_cast<TeachingTip::CloseReason>(spy.last().at(0).toInt());
    }

    FluentTestWindow* window = nullptr;
};

TEST_F(TeachingTipTest, DefaultProperties) {
    TeachingTip tip(window);

    EXPECT_FALSE(tip.isOpen());
    EXPECT_EQ(tip.target(), nullptr);
    EXPECT_EQ(tip.preferredPlacement(), TeachingTip::Auto);
    EXPECT_EQ(tip.placementMargin(), 4);
    EXPECT_FALSE(tip.isLightDismissEnabled());
    EXPECT_TRUE(tip.isTailVisible());
    EXPECT_NE(tip.contentHost(), nullptr);
    EXPECT_EQ(tip.cardSize(), QSize(360, 200));
}

TEST_F(TeachingTipTest, ShowAtAnchorsContentBelowTarget) {
    auto* anchor = makeAnchor(QPoint(360, 220));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setPreferredPlacement(TeachingTip::Bottom);
    tip.showAt(anchor);

    ASSERT_TRUE(tip.isOpen());
    const QRect anchorRect(anchor->mapTo(window, QPoint(0, 0)), anchor->size());
    const QRect content = contentInWindow(&tip);

    // content.top = target.bottom + placementMargin (tail-tip gap) + kTailSize (tail body)
    EXPECT_EQ(content.top(), anchorRect.bottom() + tip.placementMargin() + 12 /*kTailSize*/);
    EXPECT_NEAR(content.center().x(), anchorRect.center().x(), 1);
}

TEST_F(TeachingTipTest, AutoPlacementFallsBackToTopNearBottomEdge) {
    auto* anchor = makeAnchor(QPoint(360, 610));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setPreferredPlacement(TeachingTip::Auto);
    tip.showAt(anchor);

    ASSERT_TRUE(tip.isOpen());
    const QRect anchorRect(anchor->mapTo(window, QPoint(0, 0)), anchor->size());
    const QRect content = contentInWindow(&tip);

    // Auto should fall back to Top: content is above the anchor
    EXPECT_LT(content.bottom(), anchorRect.top());
}

TEST_F(TeachingTipTest, RightTopPlacementAlignsToTargetTop) {
    auto* anchor = makeAnchor(QPoint(100, 180));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setPreferredPlacement(TeachingTip::RightTop);
    tip.showAt(anchor);

    ASSERT_TRUE(tip.isOpen());
    const QRect anchorRect(anchor->mapTo(window, QPoint(0, 0)), anchor->size());
    const QRect content = contentInWindow(&tip);

    EXPECT_EQ(content.left(), anchorRect.right() + tip.placementMargin() + 12 /*kTailSize*/);
    EXPECT_EQ(content.top(), anchorRect.top());
}

TEST_F(TeachingTipTest, TracksMovingTargetAncestorAndClosesWhenClipped) {
    auto* scrollingContent = new QWidget(window);
    scrollingContent->setGeometry(0, 0, window->width(), 1000);
    scrollingContent->show();

    auto* anchor = new Button("Anchor", scrollingContent);
    anchor->setGeometry(360, 280, 120, 32);
    anchor->show();

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setPreferredPlacement(TeachingTip::Bottom);
    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());
    const QPoint initialPosition = tip.pos();

    scrollingContent->move(0, -64);
    QTRY_COMPARE_WITH_TIMEOUT(tip.pos(), initialPosition - QPoint(0, 64), 1000);

    scrollingContent->move(0, -500);
    QTRY_VERIFY_WITH_TIMEOUT(!tip.isOpen(), 1000);
}

TEST_F(TeachingTipTest, ShowAtInheritsThemeOverrideFromTarget) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    host->setGeometry(80, 120, 260, 180);
    host->show();

    auto* anchor = new Button("Anchor", host);
    anchor->setGeometry(24, 24, 120, 32);
    anchor->show();

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setPreferredPlacement(TeachingTip::Bottom);
    tip.showAt(anchor);

    EXPECT_TRUE(tip.isOpen());
    EXPECT_EQ(tip.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(tip.themeColors().bgLayer, QColor("#2C2C2C"));
}

TEST_F(TeachingTipTest, TargetDestroyedClosesWithSemanticReason) {
    auto* anchor = makeAnchor(QPoint(320, 240));

    auto* tip = new TeachingTip(window);
    tip->setAnimationEnabled(false);

    QSignalSpy closingSpy(tip, &TeachingTip::closing);
    tip->showAt(anchor);
    ASSERT_TRUE(tip->isOpen());

    delete anchor;
    QApplication::processEvents();

    EXPECT_FALSE(tip->isOpen());
    ASSERT_EQ(closingSpy.count(), 1);
    EXPECT_EQ(lastCloseReason(closingSpy), TeachingTip::TargetDestroyed);
    delete tip;
}

TEST_F(TeachingTipTest, LightDismissClosesWithLightDismissReason) {
    auto* anchor = makeAnchor(QPoint(360, 260));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setLightDismissEnabled(true);

    QSignalSpy closingSpy(&tip, &TeachingTip::closing);
    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    QTest::qWait(30);

    EXPECT_FALSE(tip.isOpen());
    ASSERT_EQ(closingSpy.count(), 1);
    EXPECT_EQ(lastCloseReason(closingSpy), TeachingTip::LightDismiss);
}

TEST_F(TeachingTipTest, EscapeFromOwnerClosesWithLightDismissReason) {
    auto* anchor = makeAnchor(QPoint(360, 260));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setLightDismissEnabled(true);

    QSignalSpy closingSpy(&tip, &TeachingTip::closing);
    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());

    QTest::keyClick(window, Qt::Key_Escape);
    QApplication::processEvents();

    EXPECT_FALSE(tip.isOpen());
    ASSERT_EQ(closingSpy.count(), 1);
    EXPECT_EQ(lastCloseReason(closingSpy), TeachingTip::LightDismiss);
}

TEST_F(TeachingTipTest, DisabledLightDismissKeepsTeachingTipOpen) {
    auto* anchor = makeAnchor(QPoint(360, 260));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setLightDismissEnabled(false);
    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(8, 8));
    QTest::keyClick(&tip, Qt::Key_Escape);
    QApplication::processEvents();

    EXPECT_TRUE(tip.isOpen());
}

TEST_F(TeachingTipTest, ContentHostMatchesCardSizeAndPlacement) {
    auto* anchor = makeAnchor(QPoint(320, 280));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setCardSize({300, 160});
    tip.setPreferredPlacement(TeachingTip::Bottom);
    tip.showAt(anchor);

    ASSERT_TRUE(tip.isOpen());
    EXPECT_EQ(tip.contentHost()->size(), QSize(300, 160));
}

TEST_F(TeachingTipTest, ShadowDiffusesAroundAllCardEdges) {
    auto* anchor = makeAnchor(QPoint(320, 280));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setTailVisible(false);
    tip.setCardSize({240, 120});
    tip.setPreferredPlacement(TeachingTip::Bottom);
    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());

    QImage image(tip.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    tip.render(&image);

    const QRect card = tip.contentHost()->geometry();
    ASSERT_GE(card.left(), 6);
    ASSERT_GE(card.top(), 6);
    ASSERT_LT(card.right() + 4, image.width());
    ASSERT_LT(card.bottom() + 4, image.height());
    EXPECT_GT(image.pixelColor(card.left() - 4, card.center().y()).alpha(), 0);
    EXPECT_GT(image.pixelColor(card.right() + 4, card.center().y()).alpha(), 0);
    EXPECT_GT(image.pixelColor(card.center().x(), card.top() - 4).alpha(), 0);
    EXPECT_GT(image.pixelColor(card.center().x(), card.bottom() + 4).alpha(), 0);
}

TEST_F(TeachingTipTest, UserChildrenStayInsideContentHost) {
    auto* anchor = makeAnchor(QPoint(320, 200));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setPreferredPlacement(TeachingTip::Bottom);

    auto* host = tip.contentHost();
    auto* label = new QLabel("Hello TeachingTip", host);
    label->setGeometry(16, 16, 200, 24);

    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());

    const QRect labelInHost(label->mapTo(host, QPoint(0, 0)), label->size());
    EXPECT_TRUE(host->rect().contains(labelInHost.topLeft()));
    EXPECT_TRUE(host->rect().contains(labelInHost.bottomRight()));
}

TEST_F(TeachingTipTest, CloseWithReasonEmitsClosingSignal) {
    auto* anchor = makeAnchor(QPoint(360, 300));

    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    QSignalSpy closingSpy(&tip, &TeachingTip::closing);
    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());

    tip.closeWithReason(TeachingTip::ActionButton);
    QApplication::processEvents();

    ASSERT_EQ(closingSpy.count(), 1);
    EXPECT_EQ(lastCloseReason(closingSpy), TeachingTip::ActionButton);
}

TEST_F(TeachingTipTest, Contract_PopupClosingAliasAndNotifyNoOps) {
    auto* anchor = makeAnchor(QPoint(360, 300));
    TeachingTip tip(window);
    tip.setAnimationEnabled(false);

    fluentRegisterMetaTypeNames<fluent::dialogs_flyouts::Popup::CloseReason>(
        "fluent::dialogs_flyouts::Popup::CloseReason",
        "CloseReason");

    QSignalSpy popupClosing(&tip, &Popup::closing);
    QSignalSpy tipClosing(&tip, &TeachingTip::closing);
    QSignalSpy targetSpy(&tip, &TeachingTip::targetChanged);
    QSignalSpy lightSpy(&tip, &TeachingTip::lightDismissEnabledChanged);

    tip.setTarget(anchor);
    tip.setTarget(anchor);
    tip.setLightDismissEnabled(false);
    tip.setLightDismissEnabled(true);
    tip.setLightDismissEnabled(true);
    EXPECT_EQ(targetSpy.count(), 1);
    EXPECT_EQ(lightSpy.count(), 1);

    QStringList order;
    QObject::connect(&tip, &Popup::opening, [&] { order << QStringLiteral("opening"); });
    QObject::connect(&tip, &Popup::aboutToShow, [&] { order << QStringLiteral("aboutToShow"); });
    QObject::connect(&tip, &Popup::opened, [&] { order << QStringLiteral("opened"); });
    QObject::connect(&tip, &Popup::aboutToHide, [&] { order << QStringLiteral("aboutToHide"); });
    QObject::connect(&tip, &Popup::closed, [&] { order << QStringLiteral("closed"); });

    tip.showAt(anchor);
    ASSERT_TRUE(tip.isOpen());
    tip.closeWithReason(TeachingTip::CloseButton);
    QApplication::processEvents();

    EXPECT_FALSE(tip.isOpen());
    ASSERT_EQ(popupClosing.count(), 1);
    EXPECT_EQ(popupClosing.at(0).at(0).toInt(), static_cast<int>(Popup::CloseButton));
    ASSERT_EQ(tipClosing.count(), 1);
    EXPECT_EQ(lastCloseReason(tipClosing), TeachingTip::CloseButton);
    EXPECT_EQ(order, (QStringList{
                          QStringLiteral("opening"),
                          QStringLiteral("aboutToShow"),
                          QStringLiteral("opened"),
                          QStringLiteral("aboutToHide"),
                          QStringLiteral("closed"),
                      }));
}


TEST_F(TeachingTipTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->hide();

    using Edge = AnchorLayout::Edge;

    auto* visual = new FluentTestWindow();
    visual->setFixedSize(960, 720);
    visual->setWindowTitle("TeachingTip VisualCheck — 点击按钮触发各款式");
    visual->onThemeUpdated();

    auto* layout = new AnchorLayout(visual);
    visual->setLayout(layout);

    // ── anchor buttons ──────────────────────────────────────────────────
    auto* simpleAnchor = new Button("Simple", visual);
    simpleAnchor->setFixedSize(120, 32);
    simpleAnchor->anchors()->top  = {visual, Edge::Top,  220};
    simpleAnchor->anchors()->left = {visual, Edge::Left, 120};
    layout->addWidget(simpleAnchor);

    auto* richAnchor = new Button("Rich", visual);
    richAnchor->setFixedSize(120, 32);
    richAnchor->anchors()->top  = {simpleAnchor, Edge::Top,  0};
    richAnchor->anchors()->left = {simpleAnchor, Edge::Right, 200};
    layout->addWidget(richAnchor);

    auto* topAnchor = new Button("Top", visual);
    topAnchor->setFixedSize(120, 32);
    topAnchor->anchors()->top  = {richAnchor, Edge::Top,  0};
    topAnchor->anchors()->left = {richAnchor, Edge::Right, 200};
    layout->addWidget(topAnchor);

    auto* edgeAnchor = new Button("RightTop", visual);
    edgeAnchor->setFixedSize(120, 32);
    edgeAnchor->anchors()->left   = {visual, Edge::Left,   120};
    edgeAnchor->anchors()->bottom = {visual, Edge::Bottom, -148};
    layout->addWidget(edgeAnchor);

    // ── Simple tip: plain text content via QLabel ────────────────────────
    auto* simpleTip = new TeachingTip(visual);
    simpleTip->setAnimationEnabled(false);
    simpleTip->setLightDismissEnabled(true);
    {
        auto* host = simpleTip->contentHost();
        auto* hostLayout = new AnchorLayout(host);
        host->setLayout(hostLayout);

        auto* title = new Label("Simple tip", host);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        title->anchors()->top  = {host, Edge::Top,  16};
        title->anchors()->left = {host, Edge::Left, 16};
        hostLayout->addWidget(title);

        auto* body = new Label("Content assembled in test code — no fixed schema.", host);
        body->setFluentTypography(Typography::FontRole::Body);
        body->setWordWrap(true);
        body->anchors()->top   = {title, Edge::Bottom, 4};
        body->anchors()->left  = {host,  Edge::Left,   16};
        body->anchors()->right = {host,  Edge::Right, -16};
        hostLayout->addWidget(body);

        auto* dismiss = new Button(host);
        dismiss->setFluentStyle(Button::Subtle);
        dismiss->setFluentLayout(Button::IconOnly);
        dismiss->setFixedSize(40, 40);
        dismiss->setIconGlyph(Typography::Icons::Dismiss);
        dismiss->anchors()->top   = {host, Edge::Top,   4};
        dismiss->anchors()->right = {host, Edge::Right, -4};
        hostLayout->addWidget(dismiss);

        QObject::connect(dismiss, &Button::clicked, simpleTip, [simpleTip]() {
            simpleTip->closeWithReason(TeachingTip::CloseButton);
        });
    }
    QObject::connect(simpleAnchor, &Button::clicked, simpleTip, [simpleTip, simpleAnchor]() {
        simpleTip->showAt(simpleAnchor);
    });

    // ── Rich tip: title + body + action/close buttons ───────────────────
    auto* richTip = new TeachingTip(visual);
    richTip->setAnimationEnabled(false);
    richTip->setCardSize({360, 180});
    {
        auto* host = richTip->contentHost();
        auto* hostLayout = new AnchorLayout(host);
        host->setLayout(hostLayout);

        auto* title = new Label("Actionable tip", host);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        title->anchors()->top   = {host, Edge::Top,   16};
        title->anchors()->left  = {host, Edge::Left,  16};
        title->anchors()->right = {host, Edge::Right, -48};
        hostLayout->addWidget(title);

        auto* body = new Label("Buttons assembled by the caller via AnchorLayout.", host);
        body->setFluentTypography(Typography::FontRole::Body);
        body->setWordWrap(true);
        body->anchors()->top   = {title, Edge::Bottom, 4};
        body->anchors()->left  = {host,  Edge::Left,   16};
        body->anchors()->right = {host,  Edge::Right, -16};
        hostLayout->addWidget(body);

        auto* dismiss = new Button(host);
        dismiss->setFluentStyle(Button::Subtle);
        dismiss->setFluentLayout(Button::IconOnly);
        dismiss->setFixedSize(40, 40);
        dismiss->setIconGlyph(Typography::Icons::Dismiss);
        dismiss->anchors()->top   = {host, Edge::Top,   4};
        dismiss->anchors()->right = {host, Edge::Right, -4};
        hostLayout->addWidget(dismiss);

        auto* actionBtn = new Button("Do it", host);
        actionBtn->setFluentStyle(Button::Accent);
        actionBtn->setMinimumWidth(96);
        actionBtn->anchors()->left   = {host, Edge::Left,   16};
        actionBtn->anchors()->bottom = {host, Edge::Bottom, -16};
        hostLayout->addWidget(actionBtn);

        auto* closeBtn = new Button("Later", host);
        closeBtn->setMinimumWidth(96);
        closeBtn->anchors()->left   = {actionBtn, Edge::Right, 8};
        closeBtn->anchors()->bottom = {host,      Edge::Bottom, -16};
        hostLayout->addWidget(closeBtn);

        QObject::connect(dismiss,   &Button::clicked, richTip, [richTip]() { richTip->closeWithReason(TeachingTip::CloseButton);  });
        QObject::connect(actionBtn, &Button::clicked, richTip, [richTip]() { richTip->closeWithReason(TeachingTip::ActionButton); });
        QObject::connect(closeBtn,  &Button::clicked, richTip, [richTip]() { richTip->closeWithReason(TeachingTip::CloseButton);  });
    }
    QObject::connect(richAnchor, &Button::clicked, richTip, [richTip, richAnchor]() {
        richTip->showAt(richAnchor);
    });

    // ── Top placement tip ────────────────────────────────────────────────
    auto* topTip = new TeachingTip(visual);
    topTip->setAnimationEnabled(false);
    topTip->setPreferredPlacement(TeachingTip::Top);
    topTip->setCardSize({300, 100});
    {
        auto* host = topTip->contentHost();
        auto* hostLayout = new AnchorLayout(host);
        host->setLayout(hostLayout);

        auto* label = new Label("Top placement — tail points down.", host);
        label->setFluentTypography(Typography::FontRole::Body);
        label->setWordWrap(true);
        label->anchors()->top   = {host, Edge::Top,   16};
        label->anchors()->left  = {host, Edge::Left,  16};
        label->anchors()->right = {host, Edge::Right, -16};
        hostLayout->addWidget(label);
    }
    QObject::connect(topAnchor, &Button::clicked, topTip, [topTip, topAnchor]() {
        topTip->showAt(topAnchor);
    });

    // ── RightTop placement tip ───────────────────────────────────────────
    auto* edgeTip = new TeachingTip(visual);
    edgeTip->setAnimationEnabled(false);
    edgeTip->setPreferredPlacement(TeachingTip::RightTop);
    edgeTip->setCardSize({280, 120});
    {
        auto* host = edgeTip->contentHost();
        auto* hostLayout = new AnchorLayout(host);
        host->setLayout(hostLayout);

        auto* label = new Label("RightTop: tail aligns to target's upper edge.", host);
        label->setFluentTypography(Typography::FontRole::Body);
        label->setWordWrap(true);
        label->anchors()->top   = {host, Edge::Top,   16};
        label->anchors()->left  = {host, Edge::Left,  16};
        label->anchors()->right = {host, Edge::Right, -16};
        hostLayout->addWidget(label);
    }
    QObject::connect(edgeAnchor, &Button::clicked, edgeTip, [edgeTip, edgeAnchor]() {
        edgeTip->showAt(edgeAnchor);
    });

    visual->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        richTip->showAt(richAnchor);
        QApplication::processEvents();

        tests::support::VisualSnapshotOptions light;
        light.windowSize = QSize(960, 720);
        light.variant = QStringLiteral("light");
        light.theme = tests::support::VisualSnapshotTheme::Light;
        ASSERT_TRUE(tests::support::captureVisualSnapshot(visual, light));

        tests::support::VisualSnapshotOptions dark = light;
        dark.variant = QStringLiteral("dark");
        dark.theme = tests::support::VisualSnapshotTheme::Dark;
        ASSERT_TRUE(tests::support::captureVisualSnapshot(visual, dark));
        delete visual;
        return;
    }
    qApp->exec();
    delete visual;
}
