#include <gtest/gtest.h>

#include <QApplication>
#include <QFontMetrics>
#include <QPalette>
#include <QResizeEvent>
#include <QSignalSpy>
#include <QTest>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/navigation/Breadcrumb.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/Label.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::navigation::Breadcrumb;
using fluent::navigation::BreadcrumbItem;
using fluent::navigation::StackContentHost;
using fluent::textfields::Label;

namespace {

class BreadcrumbTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class BreadcrumbContentPanel : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
public:
    explicit BreadcrumbContentPanel(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_host(new StackContentHost(this))
    {
        m_host->setObjectName(QStringLiteral("BreadcrumbVisualContentHost"));
        m_host->setTransitionAnimationEnabled(true);
        onThemeUpdated();
    }

    StackContentHost* host() const { return m_host; }

    void onThemeUpdated() override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, themeColors().bgLayer);
        setPalette(pal);
        setAutoFillBackground(true);
        if (m_host)
            m_host->onThemeUpdated();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (m_host)
            m_host->setGeometry(contentsRect());
    }

private:
    StackContentHost* m_host = nullptr;
};

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

QVector<int> signalIndexes(const QSignalSpy& spy, int row = 0)
{
    return qvariant_cast<QVector<int>>(spy.at(row).at(0));
}

void addAnchored(AnchorLayout* layout, QWidget* widget)
{
    layout->addWidget(widget);
}

QWidget* createBreadcrumbPage(const QString& title, const QString& detail, const QString& actionText, Button** actionButton = nullptr)
{
    using Edge = AnchorLayout::Edge;

    auto* page = new QWidget();
    auto* pageLayout = new AnchorLayout(page);
    page->setLayout(pageLayout);

    auto* heading = new Label(title, page);
    heading->setFluentTypography(QStringLiteral("Title"));
    heading->anchors()->top = {page, Edge::Top, 28};
    heading->anchors()->left = {page, Edge::Left, 28};
    heading->anchors()->right = {page, Edge::Right, -28};
    addAnchored(pageLayout, heading);

    auto* summary = new Label(detail, page);
    summary->setFluentTypography(QStringLiteral("Body"));
    summary->setTextElideMode(Qt::ElideRight);
    summary->anchors()->top = {heading, Edge::Bottom, 10};
    summary->anchors()->left = {heading, Edge::Left, 0};
    summary->anchors()->right = {heading, Edge::Right, 0};
    addAnchored(pageLayout, summary);

    if (!actionText.isEmpty()) {
        auto* action = new Button(actionText, page);
        action->setFluentStyle(Button::Accent);
        action->setFixedSize(144, 32);
        action->anchors()->top = {summary, Edge::Bottom, 22};
        action->anchors()->left = {summary, Edge::Left, 0};
        addAnchored(pageLayout, action);
        if (actionButton)
            *actionButton = action;
    }

    return page;
}

} // namespace

class BreadcrumbTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::navigation::BreadcrumbItem>("fluent::navigation::BreadcrumbItem");
        qRegisterMetaType<QVector<int>>("QVector<int>");
        qRegisterMetaType<fluent::navigation::Breadcrumb::BreadcrumbSize>(
            "fluent::navigation::Breadcrumb::BreadcrumbSize");
        qRegisterMetaType<fluent::navigation::Breadcrumb::OverflowMode>(
            "fluent::navigation::Breadcrumb::OverflowMode");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new BreadcrumbTestWindow();
        window->resize(720, 360);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    BreadcrumbTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(BreadcrumbTest, DefaultsAndInheritanceMatchComponentPattern)
{
    Breadcrumb breadcrumb;

    EXPECT_EQ(breadcrumb.itemCount(), 0);
    EXPECT_EQ(breadcrumb.breadcrumbSize(), Breadcrumb::BreadcrumbSize::Standard);
    EXPECT_EQ(breadcrumb.overflowMode(), Breadcrumb::OverflowMode::None);
    EXPECT_FALSE(breadcrumb.autoTruncateOnItemClick());
    EXPECT_TRUE(breadcrumb.isEnabled());
    EXPECT_EQ(breadcrumb.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(breadcrumb.sizeHint().isEmpty());
    EXPECT_FALSE(breadcrumb.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&breadcrumb), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&breadcrumb), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&breadcrumb), nullptr);
}

