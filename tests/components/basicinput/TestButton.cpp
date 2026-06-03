#include <gtest/gtest.h>
#include <QApplication>
#include <QPushButton>
#include <QTimer>
#include <QStyle>
#include <QGroupBox>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QMargins>
#include <QPolygonF>
#include <QSignalSpy>
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using namespace fluent::basicinput;
using namespace fluent::textfields;
using namespace fluent;

namespace {

qreal renderedDarkPixelCenterY(int iconOffsetY) {
    Button button;
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFluentStyle(Button::Subtle);
    button.setFluentLayout(Button::IconOnly);
    button.setIconGlyph(Typography::Icons::GlobalNav,
                        Typography::FontSize::Body,
                        Typography::FontFamily::SegoeFluentIcons);
    button.setIconOffset(QPoint(0, iconOffsetY));
    button.setFixedSize(48, 48);
    button.ensurePolished();
    button.show();
    QApplication::processEvents();

    QImage image(button.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    button.render(&painter);
    painter.end();

    qreal weightedY = 0.0;
    qreal totalWeight = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            const int alpha = pixel.alpha();
            const int luminance = (pixel.red() * 299 + pixel.green() * 587 + pixel.blue() * 114) / 1000;
            if (alpha <= 16 || luminance >= 128)
                continue;

            const qreal weight = alpha * (255 - luminance);
            weightedY += y * weight;
            totalWeight += weight;
        }
    }

    return totalWeight > 0.0 ? weightedY / totalWeight : -1.0;
}

} // namespace

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(600, 850); // 增加高度以容纳新内容
        window->setWindowTitle("Button Properties Comprehensive Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
    AnchorLayout* layout;
};

TEST_F(ButtonTest, IconOffsetYMovesIconFontRendering) {
    const qreal baselineCenterY = renderedDarkPixelCenterY(0);
    const qreal shiftedCenterY = renderedDarkPixelCenterY(6);

    ASSERT_GE(baselineCenterY, 0.0);
    ASSERT_GE(shiftedCenterY, 0.0);
    EXPECT_GT(shiftedCenterY - baselineCenterY, 4.0);
}

