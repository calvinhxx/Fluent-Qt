#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSignalSpy>
#include <QTest>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/basicinput/Button.h"
#include "components/navigation/StackContentHost.h"
#include "components/navigation/TabView.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"
#include "QtTestEnvironment.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::navigation::StackContentHost;
using fluent::navigation::TabView;
using fluent::navigation::TabViewItem;
using fluent::scrolling::ScrollView;
using fluent::textfields::Label;

namespace {

class TabViewTestWindow : public QWidget, public fluent::FluentElement {
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

void showAndProcess(QWidget& widget)
{
    if (widget.window() && widget.window() != &widget)
        widget.window()->show();
    widget.show();
    QApplication::processEvents();
}

void addAnchored(AnchorLayout* layout, QWidget* widget)
{
    layout->addWidget(widget);
}

QWidget* createContent(const QString& text, QWidget* parent = nullptr)
{
    using Edge = AnchorLayout::Edge;
    auto* page = new QWidget(parent);
    auto* layout = new AnchorLayout(page);
    page->setLayout(layout);

    QPixmap previewPixmap(168, 104);
    previewPixmap.fill(Qt::transparent);
    QPainter painter(&previewPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 120, 212));
    painter.drawRoundedRect(QRectF(0, 0, 168, 104), 6, 6);
    painter.setBrush(QColor(255, 255, 255, 44));
    painter.drawRoundedRect(QRectF(18, 18, 132, 14), 4, 4);
    painter.drawRoundedRect(QRectF(18, 44, 96, 10), 4, 4);
    painter.setBrush(QColor(255, 255, 255, 76));
    painter.drawEllipse(QRectF(118, 58, 34, 34));
    painter.end();

    auto* title = new Label(text, page);
    title->setFluentTypography(QStringLiteral("Body Strong"));
    title->anchors()->top = {page, Edge::Top, 20};
    title->anchors()->left = {page, Edge::Left, 20};
    title->anchors()->right = {page, Edge::Right, -240};
    addAnchored(layout, title);

    auto* description = new Label(QStringLiteral("ContentHost page with mixed controls and a generated preview image."), page);
    description->setFluentTypography(QStringLiteral("Caption"));
    description->setTextElideMode(Qt::ElideRight);
    description->anchors()->top = {title, Edge::Bottom, 8};
    description->anchors()->left = {title, Edge::Left, 0};
    description->anchors()->right = {title, Edge::Right, 0};
    addAnchored(layout, description);

    auto* preview = new Label(page);
    preview->setPixmap(previewPixmap);
    preview->setFixedSize(previewPixmap.size());
    preview->anchors()->top = {page, Edge::Top, 20};
    preview->anchors()->right = {page, Edge::Right, -20};
    addAnchored(layout, preview);

    auto* primary = new Button(QStringLiteral("Open"), page);
    primary->setFluentStyle(Button::Accent);
    primary->setFixedSize(84, 32);
    primary->anchors()->top = {description, Edge::Bottom, 18};
    primary->anchors()->left = {title, Edge::Left, 0};
    addAnchored(layout, primary);

    auto* pin = new Button(QStringLiteral("Pin"), page);
    pin->setFluentLayout(Button::IconBefore);
    pin->setIconGlyph(Typography::Icons::Pin, 16);
    pin->setFixedSize(84, 32);
    pin->anchors()->top = {primary, Edge::Top, 0};
    pin->anchors()->left = {primary, Edge::Right, 8};
    addAnchored(layout, pin);

    auto* status = new Label(QStringLiteral("Ready"), page);
    status->setFluentTypography(QStringLiteral("Caption"));
    status->anchors()->left = {title, Edge::Left, 0};
    status->anchors()->bottom = {page, Edge::Bottom, -16};
    addAnchored(layout, status);
    return page;
}

StackContentHost* createBoundHost(TabView* tabs, QWidget* parent, const QString& pagePrefix)
{
    auto* host = new StackContentHost(parent);
    for (int index = 0; index < tabs->tabCount(); ++index) {
        const TabViewItem item = tabs->tabAt(index);
        const QString pageTitle = item.text.trimmed().isEmpty()
            ? QStringLiteral("%1 %2").arg(pagePrefix).arg(index + 1)
            : item.text;
        host->insertPage(index, createContent(pageTitle));
    }

    QObject::connect(tabs, &TabView::currentChanged, host, [host](int index) {
        host->setCurrentIndex(index, 0, true);
    });
    QObject::connect(tabs, &TabView::tabMoved, host, [host](int from, int to) {
        host->movePage(from, to);
    });
    QObject::connect(tabs, &TabView::tabCloseRequested, tabs, [tabs, host](int index) {
        QWidget* page = host->takePage(index);
        if (page)
            page->deleteLater();
        tabs->closeTab(index);
        host->setCurrentIndex(tabs->selectedIndex(), 0, false);
    });
    QObject::connect(tabs, &TabView::addTabRequested, tabs, [tabs, host, pagePrefix]() {
        const int number = tabs->tabCount() + 1;
        const QString title = QStringLiteral("%1 %2").arg(pagePrefix).arg(number);
        const int index = tabs->addTab(TabViewItem(title, Typography::Icons::Document));
        host->insertPage(index, createContent(title));
        host->setCurrentIndex(tabs->selectedIndex(), 0, false);
    });
    host->setCurrentIndex(tabs->selectedIndex(), 0, false);
    return host;
}

} // namespace

class TabViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::navigation::TabViewItem>("fluent::navigation::TabViewItem");
        qRegisterMetaType<fluent::navigation::TabView::TabWidthMode>(
            "fluent::navigation::TabView::TabWidthMode");
        qRegisterMetaType<fluent::navigation::TabView::CloseButtonOverlayMode>(
            "fluent::navigation::TabView::CloseButtonOverlayMode");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new TabViewTestWindow();
        window->resize(820, 460);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    TabViewTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(TabViewTest, DefaultsInheritanceAndMetatypesMatchComponentPattern)
{
    TabView tabs;

    EXPECT_EQ(tabs.tabCount(), 0);
    EXPECT_EQ(tabs.selectedIndex(), -1);
    EXPECT_EQ(tabs.tabWidthMode(), TabView::TabWidthMode::Equal);
    EXPECT_EQ(tabs.closeButtonOverlayMode(), TabView::CloseButtonOverlayMode::Auto);
    EXPECT_TRUE(tabs.addTabButtonVisible());
    EXPECT_TRUE(tabs.isAddTabButtonVisible());
    EXPECT_TRUE(tabs.areTabsClosable());
    EXPECT_FALSE(tabs.tabReorderEnabled());
    EXPECT_FALSE(tabs.isTabReorderEnabled());
    EXPECT_TRUE(tabs.keyboardAcceleratorsEnabled());
    EXPECT_TRUE(tabs.areKeyboardAcceleratorsEnabled());
    EXPECT_TRUE(tabs.isEnabled());
    EXPECT_EQ(tabs.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(tabs.sizeHint().isEmpty());
    EXPECT_FALSE(tabs.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&tabs), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&tabs), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&tabs), nullptr);
    EXPECT_GT(qMetaTypeId<TabViewItem>(), 0);
    EXPECT_GT(qMetaTypeId<TabView::TabWidthMode>(), 0);
}

TEST_F(TabViewTest, ItemManagementPreservesOrderMetadataAndInvalidIndexes)
{
    TabView tabs;
    QSignalSpy changedSpy(&tabs, &TabView::tabsChanged);
    QSignalSpy selectionSpy(&tabs, &TabView::selectedIndexChanged);

    EXPECT_EQ(tabs.addTab(QStringLiteral("Document 1")), 0);
    EXPECT_EQ(tabs.tabCount(), 1);
    EXPECT_EQ(tabs.selectedIndex(), 0);
    EXPECT_EQ(tabs.tabAt(0).text, QStringLiteral("Document 1"));

    TabViewItem rich(QStringLiteral("Rich"), Typography::Icons::Document, false, false, QStringLiteral("payload"), QStringLiteral("Readable Rich"));
    EXPECT_EQ(tabs.addTab(rich), 1);
    EXPECT_EQ(tabs.tabAt(1).iconGlyph, Typography::Icons::Document);
    EXPECT_FALSE(tabs.tabAt(1).closable);
    EXPECT_FALSE(tabs.tabAt(1).enabled);
    EXPECT_EQ(tabs.tabAt(1).data.toString(), QStringLiteral("payload"));
    EXPECT_EQ(tabs.tabAt(1).accessibleName, QStringLiteral("Readable Rich"));

    EXPECT_TRUE(tabs.insertTab(1, QStringLiteral("Inserted")));
    EXPECT_EQ(tabs.tabAt(1).text, QStringLiteral("Inserted"));
    EXPECT_EQ(tabs.tabAt(2).text, QStringLiteral("Rich"));
    EXPECT_FALSE(tabs.insertTab(-1, QStringLiteral("Bad")));
    EXPECT_FALSE(tabs.insertTab(99, QStringLiteral("Bad")));
    EXPECT_EQ(tabs.tabAt(99).text, QString());

    EXPECT_TRUE(tabs.setTabText(1, QStringLiteral("Renamed")));
    EXPECT_TRUE(tabs.setTabIconGlyph(1, Typography::Icons::Edit));
    EXPECT_TRUE(tabs.setTabClosable(1, false));
    EXPECT_TRUE(tabs.setTabEnabled(1, false));
    EXPECT_TRUE(tabs.setTabData(1, 42));
    EXPECT_TRUE(tabs.setTabAccessibleName(1, QStringLiteral("Renamed tab")));
    EXPECT_EQ(tabs.tabAt(1).text, QStringLiteral("Renamed"));
    EXPECT_EQ(tabs.tabAt(1).data.toInt(), 42);
    EXPECT_FALSE(tabs.setTabText(1, QStringLiteral("Renamed")));
    EXPECT_FALSE(tabs.setTabEnabled(99, true));

    EXPECT_FALSE(tabs.removeTab(-1));
    EXPECT_FALSE(tabs.removeTab(99));
    EXPECT_TRUE(tabs.removeTab(1));
    EXPECT_EQ(tabs.tabCount(), 2);
    tabs.clearTabs();
    EXPECT_EQ(tabs.tabCount(), 0);
    EXPECT_EQ(tabs.selectedIndex(), -1);
    EXPECT_GE(changedSpy.count(), 7);
    EXPECT_GE(selectionSpy.count(), 1);
}