TEST_F(BreadcrumbTest, ItemManagementPreservesOrderAndMetadata)
{
    Breadcrumb breadcrumb;
    QSignalSpy itemsSpy(&breadcrumb, &Breadcrumb::itemsChanged);

    breadcrumb.setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Documents"), QStringLiteral("Images")});
    EXPECT_EQ(breadcrumb.itemCount(), 3);
    EXPECT_EQ(breadcrumb.itemAt(1).text, QStringLiteral("Documents"));

    QVector<BreadcrumbItem> rich;
    rich.append(BreadcrumbItem(QStringLiteral("Root"), QStringLiteral("root-data")));
    rich.append(BreadcrumbItem(QStringLiteral("Disabled"), 42, false, QStringLiteral("Locked folder")));
    breadcrumb.setItems(rich);

    EXPECT_EQ(breadcrumb.itemCount(), 2);
    EXPECT_EQ(breadcrumb.itemAt(0).data.toString(), QStringLiteral("root-data"));
    EXPECT_FALSE(breadcrumb.itemAt(1).enabled);
    EXPECT_EQ(breadcrumb.itemAt(1).accessibleName, QStringLiteral("Locked folder"));

    breadcrumb.insertItem(1, BreadcrumbItem(QStringLiteral("Inserted")));
    EXPECT_EQ(breadcrumb.itemAt(1).text, QStringLiteral("Inserted"));
    EXPECT_EQ(breadcrumb.itemAt(2).text, QStringLiteral("Disabled"));

    EXPECT_FALSE(breadcrumb.removeItemAt(-1));
    EXPECT_FALSE(breadcrumb.removeItemAt(99));
    EXPECT_TRUE(breadcrumb.removeItemAt(1));
    EXPECT_EQ(breadcrumb.itemAt(1).text, QStringLiteral("Disabled"));
    EXPECT_EQ(breadcrumb.itemAt(99).text, QString());

    breadcrumb.clearItems();
    EXPECT_EQ(breadcrumb.itemCount(), 0);
    EXPECT_EQ(itemsSpy.count(), 5);
}

TEST_F(BreadcrumbTest, PropertySignalsEmitOnlyForEffectiveChanges)
{
    Breadcrumb breadcrumb;
    QSignalSpy sizeSpy(&breadcrumb, &Breadcrumb::breadcrumbSizeChanged);
    QSignalSpy overflowSpy(&breadcrumb, &Breadcrumb::overflowModeChanged);
    QSignalSpy truncateSpy(&breadcrumb, &Breadcrumb::autoTruncateOnItemClickChanged);
    QSignalSpy standardRoleSpy(&breadcrumb, &Breadcrumb::standardFontRoleChanged);
    QSignalSpy largeRoleSpy(&breadcrumb, &Breadcrumb::largeFontRoleChanged);

    breadcrumb.setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);
    breadcrumb.setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);
    breadcrumb.setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Standard);

    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Beginning);
    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Beginning);
    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Middle);

    breadcrumb.setAutoTruncateOnItemClick(true);
    breadcrumb.setAutoTruncateOnItemClick(true);
    breadcrumb.setAutoTruncateOnItemClick(false);

    breadcrumb.setStandardFontRole(QStringLiteral("Body Strong"));
    breadcrumb.setStandardFontRole(QStringLiteral("Body Strong"));
    breadcrumb.setLargeFontRole(QStringLiteral("Title Large"));
    breadcrumb.setLargeFontRole(QStringLiteral("Title Large"));

    EXPECT_EQ(sizeSpy.count(), 2);
    EXPECT_EQ(overflowSpy.count(), 2);
    EXPECT_EQ(truncateSpy.count(), 2);
    EXPECT_EQ(standardRoleSpy.count(), 1);
    EXPECT_EQ(largeRoleSpy.count(), 1);
}

