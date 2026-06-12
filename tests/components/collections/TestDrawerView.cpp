#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/collections/DrawerView.h"
#include "components/textfields/Label.h"

using fluent::basicinput::Button;
using fluent::basicinput::ToggleSwitch;
using fluent::collections::DrawerView;
using fluent::textfields::Label;

namespace {

using Edge = fluent::AnchorLayout::Edge;

class DrawerTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(palette);
        setAutoFillBackground(true);
    }
};

class ContentPane : public QWidget, public fluent::FluentElement {
public:
    explicit ContentPane(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(80, 60);
        setAutoFillBackground(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        auto* label = new Label(title, this);
        label->setFluentTypography(QStringLiteral("Subtitle"));
        layout->addWidget(label);
        layout->addStretch();
        onThemeUpdated();
    }

    QSize sizeHint() const override { return QSize(220, 180); }
    QSize minimumSizeHint() const override { return QSize(90, 70); }

    void onThemeUpdated() override
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, themeColors().bgLayerAlt);
        setPalette(palette);
    }
};

class DrawerDemoContent : public QWidget {
public:
    explicit DrawerDemoContent(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(260, 210);

        auto* layout = new fluent::AnchorLayout();
        setLayout(layout);

        auto* title = new Label(QStringLiteral("Drawer actions"), this);
        title->setFluentTypography(QStringLiteral("Subtitle"));

        auto* description = new Label(QStringLiteral("Adjust quick settings, then apply or reset the drawer state."), this);
        description->setWordWrap(true);
        description->setTextElideMode(Qt::ElideRight);

        auto* notificationLabel = new Label(QStringLiteral("Notifications"), this);
        auto* notificationSwitch = new ToggleSwitch(this);
        notificationSwitch->setOnContent(QStringLiteral("On"));
        notificationSwitch->setOffContent(QStringLiteral("Off"));
        notificationSwitch->setIsOn(true);

        auto* compactLabel = new Label(QStringLiteral("Compact mode"), this);
        auto* compactSwitch = new ToggleSwitch(this);
        compactSwitch->setOnContent(QStringLiteral("On"));
        compactSwitch->setOffContent(QStringLiteral("Off"));

        auto* status = new Label(QStringLiteral("Ready"), this);
        status->setTextElideMode(Qt::ElideRight);

        auto* resetButton = new Button(QStringLiteral("Reset"), this);
        resetButton->setFixedSize(76, 32);

        auto* applyButton = new Button(QStringLiteral("Apply"), this);
        applyButton->setFluentStyle(Button::Accent);
        applyButton->setFixedSize(76, 32);

        fluent::AnchorLayout::Anchors titleAnchors;
        titleAnchors.left = {this, Edge::Left, 20};
        titleAnchors.top = {this, Edge::Top, 18};
        titleAnchors.right = {this, Edge::Right, -20};
        layout->addAnchoredWidget(title, titleAnchors);

        fluent::AnchorLayout::Anchors descriptionAnchors;
        descriptionAnchors.left = {this, Edge::Left, 20};
        descriptionAnchors.top = {title, Edge::Bottom, 6};
        descriptionAnchors.right = {this, Edge::Right, -20};
        layout->addAnchoredWidget(description, descriptionAnchors);

        fluent::AnchorLayout::Anchors notificationLabelAnchors;
        notificationLabelAnchors.left = {this, Edge::Left, 20};
        notificationLabelAnchors.top = {description, Edge::Bottom, 16};
        layout->addAnchoredWidget(notificationLabel, notificationLabelAnchors);

        fluent::AnchorLayout::Anchors notificationAnchors;
        notificationAnchors.left = {notificationLabel, Edge::Left, 0};
        notificationAnchors.top = {notificationLabel, Edge::Bottom, 4};
        layout->addAnchoredWidget(notificationSwitch, notificationAnchors);

        fluent::AnchorLayout::Anchors compactLabelAnchors;
        compactLabelAnchors.left = {notificationSwitch, Edge::Left, 0};
        compactLabelAnchors.top = {notificationSwitch, Edge::Bottom, 12};
        layout->addAnchoredWidget(compactLabel, compactLabelAnchors);

        fluent::AnchorLayout::Anchors compactAnchors;
        compactAnchors.left = {compactLabel, Edge::Left, 0};
        compactAnchors.top = {compactLabel, Edge::Bottom, 4};
        layout->addAnchoredWidget(compactSwitch, compactAnchors);

        fluent::AnchorLayout::Anchors applyAnchors;
        applyAnchors.right = {this, Edge::Right, -20};
        applyAnchors.bottom = {this, Edge::Bottom, -18};
        layout->addAnchoredWidget(applyButton, applyAnchors);

        fluent::AnchorLayout::Anchors resetAnchors;
        resetAnchors.right = {applyButton, Edge::Left, -8};
        resetAnchors.bottom = {applyButton, Edge::Bottom, 0};
        layout->addAnchoredWidget(resetButton, resetAnchors);

        fluent::AnchorLayout::Anchors statusAnchors;
        statusAnchors.left = {this, Edge::Left, 20};
        statusAnchors.right = {resetButton, Edge::Left, -12};
        statusAnchors.verticalCenter = {applyButton, Edge::VCenter, 0};
        layout->addAnchoredWidget(status, statusAnchors);

        auto updateStatus = [status, notificationSwitch, compactSwitch]() {
            status->setText(QStringLiteral("%1 / %2")
                                .arg(notificationSwitch->isOn() ? QStringLiteral("Notify on") : QStringLiteral("Notify off"))
                                .arg(compactSwitch->isOn() ? QStringLiteral("Compact") : QStringLiteral("Detailed")));
        };

        QObject::connect(notificationSwitch, &ToggleSwitch::toggled, status, updateStatus);
        QObject::connect(compactSwitch, &ToggleSwitch::toggled, status, updateStatus);
        QObject::connect(applyButton, &Button::clicked, status, [status]() {
            status->setText(QStringLiteral("Applied"));
        });
        QObject::connect(resetButton, &Button::clicked, status, [notificationSwitch, compactSwitch, updateStatus]() {
            notificationSwitch->setIsOn(true);
            compactSwitch->setIsOn(false);
            updateStatus();
        });

        updateStatus();
    }