TEST_F(TabViewTest, PropertySignalsEmitOnlyForEffectiveChanges)
{
    TabView tabs;
    QSignalSpy widthSpy(&tabs, &TabView::tabWidthModeChanged);
    QSignalSpy closeSpy(&tabs, &TabView::closeButtonOverlayModeChanged);
    QSignalSpy closableSpy(&tabs, &TabView::tabsClosableChanged);
    QSignalSpy addSpy(&tabs, &TabView::addTabButtonVisibleChanged);
    QSignalSpy reorderSpy(&tabs, &TabView::tabReorderEnabledChanged);
    QSignalSpy keyboardSpy(&tabs, &TabView::keyboardAcceleratorsEnabledChanged);
    QSignalSpy fontSpy(&tabs, &TabView::tabFontRoleChanged);
    QSignalSpy iconSpy(&tabs, &TabView::iconFontFamilyChanged);

    tabs.setTabWidthMode(TabView::TabWidthMode::SizeToContent);
    tabs.setTabWidthMode(TabView::TabWidthMode::SizeToContent);
    tabs.setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
    tabs.setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
    tabs.setTabsClosable(false);
    tabs.setTabsClosable(false);
    tabs.setAddTabButtonVisible(false);
    tabs.setAddTabButtonVisible(false);
    tabs.setTabReorderEnabled(true);
    tabs.setTabReorderEnabled(true);
    tabs.setKeyboardAcceleratorsEnabled(false);
    tabs.setKeyboardAcceleratorsEnabled(false);
    tabs.setTabFontRole(QStringLiteral("Body Strong"));
    tabs.setTabFontRole(QStringLiteral("Body Strong"));
    tabs.setIconFontFamily(QStringLiteral("Segoe Fluent Icons"));

    EXPECT_EQ(widthSpy.count(), 1);
    EXPECT_EQ(closeSpy.count(), 1);
    EXPECT_EQ(closableSpy.count(), 1);
    EXPECT_EQ(addSpy.count(), 1);
    EXPECT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(keyboardSpy.count(), 1);
    EXPECT_EQ(fontSpy.count(), 1);
    EXPECT_EQ(iconSpy.count(), 0);
    EXPECT_FALSE(tabs.areTabsClosable());
    EXPECT_FALSE(tabs.addTabButtonVisible());
    EXPECT_FALSE(tabs.isAddTabButtonVisible());
    EXPECT_TRUE(tabs.isTabReorderEnabled());
    EXPECT_FALSE(tabs.areKeyboardAcceleratorsEnabled());
}

TEST_F(TabViewTest, SelectionCanDriveExternalStackContentHostPages)
{
    using Edge = AnchorLayout::Edge;
    auto* tabs = new TabView(window);
    tabs->setFixedSize(640, 40);
    tabs->anchors()->top = {window, Edge::Top, 20};
    tabs->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, tabs);

    auto* host = new StackContentHost(window);
    host->setFixedSize(640, 240);
    host->anchors()->top = {tabs, Edge::Bottom, 0};
    host->anchors()->left = {tabs, Edge::Left, 0};
    addAnchored(layout, host);

    auto* first = createContent(QStringLiteral("First page"));
    auto* second = createContent(QStringLiteral("Second page"));
    tabs->addTab(TabViewItem(QStringLiteral("First"), Typography::Icons::Document));
    tabs->addTab(TabViewItem(QStringLiteral("Second"), Typography::Icons::Document));
    host->insertPage(0, first);
    host->insertPage(1, second);

    QObject::connect(tabs, &TabView::currentChanged, host, [host](int index) {
        host->setCurrentIndex(index, 0, true);
    });
    QObject::connect(tabs, &TabView::tabMoved, host, [host](int from, int to) {
        host->movePage(from, to);
    });
    host->setCurrentIndex(tabs->selectedIndex(), 0, false);
    showAndProcess(*window);

    EXPECT_EQ(first->parentWidget(), host);
    EXPECT_EQ(second->parentWidget(), host);
    EXPECT_TRUE(first->isVisible());
    EXPECT_FALSE(second->isVisible());
    EXPECT_EQ(first->geometry().size(), host->size());

    QSignalSpy currentSpy(tabs, &TabView::currentChanged);
    tabs->setSelectedIndex(1);
    QApplication::processEvents();
    EXPECT_EQ(host->currentIndex(), 1);
    EXPECT_TRUE(host->busy());
    EXPECT_FALSE(first->isVisible());
    EXPECT_TRUE(second->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!host->busy(), 1000);
    EXPECT_FALSE(first->isVisible());
    EXPECT_TRUE(second->isVisible());
    EXPECT_EQ(currentSpy.count(), 1);

    EXPECT_TRUE(tabs->removeTab(1));
    QWidget* removed = host->takePage(1);
    EXPECT_EQ(removed, second);
    delete removed;
    host->setCurrentIndex(tabs->selectedIndex(), 0, false);
    EXPECT_EQ(tabs->selectedIndex(), 0);
    tabs->clearTabs();
    host->clearPages();
    EXPECT_EQ(tabs->tabCount(), 0);
    EXPECT_EQ(tabs->selectedIndex(), -1);
}

