#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTest>
#include <QVBoxLayout>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"
#include "view/collections/SplitView.h"

using view::collections::SplitView;
using view::collections::SplitViewPaneOptions;

namespace {

class SplitViewTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        const auto colors = themeColors();
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, colors.bgCanvas);
        setPalette(palette);
        setAutoFillBackground(true);
    }
};

class ColorPane : public QWidget {
public:
    explicit ColorPane(const QString& title, const QColor& color, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(32, 32);
        setAutoFillBackground(true);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, color);
        setPalette(palette);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 12, 12, 12);
        auto* label = new QLabel(title, this);
        label->setAlignment(Qt::AlignCenter);
        layout->addStretch();
        layout->addWidget(label);
        layout->addStretch();
    }
};

SplitViewPaneOptions paneOptions(int minimumSize, int preferredSize, int maximumSize, bool fill = false)
{
    SplitViewPaneOptions options;
    options.minimumSize = minimumSize;
    options.preferredSize = preferredSize;
    options.maximumSize = maximumSize;
    options.fill = fill;
    return options;
}

QWidget* makePane(const QString& title)
{
    static int hue = 0;
    hue = (hue + 43) % 360;
    return new ColorPane(title, QColor::fromHsv(hue, 55, 235));
}

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

} // namespace

class SplitViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<Qt::Orientation>("Qt::Orientation");
    }

    void SetUp() override
    {
        FluentElement::setTheme(FluentElement::Light);
    }

    void TearDown() override
    {
        FluentElement::setTheme(FluentElement::Light);
    }
};

TEST_F(SplitViewTest, DefaultPropertiesAndInheritance)
{
    SplitView splitView;

    EXPECT_EQ(splitView.paneCount(), 0);
    EXPECT_EQ(splitView.orientation(), Qt::Horizontal);
    EXPECT_FALSE(splitView.resizing());
    EXPECT_GT(splitView.handleWidth(), 0);
    EXPECT_GT(splitView.handleVisualThickness(), 0);
    EXPECT_EQ(splitView.defaultSizeHint(), QSize(560, 320));
    EXPECT_EQ(splitView.defaultMinimumSizeHint(), QSize(160, 96));
    EXPECT_FALSE(splitView.sizeHint().isEmpty());
    EXPECT_FALSE(splitView.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&splitView), nullptr);
    EXPECT_NE(dynamic_cast<FluentElement*>(&splitView), nullptr);
    EXPECT_NE(dynamic_cast<view::QMLPlus*>(&splitView), nullptr);
}

TEST_F(SplitViewTest, DefaultSizeHintsAreConfigurable)
{
    SplitView splitView;
    QSignalSpy sizeHintSpy(&splitView, &SplitView::defaultSizeHintChanged);
    QSignalSpy minimumSizeHintSpy(&splitView, &SplitView::defaultMinimumSizeHintChanged);

    splitView.setDefaultSizeHint(QSize(720, 360));
    EXPECT_EQ(splitView.defaultSizeHint(), QSize(720, 360));
    EXPECT_EQ(splitView.sizeHint(), QSize(720, 360));
    EXPECT_EQ(sizeHintSpy.count(), 1);

    splitView.setDefaultSizeHint(QSize(720, 360));
    EXPECT_EQ(sizeHintSpy.count(), 1);

    splitView.setDefaultMinimumSizeHint(QSize(220, 140));
    EXPECT_EQ(splitView.defaultMinimumSizeHint(), QSize(220, 140));
    EXPECT_EQ(splitView.minimumSizeHint(), QSize(220, 140));
    EXPECT_EQ(minimumSizeHintSpy.count(), 1);

    splitView.setDefaultMinimumSizeHint(QSize(0, -4));
    EXPECT_EQ(splitView.defaultMinimumSizeHint(), QSize(1, 1));
    EXPECT_EQ(minimumSizeHintSpy.count(), 2);
}