    QSize sizeHint() const override { return QSize(320, 260); }
    QSize minimumSizeHint() const override { return QSize(260, 210); }
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

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

void prepareWindow(DrawerTestWindow& window, const QSize& size = QSize(800, 600))
{
    window.resize(size);
    window.onThemeUpdated();
    showAndProcess(window);
}

void openWithoutAnimation(DrawerView& drawer)
{
    drawer.setAnimationEnabled(false);
    drawer.open();
    processEvents();
}

void sendMouse(QWidget* target, QEvent::Type type, const QPoint& localPos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    FLUENT_MAKE_MOUSE_EVENT(event, type, target, localPos, button, buttons, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
    processEvents();
}

int renderedCenterAlpha(QWidget* widget)
{
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    painter.end();
    return QColor::fromRgba(image.pixel(image.width() / 2, image.height() / 2)).alpha();
}

int renderedAlphaAt(QWidget* widget, const QPoint& point)
{
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    painter.end();
    return QColor::fromRgba(image.pixel(point)).alpha();
}

} // namespace

class DrawerViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::collections::DrawerView::DrawerEdge>(
            "fluent::collections::DrawerView::DrawerEdge");
        qRegisterMetaType<fluent::collections::DrawerView::ClosePolicy>(
            "fluent::collections::DrawerView::ClosePolicy");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(DrawerViewTest, DefaultPropertiesAndInheritance)
{
    DrawerView drawer;

    EXPECT_FALSE(drawer.isOpen());
    EXPECT_EQ(drawer.position(), 0.0);
    EXPECT_EQ(drawer.edge(), DrawerView::DrawerEdge::Left);
    EXPECT_GT(drawer.drawerLength(), 0);
    EXPECT_TRUE(drawer.isModal());
    EXPECT_TRUE(drawer.isDim());
    EXPECT_TRUE(drawer.isInteractive());
    EXPECT_TRUE(drawer.isAnimationEnabled());
    EXPECT_EQ(drawer.outerCornerRadius(), 8);
    EXPECT_TRUE(drawer.closePolicy() & DrawerView::CloseOnPressOutside);
    EXPECT_TRUE(drawer.closePolicy() & DrawerView::CloseOnEscape);
    EXPECT_FALSE(drawer.isVisible());
    EXPECT_FALSE(drawer.sizeHint().isEmpty());
    EXPECT_FALSE(drawer.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&drawer), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&drawer), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&drawer), nullptr);
}