TEST_F(BreadcrumbTest, LayoutHelpersExposeStandardAndLargeMetrics)
{
    Breadcrumb breadcrumb(window);
    breadcrumb.setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Documents")});
    breadcrumb.resize(600, 20);
    showAndProcess(breadcrumb);

    EXPECT_EQ(breadcrumb.itemGeometry(0).height(), 20);
    EXPECT_EQ(breadcrumb.itemGeometry(0).width(),
              QFontMetrics(breadcrumb.font()).horizontalAdvance(QStringLiteral("Home")) + 8);
    EXPECT_EQ(breadcrumb.separatorGeometry(0).width(), 16);
    EXPECT_TRUE(breadcrumb.itemGeometry(0).right() < breadcrumb.separatorGeometry(0).left());
    EXPECT_TRUE(breadcrumb.overflowGeometry().isEmpty());
    EXPECT_EQ(breadcrumb.hiddenItemIndexes().size(), 0);

    breadcrumb.setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);
    breadcrumb.resize(600, 40);
    QApplication::processEvents();

    EXPECT_EQ(breadcrumb.itemGeometry(0).height(), 40);
    EXPECT_EQ(breadcrumb.itemGeometry(0).width(),
              QFontMetrics(breadcrumb.font()).horizontalAdvance(QStringLiteral("Home")) + 16);
    EXPECT_EQ(breadcrumb.separatorGeometry(0).width(), 24);
    EXPECT_TRUE(breadcrumb.itemGeometry(99).isEmpty());
    EXPECT_TRUE(breadcrumb.separatorGeometry(99).isEmpty());
}

TEST_F(BreadcrumbTest, OverflowModesExposeDeterministicHiddenIndexes)
{
    Breadcrumb breadcrumb(window);
    breadcrumb.setItems(QStringList{
        QStringLiteral("Home"),
        QStringLiteral("Projects"),
        QStringLiteral("2026"),
        QStringLiteral("May"),
        QStringLiteral("Breadcrumb")
    });
    breadcrumb.resize(80, 20);

    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::None);
    showAndProcess(breadcrumb);
    EXPECT_TRUE(breadcrumb.overflowGeometry().isEmpty());
    EXPECT_TRUE(breadcrumb.hiddenItemIndexes().isEmpty());
    EXPECT_EQ(breadcrumb.visibleItemCount(), 5);

    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Beginning);
    QApplication::processEvents();
    EXPECT_EQ(breadcrumb.hiddenItemIndexes(), QVector<int>({0, 1, 2, 3}));
    EXPECT_FALSE(breadcrumb.overflowGeometry().isEmpty());
    EXPECT_FALSE(breadcrumb.itemGeometry(4).isEmpty());

    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Middle);
    QApplication::processEvents();
    EXPECT_EQ(breadcrumb.hiddenItemIndexes(), QVector<int>({1, 2, 3}));
    EXPECT_FALSE(breadcrumb.overflowGeometry().isEmpty());
    EXPECT_FALSE(breadcrumb.itemGeometry(0).isEmpty());
    EXPECT_FALSE(breadcrumb.itemGeometry(4).isEmpty());
}

TEST_F(BreadcrumbTest, PointerActivationSeparatorOverflowAndTruncation)
{
    Breadcrumb breadcrumb(window);
    breadcrumb.setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Folder"), QStringLiteral("File")});
    breadcrumb.resize(420, 20);
    showAndProcess(breadcrumb);

    QSignalSpy clickSpy(&breadcrumb, &Breadcrumb::itemClicked);
    QSignalSpy activateSpy(&breadcrumb, &Breadcrumb::itemActivated);
    QSignalSpy itemsSpy(&breadcrumb, &Breadcrumb::itemsChanged);

    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.separatorGeometry(0).center());
    EXPECT_EQ(clickSpy.count(), 0);
    EXPECT_EQ(activateSpy.count(), 0);

    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.itemGeometry(2).center());
    EXPECT_EQ(clickSpy.count(), 0);
    EXPECT_EQ(activateSpy.count(), 0);

    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.itemGeometry(1).center());
    ASSERT_EQ(clickSpy.count(), 1);
    ASSERT_EQ(activateSpy.count(), 1);
    EXPECT_EQ(clickSpy.takeFirst().at(0).toInt(), 1);
    EXPECT_EQ(qvariant_cast<BreadcrumbItem>(activateSpy.takeFirst().at(1)).text, QStringLiteral("Folder"));
    EXPECT_EQ(breadcrumb.itemCount(), 3);

    breadcrumb.setAutoTruncateOnItemClick(true);
    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.itemGeometry(1).center());
    EXPECT_EQ(breadcrumb.itemCount(), 2);
    EXPECT_EQ(breadcrumb.itemAt(1).text, QStringLiteral("Folder"));
    EXPECT_EQ(itemsSpy.count(), 1);

    breadcrumb.setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Projects"), QStringLiteral("2026"), QStringLiteral("May"), QStringLiteral("Breadcrumb")});
    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Beginning);
    breadcrumb.resize(80, 20);
    QApplication::processEvents();
    QSignalSpy overflowSpy(&breadcrumb, &Breadcrumb::overflowActivated);
    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.overflowGeometry().center());
    ASSERT_EQ(overflowSpy.count(), 1);
    EXPECT_EQ(signalIndexes(overflowSpy), QVector<int>({0, 1, 2, 3}));
}