TEST_F(SplitViewTest, DefaultSizeHintsProvideCrossAxisFallback)
{
    SplitView splitView;
    QWidget* first = makePane(QStringLiteral("First"));
    QWidget* second = makePane(QStringLiteral("Second"));

    splitView.setDefaultSizeHint(QSize(700, 360));
    splitView.setDefaultMinimumSizeHint(QSize(240, 120));
    splitView.setHandleWidth(8);
    splitView.addPane(first, paneOptions(80, 120, 220));
    splitView.addPane(second, paneOptions(60, 180, 320));

    EXPECT_EQ(splitView.sizeHint(), QSize(308, 360));
    EXPECT_EQ(splitView.minimumSizeHint(), QSize(148, 120));

    splitView.setOrientation(Qt::Vertical);
    EXPECT_EQ(splitView.sizeHint(), QSize(700, 308));
    EXPECT_EQ(splitView.minimumSizeHint(), QSize(240, 148));
}

TEST_F(SplitViewTest, PaneManagementParentsQueriesAndRemoves)
{
    SplitView splitView;
    QWidget* first = makePane(QStringLiteral("First"));
    QWidget* second = makePane(QStringLiteral("Second"));
    QWidget* inserted = makePane(QStringLiteral("Inserted"));
    QSignalSpy countSpy(&splitView, &SplitView::paneCountChanged);

    EXPECT_EQ(splitView.addPane(first), 0);
    EXPECT_EQ(splitView.addPane(second), 1);
    EXPECT_EQ(splitView.insertPane(1, inserted), 1);
    EXPECT_EQ(splitView.paneCount(), 3);
    EXPECT_EQ(splitView.paneAt(0), first);
    EXPECT_EQ(splitView.paneAt(1), inserted);
    EXPECT_EQ(splitView.indexOf(second), 2);
    EXPECT_EQ(first->parentWidget(), &splitView);

    QWidget* removed = splitView.removePaneAt(1);
    EXPECT_EQ(removed, inserted);
    EXPECT_EQ(removed->parentWidget(), nullptr);
    EXPECT_EQ(splitView.paneCount(), 2);
    EXPECT_TRUE(splitView.removePane(second));
    EXPECT_EQ(splitView.indexOf(second), -1);
    EXPECT_GE(countSpy.count(), 4);
    delete removed;
    delete second;
}

TEST_F(SplitViewTest, HorizontalLayoutUsesPreferredAndFillSizes)
{
    SplitView splitView;
    QWidget* left = makePane(QStringLiteral("Left"));
    QWidget* center = makePane(QStringLiteral("Center"));
    QWidget* right = makePane(QStringLiteral("Right"));
    splitView.setHandleWidth(8);
    splitView.resize(600, 200);
    splitView.addPane(left, paneOptions(80, 120, 220));
    splitView.addPane(center, paneOptions(80, 200, 500, true));
    splitView.addPane(right, paneOptions(60, 100, 180));
    showAndProcess(splitView);

    EXPECT_EQ(splitView.handleCount(), 2);
    EXPECT_EQ(splitView.paneGeometry(0), QRect(0, 0, 120, 200));
    EXPECT_EQ(splitView.paneGeometry(1), QRect(128, 0, 364, 200));
    EXPECT_EQ(splitView.paneGeometry(2), QRect(500, 0, 100, 200));
    EXPECT_EQ(splitView.handleGeometry(0), QRect(120, 0, 8, 200));
    EXPECT_EQ(splitView.fillPaneIndex(), 1);
}

TEST_F(SplitViewTest, VerticalLayoutAndConstraintClamping)
{
    SplitView splitView;
    QWidget* top = makePane(QStringLiteral("Top"));
    QWidget* middle = makePane(QStringLiteral("Middle"));
    QWidget* bottom = makePane(QStringLiteral("Bottom"));
    splitView.setOrientation(Qt::Vertical);
    splitView.setHandleWidth(6);
    splitView.resize(300, 400);
    splitView.addPane(top, paneOptions(80, 120, 160));
    splitView.addPane(middle, paneOptions(100, 200, 600, true));
    splitView.addPane(bottom, paneOptions(60, 300, 140));
    showAndProcess(splitView);

    EXPECT_EQ(splitView.handleCount(), 2);
    EXPECT_EQ(splitView.panePreferredSize(bottom), 140);
    EXPECT_EQ(splitView.paneGeometry(0), QRect(0, 0, 300, 120));
    EXPECT_EQ(splitView.paneGeometry(1), QRect(0, 126, 300, 128));
    EXPECT_EQ(splitView.paneGeometry(2), QRect(0, 260, 300, 140));
}

