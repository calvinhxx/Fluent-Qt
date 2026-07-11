#include <gtest/gtest.h>
#include <QApplication>
#include <QTest>
#include <QWidget>
#include "components/foundation/FluentElement.h"
#include "components/windowing/WindowBackdrop.h"
#include "design/CornerRadius.h"

// 模拟一个继承自 fluent::FluentElement 的组件
class MockComponent : public QWidget, public fluent::FluentElement {
public:
    explicit MockComponent(QWidget* parent = nullptr) : QWidget(parent) {}
    
    int updateCount = 0;
    void onThemeUpdated() override {
        updateCount++;
    }
};

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include "components/basicinput/Button.h"
#include <QFrame>
#include <QTimer>
#include <QVariant>
#include <QElapsedTimer>
#include <QScrollArea>
#include <QGridLayout>
#include <QGroupBox>
#include <QGraphicsDropShadowEffect>
#include <QStackedLayout>
#include <QMap>

#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

// 材质预览卡片：用 paintEvent 手动合成渐变底图 + 材质覆盖层，避免 QSS/QPalette 无法做透明合成的问题
class MaterialPreviewCard : public QWidget {
    Q_OBJECT
public:
    explicit MaterialPreviewCard(const QString& name, QWidget* parent = nullptr)
        : QWidget(parent), m_name(name) {
        setMinimumSize(160, 88);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setMaterialColor(const QColor& color) {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF r = rect().adjusted(1, 1, -1, -1);
        const qreal radius = 8;
        QPainterPath clip;
        clip.addRoundedRect(r, radius, radius);
        p.setClipPath(clip);

        // 1. 底图：高对比度渐变，确保透明色能被感知
        QLinearGradient grad(r.topLeft(), r.bottomRight());
        grad.setColorAt(0.0, QColor(0x4f, 0x7f, 0xd4));
        grad.setColorAt(0.5, QColor(0x59, 0xb0, 0x7a));
        grad.setColorAt(1.0, QColor(0x9a, 0x67, 0xd9));
        p.fillRect(r, grad);

        // 2. 材质叠加层（SourceOver 自动做透明合成）
        p.fillRect(r, m_color);

        // 3. 边框
        p.setClipping(false);
        p.setPen(QPen(QColor(0, 0, 0, 40), 1));
        p.drawRoundedRect(r, radius, radius);

        // 4. 标签
        const bool darkOverlay = m_color.alpha() > 160 && m_color.lightness() < 60;
        p.setPen(darkOverlay ? QColor(240, 240, 240) : QColor(30, 30, 30));
        QFont f = p.font();
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, m_name);
    }

private:
    QString m_name;
    QColor  m_color = Qt::transparent;
};

// 一个全功能的设计元素预览组件，用于测试 src/design 中的所有 Token
class VisualMockComponent : public QWidget, public fluent::FluentElement {
    Q_OBJECT
public:
    explicit VisualMockComponent(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowTitle("Fluent Design System Tokens Preview");
        setMinimumSize(320, 500); 
        resize(800, 600);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // --- 顶部控制栏 ---
        QHBoxLayout* header = new QHBoxLayout();
        m_themeBtn = new fluent::basicinput::Button("Toggle Theme (Light/Dark)", this);
        header->addWidget(m_themeBtn);
        header->addStretch();
        
        m_breakpointLabel = new QLabel(this);
        header->addWidget(m_breakpointLabel);
        mainLayout->addLayout(header);

        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        QWidget* container = new QWidget();
        m_contentLayout = new QVBoxLayout(container);
        
        setupTypographySection();
        setupColorsSection();
        setupRadiusAndShadowSection();
        setupSpacingSection();
        setupMaterialSection();
        setupAnimationSection();
        
        scrollArea->setWidget(container);
        mainLayout->addWidget(scrollArea);
        
        // 绑定主题切换按钮
        connect(m_themeBtn, &QPushButton::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                                ? fluent::FluentElement::Dark
                                                : fluent::FluentElement::Light);
        });

        onThemeUpdated();
    }
    
    void onThemeUpdated() override {
        const auto& colors = themeColors();
        
        // 辅助 lambda：将 QColor 转换为 QSS 兼容的 rgba 字符串
        auto toQss = [](const QColor& c) {
            return QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
        };

        // 更新整体背景
        setStyleSheet(QString("QWidget { background-color: %1; color: %2; }")
                      .arg(colors.bgCanvas.name())
                      .arg(colors.textPrimary.name()));
        
        // 注意：m_themeBtn 作为一个继承自 fluent::FluentElement 的组件，
        // 它的 onThemeUpdated 会被全局管理器自动调用，这里无需手动处理。

        // 更新各部分的具体样式
        updateTypography();
        updateColors();
        updateRadiusAndShadow();
        updateSpacing();
        updateMaterials();
        updateAnimationPreview();
        updateBreakpointInfo();
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        updateBreakpointInfo();
    }

