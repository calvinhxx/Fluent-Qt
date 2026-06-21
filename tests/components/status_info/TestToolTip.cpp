#include <gtest/gtest.h>
#include <QApplication>
#include <QHelpEvent>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include "components/status_info/ToolTip.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h" // For AnchorLayout

using namespace fluent::status_info;
using namespace fluent::basicinput;
using namespace fluent::textfields;
using namespace fluent;

// Window tailored for ToolTip testing
class ToolTipTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ToolTipTest : public ::testing::Test {
protected:
    void SetUp() override {
        scrollArea = new QScrollArea();
        scrollArea->setWindowTitle("ToolTip Gallery (WinUI 3 Inspired)");
        scrollArea->resize(600, 400);

        container = new ToolTipTestWindow();
        container->resize(600, 600); // Should differ from scrollarea size to show scrolling if needed
        
        layout = new AnchorLayout(container);
        container->setLayout(layout);
        container->onThemeUpdated();

        scrollArea->setWidget(container);
    }

    void TearDown() override {
        delete scrollArea; // Deletes container as well
    }

    QScrollArea* scrollArea;
    ToolTipTestWindow* container;
    AnchorLayout* layout;
};

// Helper for "Move & Show" behavior in test
class ToolTipBehavior : public QObject {
public:
    ToolTipBehavior(QWidget* target, ToolTip* tip) : m_target(target), m_tip(tip) {
        target->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (obj == m_target) {
            if (event->type() == QEvent::Enter) {
                // Manual positioning logic (as requested by user)
                QPoint globalPos = m_target->mapToGlobal(QPoint(0, 0));
                int x = globalPos.x() + (m_target->width() - m_tip->width()) / 2;
                int y = globalPos.y() - m_tip->height() - 8; // generic spacing
                m_tip->move(x, y);
                m_tip->show();
            } else if (event->type() == QEvent::Leave) {
                m_tip->hide();
            }
        }
        return QObject::eventFilter(obj, event);
    }
private:
    QWidget* m_target;
    ToolTip* m_tip;
};

TEST_F(ToolTipTest, InitialState) {
    ToolTip tooltip;
    EXPECT_TRUE(tooltip.text().isEmpty());
    EXPECT_TRUE(tooltip.windowFlags() & Qt::ToolTip);
    EXPECT_TRUE(tooltip.windowFlags() & Qt::FramelessWindowHint);
}

TEST_F(ToolTipTest, SetText) {
    ToolTip tooltip;
    QString text = "Hello ToolTip";
    tooltip.setText(text);
    EXPECT_EQ(tooltip.text(), text);
}

TEST_F(ToolTipTest, AttachedToolTipUsesFluentBubbleAboveTarget) {
    auto* target = new Button("Target", container);
    target->setGeometry(240, 180, 120, 36);
    auto* tip = ToolTip::attach(target, QStringLiteral("Attached help"));
    ASSERT_NE(tip, nullptr);
    EXPECT_EQ(ToolTip::attach(target, QStringLiteral("Updated help")), tip);
    EXPECT_EQ(tip->text(), QStringLiteral("Updated help"));

    scrollArea->show();
    QApplication::processEvents();
    QHelpEvent help(QEvent::ToolTip,
                    target->rect().center(),
                    target->mapToGlobal(target->rect().center()));
    QApplication::sendEvent(target, &help);
    QApplication::processEvents();

    ASSERT_TRUE(tip->isVisible());
    const QRect bubble = tip->geometry().adjusted(tip->shadowMargin(),
                                                   tip->shadowMargin(),
                                                   -tip->shadowMargin(),
                                                   -tip->shadowMargin());
    const QRect targetGlobal(target->mapToGlobal(QPoint(0, 0)), target->size());
    EXPECT_NEAR(bubble.center().x(), targetGlobal.center().x(), 1);
    EXPECT_LT(bubble.bottom(), targetGlobal.top());

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(target, &leave);
    QTRY_VERIFY_WITH_TIMEOUT(!tip->isVisible(), 1000);
}

TEST_F(ToolTipTest, StylingPropertiesRemainStable) {
    ToolTip tooltip;
    tooltip.setText("Styled ToolTip");

    const QMargins margins(20, 10, 18, 8);
    tooltip.setMargins(margins);
    EXPECT_EQ(tooltip.margins(), margins);

    QFont font;
    font.setPixelSize(15);
    font.setItalic(true);
    tooltip.setFont(font);

    EXPECT_EQ(tooltip.text(), QStringLiteral("Styled ToolTip"));
    EXPECT_EQ(tooltip.font().pixelSize(), 15);
    EXPECT_TRUE(tooltip.font().italic());
    EXPECT_TRUE(tooltip.windowFlags() & Qt::ToolTip);
    EXPECT_TRUE(tooltip.windowFlags() & Qt::FramelessWindowHint);
}