TEST_F(TabViewTest, GeometryWidthModesCloseModesAndOverflowAreDeterministic)
{
    TabView tabs(window);
    tabs.resize(760, 320);
    tabs.addTab(QStringLiteral("One"));
    tabs.addTab(QStringLiteral("Two"));
    tabs.addTab(QStringLiteral("Three"));
    showAndProcess(tabs);

    EXPECT_EQ(tabs.tabGeometry(0).height(), 32);
    EXPECT_EQ(tabs.addButtonGeometry().size(), QSize(40, 32));
    EXPECT_EQ(tabs.sizeHint().height(), 40);
    EXPECT_EQ(tabs.visibleTabIndexes(), QVector<int>({0, 1, 2}));
    const int equalWidth = tabs.tabGeometry(0).width();
    EXPECT_EQ(equalWidth, tabs.tabGeometry(1).width());

    tabs.setTabWidthMode(TabView::TabWidthMode::SizeToContent);
    QApplication::processEvents();
    EXPECT_LE(tabs.tabGeometry(0).width(), equalWidth);

    tabs.setTabWidthMode(TabView::TabWidthMode::Compact);
    tabs.setSelectedIndex(2);
    QApplication::processEvents();
    EXPECT_LT(tabs.tabGeometry(0).width(), tabs.tabGeometry(2).width());

    tabs.setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
    QApplication::processEvents();
    EXPECT_FALSE(tabs.closeButtonGeometry(0).isEmpty());
    tabs.setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::OnHover);
    QTest::mouseMove(&tabs, tabs.addButtonGeometry().center());
    QApplication::processEvents();
    EXPECT_TRUE(tabs.closeButtonGeometry(0).isEmpty());

    const int compactCollapsedWidth = tabs.tabGeometry(0).width();
    QTest::mouseMove(&tabs, tabs.tabGeometry(0).center());
    QApplication::processEvents();
    EXPECT_GE(tabs.tabGeometry(0).width(), compactCollapsedWidth);
    QTRY_VERIFY_WITH_TIMEOUT(tabs.tabGeometry(0).width() > compactCollapsedWidth, 500);
    QTRY_VERIFY_WITH_TIMEOUT(!tabs.closeButtonGeometry(0).isEmpty(), 500);
    const QRect compactHoverTab = tabs.tabGeometry(0);
    const QRect compactHoverClose = tabs.closeButtonGeometry(0);
    EXPECT_GT(compactHoverTab.width(), compactCollapsedWidth);
    EXPECT_GT(compactHoverClose.left(), compactHoverTab.left() + 36);
    QTest::mouseMove(&tabs, compactHoverClose.center());
    QApplication::processEvents();
    EXPECT_FALSE(tabs.closeButtonGeometry(0).isEmpty());
    EXPECT_GE(tabs.tabGeometry(0).width(), compactHoverTab.width() - 1);

    tabs.setTabWidthMode(TabView::TabWidthMode::SizeToContent);
    for (int index = 0; index < 10; ++index)
        tabs.addTab(QStringLiteral("Very long document %1").arg(index));
    tabs.resize(240, 240);
    tabs.setSelectedIndex(11);
    QApplication::processEvents();
    EXPECT_LT(tabs.visibleTabIndexes().size(), tabs.tabCount());
    EXPECT_FALSE(tabs.overflowBackGeometry().isEmpty());
    EXPECT_FALSE(tabs.overflowForwardGeometry().isEmpty());
    EXPECT_TRUE(tabs.visibleTabIndexes().contains(11));
    const QVector<int> visibleBeforeBack = tabs.visibleTabIndexes();
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.overflowBackGeometry().center());
    QApplication::processEvents();
    EXPECT_NE(tabs.visibleTabIndexes(), visibleBeforeBack);
    const QVector<int> visibleBeforeForward = tabs.visibleTabIndexes();
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.overflowForwardGeometry().center());
    QApplication::processEvents();
    EXPECT_NE(tabs.visibleTabIndexes(), visibleBeforeForward);
    const QVector<int> visibleBeforeWheel = tabs.visibleTabIndexes();
    const QPoint wheelPoint = tabs.overflowForwardGeometry().center();
    FLUENT_MAKE_WHEEL_EVENT(smallHeaderWheel, wheelPoint.x(), wheelPoint.y(), 30, Qt::NoModifier);
    QApplication::sendEvent(&tabs, &smallHeaderWheel);
    QApplication::processEvents();
    EXPECT_EQ(tabs.visibleTabIndexes(), visibleBeforeWheel);
    FLUENT_MAKE_WHEEL_EVENT(headerWheel, wheelPoint.x(), wheelPoint.y(), 120, Qt::NoModifier);
    QApplication::sendEvent(&tabs, &headerWheel);
    QApplication::processEvents();
    EXPECT_NE(tabs.visibleTabIndexes(), visibleBeforeWheel);
    EXPECT_TRUE(tabs.tabGeometry(99).isEmpty());
    EXPECT_TRUE(tabs.closeButtonGeometry(99).isEmpty());

    TabView scrollTabs(window);
    scrollTabs.resize(560, 40);
    scrollTabs.setTabWidthMode(TabView::TabWidthMode::SizeToContent);
    for (int index = 0; index < 8; ++index)
        scrollTabs.addTab(TabViewItem(QStringLiteral("Doc %1").arg(index), Typography::Icons::Document));
    showAndProcess(scrollTabs);
    scrollTabs.setSelectedIndex(6);
    QApplication::processEvents();
    const QVector<int> visibleBeforeSelect = scrollTabs.visibleTabIndexes();
    ASSERT_GT(visibleBeforeSelect.size(), 1);
    const int visibleTarget = visibleBeforeSelect.first() == scrollTabs.selectedIndex() ? visibleBeforeSelect.last() : visibleBeforeSelect.first();
    QTest::mouseClick(&scrollTabs, Qt::LeftButton, Qt::NoModifier, scrollTabs.tabGeometry(visibleTarget).center());
    QApplication::processEvents();
    EXPECT_EQ(scrollTabs.selectedIndex(), visibleTarget);
    EXPECT_EQ(scrollTabs.visibleTabIndexes(), visibleBeforeSelect);
}