TEST_F(ButtonTest, VisualPropertyVerification) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
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

    // --- 1. Style Test (Standard, Accent, Subtle) ---
    Label* lblStyle = new Label("1. Button Styles:", window);
    lblStyle->anchors()->top = {window, Edge::Top, 20};
    lblStyle->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lblStyle);

    Button* btnStd = new Button("Standard", window);
    btnStd->setFluentStyle(Button::Standard);
    btnStd->setFixedSize(120, 32);
    btnStd->anchors()->top = {lblStyle, Edge::Bottom, 10};
    btnStd->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btnStd);

    Button* btnAccent = new Button("Accent", window);
    btnAccent->setFluentStyle(Button::Accent);
    btnAccent->setFixedSize(120, 32);
    btnAccent->anchors()->verticalCenter = {btnStd, Edge::VCenter, 0};
    btnAccent->anchors()->left = {btnStd, Edge::Right, 20};
    layout->addWidget(btnAccent);

    Button* btnSubtle = new Button("Subtle", window);
    btnSubtle->setFluentStyle(Button::Subtle);
    btnSubtle->setFixedSize(120, 32);
    btnSubtle->anchors()->verticalCenter = {btnStd, Edge::VCenter, 0};
    btnSubtle->anchors()->left = {btnAccent, Edge::Right, 20};
    layout->addWidget(btnSubtle);

    // --- 2. Size Test (Small, Standard, Large) ---
    Label* lblSize = createLabel("2. Button Sizes:", btnStd);
    
    Button* btnSmall = new Button("Small", window);
    btnSmall->setFluentSize(Button::Small);
    btnSmall->anchors()->top = {lblSize, Edge::Bottom, 10};
    btnSmall->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btnSmall);

    Button* btnNormalSize = new Button("Standard", window);
    btnNormalSize->setFluentSize(Button::StandardSize);
    btnNormalSize->anchors()->verticalCenter = {btnSmall, Edge::VCenter, 0};
    btnNormalSize->anchors()->left = {btnSmall, Edge::Right, 20};
    layout->addWidget(btnNormalSize);

    Button* btnLarge = new Button("Large Size", window);
    btnLarge->setFluentSize(Button::Large);
    btnLarge->anchors()->verticalCenter = {btnSmall, Edge::VCenter, 0};
    btnLarge->anchors()->left = {btnNormalSize, Edge::Right, 20};
    layout->addWidget(btnLarge);

    // --- 3. Layout Test (TextOnly, IconBefore, IconOnly, IconAfter) ---
    Label* lblLayout = createLabel("3. Content Layouts (with IconFont):", btnLarge);

    Button* l1 = new Button("Text Only", window);
    l1->setFluentLayout(Button::TextOnly);
    l1->anchors()->top = {lblLayout, Edge::Bottom, 10};
    l1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(l1);

    // IconBefore + IconFont（略微向下 1px，让 glyph 视觉更居中）
    Button* l2 = new Button("Icon Before", window);
    l2->setFluentLayout(Button::IconBefore);
    l2->setIconGlyph(Typography::Icons::GlobalNav,
                     Typography::FontSize::Caption,
                     Typography::FontFamily::SegoeFluentIcons);
    l2->setIconOffset(QPoint(0, 1));
    l2->anchors()->verticalCenter = {l1, Edge::VCenter, 0};
    l2->anchors()->left = {l1, Edge::Right, 20};
    layout->addWidget(l2);

    // IconOnly + IconFont
    Button* l3 = new Button("", window);
    l3->setFluentLayout(Button::IconOnly);
    l3->setFixedSize(40, 40);
    l3->setIconGlyph(Typography::Icons::More,
                     Typography::FontSize::Caption,
                     Typography::FontFamily::SegoeFluentIcons);
    l3->setIconOffset(QPoint(0, 1));
    l3->anchors()->verticalCenter = {l1, Edge::VCenter, 0};
    l3->anchors()->left = {l2, Edge::Right, 20};
    layout->addWidget(l3);

    // IconAfter + IconFont
    Button* l4 = new Button("Icon After", window);
    l4->setFluentLayout(Button::IconAfter);
    l4->setIconGlyph(Typography::Icons::ChevronRight,
                     Typography::FontSize::Caption,
                     Typography::FontFamily::SegoeFluentIcons);
    l4->setIconOffset(QPoint(0, 1));
    l4->anchors()->verticalCenter = {l1, Edge::VCenter, 0};
    l4->anchors()->left = {l3, Edge::Right, 20};
    layout->addWidget(l4);

    // 额外的 IconOnly 示例：再展示一个不同 glyph
    Button* l5 = new Button("", window);
    l5->setFluentLayout(Button::IconOnly);
    l5->setFixedSize(40, 40);
    l5->setIconGlyph(Typography::Icons::More,
                     Typography::FontSize::Caption,
                     Typography::FontFamily::SegoeFluentIcons);
    l5->setIconOffset(QPoint(0, 1));
    l5->anchors()->verticalCenter = {l1, Edge::VCenter, 0};
    l5->anchors()->left = {l4, Edge::Right, 20};
    layout->addWidget(l5);

    // --- 3.5. Layout Test with Regular Icons (对比 iconfont) ---
    Label* lblRegularIcons = createLabel("3.5. Content Layouts (with Regular Icons - for comparison):", l5, 40);

    // 创建一些简单的图标用于对比
    auto createSimpleIcon = [](const QColor& color, int size = 16) -> QIcon {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(2, 2, size - 4, size - 4);
        return QIcon(pm);
    };

    auto createMenuIcon = [](const QColor& color, int size = 16) -> QIcon {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 2));
        int lineY = size / 4;
        int spacing = size / 4;
        for (int i = 0; i < 3; ++i) {
            p.drawLine(3, lineY + i * spacing, size - 3, lineY + i * spacing);
        }
        return QIcon(pm);
    };

    auto createChevronIcon = [](const QColor& color, int size = 16) -> QIcon {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 2));
        p.setBrush(Qt::NoBrush);
        QPolygonF arrow;
        arrow << QPointF(size * 0.3, size * 0.3)
              << QPointF(size * 0.7, size * 0.5)
              << QPointF(size * 0.3, size * 0.7);
        p.drawPolyline(arrow);
        return QIcon(pm);
    };

    // 获取当前主题颜色（通过 window 实例）
    const auto& colors = window->themeColors();
    QColor iconColor = colors.textPrimary;

    Button* r1 = new Button("Text Only", window);
    r1->setFluentLayout(Button::TextOnly);
    r1->anchors()->top = {lblRegularIcons, Edge::Bottom, 10};
    r1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(r1);

    // IconBefore + Regular Icon
    Button* r2 = new Button("Icon Before", window);
    r2->setFluentLayout(Button::IconBefore);
    r2->setIcon(createMenuIcon(iconColor, 16));
    r2->setIconSize(QSize(16, 16));
    r2->anchors()->verticalCenter = {r1, Edge::VCenter, 0};
    r2->anchors()->left = {r1, Edge::Right, 20};
    layout->addWidget(r2);

    // IconOnly + Regular Icon
    Button* r3 = new Button("", window);
    r3->setFluentLayout(Button::IconOnly);
    r3->setFixedSize(40, 40);
    r3->setIcon(createSimpleIcon(iconColor, 16));
    r3->setIconSize(QSize(16, 16));
    r3->anchors()->verticalCenter = {r1, Edge::VCenter, 0};
    r3->anchors()->left = {r2, Edge::Right, 20};
    layout->addWidget(r3);

    // IconAfter + Regular Icon
    Button* r4 = new Button("Icon After", window);
    r4->setFluentLayout(Button::IconAfter);
    r4->setIcon(createChevronIcon(iconColor, 16));
    r4->setIconSize(QSize(16, 16));
    r4->anchors()->verticalCenter = {r1, Edge::VCenter, 0};
    r4->anchors()->left = {r3, Edge::Right, 20};
    layout->addWidget(r4);

    // 额外的 IconOnly 示例：使用系统标准图标
    Button* r5 = new Button("", window);
    r5->setFluentLayout(Button::IconOnly);
    r5->setFixedSize(40, 40);
    QStyle* style = qApp->style();
    QIcon standardIcon = style->standardIcon(QStyle::SP_FileDialogNewFolder);
    if (!standardIcon.isNull()) {
        r5->setIcon(standardIcon);
        r5->setIconSize(QSize(16, 16));
    }
    r5->anchors()->verticalCenter = {r1, Edge::VCenter, 0};
    r5->anchors()->left = {r4, Edge::Right, 20};
    layout->addWidget(r5);

    // --- 4. Interaction States (Forced) ---
    Label* lblStates = createLabel("4. Forced Interaction States:", r3, 40);

    Button* sRest = new Button("Rest", window);
    sRest->setInteractionState(Button::Rest);
    sRest->anchors()->top = {lblStates, Edge::Bottom, 10};
    sRest->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(sRest);

    Button* sHover = new Button("Forced Hover", window);
    sHover->setInteractionState(Button::Hover);
    sHover->anchors()->verticalCenter = {sRest, Edge::VCenter, 0};
    sHover->anchors()->left = {sRest, Edge::Right, 20};
    layout->addWidget(sHover);

    Button* sPressed = new Button("Forced Pressed", window);
    sPressed->setInteractionState(Button::Pressed);
    sPressed->anchors()->verticalCenter = {sRest, Edge::VCenter, 0};
    sPressed->anchors()->left = {sHover, Edge::Right, 20};
    layout->addWidget(sPressed);

    Button* sDisabled = new Button("Forced Disabled", window);
    sDisabled->setInteractionState(Button::Disabled);
    sDisabled->anchors()->verticalCenter = {sRest, Edge::VCenter, 0};
    sDisabled->anchors()->left = {sPressed, Edge::Right, 20};
    layout->addWidget(sDisabled);

    // --- 5. Focus Visual & Dynamic State ---
    Label* lblFocus = createLabel("5. Focus Visual & State Transition:", sRest, 40);

    Button* focusBtn = new Button("Forced Focus Visual", window);
    focusBtn->setFocusVisual(true);
    focusBtn->setFluentStyle(Button::Accent);
    focusBtn->setFixedSize(200, 40);
    focusBtn->anchors()->top = {lblFocus, Edge::Bottom, 10};
    focusBtn->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(focusBtn);

    Button* stateToggle = new Button("Toggle Dynamic State", window);
    stateToggle->setFixedSize(200, 40);
    stateToggle->anchors()->verticalCenter = {focusBtn, Edge::VCenter, 0};
    stateToggle->anchors()->left = {focusBtn, Edge::Right, 30};
    layout->addWidget(stateToggle);

    QMLState active;
    active.name = "active";
    active.changes = {
        { stateToggle, "text", "ACTIVE STATE" },
        { stateToggle, "fluentStyle", Button::Accent },
        { stateToggle, "focusVisual", true }
    };
    stateToggle->addState(active);

    QObject::connect(stateToggle, &Button::clicked, [stateToggle]() {
        stateToggle->setState(stateToggle->state() == "" ? "active" : "");
    });

    // --- Theme Switcher ---
    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}

