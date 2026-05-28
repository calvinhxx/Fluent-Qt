#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <QStyle>
#include <QScrollArea>

#include "view/scrolling/ScrollBar.h"
#include "view/foundation/QMLPlus.h"
#include "view/foundation/FluentElement.h"
#include "view/textfields/Label.h"
#include "view/basicinput/Button.h" 

using namespace view::scrolling;
using namespace view::textfields;
using namespace view::basicinput;
using namespace view;

// Helper Window Class
class FluentTestScrollWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ScrollBarTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestScrollWindow();
        window->setFixedSize(600, 800);
        window->setWindowTitle("ScrollBar Visual Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated(); // Apply initial theme
    }

    void TearDown() override {
        delete window;
    }

    FluentTestScrollWindow* window;
    AnchorLayout* layout;
};

TEST_F(ScrollBarTest, VisualPropertyVerification) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    using Edge = AnchorLayout::Edge;

    // Helper to create section labels
    auto createLabel = [&](const QString& text, QObject* anchorTop, int topMargin = 20) {
        Label* l = new Label(text, window);
        if (anchorTop == window) {
             l->anchors()->top = {window, Edge::Top, topMargin};
        } else {
             // Basic casting for layout logic
             auto* w = qobject_cast<QWidget*>(anchorTop);
             if (w) l->anchors()->top = {w, Edge::Bottom, topMargin};
        }
        l->anchors()->left = {window, Edge::Left, 40};
        layout->addWidget(l);
        return l;
    };

    // --- 1. Horizontal ScrollBars ---
    Label* lblHoriz = new Label("1. Horizontal ScrollBars (Various Thickness):", window);
    lblHoriz->anchors()->top = {window, Edge::Top, 20};
    lblHoriz->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lblHoriz);

    // Standard Thickness (default ~12)
    ScrollBar* sbH1 = new ScrollBar(Qt::Horizontal, window);
    sbH1->setFixedWidth(300);
    sbH1->anchors()->top = {lblHoriz, Edge::Bottom, 10};
    sbH1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(sbH1);

    // Thicker
    ScrollBar* sbH2 = new ScrollBar(Qt::Horizontal, window);
    sbH2->setFixedWidth(300);
    sbH2->setThickness(24);
    sbH2->anchors()->top = {sbH1, Edge::Bottom, 10};
    sbH2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(sbH2);

    // Thinner
    ScrollBar* sbH3 = new ScrollBar(Qt::Horizontal, window);
    sbH3->setFixedWidth(300);
    sbH3->setThickness(6);
    sbH3->anchors()->top = {sbH2, Edge::Bottom, 10};
    sbH3->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(sbH3);

    // --- 2. Vertical ScrollBars ---
    Label* lblVert = createLabel("2. Vertical ScrollBars:", sbH3, 30);

    // Standard Vertical
    ScrollBar* sbV1 = new ScrollBar(Qt::Vertical, window);
    sbV1->setFixedHeight(200);
    sbV1->anchors()->top = {lblVert, Edge::Bottom, 10};
    sbV1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(sbV1);

    // Thicker Vertical
    ScrollBar* sbV2 = new ScrollBar(Qt::Vertical, window);
    sbV2->setFixedHeight(200);
    sbV2->setThickness(24);
    sbV2->anchors()->top = {lblVert, Edge::Bottom, 10};
    sbV2->anchors()->left = {sbV1, Edge::Right, 20};
    layout->addWidget(sbV2);
    
    // --- 3. Interaction Test ---
    Label* lblInteraction = createLabel("3. Try Hover and Drag:", sbV1, 30);
    // ensure alignment for next label
    lblInteraction->anchors()->top = {sbV1, Edge::Bottom, 30};

    // A long scrollbar to test dragging feel
    ScrollBar* sbInteract = new ScrollBar(Qt::Horizontal, window);
    sbInteract->setFixedWidth(400);
    sbInteract->setRange(0, 1000);
    sbInteract->setPageStep(100);
    sbInteract->anchors()->top = {lblInteraction, Edge::Bottom, 10};
    sbInteract->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(sbInteract);
    
    // Label to show value
    Label* lblValue = new Label("Value: 0", window);
    lblValue->anchors()->verticalCenter = {sbInteract, Edge::VCenter, 0};
    lblValue->anchors()->left = {sbInteract, Edge::Right, 20};
    layout->addWidget(lblValue);
    
    QObject::connect(sbInteract, &ScrollBar::valueChanged, [lblValue](int val){
        lblValue->setText(QString("Value: %1").arg(val));
    });

    // --- 4. Integration with QScrollArea Example ---
    // This is tricky because QScrollArea manages its own scrollbars usually.
    // Ideally we'd setVerticalScrollBar(new ScrollBar(...)) but let's just show it works locally.
    
class FluentScrollArea : public QScrollArea, public QMLPlus {
public:
    using QScrollArea::QScrollArea; 
};

// ...

    Label* lblIntegration = createLabel("4. Integrated in ScrollArea:", sbInteract, 30);
    
    FluentScrollArea* scrollArea = new FluentScrollArea(window);
    scrollArea->setFixedSize(200, 150);
    // Replace default scrollbars
    scrollArea->setVerticalScrollBar(new ScrollBar(Qt::Vertical, scrollArea));
    scrollArea->setHorizontalScrollBar(new ScrollBar(Qt::Horizontal, scrollArea));

    // Content
    QWidget* content = new QWidget;
    content->setFixedSize(400, 400); // larger than viewport
    // subtle gradient to see scrolling
    content->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eee, stop:1 #ccc);");
    scrollArea->setWidget(content);

    scrollArea->anchors()->top = {lblIntegration, Edge::Bottom, 10};
    scrollArea->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(scrollArea);


    // --- Theme Switcher ---
    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light ? FluentElement::Dark : FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