TEST_F(TabViewTest, PointerAddCloseSelectDisabledAndReorderBehavior)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    TabView tabs(window);
    tabs.resize(760, 320);
    tabs.addTab(QStringLiteral("One"));
    tabs.addTab(QStringLiteral("Two"));
    tabs.addTab(QStringLiteral("Three"));
    tabs.setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
    showAndProcess(tabs);

    QSignalSpy selectedSpy(&tabs, &TabView::selectedIndexChanged);
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.tabGeometry(1).center());
    EXPECT_EQ(tabs.selectedIndex(), 1);
    EXPECT_EQ(selectedSpy.count(), 1);

    tabs.setTabEnabled(2, false);
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.tabGeometry(2).center());
    EXPECT_EQ(tabs.selectedIndex(), 1);

    QSignalSpy addSpy(&tabs, &TabView::addTabRequested);
    QObject::connect(&tabs, &TabView::addTabRequested, &tabs, [&tabs]() {
        tabs.addTab(QStringLiteral("Added"));
    });
    const int selectedBeforeAdd = tabs.selectedIndex();
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.addButtonGeometry().center());
    EXPECT_EQ(addSpy.count(), 1);
    EXPECT_EQ(tabs.tabCount(), 4);
    EXPECT_EQ(tabs.selectedIndex(), selectedBeforeAdd);

    QSignalSpy closeSpy(&tabs, &TabView::tabCloseRequested);
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.closeButtonGeometry(1).center());
    ASSERT_EQ(closeSpy.count(), 1);
    EXPECT_EQ(closeSpy.takeFirst().at(0).toInt(), 1);
    EXPECT_EQ(tabs.selectedIndex(), 1);

    tabs.setTabClosable(1, false);
    QTest::mouseClick(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.closeButtonGeometry(1).center());
    EXPECT_EQ(closeSpy.count(), 0);
    EXPECT_FALSE(tabs.closeTab(1));
    tabs.setTabClosable(1, true);
    EXPECT_TRUE(tabs.closeTab(1));
    EXPECT_EQ(tabs.tabCount(), 3);

    tabs.clearTabs();
    tabs.addTab(QStringLiteral("A"));
    tabs.addTab(QStringLiteral("B"));
    tabs.addTab(QStringLiteral("C"));
    tabs.setTabReorderEnabled(true);
    QApplication::processEvents();
    QSignalSpy movedSpy(&tabs, &TabView::tabMoved);
    const QPoint firstTabCenter = tabs.tabGeometry(0).center();
    const QPoint beforeSecondMidpoint = QPoint(tabs.tabGeometry(1).left() + 6, tabs.tabGeometry(1).center().y());
    QTest::mousePress(&tabs, Qt::LeftButton, Qt::NoModifier, firstTabCenter);
    QTest::mouseMove(&tabs, firstTabCenter + QPoint(QApplication::startDragDistance() + 4, 0), 50);
    QTest::mouseMove(&tabs, beforeSecondMidpoint, 50);
    QTest::mouseRelease(&tabs, Qt::LeftButton, Qt::NoModifier, beforeSecondMidpoint);
    EXPECT_EQ(movedSpy.count(), 0);
    EXPECT_EQ(tabs.tabAt(0).text, QStringLiteral("A"));

    const QPoint from = tabs.tabGeometry(0).center();
    const QPoint to = QPoint(tabs.tabGeometry(2).right() + 36, tabs.tabGeometry(2).center().y());
    QTest::mousePress(&tabs, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(&tabs, from + QPoint(QApplication::startDragDistance() + 4, 0), 50);
    QApplication::processEvents();
    EXPECT_EQ(movedSpy.count(), 0);
    QTest::mouseMove(&tabs, to, 50);
    QTest::mouseRelease(&tabs, Qt::LeftButton, Qt::NoModifier, to);
    ASSERT_EQ(movedSpy.count(), 1);
    EXPECT_EQ(tabs.tabAt(2).text, QStringLiteral("A"));

    tabs.setTabReorderEnabled(false);
    QTest::mousePress(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.tabGeometry(0).center());
    QTest::mouseMove(&tabs, tabs.tabGeometry(1).center(), 50);
    QTest::mouseRelease(&tabs, Qt::LeftButton, Qt::NoModifier, tabs.tabGeometry(1).center());
    EXPECT_EQ(movedSpy.count(), 1);
}