TEST_F(DrawerViewTest, DuplicatePropertyValuesSuppressSignals)
{
    DrawerView drawer;
    QSignalSpy edgeSpy(&drawer, &DrawerView::edgeChanged);
    QSignalSpy lengthSpy(&drawer, &DrawerView::drawerLengthChanged);
    QSignalSpy positionSpy(&drawer, &DrawerView::positionChanged);
    QSignalSpy modalSpy(&drawer, &DrawerView::modalChanged);
    QSignalSpy dimSpy(&drawer, &DrawerView::dimChanged);
    QSignalSpy interactiveSpy(&drawer, &DrawerView::interactiveChanged);
    QSignalSpy dragMarginSpy(&drawer, &DrawerView::dragMarginChanged);
    QSignalSpy radiusSpy(&drawer, &DrawerView::outerCornerRadiusChanged);

    drawer.setEdge(DrawerView::DrawerEdge::Right);
    drawer.setEdge(DrawerView::DrawerEdge::Right);
    EXPECT_EQ(edgeSpy.count(), 1);

    drawer.setDrawerLength(280);
    drawer.setDrawerLength(280);
    EXPECT_EQ(lengthSpy.count(), 1);

    drawer.setPosition(0.25);
    drawer.setPosition(0.25);
    EXPECT_EQ(positionSpy.count(), 1);

    drawer.setModal(false);
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setDim(false);
    drawer.setInteractive(false);
    drawer.setInteractive(false);
    drawer.setDragMargin(-10);
    drawer.setDragMargin(0);
    drawer.setOuterCornerRadius(12);
    drawer.setOuterCornerRadius(12);
    drawer.setOuterCornerRadius(-4);
    drawer.setOuterCornerRadius(0);
    EXPECT_EQ(modalSpy.count(), 1);
    EXPECT_EQ(dimSpy.count(), 1);
    EXPECT_EQ(interactiveSpy.count(), 1);
    EXPECT_EQ(drawer.dragMargin(), 0);
    EXPECT_EQ(dragMarginSpy.count(), 1);
    EXPECT_EQ(drawer.outerCornerRadius(), 0);
    EXPECT_EQ(radiusSpy.count(), 2);
}

TEST_F(DrawerViewTest, AnimationDisabledOpenCloseLifecycleAndAttachment)
{
    DrawerTestWindow window;
    prepareWindow(window);
    DrawerView drawer(&window);
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setAnimationEnabled(false);

    QSignalSpy aboutToShowSpy(&drawer, &DrawerView::aboutToShow);
    QSignalSpy openedSpy(&drawer, &DrawerView::opened);
    QSignalSpy openChangedSpy(&drawer, &DrawerView::isOpenChanged);
    QSignalSpy aboutToHideSpy(&drawer, &DrawerView::aboutToHide);
    QSignalSpy closedSpy(&drawer, &DrawerView::closed);

    drawer.open();
    processEvents();

    EXPECT_EQ(drawer.parentWidget(), &window);
    EXPECT_FALSE(drawer.isWindow());
    EXPECT_NE(drawer.windowType(), Qt::Dialog);
    EXPECT_TRUE(drawer.isVisible());
    EXPECT_TRUE(drawer.isOpen());
    EXPECT_EQ(drawer.position(), 1.0);
    EXPECT_EQ(drawer.panelGeometry(), QRect(0, 0, drawer.drawerLength(), window.height()));
    EXPECT_EQ(aboutToShowSpy.count(), 1);
    EXPECT_EQ(openedSpy.count(), 1);
    ASSERT_EQ(openChangedSpy.count(), 1);
    EXPECT_TRUE(openChangedSpy.at(0).at(0).toBool());

    drawer.close();
    processEvents();

    EXPECT_FALSE(drawer.isVisible());
    EXPECT_FALSE(drawer.isOpen());
    EXPECT_EQ(drawer.position(), 0.0);
    EXPECT_EQ(aboutToHideSpy.count(), 1);
    EXPECT_EQ(closedSpy.count(), 1);
    ASSERT_EQ(openChangedSpy.count(), 2);
    EXPECT_FALSE(openChangedSpy.at(1).at(0).toBool());
}