TEST_F(SplitViewTest, DefaultAndMultipleFillPaneResolution)
{
    SplitView splitView;
    QWidget* first = makePane(QStringLiteral("First"));
    QWidget* second = makePane(QStringLiteral("Second"));
    QWidget* third = makePane(QStringLiteral("Third"));
    splitView.resize(420, 160);
    splitView.addPane(first, paneOptions(40, 90, 180));
    splitView.addPane(second, paneOptions(40, 90, 180));
    splitView.addPane(third, paneOptions(40, 90, 400));
    showAndProcess(splitView);

    EXPECT_EQ(splitView.fillPaneIndex(), 2);
    splitView.setPaneFill(second, true);
    splitView.setPaneFill(third, true);
    EXPECT_EQ(splitView.fillPaneIndex(), 1);
    splitView.setFillPaneIndex(0);
    EXPECT_TRUE(splitView.isPaneFill(first));
    EXPECT_FALSE(splitView.isPaneFill(second));
}

TEST_F(SplitViewTest, HiddenPaneDoesNotReserveSpaceOrHandles)
{
    SplitView splitView;
    QWidget* first = makePane(QStringLiteral("First"));
    QWidget* second = makePane(QStringLiteral("Second"));
    QWidget* third = makePane(QStringLiteral("Third"));
    splitView.resize(360, 120);
    splitView.addPane(first, paneOptions(40, 80, 160));
    splitView.addPane(second, paneOptions(40, 80, 160));
    splitView.addPane(third, paneOptions(40, 80, 400, true));
    showAndProcess(splitView);
    ASSERT_EQ(splitView.handleCount(), 2);

    second->hide();
    QApplication::processEvents();
    EXPECT_EQ(splitView.handleCount(), 1);
    EXPECT_TRUE(splitView.paneGeometry(1).isEmpty());
    EXPECT_EQ(splitView.paneGeometry(0).width(), 80);
    EXPECT_EQ(splitView.paneGeometry(2).width(), 272);
}

TEST_F(SplitViewTest, DraggingHandleUpdatesPreferredSizesAndResizingState)
{
    SplitView splitView;
    QWidget* left = makePane(QStringLiteral("Left"));
    QWidget* right = makePane(QStringLiteral("Right"));
    splitView.resize(420, 160);
    splitView.addPane(left, paneOptions(60, 120, 240));
    splitView.addPane(right, paneOptions(60, 200, 300, true));
    showAndProcess(splitView);

    QSignalSpy resizingSpy(&splitView, &SplitView::resizingChanged);
    QSignalSpy sizeSpy(&splitView, &SplitView::paneSizeChanged);
    const QPoint start = splitView.handleGeometry(0).center();
    QTest::mousePress(&splitView, Qt::LeftButton, Qt::NoModifier, start);
    EXPECT_TRUE(splitView.resizing());
    QTest::mouseMove(&splitView, start + QPoint(40, 0));
    QTest::mouseRelease(&splitView, Qt::LeftButton, Qt::NoModifier, start + QPoint(40, 0));

    EXPECT_FALSE(splitView.resizing());
    EXPECT_EQ(splitView.panePreferredSize(left), 160);
    EXPECT_EQ(splitView.panePreferredSize(right), 160);
    EXPECT_GE(resizingSpy.count(), 2);
    EXPECT_GE(sizeSpy.count(), 2);
}

