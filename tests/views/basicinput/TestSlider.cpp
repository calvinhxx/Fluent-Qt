#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QStyle>
#include <QLabel>
#include <QSpinBox>
#include <QRadioButton>
#include <QScrollArea>

#include "view/basicinput/Slider.h"
#include "view/basicinput/Button.h"
#include "view/textfields/Label.h"
#include "view/foundation/QMLPlus.h"
#include "view/foundation/FluentElement.h"

using namespace view::basicinput;
using namespace view::textfields;
using namespace view;

// 与 TestButton 中一致的简易 Fluent 容器窗口
class SliderFluentTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class SliderTest : public ::testing::Test {
protected:
    void SetUp() override {
        scrollArea = new QScrollArea();
        scrollArea->setWindowTitle("Slider Visual Test (WinUI 3 Inspired)");
        scrollArea->resize(850, 600);

        window = new SliderFluentTestWindow();
        // Make window tall enough to show all content without being cut off
        window->setFixedSize(800, 950);
        
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();

        scrollArea->setWidget(window);
    }

    void TearDown() override {
        delete scrollArea;
    }

    QScrollArea* scrollArea = nullptr;
    SliderFluentTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(SliderTest, VisualSliderGalleryLike) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") &&
        qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    using Edge = AnchorLayout::Edge;

    auto createLabel = [&](const QString& text, QWidget* anchor, int topMargin = 30) {
        Label* l = new Label(text, window);
        l->anchors()->top = {anchor, Edge::Bottom, topMargin};
        l->anchors()->left = {window, Edge::Left, 40};
        layout->addWidget(l);
        return l;
    };

    // 顶部标题 + 描述（参考 WinUI 3 Gallery）
    Label* pageTitle = new Label("Slider", window);
    pageTitle->anchors()->top = {window, Edge::Top, 16};
    pageTitle->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(pageTitle);

    Label* pageDesc = new Label(
        "Use a Slider when you want your users to be able to set defined, contiguous values "
        "(such as volume or brightness) or a range of discrete values (such as screen resolution settings).",
        window);
    pageDesc->setWordWrap(true);
    pageDesc->anchors()->top = {pageTitle, Edge::Bottom, 8};
    pageDesc->anchors()->left = {window, Edge::Left, 40};
    pageDesc->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(pageDesc);

    // --- 1. Simple Slider ---
    Label* title1 = new Label("A simple Slider.", window);
    title1->anchors()->top = {pageDesc, Edge::Bottom, 24};
    title1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(title1);

    Slider* simpleSlider = new Slider(Qt::Horizontal, window);
    simpleSlider->setRange(0, 100);
    simpleSlider->setValue(32);
    simpleSlider->setFixedWidth(260);
    simpleSlider->anchors()->top = {title1, Edge::Bottom, 10};
    simpleSlider->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(simpleSlider);

    Label* simpleOutputLabel = new Label("Output: 32", window);
    simpleOutputLabel->anchors()->verticalCenter = {simpleSlider, Edge::VCenter, 0};
    simpleOutputLabel->anchors()->left = {simpleSlider, Edge::Right, 30};
    layout->addWidget(simpleOutputLabel);

    QObject::connect(simpleSlider, &QSlider::valueChanged, [simpleOutputLabel](int v) {
        simpleOutputLabel->setText(QStringLiteral("Output: %1").arg(v));
    });

    // --- 2. Slider with range and steps specified ---
    Label* title2 = createLabel("A Slider with range and steps specified.", simpleSlider);

    Slider* rangeSlider = new Slider(Qt::Horizontal, window);
    rangeSlider->setRange(500, 1000);
    rangeSlider->setSingleStep(10);  // SmallChange
    rangeSlider->setPageStep(50);
    rangeSlider->setValue(800);
    rangeSlider->setFixedWidth(260);
    rangeSlider->anchors()->top = {title2, Edge::Bottom, 10};
    rangeSlider->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(rangeSlider);

    Label* rangeOutputLabel = new Label("Output: 800", window);
    rangeOutputLabel->anchors()->verticalCenter = {rangeSlider, Edge::VCenter, 0};
    rangeOutputLabel->anchors()->left = {rangeSlider, Edge::Right, 30};
    layout->addWidget(rangeOutputLabel);

    QObject::connect(rangeSlider, &QSlider::valueChanged, [rangeOutputLabel](int v) {
        rangeOutputLabel->setText(QStringLiteral("Output: %1").arg(v));
    });

    // 右侧显示当前最小值 / 最大值 / 步长设置（仿 WinUI 属性面板）
    const int panelLeft = 420;

    auto createSpinRow = [&](const QString& text, int value, int rowIndex) {
        Label* lbl = new Label(text, window);
        lbl->anchors()->top = {title2, Edge::Bottom, 10 + rowIndex * 36};
        lbl->anchors()->left = {window, Edge::Left, panelLeft};
        layout->addWidget(lbl);

        QSpinBox* spin = new QSpinBox(window);
        spin->setRange(-100000, 100000);
        spin->setValue(value);
        AnchorLayout::Anchors a;
        a.verticalCenter = {lbl, Edge::VCenter, 0};
        a.left = {lbl, Edge::Right, 10};
        layout->addAnchoredWidget(spin, a);
        return spin;
    };