TEST_F(ButtonTest, CriticalHoverAndCornerRadiiProperties) {
    Button button("Danger", window);
    QSignalSpy criticalSpy(&button, &Button::criticalOnHoverChanged);
    QSignalSpy cornerSpy(&button, &Button::cornerRadiiChanged);

    EXPECT_FALSE(button.criticalOnHover());

    button.setCriticalOnHover(true);
    EXPECT_TRUE(button.criticalOnHover());
    EXPECT_EQ(criticalSpy.count(), 1);

    button.setCriticalOnHover(true);
    EXPECT_EQ(criticalSpy.count(), 1);

    const int defaultRadius = button.themeRadius().control;
    EXPECT_EQ(button.cornerRadii(), QMargins(defaultRadius, defaultRadius, defaultRadius, defaultRadius));

    button.setCornerRadii(QMargins(0, 4, 0, 0));
    EXPECT_EQ(button.cornerRadii(), QMargins(0, 4, 0, 0));
    EXPECT_EQ(cornerSpy.count(), 1);

    button.setCornerRadii(QMargins(0, 4, 0, 0));
    EXPECT_EQ(cornerSpy.count(), 1);

    button.setCornerRadii(QMargins(-1, 2, -3, 4));
    EXPECT_EQ(button.cornerRadii(), QMargins(0, 2, 0, 4));
    EXPECT_EQ(cornerSpy.count(), 2);

    button.resetCornerRadii();
    EXPECT_EQ(button.cornerRadii(), QMargins(defaultRadius, defaultRadius, defaultRadius, defaultRadius));
    EXPECT_EQ(cornerSpy.count(), 3);
}