TEST_F(DrawerViewTest, EdgeGeometryAndPartialPosition)
{
    DrawerTestWindow window;
    prepareWindow(window);
    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setDrawerLength(320);
    drawer.open();
    processEvents();

    EXPECT_EQ(drawer.panelGeometry(), QRect(0, 0, 320, 600));

    drawer.setEdge(DrawerView::DrawerEdge::Right);
    EXPECT_EQ(drawer.panelGeometry(), QRect(480, 0, 320, 600));

    drawer.setDrawerLength(240);
    drawer.setEdge(DrawerView::DrawerEdge::Top);
    EXPECT_EQ(drawer.panelGeometry(), QRect(0, 0, 800, 240));

    drawer.setEdge(DrawerView::DrawerEdge::Bottom);
    EXPECT_EQ(drawer.panelGeometry(), QRect(0, 360, 800, 240));

    drawer.setEdge(DrawerView::DrawerEdge::Left);
    drawer.setDrawerLength(320);
    drawer.setPosition(0.5);
    EXPECT_EQ(drawer.panelGeometry(), QRect(-160, 0, 320, 600));
    EXPECT_EQ(drawer.geometry(), drawer.panelGeometry());
}

TEST_F(DrawerViewTest, DrawerLengthClampsToAvailableGeometryAndResizeRecomputes)
{
    DrawerTestWindow window;
    prepareWindow(window);
    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setDrawerLength(1200);
    drawer.open();
    processEvents();

    EXPECT_EQ(drawer.drawerLength(), 1200);
    EXPECT_EQ(drawer.panelGeometry(), QRect(0, 0, 800, 600));

    window.resize(640, 480);
    processEvents();
    EXPECT_EQ(drawer.panelGeometry(), QRect(0, 0, 640, 480));

    drawer.setDrawerLength(-4);
    EXPECT_EQ(drawer.drawerLength(), 1);
}

TEST_F(DrawerViewTest, RoundedFloatingCornersStayTransparent)
{
    DrawerTestWindow window;
    prepareWindow(window, QSize(420, 300));
    ContentPane content(QStringLiteral("Rounded content"));
    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setDrawerLength(220);
    drawer.setContentWidget(&content);
    openWithoutAnimation(drawer);

    EXPECT_EQ(renderedAlphaAt(&drawer, QPoint(drawer.width() - 1, 0)), 0);
    EXPECT_EQ(renderedAlphaAt(&drawer, QPoint(drawer.width() - 1, drawer.height() - 1)), 0);

    drawer.setEdge(DrawerView::DrawerEdge::Bottom);
    drawer.setDrawerLength(140);
    processEvents();
    EXPECT_EQ(renderedAlphaAt(&drawer, QPoint(0, 0)), 0);
    EXPECT_EQ(renderedAlphaAt(&drawer, QPoint(drawer.width() - 1, 0)), 0);
}

TEST_F(DrawerViewTest, ContentWidgetHostingReplacesAndClearsWithoutDeleting)
{
    DrawerTestWindow window;
    prepareWindow(window);
    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);

    auto* first = new ContentPane(QStringLiteral("First"));
    auto* second = new ContentPane(QStringLiteral("Second"));
    QPointer<QWidget> firstPointer = first;
    QSignalSpy contentSpy(&drawer, &DrawerView::contentWidgetChanged);

    drawer.setContentWidget(first);
    openWithoutAnimation(drawer);
    EXPECT_EQ(drawer.contentWidget(), first);
    EXPECT_EQ(first->parentWidget(), &drawer);
    EXPECT_TRUE(first->isVisible());
    EXPECT_EQ(first->geometry(), drawer.contentGeometry());

    drawer.setContentWidget(second);
    EXPECT_EQ(drawer.contentWidget(), second);
    EXPECT_EQ(second->parentWidget(), &drawer);
    EXPECT_EQ(first->parentWidget(), nullptr);
    EXPECT_FALSE(firstPointer.isNull());
    EXPECT_EQ(second->geometry(), drawer.contentGeometry());

    drawer.setContentWidget(nullptr);
    EXPECT_EQ(drawer.contentWidget(), nullptr);
    EXPECT_EQ(second->parentWidget(), nullptr);
    EXPECT_FALSE(firstPointer.isNull());
    EXPECT_EQ(contentSpy.count(), 3);

    delete first;
    delete second;
}

