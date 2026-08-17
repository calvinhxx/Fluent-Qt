#include "QtTestEnvironment.h"

#include "components/basicinput/Button.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/textfields/Label.h"

#include <QPaintEvent>
#include <QPainter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QWidget>

#include <memory>

#include <gtest/gtest.h>

using fluent::basicinput::Button;
using fluent::collections::TreeView;
using fluent::textfields::Label;

namespace {

class GateSurface : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), themeColors().bgCanvas);
    }
};

void skipUnlessVisualGate()
{
    if (!tests::support::shouldRunVisualGate()) {
        GTEST_SKIP() << "Opt-in visual gate. Run with VISUAL_SNAPSHOT=1 VISUAL_COMPARE=1 "
                        "(or VISUAL_UPDATE_BASELINE=1). Automated CTest keeps SKIP_VISUAL_TEST=1.";
    }
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Visual gate baselines are desktop-captured; "
                        "offscreen/minimal is not an approval host.";
    }
    if (!tests::support::isVisualGateApprovalHost()) {
        GTEST_SKIP() << "Visual gate baselines require the documented macOS arm64 "
                        "Fusion approval host with QT_SCALE_FACTOR=1 and QT_FONT_DPI=96.";
    }
}

GateSurface* makeButtonStateWindow()
{
    auto* window = new GateSurface;
    window->onThemeUpdated();

    using Edge = fluent::AnchorLayout::Edge;
    auto* layout = new fluent::AnchorLayout(window);
    window->setLayout(layout);

    auto* rest = new Button(QStringLiteral("Rest"), window);
    rest->setInteractionState(Button::Rest);
    rest->anchors()->top = {window, Edge::Top, 24};
    rest->anchors()->left = {window, Edge::Left, 24};
    layout->addWidget(rest);

    auto* hover = new Button(QStringLiteral("Hover"), window);
    hover->setInteractionState(Button::Hover);
    hover->anchors()->verticalCenter = {rest, Edge::VCenter, 0};
    hover->anchors()->left = {rest, Edge::Right, 16};
    layout->addWidget(hover);

    auto* pressed = new Button(QStringLiteral("Pressed"), window);
    pressed->setInteractionState(Button::Pressed);
    pressed->anchors()->verticalCenter = {rest, Edge::VCenter, 0};
    pressed->anchors()->left = {hover, Edge::Right, 16};
    layout->addWidget(pressed);

    auto* focus = new Button(QStringLiteral("Focus"), window);
    focus->setObjectName(QStringLiteral("visualGateFocusButton"));
    focus->setFluentStyle(Button::Accent);
    focus->setFocusVisual(true);
    focus->anchors()->verticalCenter = {rest, Edge::VCenter, 0};
    focus->anchors()->left = {pressed, Edge::Right, 16};
    layout->addWidget(focus);

    auto* disabled = new Button(QStringLiteral("Disabled"), window);
    disabled->setInteractionState(Button::Disabled);
    disabled->anchors()->verticalCenter = {rest, Edge::VCenter, 0};
    disabled->anchors()->left = {focus, Edge::Right, 16};
    layout->addWidget(disabled);

    return window;
}

GateSurface* makeTreeViewRtlWindow()
{
    auto* window = new GateSurface;
    window->setLayoutDirection(Qt::RightToLeft);
    window->onThemeUpdated();

    using Edge = fluent::AnchorLayout::Edge;
    auto* layout = new fluent::AnchorLayout(window);
    window->setLayout(layout);

    auto* caption = new Label(QStringLiteral("TreeView RTL"), window);
    caption->anchors()->top = {window, Edge::Top, 16};
    caption->anchors()->left = {window, Edge::Left, 16};
    caption->anchors()->right = {window, Edge::Right, -16};
    layout->addWidget(caption);

    auto* tree = new TreeView(window);
    tree->setLayoutDirection(Qt::RightToLeft);
    tree->setHeaderHidden(true);
    tree->setBorderVisible(false);
    tree->setBackgroundVisible(true);
    tree->setAttribute(Qt::WA_MacShowFocusRect, false);
    tree->setIndicatorMotionAnimationEnabled(false);
    tree->setAnimated(false);

    auto* model = new QStandardItemModel(tree);
    auto* documents = new QStandardItem(QStringLiteral("Documents"));
    documents->appendRow(new QStandardItem(QStringLiteral("Report")));
    documents->appendRow(new QStandardItem(QStringLiteral("Notes")));
    model->appendRow(documents);
    model->appendRow(new QStandardItem(QStringLiteral("Pictures")));
    tree->setModel(model);
    tree->expandAll();

    tree->anchors()->top = {caption, Edge::Bottom, 12};
    tree->anchors()->left = {window, Edge::Left, 16};
    tree->anchors()->right = {window, Edge::Right, -16};
    tree->anchors()->bottom = {window, Edge::Bottom, -16};
    layout->addWidget(tree);

    return window;
}

constexpr QSize kButtonGateSize(720, 120);
constexpr QSize kTreeGateSize(360, 280);

} // namespace

TEST(VisualGateTest, ButtonStatesLightLtr)
{
    skipUnlessVisualGate();

    std::unique_ptr<GateSurface> window(makeButtonStateWindow());
    tests::support::VisualSnapshotOptions options;
    options.windowSize = kButtonGateSize;
    options.variant = QStringLiteral("button-states-light-ltr");
    options.focusObjectName = QStringLiteral("visualGateFocusButton");
    options.theme = tests::support::VisualSnapshotTheme::Light;
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window.get(), options));
}

TEST(VisualGateTest, ButtonStatesDarkLtr)
{
    skipUnlessVisualGate();

    std::unique_ptr<GateSurface> window(makeButtonStateWindow());
    tests::support::VisualSnapshotOptions options;
    options.windowSize = kButtonGateSize;
    options.variant = QStringLiteral("button-states-dark-ltr");
    options.focusObjectName = QStringLiteral("visualGateFocusButton");
    options.theme = tests::support::VisualSnapshotTheme::Dark;
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window.get(), options));
}

TEST(VisualGateTest, TreeViewRtl)
{
    skipUnlessVisualGate();

    std::unique_ptr<GateSurface> window(makeTreeViewRtlWindow());
    tests::support::VisualSnapshotOptions options;
    options.windowSize = kTreeGateSize;
    options.variant = QStringLiteral("tree-view-rtl");
    options.theme = tests::support::VisualSnapshotTheme::Light;
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window.get(), options));
}