    QSpinBox* minSpin = createSpinRow("Minimum:", rangeSlider->minimum(), 0);
    QSpinBox* maxSpin = createSpinRow("Maximum:", rangeSlider->maximum(), 1);
    QSpinBox* stepSpin = createSpinRow("StepFrequency:", rangeSlider->singleStep(), 2);
    QSpinBox* smallSpin = createSpinRow("SmallChange:", rangeSlider->singleStep(), 3);

    QObject::connect(minSpin, qOverload<int>(&QSpinBox::valueChanged),
                     rangeSlider, &QSlider::setMinimum);
    QObject::connect(maxSpin, qOverload<int>(&QSpinBox::valueChanged),
                     rangeSlider, &QSlider::setMaximum);
    QObject::connect(stepSpin, qOverload<int>(&QSpinBox::valueChanged),
                     rangeSlider, &QSlider::setTickInterval);
    QObject::connect(smallSpin, qOverload<int>(&QSpinBox::valueChanged),
                     rangeSlider, &QSlider::setSingleStep);

    // --- 3. Slider with tick marks ---
    // Fix: Anchor to smallSpin because the property panel on the right is taller than the slider itself
    Label* title3 = createLabel("A Slider with tick marks.", smallSpin);

    Slider* tickSlider = new Slider(Qt::Horizontal, window);
    tickSlider->setRange(0, 10);
    tickSlider->setTickInterval(1);
    tickSlider->setTickPosition(QSlider::TicksBelow);
    tickSlider->setFixedWidth(260);
    tickSlider->anchors()->top = {title3, Edge::Bottom, 10};
    tickSlider->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(tickSlider);

    Label* tickOutputLabel = new Label("Output: 0", window);
    tickOutputLabel->anchors()->verticalCenter = {tickSlider, Edge::VCenter, 0};
    tickOutputLabel->anchors()->left = {tickSlider, Edge::Right, 30};
    layout->addWidget(tickOutputLabel);

    QObject::connect(tickSlider, &QSlider::valueChanged, [tickOutputLabel](int v) {
        tickOutputLabel->setText(QStringLiteral("Output: %1").arg(v));
    });

    // Snaps to：StepValues / Ticks
    Label* snapsLabel = new Label("Snaps to:", window);
    snapsLabel->anchors()->top = {tickSlider, Edge::Top, 0};
    snapsLabel->anchors()->left = {tickSlider, Edge::Right, 80};
    layout->addWidget(snapsLabel);

    QRadioButton* snapsStep = new QRadioButton("StepValues", window);
    snapsStep->setChecked(true);
    {
        AnchorLayout::Anchors a;
        a.top = {snapsLabel, Edge::Bottom, 8};
        a.left = {snapsLabel, Edge::Left, 0};
        layout->addAnchoredWidget(snapsStep, a);
    }

    QRadioButton* snapsTicks = new QRadioButton("Ticks", window);
    {
        AnchorLayout::Anchors a;
        a.top = {snapsStep, Edge::Bottom, 4};
        a.left = {snapsStep, Edge::Left, 0};
        layout->addAnchoredWidget(snapsTicks, a);
    }

    QObject::connect(snapsStep, &QRadioButton::toggled, [tickSlider](bool on) {
        if (on) {
            tickSlider->setSingleStep(1);
        }
    });
    QObject::connect(snapsTicks, &QRadioButton::toggled, [tickSlider](bool on) {
        if (on) {
            int interval = tickSlider->tickInterval();
            if (interval <= 0) interval = 1;
            tickSlider->setSingleStep(interval);
        }
    });

    // --- 4. Vertical Slider 示例 ---
    Label* title4 = createLabel("Vertical Slider", tickSlider);

    Slider* verticalSlider = new Slider(Qt::Vertical, window);
    verticalSlider->setRange(0, 100);
    verticalSlider->setValue(25);
    verticalSlider->setFixedHeight(160);
    verticalSlider->anchors()->top = {title4, Edge::Bottom, 10};
    verticalSlider->anchors()->left = {window, Edge::Left, 80};
    layout->addWidget(verticalSlider);

    Label* verticalOutputLabel = new Label("Output: 25", window);
    verticalOutputLabel->anchors()->top = {verticalSlider, Edge::Top, 0};
    verticalOutputLabel->anchors()->left = {verticalSlider, Edge::Right, 40};
    layout->addWidget(verticalOutputLabel);

    QObject::connect(verticalSlider, &QSlider::valueChanged, [verticalOutputLabel](int v) {
        verticalOutputLabel->setText(QStringLiteral("Output: %1").arg(v));
    });

    // 主题切换按钮，方便观察浅色 / 深色下的滑块样式
    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light
                                    ? FluentElement::Dark
                                    : FluentElement::Light);
    });

    scrollArea->show();
    qApp->exec();
}