TEST_F(DrawerViewTest, ModalDimAndClosePolicies)
{
    DrawerTestWindow window;
    prepareWindow(window);
    auto* background = new PressProbe();
    background->setParent(&window);
    background->setGeometry(window.rect());
    background->show();

    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setDrawerLength(280);
    drawer.setModal(true);
    drawer.setDim(true);
    drawer.open();
    processEvents();

    QWidget* childAtBackground = window.childAt(QPoint(500, 300));
    EXPECT_NE(childAtBackground, nullptr);
    EXPECT_NE(childAtBackground, background);
    EXPECT_NE(childAtBackground, &drawer);
    EXPECT_EQ(drawer.scrimGeometry(), window.rect());
    QPointer<QWidget> scrim = window.findChild<QWidget*>(QStringLiteral("DrawerViewScrim"));
    ASSERT_FALSE(scrim.isNull());

    drawer.close();
    processEvents();
    EXPECT_TRUE(scrim.isNull());
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setClosePolicy(DrawerView::ClosePolicy(DrawerView::CloseOnPressOutside | DrawerView::CloseOnEscape));
    drawer.open();
    processEvents();

    sendMouse(background, QEvent::MouseButtonPress, QPoint(500, 300), Qt::LeftButton, Qt::LeftButton);
    EXPECT_EQ(background->presses, 1);
    EXPECT_FALSE(drawer.isOpen());

    drawer.open();
    processEvents();
    QKeyEvent escapePress(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &escapePress);
    processEvents();
    EXPECT_FALSE(drawer.isOpen());

    drawer.setClosePolicy(DrawerView::ClosePolicy(DrawerView::NoAutoClose));
    drawer.open();
    processEvents();
    sendMouse(background, QEvent::MouseButtonPress, QPoint(500, 300), Qt::LeftButton, Qt::LeftButton);
    EXPECT_TRUE(drawer.isOpen());
    QKeyEvent secondEscape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&window, &secondEscape);
    processEvents();
    EXPECT_TRUE(drawer.isOpen());
}

TEST_F(DrawerViewTest, TopLevelResizeKeepsOverlayStackAboveSiblings)
{
    DrawerTestWindow window;
    prepareWindow(window);
    auto* sibling = new QWidget(&window);
    sibling->setGeometry(window.rect());
    sibling->show();

    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(true);
    drawer.setDim(true);
    drawer.setDrawerLength(260);
    openWithoutAnimation(drawer);

    sibling->raise();
    window.resize(840, 640);
    processEvents();

    EXPECT_EQ(window.childAt(QPoint(20, 20)), &drawer);
    QWidget* outsidePanelChild = window.childAt(QPoint(500, 300));
    EXPECT_NE(outsidePanelChild, nullptr);
    EXPECT_NE(outsidePanelChild, sibling);
    EXPECT_NE(outsidePanelChild, &drawer);
    EXPECT_EQ(drawer.scrimGeometry(), window.rect());
}

TEST_F(DrawerViewTest, DimScrimOpacityFollowsPosition)
{
    DrawerTestWindow window;
    prepareWindow(window);
    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(true);
    drawer.setDim(true);
    drawer.open();
    processEvents();

    auto* scrim = window.findChild<QWidget*>(QStringLiteral("DrawerViewScrim"));
    ASSERT_NE(scrim, nullptr);
    const int openAlpha = renderedCenterAlpha(scrim);
    EXPECT_GT(openAlpha, 0);

    drawer.setPosition(0.5);
    processEvents();
    const int halfAlpha = renderedCenterAlpha(scrim);
    EXPECT_GT(halfAlpha, 0);
    EXPECT_LT(halfAlpha, openAlpha);

    drawer.setPosition(0.0);
    processEvents();
    EXPECT_LT(renderedCenterAlpha(scrim), halfAlpha);
}

TEST_F(DrawerViewTest, DisabledBehaviorIgnoresPointerDismissAndDrag)
{
    DrawerTestWindow window;
    prepareWindow(window);
    auto* background = new PressProbe();
    background->setParent(&window);
    background->setGeometry(window.rect());
    background->show();

    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);
    openWithoutAnimation(drawer);
    drawer.setEnabled(false);

    sendMouse(background, QEvent::MouseButtonPress, QPoint(500, 300), Qt::LeftButton, Qt::LeftButton);
    EXPECT_TRUE(drawer.isOpen());

    drawer.close();
    processEvents();
    sendMouse(background, QEvent::MouseButtonPress, QPoint(2, 120), Qt::LeftButton, Qt::LeftButton);
    sendMouse(background, QEvent::MouseMove, QPoint(220, 120), Qt::NoButton, Qt::LeftButton);
    sendMouse(background, QEvent::MouseButtonRelease, QPoint(220, 120), Qt::LeftButton, Qt::NoButton);
    EXPECT_FALSE(drawer.isOpen());
    EXPECT_EQ(drawer.position(), 0.0);
}