TEST_F(BreadcrumbTest, KeyboardActivationSkipsDisabledItems)
{
    Breadcrumb breadcrumb(window);
    QVector<BreadcrumbItem> items;
    items.append(BreadcrumbItem(QStringLiteral("Home")));
    items.append(BreadcrumbItem(QStringLiteral("Locked"), QVariant(), false));
    items.append(BreadcrumbItem(QStringLiteral("Section")));
    items.append(BreadcrumbItem(QStringLiteral("Current")));
    breadcrumb.setItems(items);
    breadcrumb.resize(500, 20);
    showAndProcess(breadcrumb);

    QSignalSpy activateSpy(&breadcrumb, &Breadcrumb::itemActivated);
    breadcrumb.setFocus();
    QApplication::processEvents();
    QTest::keyClick(&breadcrumb, Qt::Key_Right);
    QTest::keyClick(&breadcrumb, Qt::Key_Space);
    ASSERT_EQ(activateSpy.count(), 1);
    EXPECT_EQ(activateSpy.takeFirst().at(0).toInt(), 2);

    QTest::keyClick(&breadcrumb, Qt::Key_End);
    QTest::keyClick(&breadcrumb, Qt::Key_Space);
    ASSERT_EQ(activateSpy.count(), 1);
    EXPECT_EQ(activateSpy.takeFirst().at(0).toInt(), 2);

    QTest::keyClick(&breadcrumb, Qt::Key_Home);
    QTest::keyClick(&breadcrumb, Qt::Key_Return);
    ASSERT_EQ(activateSpy.count(), 1);
    EXPECT_EQ(activateSpy.takeFirst().at(0).toInt(), 0);
}

TEST_F(BreadcrumbTest, DisabledItemsAndDisabledControlBlockActivation)
{
    Breadcrumb breadcrumb(window);
    QVector<BreadcrumbItem> items;
    items.append(BreadcrumbItem(QStringLiteral("Home")));
    items.append(BreadcrumbItem(QStringLiteral("Locked"), QVariant(), false));
    items.append(BreadcrumbItem(QStringLiteral("Current")));
    breadcrumb.setItems(items);
    breadcrumb.resize(500, 20);
    showAndProcess(breadcrumb);

    QSignalSpy activateSpy(&breadcrumb, &Breadcrumb::itemActivated);
    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.itemGeometry(1).center());
    EXPECT_EQ(activateSpy.count(), 0);

    breadcrumb.setEnabled(false);
    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.itemGeometry(0).center());
    breadcrumb.setFocus();
    QTest::keyClick(&breadcrumb, Qt::Key_End);
    QTest::keyClick(&breadcrumb, Qt::Key_Space);
    EXPECT_EQ(activateSpy.count(), 0);
    EXPECT_EQ(breadcrumb.itemCount(), 3);
}