private:
    void setupTypographySection() {
        QGroupBox* group = new QGroupBox("1. Typography (Typography.h)", this);
        QVBoxLayout* layout = new QVBoxLayout(group);
        QStringList styles = {"Display", "TitleLarge", "Title", "Subtitle", "BodyStrong", "Body", "Caption"};
        for (const QString& s : styles) {
            QLabel* label = new QLabel(s + " - The quick brown fox jumps over the lazy dog", this);
            m_typoLabels[s] = label;
            layout->addWidget(label);
        }
        m_contentLayout->addWidget(group);
    }

    void setupColorsSection() {
        QGroupBox* group = new QGroupBox("2. Colors (ThemeColors.h)", this);
        QGridLayout* layout = new QGridLayout(group);
        
        auto addColorBlock = [&](const QString& name, int row, int col) {
            QWidget* block = new QWidget(this);
            block->setFixedSize(100, 40);
            QLabel* label = new QLabel(name, this);
            label->setAlignment(Qt::AlignCenter);
            layout->addWidget(block, row * 2, col);
            layout->addWidget(label, row * 2 + 1, col);
            m_colorBlocks[name] = block;
        };

        addColorBlock("Accent", 0, 0);
        addColorBlock("Control", 0, 1);
        addColorBlock("Layer", 0, 2);
        addColorBlock("Grey50", 1, 0);
        addColorBlock("Grey90", 1, 1);
        addColorBlock("Chart0", 1, 2);
        
        m_contentLayout->addWidget(group);
    }

    void setupRadiusAndShadowSection() {
        QGroupBox* group = new QGroupBox("3. Radius & Elevation (CornerRadius.h / Elevation.h)", this);
        QHBoxLayout* layout = new QHBoxLayout(group);
        
        auto addCard = [&](const QString& name) {
            QFrame* card = new QFrame(this);
            card->setFixedSize(150, 100);
            QLabel* label = new QLabel(name, card);
            label->setAlignment(Qt::AlignCenter);
            QVBoxLayout* l = new QVBoxLayout(card);
            l->addWidget(label);
            layout->addWidget(card);
            m_cards[name] = card;
        };

        addCard("Control + Low");
        addCard("Control + Med");
        addCard("Overlay + High");
        
        m_contentLayout->addWidget(group);
    }

    void setupSpacingSection() {
        QGroupBox* group = new QGroupBox("4. Spacing (Spacing.h)", this);
        QVBoxLayout* layout = new QVBoxLayout(group);
        m_spacingFrame = new QFrame(this);
        m_spacingFrame->setMinimumHeight(50);
        layout->addWidget(m_spacingFrame);
        m_contentLayout->addWidget(group);
    }

    void setupMaterialSection() {
        QGroupBox* group = new QGroupBox("5. Materials (Material.h)", this);
        QHBoxLayout* layout = new QHBoxLayout(group);
        layout->setSpacing(12);

        auto addCard = [&](const QString& name) -> MaterialPreviewCard* {
            auto* card = new MaterialPreviewCard(name, this);
            layout->addWidget(card);
            return card;
        };

        m_acrylicCard = addCard("Acrylic");
        m_micaCard    = addCard("Mica");
        m_smokeCard   = addCard("Smoke");
        m_contentLayout->addWidget(group);
    }

    void setupAnimationSection() {
        QGroupBox* group = new QGroupBox("6. Animation (Animation.h)", this);
        QVBoxLayout* layout = new QVBoxLayout(group);
        
        // --- 动画控制栏 ---
        QHBoxLayout* controls = new QHBoxLayout();
        m_durationCombo = new QComboBox(this);
        m_durationCombo->addItems({"Fast", "Normal", "Slow", "VerySlow"});
        m_durationCombo->setCurrentText("Normal");
        
        m_easingCombo = new QComboBox(this);
        m_easingCombo->addItems({"Standard", "Entrance", "Exit", "Accelerate", "Decelerate"});
        m_easingCombo->setCurrentText("Entrance");
        
        controls->addWidget(new QLabel("Duration:"));
        controls->addWidget(m_durationCombo);
        controls->addWidget(new QLabel("Easing:"));
        controls->addWidget(m_easingCombo);
        layout->addLayout(controls);

        m_animContainer = new QFrame(this);
        m_animContainer->setMinimumHeight(150);
        m_animContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_animContainer->setStyleSheet("background: palette(dark); border-radius: 4px;");
        
        m_animBox = new QFrame(m_animContainer);
        m_animBox->setFixedSize(50, 50);
        m_animBox->move(20, 50);
        
        QPushButton* startBtn = new QPushButton("Play Animation", group);
        layout->addWidget(m_animContainer);
        layout->addWidget(startBtn);
        
        connect(startBtn, &QPushButton::clicked, [this]() {
            QPropertyAnimation* anim = new QPropertyAnimation(m_animBox, "pos", this);
            const auto& a = themeAnimation();
            
            // 获取选择的时长
            int duration = a.normal;
            QString dStr = m_durationCombo->currentText();
            if (dStr == "Fast") duration = a.fast;
            else if (dStr == "Slow") duration = a.slow;
            else if (dStr == "VerySlow") duration = a.verySlow;
            
            // 获取选择的曲线
            QEasingCurve easing = a.entrance;
            QString eStr = m_easingCombo->currentText();
            if (eStr == "Standard") easing = a.standard;
            else if (eStr == "Exit") easing = a.exit;
            else if (eStr == "Accelerate") easing = a.accelerate;
            else if (eStr == "Decelerate") easing = a.decelerate;

            anim->setDuration(duration);
            anim->setEasingCurve(easing);
            anim->setStartValue(QPoint(20, 50));
            anim->setEndValue(QPoint(530, 50));
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        });

        m_contentLayout->addWidget(group);
    }

    void updateTypography() {
        for (auto it = m_typoLabels.begin(); it != m_typoLabels.end(); ++it) {
            it.value()->setFont(themeFont(it.key()).toQFont());
        }
    }

    void updateColors() {
        const auto& c = themeColors();
        auto setStyle = [](QWidget* w, QColor color) {
            w->setStyleSheet(QString("background-color: %1; border: 1px solid palette(mid);").arg(color.name()));
        };
        setStyle(m_colorBlocks["Accent"], c.accentDefault);
        setStyle(m_colorBlocks["Control"], c.controlDefault);
        setStyle(m_colorBlocks["Layer"], c.bgLayer);
        setStyle(m_colorBlocks["Grey50"], c.grey50);
        setStyle(m_colorBlocks["Grey90"], c.grey90);
        if (!c.charts.isEmpty()) setStyle(m_colorBlocks["Chart0"], c.charts[0]);
    }

    void updateRadiusAndShadow() {
        const auto& r = themeRadius();
        const auto& c = themeColors();
        
        auto applyEffect = [&](const QString& key, int rad, Elevation::Level level) {
            QFrame* card = m_cards[key];
            card->setStyleSheet(QString("background: %1; border-radius: %2px;").arg(c.bgLayer.name()).arg(rad));
            
            auto shadow = themeShadow(level);
            QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect(this);
            effect->setBlurRadius(shadow.blurRadius);
            effect->setOffset(shadow.offsetX, shadow.offsetY);
            QColor sc = shadow.color; 
            sc.setAlphaF(shadow.opacity);
            effect->setColor(sc);
            card->setGraphicsEffect(effect);
        };

        applyEffect("Control + Low",  r.control, Elevation::Low);
        applyEffect("Control + Med",  r.control, Elevation::Medium);
        applyEffect("Overlay + High", r.overlay, Elevation::High);
    }

    void updateSpacing() {
        const auto& s = themeSpacing();
        m_spacingFrame->setStyleSheet(QString("background: palette(midlight); border: %1px solid %2; margin: %3px;")
            .arg(s.small)
            .arg(themeColors().accentDefault.name())
            .arg(s.standard));
    }

    void updateMaterials() {
        auto toColor = [](QColor base, double opacity) {
            base.setAlphaF(opacity);
            return base;
        };
        const auto acrylic = themeAcrylic();
        const auto mica    = themeMica();
        const auto smoke   = themeSmoke();

        m_acrylicCard->setMaterialColor(toColor(acrylic.tintColor, acrylic.tintOpacity));
        m_micaCard->setMaterialColor(toColor(mica.baseColor, mica.opacity));
        m_smokeCard->setMaterialColor(toColor(smoke.baseColor, smoke.opacity));
    }

    void updateAnimationPreview() {
        m_animBox->setStyleSheet(QString("background: %1; border-radius: 4px;")
            .arg(themeColors().accentDefault.name()));
    }

    void updateBreakpointInfo() {
        int w = width();
        int small = themeBreakpoint("Small");
        int medium = themeBreakpoint("Medium");
        
        QString name;
        QString color;
        if (w <= small) {
            name = "Small (Compact)";
            color = "#E81123"; // Windows Red
        } else if (w <= medium) {
            name = "Medium (Medium)";
            color = "#0078D7"; // Windows Blue
        } else {
            name = "Large (Expanded)";
            color = "#107C10"; // Windows Green
        }
        
        m_breakpointLabel->setText(QString(
            "<html><body>"
            "Current Width: <b style='font-size: 16px;'>%1 px</b> | "
            "Breakpoint: <b style='color: %2; font-size: 16px;'>%3</b>"
            "<br><small style='color: gray;'>Guide: Small &lt;= %4px | Medium &lt;= %5px | Large &gt; %5px</small>"
            "</body></html>"
        ).arg(w).arg(color).arg(name).arg(small).arg(medium));
    }

    QVBoxLayout* m_contentLayout;
    QMap<QString, QLabel*> m_typoLabels;
    QMap<QString, QWidget*> m_colorBlocks;
    QMap<QString, QFrame*> m_cards;
    QFrame* m_spacingFrame;
    MaterialPreviewCard* m_acrylicCard = nullptr;
    MaterialPreviewCard* m_micaCard    = nullptr;
    MaterialPreviewCard* m_smokeCard   = nullptr;

    fluent::basicinput::Button* m_themeBtn;
    QLabel* m_breakpointLabel;
    QFrame* m_animContainer;
    QFrame* m_animBox;
    QComboBox* m_durationCombo;
    QComboBox* m_easingCombo;
};