TEST_F(DrawerViewTest, InteractiveDragOpensAndClosesWithThresholds)
{
    DrawerTestWindow window;
    prepareWindow(window);
    DrawerView drawer(&window);
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);
    drawer.setDrawerLength(320);

    sendMouse(&window, QEvent::MouseButtonPress, QPoint(2, 140), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&window, QEvent::MouseMove, QPoint(220, 140), Qt::NoButton, Qt::LeftButton);
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(220, 140), Qt::LeftButton, Qt::NoButton);
    EXPECT_TRUE(drawer.isOpen());
    EXPECT_EQ(drawer.position(), 1.0);

    sendMouse(&window, QEvent::MouseButtonPress, QPoint(220, 140), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&window, QEvent::MouseMove, QPoint(40, 140), Qt::NoButton, Qt::LeftButton);
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(40, 140), Qt::LeftButton, Qt::NoButton);
    EXPECT_FALSE(drawer.isOpen());
    EXPECT_EQ(drawer.position(), 0.0);

    drawer.setInteractive(false);
    sendMouse(&window, QEvent::MouseButtonPress, QPoint(2, 140), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&window, QEvent::MouseMove, QPoint(220, 140), Qt::NoButton, Qt::LeftButton);
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(220, 140), Qt::LeftButton, Qt::NoButton);
    EXPECT_FALSE(drawer.isOpen());
}

TEST_F(DrawerViewTest, ThemeRefreshAndParentDestructionAreSafe)
{
    auto* window = new DrawerTestWindow();
    prepareWindow(*window);
    auto* drawer = new DrawerView(window);
    drawer->setAnimationEnabled(false);
    drawer->open();
    processEvents();

    const QRect geometryBeforeTheme = drawer->panelGeometry();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();
    EXPECT_TRUE(drawer->isOpen());
    EXPECT_EQ(drawer->panelGeometry(), geometryBeforeTheme);

    delete window;
    processEvents();
    SUCCEED();
}