TEST_F(BreadcrumbTest, ThemeRefreshAndAccessibilityTextStayStable)
{
    Breadcrumb breadcrumb(window);
    breadcrumb.setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Documents")});
    breadcrumb.resize(420, 20);
    showAndProcess(breadcrumb);

    EXPECT_TRUE(breadcrumb.accessibleName().contains(QStringLiteral("Home > Documents")));
    EXPECT_TRUE(breadcrumb.accessibleDescription().contains(QStringLiteral("Documents")));
    const QSize lightHint = breadcrumb.sizeHint();

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    breadcrumb.onThemeUpdated();
    QApplication::processEvents();

    EXPECT_EQ(breadcrumb.itemCount(), 2);
    EXPECT_FALSE(breadcrumb.itemGeometry(1).isEmpty());
    EXPECT_FALSE(lightHint.isEmpty());
    EXPECT_FALSE(breadcrumb.sizeHint().isEmpty());
    EXPECT_TRUE(breadcrumb.accessibleDescription().contains(QStringLiteral("Documents")));
}

TEST_F(BreadcrumbTest, BreadcrumbActivationCanDriveStackContentHostPages)
{
    Breadcrumb breadcrumb(window);
    StackContentHost host(window);
    host.setTransitionAnimationEnabled(false);
    host.setGeometry(20, 60, 420, 180);
    breadcrumb.setGeometry(20, 20, 420, 40);
    breadcrumb.setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);
    breadcrumb.setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Projects"), QStringLiteral("Breadcrumb")});

    QWidget* homePage = createBreadcrumbPage(QStringLiteral("Home"), QStringLiteral("Root content page"), QString());
    QWidget* projectsPage = createBreadcrumbPage(QStringLiteral("Projects"), QStringLiteral("Project listing page"), QString());
    QWidget* breadcrumbPage = createBreadcrumbPage(QStringLiteral("Breadcrumb"), QStringLiteral("Selected leaf page"), QString());
    ASSERT_TRUE(host.insertPage(0, homePage));
    ASSERT_TRUE(host.insertPage(1, projectsPage));
    ASSERT_TRUE(host.insertPage(2, breadcrumbPage));
    host.setCurrentIndex(2, 0, false);

    QObject::connect(&breadcrumb, &Breadcrumb::itemActivated, &host, [&host](int index, const BreadcrumbItem&) {
        host.setCurrentIndex(index, 0, false);
    });

    showAndProcess(*window);
    EXPECT_EQ(host.currentIndex(), 2);
    EXPECT_TRUE(breadcrumbPage->isVisible());

    QTest::mouseClick(&breadcrumb, Qt::LeftButton, Qt::NoModifier, breadcrumb.itemGeometry(1).center());
    QApplication::processEvents();

    EXPECT_EQ(host.currentIndex(), 1);
    EXPECT_FALSE(breadcrumbPage->isVisible());
    EXPECT_TRUE(projectsPage->isVisible());
}