TEST_F(SplitViewTest, DisabledSplitViewIgnoresHandleInput)
{
    SplitView splitView;
    QWidget* left = makePane(QStringLiteral("Left"));
    QWidget* right = makePane(QStringLiteral("Right"));
    splitView.resize(320, 120);
    splitView.addPane(left, paneOptions(60, 120, 240));
    splitView.addPane(right, paneOptions(60, 160, 300, true));
    splitView.setEnabled(false);
    showAndProcess(splitView);

    const QPoint start = splitView.handleGeometry(0).center();
    QTest::mousePress(&splitView, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&splitView, start + QPoint(50, 0));
    QTest::mouseRelease(&splitView, Qt::LeftButton, Qt::NoModifier, start + QPoint(50, 0));

    EXPECT_FALSE(splitView.resizing());
    EXPECT_EQ(splitView.panePreferredSize(left), 120);
    EXPECT_EQ(splitView.panePreferredSize(right), 160);
}

TEST_F(SplitViewTest, StatePersistenceRestoresMatchingLayoutAndRejectsBadData)
{
    SplitView splitView;
    QWidget* first = makePane(QStringLiteral("First"));
    QWidget* second = makePane(QStringLiteral("Second"));
    splitView.addPane(first, paneOptions(50, 110, 300));
    splitView.addPane(second, paneOptions(50, 210, 400, true));
    splitView.setOrientation(Qt::Vertical);
    splitView.setFillPaneIndex(1);

    const QByteArray state = splitView.saveState();
    ASSERT_FALSE(state.isEmpty());

    SplitView restored;
    QWidget* restoredFirst = makePane(QStringLiteral("Restored First"));
    QWidget* restoredSecond = makePane(QStringLiteral("Restored Second"));
    restored.addPane(restoredFirst, paneOptions(40, 90, 300));
    restored.addPane(restoredSecond, paneOptions(40, 90, 400));
    ASSERT_TRUE(restored.restoreState(state));
    EXPECT_EQ(restored.orientation(), Qt::Vertical);
    EXPECT_EQ(restored.fillPaneIndex(), 1);
    EXPECT_EQ(restored.panePreferredSize(0), 110);
    EXPECT_EQ(restored.panePreferredSize(1), 210);

    const int oldPreferred = restored.panePreferredSize(0);
    EXPECT_FALSE(restored.restoreState(QByteArray("bad")));
    EXPECT_EQ(restored.panePreferredSize(0), oldPreferred);
    restored.addPane(makePane(QStringLiteral("Extra")));
    EXPECT_FALSE(restored.restoreState(state));
    EXPECT_EQ(restored.orientation(), Qt::Vertical);
}

TEST_F(SplitViewTest, ThemeUpdateAndHandleMetricsAreSafe)
{
    SplitView splitView;
    QWidget* first = makePane(QStringLiteral("First"));
    QWidget* second = makePane(QStringLiteral("Second"));
    splitView.addPane(first, paneOptions(50, 120, 300));
    splitView.addPane(second, paneOptions(50, 180, 400, true));
    splitView.setHandleWidth(-10);
    splitView.setHandleVisualThickness(100);

    EXPECT_EQ(splitView.handleWidth(), 1);
    EXPECT_EQ(splitView.handleVisualThickness(), 1);
    const int oldPreferred = splitView.panePreferredSize(first);
    FluentElement::setTheme(FluentElement::Dark);
    splitView.onThemeUpdated();
    EXPECT_EQ(splitView.panePreferredSize(first), oldPreferred);
}