TEST_F(DrawerViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    auto* window = new DrawerTestWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(QStringLiteral("DrawerView VisualCheck"));
    window->resize(900, 620);
    window->onThemeUpdated();

    auto* layout = new fluent::AnchorLayout();
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("Fluent DrawerView"), window);
    title->setFluentTypography(QStringLiteral("Title"));

    auto* leftButton = new Button(QStringLiteral("Left"), window);
    leftButton->setFixedSize(76, 32);
    auto* rightButton = new Button(QStringLiteral("Right"), window);
    rightButton->setFixedSize(76, 32);
    auto* topButton = new Button(QStringLiteral("Top"), window);
    topButton->setFixedSize(76, 32);
    auto* bottomButton = new Button(QStringLiteral("Bottom"), window);
    bottomButton->setFixedSize(88, 32);
    auto* themeButton = new Button(QStringLiteral("Theme"), window);
    themeButton->setFixedSize(84, 32);

    auto* modalLabel = new Label(QStringLiteral("Modal"), window);
    auto* modalSwitch = new ToggleSwitch(window);
    modalSwitch->setIsOn(true);
    auto* dimLabel = new Label(QStringLiteral("Dim"), window);
    auto* dimSwitch = new ToggleSwitch(window);
    dimSwitch->setIsOn(true);
    auto* interactiveLabel = new Label(QStringLiteral("Interactive"), window);
    auto* interactiveSwitch = new ToggleSwitch(window);
    interactiveSwitch->setIsOn(true);

    auto* preview = new ContentPane(QStringLiteral("Use the buttons above or drag from an edge."), window);
    auto* drawer = new DrawerView(window);
    drawer->setContentWidget(new DrawerDemoContent());

    fluent::AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {window, Edge::Left, 28};
    titleAnchors.top = {window, Edge::Top, 24};
    layout->addAnchoredWidget(title, titleAnchors);

    fluent::AnchorLayout::Anchors themeAnchors;
    themeAnchors.right = {window, Edge::Right, -28};
    themeAnchors.top = {window, Edge::Top, 24};
    layout->addAnchoredWidget(themeButton, themeAnchors);

    fluent::AnchorLayout::Anchors leftAnchors;
    leftAnchors.left = {window, Edge::Left, 28};
    leftAnchors.top = {title, Edge::Bottom, 22};
    layout->addAnchoredWidget(leftButton, leftAnchors);

    fluent::AnchorLayout::Anchors rightAnchors;
    rightAnchors.left = {leftButton, Edge::Right, 8};
    rightAnchors.top = {leftButton, Edge::Top, 0};
    layout->addAnchoredWidget(rightButton, rightAnchors);

    fluent::AnchorLayout::Anchors topAnchors;
    topAnchors.left = {rightButton, Edge::Right, 8};
    topAnchors.top = {leftButton, Edge::Top, 0};
    layout->addAnchoredWidget(topButton, topAnchors);

    fluent::AnchorLayout::Anchors bottomAnchors;
    bottomAnchors.left = {topButton, Edge::Right, 8};
    bottomAnchors.top = {leftButton, Edge::Top, 0};
    layout->addAnchoredWidget(bottomButton, bottomAnchors);

    fluent::AnchorLayout::Anchors modalLabelAnchors;
    modalLabelAnchors.left = {window, Edge::Left, 28};
    modalLabelAnchors.top = {leftButton, Edge::Bottom, 18};
    layout->addAnchoredWidget(modalLabel, modalLabelAnchors);

    fluent::AnchorLayout::Anchors modalAnchors;
    modalAnchors.left = {modalLabel, Edge::Left, 0};
    modalAnchors.top = {modalLabel, Edge::Bottom, 4};
    layout->addAnchoredWidget(modalSwitch, modalAnchors);

    fluent::AnchorLayout::Anchors dimLabelAnchors;
    dimLabelAnchors.left = {modalSwitch, Edge::Right, 32};
    dimLabelAnchors.top = {modalLabel, Edge::Top, 0};
    layout->addAnchoredWidget(dimLabel, dimLabelAnchors);

    fluent::AnchorLayout::Anchors dimAnchors;
    dimAnchors.left = {dimLabel, Edge::Left, 0};
    dimAnchors.top = {dimLabel, Edge::Bottom, 4};
    layout->addAnchoredWidget(dimSwitch, dimAnchors);

    fluent::AnchorLayout::Anchors interactiveLabelAnchors;
    interactiveLabelAnchors.left = {dimSwitch, Edge::Right, 32};
    interactiveLabelAnchors.top = {modalLabel, Edge::Top, 0};
    layout->addAnchoredWidget(interactiveLabel, interactiveLabelAnchors);

    fluent::AnchorLayout::Anchors interactiveAnchors;
    interactiveAnchors.left = {interactiveLabel, Edge::Left, 0};
    interactiveAnchors.top = {interactiveLabel, Edge::Bottom, 4};
    layout->addAnchoredWidget(interactiveSwitch, interactiveAnchors);

    fluent::AnchorLayout::Anchors previewAnchors;
    previewAnchors.left = {window, Edge::Left, 28};
    previewAnchors.right = {window, Edge::Right, -28};
    previewAnchors.top = {modalSwitch, Edge::Bottom, 28};
    previewAnchors.bottom = {window, Edge::Bottom, -28};
    layout->addAnchoredWidget(preview, previewAnchors);

    auto openDrawer = [drawer, modalSwitch, dimSwitch, interactiveSwitch](DrawerView::DrawerEdge edge) {
        drawer->setEdge(edge);
        drawer->setModal(modalSwitch->isOn());
        drawer->setDim(dimSwitch->isOn());
        drawer->setInteractive(interactiveSwitch->isOn());
        drawer->setDrawerLength(edge == DrawerView::DrawerEdge::Top || edge == DrawerView::DrawerEdge::Bottom ? 260 : 340);
        drawer->open();
    };

    QObject::connect(leftButton, &Button::clicked, drawer, [openDrawer]() { openDrawer(DrawerView::DrawerEdge::Left); });
    QObject::connect(rightButton, &Button::clicked, drawer, [openDrawer]() { openDrawer(DrawerView::DrawerEdge::Right); });
    QObject::connect(topButton, &Button::clicked, drawer, [openDrawer]() { openDrawer(DrawerView::DrawerEdge::Top); });
    QObject::connect(bottomButton, &Button::clicked, drawer, [openDrawer]() { openDrawer(DrawerView::DrawerEdge::Bottom); });
    QObject::connect(themeButton, &Button::clicked, window, [window]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
        window->onThemeUpdated();
    });

    window->show();
    qApp->exec();
}