#include "TestFluentElement.moc"


class FluentElementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每次测试前重置为 Light 主题
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        
        window = new QWidget();
        window->setWindowTitle("FluentElement Visual Preview");
        layout = new QVBoxLayout(window);
        window->setLayout(layout);
    }

    void TearDown() override {
        delete window;
    }

    QWidget* window;
    QVBoxLayout* layout;
};

TEST_F(FluentElementTest, ThemeSwitching) {
    MockComponent component;
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(component.updateCount, 0);

    // 切换到 Dark
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(component.updateCount, 1);

    // 再次切换回 Light
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(component.updateCount, 2);
}

TEST_F(FluentElementTest, DeferredThemeSwitchThemesVisibleSynchronouslyThenHidden) {
    auto* visibleComponent = new MockComponent(window);
    layout->addWidget(visibleComponent);
    window->show();
    QApplication::processEvents();
    MockComponent hiddenComponent;

    fluent::FluentElement::setThemeDeferred(fluent::FluentElement::Dark);
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    // Visible elements are themed synchronously so the on-screen switch is atomic (one coalesced
    // repaint); only off-screen elements are deferred. zh_CN: 可见元素同步刷新，使屏幕切换是原子的
    //（一次合并重绘）；仅屏外元素延后。
    EXPECT_EQ(visibleComponent->updateCount, 1);
    EXPECT_EQ(hiddenComponent.updateCount, 0);

    // The deferred hidden element catches up on a later tick, and the visible one is themed exactly once.
    // zh_CN: 延后的隐藏元素在之后的 tick 补刷；可见元素恰好刷新一次。
    QTRY_COMPARE_WITH_TIMEOUT(hiddenComponent.updateCount, 1, 1000);
    EXPECT_EQ(visibleComponent->updateCount, 1);
}