TEST_F(BreadcrumbTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    auto* visual = new BreadcrumbTestWindow();
    visual->setAttribute(Qt::WA_DeleteOnClose);
    visual->resize(980, 660);
    visual->setWindowTitle(QStringLiteral("Breadcrumb VisualCheck"));
    visual->onThemeUpdated();

    auto* visualLayout = new AnchorLayout(visual);
    visual->setLayout(visualLayout);

    auto* title = new Label(QStringLiteral("Breadcrumb"), visual);
    title->setFluentTypography(QStringLiteral("Subtitle"));
    title->anchors()->top = {visual, Edge::Top, 24};
    title->anchors()->left = {visual, Edge::Left, 32};
    addAnchored(visualLayout, title);

    auto* themeButton = new Button(QStringLiteral("Dark"), visual);
    themeButton->setFixedSize(96, 32);
    themeButton->anchors()->top = {visual, Edge::Top, 24};
    themeButton->anchors()->right = {visual, Edge::Right, -32};
    addAnchored(visualLayout, themeButton);

    auto* resetButton = new Button(QStringLiteral("Home"), visual);
    resetButton->setFixedSize(96, 32);
    resetButton->anchors()->top = {visual, Edge::Top, 24};
    resetButton->anchors()->right = {themeButton, Edge::Left, -12};
    addAnchored(visualLayout, resetButton);

    auto* leafButton = new Button(QStringLiteral("Leaf"), visual);
    leafButton->setFixedSize(96, 32);
    leafButton->anchors()->top = {visual, Edge::Top, 24};
    leafButton->anchors()->right = {resetButton, Edge::Left, -12};
    addAnchored(visualLayout, leafButton);

    auto* navigation = new Breadcrumb(visual);
    navigation->setBreadcrumbSize(Breadcrumb::BreadcrumbSize::Large);
    navigation->setOverflowMode(Breadcrumb::OverflowMode::Middle);
    navigation->setFixedHeight(40);
    navigation->anchors()->top = {title, Edge::Bottom, 26};
    navigation->anchors()->left = {visual, Edge::Left, 32};
    navigation->anchors()->right = {visual, Edge::Right, -32};
    addAnchored(visualLayout, navigation);

    auto* status = new Label(visual);
    status->setFluentTypography(QStringLiteral("Caption"));
    status->setFixedHeight(24);
    status->anchors()->top = {navigation, Edge::Bottom, 8};
    status->anchors()->left = {navigation, Edge::Left, 0};
    status->anchors()->right = {navigation, Edge::Right, 0};
    addAnchored(visualLayout, status);

    auto* contentPanel = new BreadcrumbContentPanel(visual);
    contentPanel->anchors()->top = {status, Edge::Bottom, 12};
    contentPanel->anchors()->left = {visual, Edge::Left, 32};
    contentPanel->anchors()->right = {visual, Edge::Right, -32};
    contentPanel->anchors()->bottom = {visual, Edge::Bottom, -188};
    addAnchored(visualLayout, contentPanel);

    StackContentHost* contentHost = contentPanel->host();
    Button* openProjects = nullptr;
    Button* openFluent = nullptr;
    Button* openControls = nullptr;
    Button* openBreadcrumb = nullptr;
    contentHost->insertPage(0, createBreadcrumbPage(QStringLiteral("Home"), QStringLiteral("Root landing page hosted by StackContentHost."), QStringLiteral("Open projects"), &openProjects));
    contentHost->insertPage(1, createBreadcrumbPage(QStringLiteral("Projects"), QStringLiteral("Project folder with Fluent component workspaces."), QStringLiteral("Open Fluent"), &openFluent));
    contentHost->insertPage(2, createBreadcrumbPage(QStringLiteral("Fluent"), QStringLiteral("Component family page. The breadcrumb above can jump back to any ancestor."), QStringLiteral("Open Controls"), &openControls));
    contentHost->insertPage(3, createBreadcrumbPage(QStringLiteral("Controls"), QStringLiteral("Navigation components listed as pages inside the shared stack host."), QStringLiteral("Open Breadcrumb"), &openBreadcrumb));
    contentHost->insertPage(4, createBreadcrumbPage(QStringLiteral("Breadcrumb"), QStringLiteral("Leaf page selected from the breadcrumb trail."), QString()));

    const QStringList fullPath{
        QStringLiteral("Home"),
        QStringLiteral("Projects"),
        QStringLiteral("Fluent"),
        QStringLiteral("Controls"),
        QStringLiteral("Breadcrumb")
    };

    auto navigateTo = [navigation, contentHost, status, fullPath](int index, bool animated) {
        const int boundedIndex = qBound(0, index, fullPath.size() - 1);
        navigation->setItems(fullPath.mid(0, boundedIndex + 1));
        contentHost->setCurrentIndex(boundedIndex, 0, animated);
        status->setText(QStringLiteral("StackContentHost page %1 of %2: %3")
                            .arg(boundedIndex + 1)
                            .arg(fullPath.size())
                            .arg(fullPath.at(boundedIndex)));
    };

    navigateTo(0, false);
    QObject::connect(navigation, &Breadcrumb::itemActivated, [navigateTo](int index, const BreadcrumbItem&) {
        navigateTo(index, true);
    });
    QObject::connect(resetButton, &Button::clicked, [navigateTo]() { navigateTo(0, true); });
    QObject::connect(leafButton, &Button::clicked, [navigateTo]() { navigateTo(4, true); });
    QObject::connect(openProjects, &Button::clicked, [navigateTo]() { navigateTo(1, true); });
    QObject::connect(openFluent, &Button::clicked, [navigateTo]() { navigateTo(2, true); });
    QObject::connect(openControls, &Button::clicked, [navigateTo]() { navigateTo(3, true); });
    QObject::connect(openBreadcrumb, &Button::clicked, [navigateTo]() { navigateTo(4, true); });

    auto* beginning = new Breadcrumb(visual);
    beginning->setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Projects"), QStringLiteral("Fluent"), QStringLiteral("Controls"), QStringLiteral("Breadcrumb")});
    beginning->setOverflowMode(Breadcrumb::OverflowMode::Beginning);
    beginning->setFixedHeight(20);
    beginning->anchors()->top = {contentPanel, Edge::Bottom, 28};
    beginning->anchors()->left = {visual, Edge::Left, 32};
    beginning->anchors()->right = {visual, Edge::Right, -620};
    addAnchored(visualLayout, beginning);

    auto* middle = new Breadcrumb(visual);
    middle->setItems(QStringList{QStringLiteral("Home"), QStringLiteral("Projects"), QStringLiteral("Fluent"), QStringLiteral("Controls"), QStringLiteral("Breadcrumb")});
    middle->setOverflowMode(Breadcrumb::OverflowMode::Middle);
    middle->setFixedHeight(20);
    middle->anchors()->top = {beginning, Edge::Bottom, 26};
    middle->anchors()->left = {visual, Edge::Left, 32};
    middle->anchors()->right = {visual, Edge::Right, -620};
    addAnchored(visualLayout, middle);

    auto* disabled = new Breadcrumb(visual);
    QVector<BreadcrumbItem> disabledItems;
    disabledItems.append(BreadcrumbItem(QStringLiteral("Home")));
    disabledItems.append(BreadcrumbItem(QStringLiteral("Locked"), QVariant(), false, QStringLiteral("Locked folder")));
    disabledItems.append(BreadcrumbItem(QStringLiteral("Current")));
    disabled->setItems(disabledItems);
    disabled->setFixedHeight(20);
    disabled->anchors()->top = {contentPanel, Edge::Bottom, 28};
    disabled->anchors()->left = {beginning, Edge::Right, 48};
    disabled->anchors()->right = {visual, Edge::Right, -32};
    addAnchored(visualLayout, disabled);

    auto* truncating = new Breadcrumb(visual);
    truncating->setAutoTruncateOnItemClick(true);
    truncating->setItems(QStringList{QStringLiteral("This PC"), QStringLiteral("Local Disk"), QStringLiteral("Users"), QStringLiteral("Public"), QStringLiteral("Pictures")});
    truncating->setFixedHeight(20);
    truncating->anchors()->top = {disabled, Edge::Bottom, 26};
    truncating->anchors()->left = {disabled, Edge::Left, 0};
    truncating->anchors()->right = {disabled, Edge::Right, -112};
    addAnchored(visualLayout, truncating);

    auto* restoreButton = new Button(QStringLiteral("Reset"), visual);
    restoreButton->setFixedSize(96, 32);
    restoreButton->anchors()->verticalCenter = {truncating, Edge::VCenter, 0};
    restoreButton->anchors()->right = {disabled, Edge::Right, 0};
    addAnchored(visualLayout, restoreButton);

    QObject::connect(themeButton, &Button::clicked, [visual, themeButton, navigation, status, contentPanel, beginning, middle, disabled, truncating]() {
        const bool useDark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light;
        fluent::FluentElement::setTheme(useDark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
        visual->onThemeUpdated();
        status->onThemeUpdated();
        contentPanel->onThemeUpdated();
        for (Breadcrumb* breadcrumb : {navigation, beginning, middle, disabled, truncating})
            breadcrumb->onThemeUpdated();
        themeButton->setText(useDark ? QStringLiteral("Light") : QStringLiteral("Dark"));
    });
    QObject::connect(restoreButton, &Button::clicked, [truncating]() {
        truncating->setItems(QStringList{QStringLiteral("This PC"), QStringLiteral("Local Disk"), QStringLiteral("Users"), QStringLiteral("Public"), QStringLiteral("Pictures")});
    });

    visual->show();
    qApp->exec();
}