TEST_F(ToolTipTest, AnimationEnabledDefaultAndDisabledBehavior) {
    ToolTip tooltip;
    tooltip.setText("Animation disabled");

    EXPECT_TRUE(tooltip.isAnimationEnabled());

    QSignalSpy animationSpy(&tooltip, &ToolTip::animationEnabledChanged);
    tooltip.setAnimationEnabled(false);
    EXPECT_FALSE(tooltip.isAnimationEnabled());
    EXPECT_EQ(animationSpy.count(), 1);

    tooltip.show();
    QApplication::processEvents();
    EXPECT_TRUE(tooltip.isVisible());
    EXPECT_NEAR(tooltip.windowOpacity(), 1.0, 0.001);

    tooltip.hide();
    QApplication::processEvents();
    EXPECT_FALSE(tooltip.isVisible());
    EXPECT_NEAR(tooltip.windowOpacity(), 0.0, 0.001);
}

TEST_F(ToolTipTest, AnimatedHideKeepsVisibleUntilFadeOutCompletes) {
    ToolTip tooltip;
    tooltip.setText("Animated hide");

    tooltip.setAnimationEnabled(false);
    tooltip.show();
    QApplication::processEvents();
    ASSERT_TRUE(tooltip.isVisible());
    EXPECT_NEAR(tooltip.windowOpacity(), 1.0, 0.001);

    tooltip.setAnimationEnabled(true);
    tooltip.hide();
    QApplication::processEvents();

    EXPECT_TRUE(tooltip.isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!tooltip.isVisible(), 1000);
    EXPECT_NEAR(tooltip.windowOpacity(), 0.0, 0.001);
}

TEST_F(ToolTipTest, HideDuringEntryReversesCleanly) {
    ToolTip tooltip;
    tooltip.setText("Reverse during entry");

    tooltip.show();
    QApplication::processEvents();
    ASSERT_TRUE(tooltip.isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(tooltip.windowOpacity() > 0.0, 1000);

    tooltip.hide();
    QApplication::processEvents();

    EXPECT_TRUE(tooltip.isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!tooltip.isVisible(), 1000);
    EXPECT_NEAR(tooltip.windowOpacity(), 0.0, 0.001);
}

TEST_F(ToolTipTest, VisualGallery) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    using Edge = AnchorLayout::Edge;

    // --- Title ---
    Label* title = new Label("ToolTip Gallery", container);
    QFont f = title->font();
    f.setPixelSize(20);
    f.setBold(true);
    title->setFont(f);
    title->anchors()->top = {container, Edge::Top, 20};
    title->anchors()->left = {container, Edge::Left, 40};
    layout->addWidget(title);

    // --- 1. Simple ToolTip ---
    Label* section1 = new Label("1. Button with a simple ToolTip", container);
    section1->anchors()->top = {title, Edge::Bottom, 30};
    section1->anchors()->left = {container, Edge::Left, 40};
    layout->addWidget(section1);

    Button* btn1 = new Button("Hover me", container);
    btn1->anchors()->top = {section1, Edge::Bottom, 10};
    btn1->anchors()->left = {container, Edge::Left, 40};
    layout->addWidget(btn1);

    // Create ToolTip and attach behavior helper
    ToolTip* tip1 = new ToolTip(container); 
    tip1->setText("Button with a simple ToolTip");
    // Attach behavior without modifying ToolTip class
    new ToolTipBehavior(btn1, tip1);

    // --- 2. Custom Config ToolTip ---
    Label* section2 = new Label("2. Custom Config (Margins, Font)", container);
    section2->anchors()->top = {btn1, Edge::Bottom, 60};
    section2->anchors()->left = {container, Edge::Left, 40};
    layout->addWidget(section2);

    Button* btn2 = new Button("Hover for Custom ToolTip", container);
    btn2->anchors()->top = {section2, Edge::Bottom, 10};
    btn2->anchors()->left = {container, Edge::Left, 40};
    layout->addWidget(btn2);

    ToolTip* tip2 = new ToolTip(container);
    tip2->setText("Custom Font & Large Margins");
    QFont customFont; 
    customFont.setItalic(true);
    customFont.setPixelSize(16);
    tip2->setFont(customFont);
    tip2->setMargins(QMargins(20, 10, 20, 10));
    // Attach behavior
    new ToolTipBehavior(btn2, tip2);

    scrollArea->show();
    
    QApplication::exec();
}