TEST_F(TabViewTest, KeyboardAcceleratorsFocusAndThemeAccessibilityStayStable)
{
    TabView tabs(window);
    tabs.resize(760, 320);
    tabs.addTab(QStringLiteral("One"));
    tabs.addTab(QStringLiteral("Two"));
    tabs.addTab(QStringLiteral("Three"));
    showAndProcess(tabs);

    QSignalSpy addSpy(&tabs, &TabView::addTabRequested);
    QSignalSpy closeSpy(&tabs, &TabView::tabCloseRequested);
    tabs.setFocus();
    QApplication::processEvents();
    QTest::keyClick(&tabs, Qt::Key_Right);
    QTest::keyClick(&tabs, Qt::Key_Space);
    EXPECT_EQ(tabs.selectedIndex(), 1);

    QTest::keyClick(&tabs, Qt::Key_Home);
    QTest::keyClick(&tabs, Qt::Key_Return);
    EXPECT_EQ(tabs.selectedIndex(), 0);
    QTest::keyClick(&tabs, Qt::Key_End);
    QTest::keyClick(&tabs, Qt::Key_Return);
    EXPECT_EQ(tabs.selectedIndex(), 2);

    QTest::keyClick(&tabs, Qt::Key_T, Qt::ControlModifier);
    EXPECT_EQ(addSpy.count(), 1);
    QTest::keyClick(&tabs, Qt::Key_W, Qt::ControlModifier);
    QTest::keyClick(&tabs, Qt::Key_Delete);
    EXPECT_EQ(closeSpy.count(), 2);
    QTest::keyClick(&tabs, Qt::Key_1, Qt::ControlModifier);
    EXPECT_EQ(tabs.selectedIndex(), 0);
    QTest::keyClick(&tabs, Qt::Key_9, Qt::ControlModifier);
    EXPECT_EQ(tabs.selectedIndex(), 2);
    QTest::keyClick(&tabs, Qt::Key_8, Qt::ControlModifier);
    EXPECT_EQ(tabs.selectedIndex(), 2);

    TabView shortcutTabs(window);
    shortcutTabs.resize(760, 40);
    shortcutTabs.addTab(TabViewItem(QStringLiteral("Shortcut A"), Typography::Icons::Document));
    shortcutTabs.addTab(TabViewItem(QStringLiteral("Shortcut B"), Typography::Icons::Document));
    shortcutTabs.addTab(TabViewItem(QStringLiteral("Shortcut C"), Typography::Icons::Document));
    showAndProcess(shortcutTabs);
    QSignalSpy shortcutAddSpy(&shortcutTabs, &TabView::addTabRequested);
    QSignalSpy shortcutCloseSpy(&shortcutTabs, &TabView::tabCloseRequested);
    QObject::connect(&shortcutTabs, &TabView::addTabRequested, &shortcutTabs, [&shortcutTabs]() {
        shortcutTabs.addTab(TabViewItem(QStringLiteral("Added"), Typography::Icons::Document));
    });
    QObject::connect(&shortcutTabs, &TabView::tabCloseRequested, &shortcutTabs, [&shortcutTabs](int index) {
        shortcutTabs.closeTab(index);
    });
    shortcutTabs.setFocus();
    const int countBeforeShortcutAdd = shortcutTabs.tabCount();
    QTest::keyClick(&shortcutTabs, Qt::Key_T, Qt::ControlModifier);
    EXPECT_EQ(shortcutAddSpy.count(), 1);
    EXPECT_EQ(shortcutTabs.tabCount(), countBeforeShortcutAdd + 1);
    shortcutTabs.setSelectedIndex(1);
    QTest::keyClick(&shortcutTabs, Qt::Key_W, Qt::ControlModifier);
    EXPECT_EQ(shortcutCloseSpy.count(), 1);
    EXPECT_EQ(shortcutTabs.tabCount(), countBeforeShortcutAdd);
    EXPECT_NE(shortcutTabs.tabAt(1).text, QStringLiteral("Shortcut B"));

    auto* focusSink = new Button(QStringLiteral("Focus sink"), window);
    focusSink->setFixedSize(120, 32);
    showAndProcess(*focusSink);
    focusSink->setFocus();
    QApplication::processEvents();
    QTest::keyClick(focusSink, Qt::Key_T, Qt::ControlModifier);
    EXPECT_EQ(shortcutAddSpy.count(), 2);
    EXPECT_EQ(shortcutTabs.tabCount(), countBeforeShortcutAdd + 1);

    const int addCountBeforeDisabled = addSpy.count();
    const int closeCountBeforeDisabled = closeSpy.count();
    tabs.setKeyboardAcceleratorsEnabled(false);
    QTest::keyClick(&tabs, Qt::Key_T, Qt::ControlModifier);
    QTest::keyClick(&tabs, Qt::Key_W, Qt::ControlModifier);
    QTest::keyClick(&tabs, Qt::Key_Delete);
    EXPECT_EQ(addSpy.count(), addCountBeforeDisabled);
    EXPECT_EQ(closeSpy.count(), closeCountBeforeDisabled);

    tabs.setSelectedIndex(1);
    EXPECT_TRUE(tabs.accessibleDescription().contains(QStringLiteral("Selected tab: Two")));
    const QVector<TabViewItem> before = tabs.tabs();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    tabs.onThemeUpdated();
    QApplication::processEvents();
    EXPECT_EQ(tabs.tabCount(), before.size());
    EXPECT_EQ(tabs.selectedIndex(), 1);
    EXPECT_EQ(tabs.tabAt(1).text, before.at(1).text);
    EXPECT_FALSE(tabs.tabGeometry(1).isEmpty());
}