TEST_F(FluentElementTest, ColorTokenMapping) {
    MockComponent component;
    
    // Light 主题下的颜色
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    auto lightColors = component.themeColors();
    EXPECT_TRUE(lightColors.accentDefault.isValid());
    EXPECT_EQ(lightColors.textPrimary.alpha(), 230); // 90% black
    EXPECT_TRUE(lightColors.controlAltSecondary.isValid());
    EXPECT_TRUE(lightColors.grey10.isValid());
    EXPECT_FALSE(lightColors.charts.isEmpty());

    // Dark 主题下的颜色
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    auto darkColors = component.themeColors();
    EXPECT_NE(lightColors.accentDefault, darkColors.accentDefault);
    EXPECT_EQ(darkColors.textPrimary, QColor("#FFFFFF"));
    EXPECT_NE(lightColors.bgCanvas, darkColors.bgCanvas);
    EXPECT_TRUE(darkColors.grey190.isValid());
}

TEST_F(FluentElementTest, WidgetThemeOverrideIsInheritedWithoutChangingGlobalTheme)
{
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    QWidget host;
    MockComponent child(&host);
    MockComponent outside;

    host.setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));

    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(child.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(outside.effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(child.themeColors().bgLayer, QColor("#2C2C2C"));
    EXPECT_EQ(outside.themeColors().bgLayer, QColor("#FFFFFF"));

    host.setProperty("fluentThemeOverride", QVariant());
    EXPECT_EQ(child.effectiveTheme(), fluent::FluentElement::Light);
    EXPECT_EQ(child.themeColors().bgLayer, QColor("#FFFFFF"));
}

TEST_F(FluentElementTest, ChromeBackdropFillFollowsHostBackdropAndFocus) {
    MockComponent component;
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    // No host window: solid fallback, and active/inactive visibly differ (the cross-platform
    // stand-in for Mica's active/inactive). zh_CN: 无宿主窗口：纯色回退，且激活/非激活明显不同。
    const QColor active = component.chromeBackdropFill(nullptr, /*active*/ true);
    const QColor inactive = component.chromeBackdropFill(nullptr, /*active*/ false);
    EXPECT_TRUE(active.isValid());
    EXPECT_TRUE(inactive.isValid());
    EXPECT_EQ(active, component.themeBackdrop(true));
    EXPECT_EQ(inactive, component.themeBackdrop(false));
    EXPECT_NE(active, inactive);

    // A host without a system backdrop falls back identically.
    // zh_CN: 不带系统背景的宿主，回退结果一致。
    QWidget plainHost;
    EXPECT_EQ(component.chromeBackdropFill(&plainHost, true), component.themeBackdrop(true));

    // Unsupported platforms still keep the requested effect as an opaque, token-based fallback.
    QWidget paintedMicaHost;
    paintedMicaHost.setProperty("fluentWindowBackdropEffect", 1);
    const QColor paintedMica = component.chromeBackdropFill(&paintedMicaHost, true);
    EXPECT_TRUE(paintedMica.isValid());
    EXPECT_EQ(paintedMica.alpha(), 255);
    EXPECT_NE(paintedMica, component.themeBackdrop(true));

    QWidget paintedAcrylicHost;
    paintedAcrylicHost.setProperty("fluentWindowBackdropEffect", 2);
    const QColor paintedAcrylic = component.chromeBackdropFill(&paintedAcrylicHost, true);
    EXPECT_TRUE(paintedAcrylic.isValid());
    EXPECT_EQ(paintedAcrylic.alpha(), 255);
    EXPECT_NE(paintedAcrylic, component.themeBackdrop(true));
    EXPECT_NE(paintedAcrylic, paintedMica);

    // A host carrying a real OS-composited backdrop (Windows DWM/Acrylic or macOS vibrancy) yields an invalid
    // color regardless of focus — the caller's contract is "erase to transparent".
    // zh_CN: 带真实系统合成背景（Windows DWM/Acrylic 或 macOS vibrancy）的宿主，无论焦点都返回无效色——调用方据此擦透明。
    QWidget micaHost;
    micaHost.setProperty("fluentWindowBackdropEffect", 2);
    micaHost.setProperty("fluentMicaBackdrop", true);
    EXPECT_FALSE(component.chromeBackdropFill(&micaHost, true).isValid());
    EXPECT_FALSE(component.chromeBackdropFill(&micaHost, false).isValid());

    // A typed state is authoritative even when stale legacy properties disagree.
    // zh_CN: 即使旧属性残留冲突值，强类型状态仍是唯一权威来源。
    fluent::windowing::BackdropState typedPainted;
    typedPainted.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    typedPainted.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    typedPainted.backend = fluent::windowing::BackdropBackend::PaintedMaterial;
    typedPainted.fidelity = fluent::windowing::BackdropFidelity::Emulated;
    typedPainted.surfaceMode = fluent::windowing::BackdropSurfaceMode::PaintedOpaque;
    fluent::windowing::publishWindowBackdropState(&micaHost, typedPainted);
    const QColor authoritativePainted = component.chromeBackdropFill(&micaHost, true);
    EXPECT_TRUE(authoritativePainted.isValid());
    EXPECT_EQ(authoritativePainted.alpha(), 255);
}

TEST_F(FluentElementTest, FontTokenMapping) {
    MockComponent component;
    
    auto bodyFont = component.themeFont("Body");
    EXPECT_EQ(bodyFont.size, 14);
    EXPECT_FALSE(bodyFont.family.isEmpty());

    auto titleFont = component.themeFont("TitleLarge");
    EXPECT_EQ(titleFont.size, Typography::FontSize::TitleLarge);  // 40px (Figma MCP 实测)
    EXPECT_GT(titleFont.weight, bodyFont.weight);
}

TEST_F(FluentElementTest, RadiusAndSpacingMapping) {
    MockComponent component;
    
    auto radius = component.themeRadius();
    EXPECT_EQ(radius.none,    0);
    EXPECT_EQ(radius.control, CornerRadius::Control);
    EXPECT_EQ(radius.overlay, CornerRadius::Overlay);

    auto spacing = component.themeSpacing();
    EXPECT_EQ(spacing.padding.controlH, 12);
    EXPECT_EQ(spacing.medium, 12);
}

TEST_F(FluentElementTest, AnimationTokenMapping) {
    MockComponent component;
    
    auto anim = component.themeAnimation();
    // 验证持续时间
    EXPECT_EQ(anim.fast, 150);
    EXPECT_EQ(anim.normal, 250);
    EXPECT_EQ(anim.slow, 400);
    EXPECT_EQ(anim.verySlow, 700);
    
    // 验证缓动曲线
    EXPECT_EQ(anim.standard.type(), QEasingCurve::InOutSine);
    EXPECT_EQ(anim.entrance.type(), QEasingCurve::OutBack);
    EXPECT_EQ(anim.exit.type(), QEasingCurve::InQuint);
    EXPECT_EQ(anim.accelerate.type(), QEasingCurve::InCubic);
    EXPECT_EQ(anim.decelerate.type(), QEasingCurve::OutCubic);
}

TEST_F(FluentElementTest, MaterialAndShadow) {
    MockComponent component;

    auto acrylic = component.themeAcrylic();
    EXPECT_GT(acrylic.blurRadius, 0);
    EXPECT_GE(acrylic.tintOpacity, 0.0);
    EXPECT_LE(acrylic.tintOpacity, 1.0);

    auto mica = component.themeMica();
    EXPECT_GE(mica.opacity, 0.0);
    EXPECT_LE(mica.opacity, 1.0);

    auto smoke = component.themeSmoke();
    EXPECT_GE(smoke.opacity, 0.0);
    EXPECT_LE(smoke.opacity, 1.0);

    auto shadow = component.themeShadow(Elevation::High);
    EXPECT_GT(shadow.blurRadius, 0);
}

TEST_F(FluentElementTest, BreakpointMapping) {
    MockComponent component;
    EXPECT_EQ(component.themeBreakpoint("Small"), 640);
    EXPECT_EQ(component.themeBreakpoint("Medium"), 1007);
    EXPECT_EQ(component.themeBreakpoint("Large"), 1920);
}

TEST_F(FluentElementTest, VisualExample) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    // 如果没有显示设备（如在某些沙盒/CI中），跳过可视化测试
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    // 将预览组件放入固件管理的 layout 中
    VisualMockComponent* preview = new VisualMockComponent(window);
    layout->addWidget(preview);
    
    window->show();
    
    // 使用 qApp->exec() 保持窗口开启，直到手动关闭
    qApp->exec();
}