TEST_F(SplitViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* window = new SplitViewTestWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(QStringLiteral("SplitView VisualCheck"));
    window->resize(1120, 760);
    window->onThemeUpdated();

    auto* rootLayout = new QGridLayout(window);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setHorizontalSpacing(16);
    rootLayout->setVerticalSpacing(16);

    auto* qmlPanel = new QWidget(window);
    auto* qmlLayout = new QVBoxLayout(qmlPanel);
    qmlLayout->setContentsMargins(0, 0, 0, 0);
    qmlLayout->setSpacing(10);
    auto* qmlTitle = new QLabel(QStringLiteral("QML-style resizable panes"), qmlPanel);
    auto* horizontalSplit = new SplitView(qmlPanel);
    auto* verticalSplit = new SplitView(qmlPanel);
    horizontalSplit->setMinimumSize(520, 220);
    verticalSplit->setMinimumSize(520, 220);
    verticalSplit->setOrientation(Qt::Vertical);
    horizontalSplit->addPane(new ColorPane(QStringLiteral("Project"), QColor("#EAF3FF")), paneOptions(90, 140, 260));
    horizontalSplit->addPane(new ColorPane(QStringLiteral("Editor"), QColor("#F6F6F6")), paneOptions(180, 320, 900, true));
    horizontalSplit->addPane(new ColorPane(QStringLiteral("Inspector"), QColor("#FFF4CE")), paneOptions(100, 160, 320));
    verticalSplit->addPane(new ColorPane(QStringLiteral("Timeline"), QColor("#EDE7F6")), paneOptions(80, 130, 260));
    verticalSplit->addPane(new ColorPane(QStringLiteral("Canvas"), QColor("#E8F5E9")), paneOptions(120, 260, 900, true));
    verticalSplit->addPane(new ColorPane(QStringLiteral("Output"), QColor("#FCE4EC")), paneOptions(70, 120, 240));
    qmlLayout->addWidget(qmlTitle);
    qmlLayout->addWidget(horizontalSplit, 1);
    qmlLayout->addWidget(verticalSplit, 1);
    rootLayout->addWidget(qmlPanel, 0, 0, 2, 1);

    auto* navPanel = new QWidget(window);
    auto* navAnchors = new view::AnchorLayout(navPanel);
    auto* controls = new QWidget(navPanel);
    auto* controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    auto* placementToggle = new QCheckBox(QStringLiteral("Pane right"), controls);
    auto* widthSlider = new QSlider(Qt::Horizontal, controls);
    auto* themeButton = new QPushButton(QStringLiteral("Theme"), controls);
    widthSlider->setRange(120, 280);
    widthSlider->setValue(180);
    controlsLayout->addWidget(placementToggle);
    controlsLayout->addWidget(widthSlider, 1);
    controlsLayout->addWidget(themeButton);

    auto* navSplit = new SplitView(navPanel);
    auto* pane = new ColorPane(QStringLiteral("People\nGlobe\nMessage\nMail"), QColor("#F3F2F1"));
    auto* content = new ColorPane(QStringLiteral("Content Page"), QColor("#FFFFFF"));
    navSplit->addPane(pane, paneOptions(96, 180, 320));
    navSplit->addPane(content, paneOptions(220, 420, 1000, true));

    view::AnchorLayout::Anchors controlsAnchors;
    controlsAnchors.left = {navPanel, view::AnchorLayout::Edge::Left, 0};
    controlsAnchors.right = {navPanel, view::AnchorLayout::Edge::Right, 0};
    controlsAnchors.top = {navPanel, view::AnchorLayout::Edge::Top, 0};
    controls->setFixedHeight(34);
    navAnchors->addAnchoredWidget(controls, controlsAnchors);

    view::AnchorLayout::Anchors splitAnchors;
    splitAnchors.left = {navPanel, view::AnchorLayout::Edge::Left, 0};
    splitAnchors.right = {navPanel, view::AnchorLayout::Edge::Right, 0};
    splitAnchors.top = {controls, view::AnchorLayout::Edge::Bottom, 10};
    splitAnchors.bottom = {navPanel, view::AnchorLayout::Edge::Bottom, 0};
    navAnchors->addAnchoredWidget(navSplit, splitAnchors);
    navPanel->setMinimumSize(500, 460);
    rootLayout->addWidget(navPanel, 0, 1, 2, 1);

    QObject::connect(widthSlider, &QSlider::valueChanged, navSplit, [navSplit, pane](int value) {
        navSplit->setPanePreferredSize(pane, value);
    });
    QObject::connect(placementToggle, &QCheckBox::toggled, navSplit, [navSplit, pane, content](bool right) {
        navSplit->removePane(pane);
        if (right)
            navSplit->addPane(pane, paneOptions(96, 180, 320));
        else
            navSplit->insertPane(0, pane, paneOptions(96, 180, 320));
        navSplit->setPaneFill(content, true);
    });
    QObject::connect(themeButton, &QPushButton::clicked, window, [window]() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light ? FluentElement::Dark : FluentElement::Light);
        window->onThemeUpdated();
    });

    window->show();
    qApp->exec();
}