TEST_F(TabViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    auto* visual = new TabViewTestWindow();
    visual->setAttribute(Qt::WA_DeleteOnClose);
    visual->resize(1080, 720);
    visual->setWindowTitle(QStringLiteral("TabView VisualCheck"));
    visual->onThemeUpdated();

    auto* visualLayout = new AnchorLayout(visual);
    visual->setLayout(visualLayout);

    auto* scrollView = new ScrollView(visual);
    scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Hidden);
    scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
    scrollView->anchors()->top = {visual, Edge::Top, 0};
    scrollView->anchors()->left = {visual, Edge::Left, 0};
    scrollView->anchors()->right = {visual, Edge::Right, 0};
    scrollView->anchors()->bottom = {visual, Edge::Bottom, 0};
    addAnchored(visualLayout, scrollView);

    auto* content = new TabViewTestWindow();
    content->resize(1016, 2200);
    content->setMinimumSize(1016, 2200);
    content->onThemeUpdated();
    auto* contentLayout = new AnchorLayout(content);
    content->setLayout(contentLayout);
    scrollView->setWidget(content);

    auto* title = new Label(QStringLiteral("TabView"), content);
    title->setFluentTypography(QStringLiteral("Subtitle"));
    title->anchors()->top = {content, Edge::Top, 24};
    title->anchors()->left = {content, Edge::Left, 32};
    addAnchored(contentLayout, title);

    auto* themeButton = new Button(QStringLiteral("Dark"), content);
    themeButton->setFixedSize(96, 32);
    themeButton->anchors()->top = {content, Edge::Top, 24};
    themeButton->anchors()->right = {content, Edge::Right, -32};
    addAnchored(contentLayout, themeButton);

    struct DemoSection {
        TabView* tabs = nullptr;
        StackContentHost* host = nullptr;
    };

    auto addSection = [content, contentLayout](QWidget* topAnchor, int topMargin, int hostHeight, const QString& pagePrefix, auto configureTabs) -> DemoSection {
        auto* tabs = new TabView(content);
        configureTabs(tabs);
        tabs->setFixedHeight(40);
        tabs->anchors()->top = {topAnchor, Edge::Bottom, topMargin};
        tabs->anchors()->left = {content, Edge::Left, 32};
        tabs->anchors()->right = {content, Edge::Right, -32};
        addAnchored(contentLayout, tabs);

        auto* host = createBoundHost(tabs, content, pagePrefix);
        host->setFixedHeight(hostHeight);
        host->anchors()->top = {tabs, Edge::Bottom, 0};
        host->anchors()->left = {tabs, Edge::Left, 0};
        host->anchors()->right = {tabs, Edge::Right, 0};
        addAnchored(contentLayout, host);
        return {tabs, host};
    };

    const DemoSection workspace = addSection(title, 24, 220, QStringLiteral("Workspace"), [](TabView* tabs) {
        tabs->addTab(TabViewItem(QStringLiteral("Home.cpp"), Typography::Icons::Document));
        tabs->addTab(TabViewItem(QStringLiteral("Notes.md"), Typography::Icons::Edit));
        tabs->addTab(TabViewItem(QStringLiteral("Locked"), Typography::Icons::Lock, true, false));
        tabs->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Auto);
        tabs->setTabReorderEnabled(true);
    });

    const DemoSection alwaysClose = addSection(workspace.host, 28, 184, QStringLiteral("Always close"), [](TabView* tabs) {
        tabs->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
        tabs->addTab(TabViewItem(QStringLiteral("Primary"), Typography::Icons::AppIconDefault));
        tabs->addTab(TabViewItem(QStringLiteral("Always close"), Typography::Icons::Document));
        tabs->addTab(TabViewItem(QStringLiteral("Reference"), Typography::Icons::Document));
    });

    const DemoSection compact = addSection(alwaysClose.host, 28, 184, QStringLiteral("Compact"), [](TabView* tabs) {
        tabs->setTabWidthMode(TabView::TabWidthMode::Compact);
        tabs->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::OnHover);
        tabs->addTab(TabViewItem(QString(), Typography::Icons::Calendar, true, true, QVariant(), QStringLiteral("Calendar")));
        tabs->addTab(TabViewItem(QStringLiteral("Mail"), Typography::Icons::Mail));
        tabs->addTab(TabViewItem(QString(), Typography::Icons::Settings, true, true, QVariant(), QStringLiteral("Settings")));
        tabs->addTab(TabViewItem(QString(), Typography::Icons::Document, true, true, QVariant(), QStringLiteral("Document")));
        tabs->setSelectedIndex(1);
    });

    const DemoSection overflow = addSection(compact.host, 28, 184, QStringLiteral("Overflow"), [](TabView* tabs) {
        tabs->setTabWidthMode(TabView::TabWidthMode::SizeToContent);
        for (int index = 1; index <= 10; ++index)
            tabs->addTab(TabViewItem(QStringLiteral("Document %1 with longer title").arg(index), Typography::Icons::Document));
        tabs->setSelectedIndex(7);
    });

    const DemoSection pinned = addSection(overflow.host, 28, 184, QStringLiteral("Pinned"), [](TabView* tabs) {
        tabs->setAddTabButtonVisible(false);
        tabs->setTabsClosable(false);
        tabs->addTab(TabViewItem(QStringLiteral("Pinned"), Typography::Icons::Pin, false));
        tabs->addTab(TabViewItem(QStringLiteral("Read only"), Typography::Icons::Document, false));
        tabs->addTab(TabViewItem(QStringLiteral("Protected"), Typography::Icons::Lock, false));
    });

    const DemoSection equal = addSection(pinned.host, 28, 184, QStringLiteral("Equal"), [](TabView* tabs) {
        tabs->setTabWidthMode(TabView::TabWidthMode::Equal);
        tabs->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::Always);
        tabs->addTab(TabViewItem(QStringLiteral("Equal A"), Typography::Icons::Document));
        tabs->addTab(TabViewItem(QStringLiteral("Equal B"), Typography::Icons::Document));
        tabs->addTab(TabViewItem(QStringLiteral("Equal C"), Typography::Icons::Document));
    });

    const DemoSection hover = addSection(equal.host, 28, 184, QStringLiteral("Hover"), [](TabView* tabs) {
        tabs->setTabWidthMode(TabView::TabWidthMode::SizeToContent);
        tabs->setCloseButtonOverlayMode(TabView::CloseButtonOverlayMode::OnHover);
        tabs->setKeyboardAcceleratorsEnabled(false);
        tabs->addTab(TabViewItem(QStringLiteral("Hover close"), Typography::Icons::Document));
        tabs->addTab(TabViewItem(QStringLiteral("Keyboard off"), Typography::Icons::Keyboard));
        tabs->addTab(TabViewItem(QStringLiteral("Disabled"), Typography::Icons::Lock, true, false));
    });

    const DemoSection keyboard = addSection(hover.host, 28, 184, QStringLiteral("Keyboard"), [](TabView* tabs) {
        tabs->setTabReorderEnabled(true);
        for (int index = 0; index < 10; ++index)
            tabs->addTab(TabViewItem(QStringLiteral("Document %1").arg(index), Typography::Icons::Document));
        tabs->setSelectedIndex(3);
    });

    const QVector<TabView*> tabViews = {workspace.tabs, alwaysClose.tabs, compact.tabs, overflow.tabs, pinned.tabs, equal.tabs, hover.tabs, keyboard.tabs};
    const QVector<StackContentHost*> hosts = {workspace.host, alwaysClose.host, compact.host, overflow.host, pinned.host, equal.host, hover.host, keyboard.host};
    QObject::connect(themeButton, &Button::clicked, [visual, content, themeButton, title, tabViews, hosts]() {
        const bool useDark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light;
        fluent::FluentElement::setTheme(useDark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
        visual->onThemeUpdated();
        content->onThemeUpdated();
        title->onThemeUpdated();
        for (TabView* tabs : tabViews)
            tabs->onThemeUpdated();
        for (StackContentHost* host : hosts)
            host->onThemeUpdated();
        themeButton->setText(useDark ? QStringLiteral("Light") : QStringLiteral("Dark"));
    });

    visual->show();
    qApp->exec();
}

// Headless render coverage for the brand-specific tab treatments. Each case sets a
// (design language, theme) pair on the global ThemeRegistry / FluentElement, builds a TabView with
// several tabs + a selected one, grabs it to a QImage, and asserts it renders without crashing and
// actually paints content. Globals are reset in TearDown so a brand/theme never leaks into the other
// suites. zh_CN: 品牌专属 tab 绘制的无头渲染覆盖。每个用例设置一对(设计语言, 主题)到全局
// ThemeRegistry / FluentElement，构建带多个 tab 及一个选中项的 TabView，将其抓取为 QImage，断言无崩溃
// 且确有绘制内容。TearDown 中重置全局，避免品牌/主题泄漏到其它套件。
class TabViewDesignLanguageTest
    : public ::testing::TestWithParam<std::pair<fluent::FluentElement::DesignLanguage,
                                                fluent::FluentElement::Theme>> {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::navigation::TabViewItem>("fluent::navigation::TabViewItem");
        qRegisterMetaType<fluent::navigation::TabView::TabWidthMode>(
            "fluent::navigation::TabView::TabWidthMode");
        qRegisterMetaType<fluent::navigation::TabView::CloseButtonOverlayMode>(
            "fluent::navigation::TabView::CloseButtonOverlayMode");
    }

    void SetUp() override
    {
        const auto [lang, theme] = GetParam();
        fluent::ThemeRegistry::instance().setDesignLanguage(lang);
        fluent::FluentElement::setTheme(theme);
    }

    void TearDown() override
    {
        // CRITICAL: reset globals so other suites see Fluent + Light. zh_CN: 关键：重置全局，使其它套件回到 Fluent + Light。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static TabView* makeTabView()
    {
        auto* tabs = new TabView();
        tabs->addTab(TabViewItem(QStringLiteral("Home.cpp"), Typography::Icons::Document));
        tabs->addTab(TabViewItem(QStringLiteral("Notes.md"), Typography::Icons::Edit));
        tabs->addTab(TabViewItem(QStringLiteral("Readme"), Typography::Icons::Document));
        tabs->setSelectedIndex(1);
        tabs->resize(500, 200);
        return tabs;
    }

    // True if any pixel differs from the top-left background pixel (i.e. something was painted).
    // zh_CN: 若有任一像素不同于左上角背景像素则为真（即确有绘制）。
    static bool hasPaintedContent(const QImage& img)
    {
        if (img.isNull() || img.width() < 2 || img.height() < 2)
            return false;
        const QRgb background = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                if (img.pixel(x, y) != background)
                    return true;
            }
        }
        return false;
    }
};

TEST_P(TabViewDesignLanguageTest, RendersTabsWithoutCrashingAndPaintsContent)
{
    std::unique_ptr<TabView> tabs(makeTabView());

    QImage img = tabs->grab().toImage();
    ASSERT_FALSE(img.isNull());
    EXPECT_EQ(img.width(), 500);
    EXPECT_EQ(img.height(), 200);
    EXPECT_TRUE(hasPaintedContent(img)) << "Expected the TabView to paint tab labels/indicator.";
}

TEST_P(TabViewDesignLanguageTest, ChangingSelectionRepaintsTabs)
{
    std::unique_ptr<TabView> tabs(makeTabView());

    const QImage first = tabs->grab().toImage();
    ASSERT_FALSE(first.isNull());

    tabs->setSelectedIndex(2);

    // The selection indicator ANIMATES and the header labels recolor asynchronously, so a single
    // processEvents() can grab before anything visibly moved (this is timing, not a missing repaint —
    // even the unchanged DesignFluent path needs the animation to advance). Pump the event loop and
    // re-grab until the image differs (or a generous timeout), which is robust to the animation length.
    // zh_CN: 选中指示条是动画的、表头标签异步重着色,单次 processEvents 可能在视觉变化前就抓帧(这是时序
    // 问题,并非漏重绘——连未改动的 Fluent 路径也要等动画推进)。泵事件循环并重抓,直到图像变化或超时。
    QImage second = tabs->grab().toImage();
    ASSERT_FALSE(second.isNull());
    for (int i = 0; i < 80 && first == second; ++i) {
        QTest::qWait(10);
        second = tabs->grab().toImage();
    }

    // Selection moved the accent text/indicator/segment, so the rendered image must change.
    // zh_CN: 选中项移动了强调文字/指示条/段背景，渲染图像必须随之变化。
    EXPECT_NE(first, second);
}

INSTANTIATE_TEST_SUITE_P(
    AllDesignLanguagesAndThemes,
    TabViewDesignLanguageTest,
    ::testing::Values(
        std::make_pair(fluent::FluentElement::DesignFluent, fluent::FluentElement::Light),
        std::make_pair(fluent::FluentElement::DesignFluent, fluent::FluentElement::Dark),
        std::make_pair(fluent::FluentElement::DesignMaterial, fluent::FluentElement::Light),
        std::make_pair(fluent::FluentElement::DesignMaterial, fluent::FluentElement::Dark),
        std::make_pair(fluent::FluentElement::DesignCupertino, fluent::FluentElement::Light),
        std::make_pair(fluent::FluentElement::DesignCupertino, fluent::FluentElement::Dark)));
