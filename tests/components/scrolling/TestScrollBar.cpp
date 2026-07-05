#include <gtest/gtest.h>
#include <QApplication>
#include <QImage>
#include <QPalette>
#include <QPixmap>
#include <QWidget>
#include <QTimer>
#include <QStyle>
#include <QScrollArea>

#include "components/scrolling/ScrollBar.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/textfields/Label.h"
#include "components/basicinput/Button.h"

using namespace fluent::scrolling;
using namespace fluent::textfields;
using namespace fluent::basicinput;
using namespace fluent;

namespace {

QImage renderScrollBarImage(ScrollBar* scrollBar) {
    QPixmap pixmap(scrollBar->size());
    pixmap.fill(Qt::transparent);
    scrollBar->render(&pixmap);
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);

    // ScrollBar allows Qt to propagate the parent/background surface, so render() may produce an
    // opaque uniform backing. Strip only that uniform backing so the existing thumb-geometry
    // assertions keep inspecting the painted thumb.
    const QColor backing = image.pixelColor(0, 0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel == backing)
                image.setPixelColor(x, y, Qt::transparent);
        }
    }
    return image;
}

QRect alphaBounds(const QImage& image) {
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (QColor::fromRgba(image.pixel(x, y)).alpha() > 0)
                bounds = bounds.united(QRect(x, y, 1, 1));
        }
    }
    return bounds;
}

int pixelAlpha(const QImage& image, int x, int y) {
    return QColor::fromRgba(image.pixel(x, y)).alpha();
}

} // namespace

// Helper Window Class
class FluentTestScrollWindow : public QWidget, public fluent::FluentElement {
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

TEST_F(ScrollBarTest, VerticalThumbKeepsRoundedCapsAtExtremes) {
    ScrollBar scrollBar(Qt::Vertical);
    scrollBar.setThickness(9);
    scrollBar.setFixedSize(9, 96);
    scrollBar.setRange(0, 100);
    scrollBar.setPageStep(20);
    scrollBar.setOpacity(1.0);

    scrollBar.setValue(scrollBar.minimum());
    QImage topImage = renderScrollBarImage(&scrollBar);
    QRect topBounds = alphaBounds(topImage);
    ASSERT_TRUE(topBounds.isValid());
    EXPECT_GT(topBounds.top(), 0);
    EXPECT_GT(pixelAlpha(topImage, topBounds.center().x(), topBounds.top()), 0);
    EXPECT_EQ(pixelAlpha(topImage, topBounds.left(), topBounds.top()), 0);
    EXPECT_EQ(pixelAlpha(topImage, topBounds.right(), topBounds.top()), 0);

    scrollBar.setValue(scrollBar.maximum());
    QImage bottomImage = renderScrollBarImage(&scrollBar);
    QRect bottomBounds = alphaBounds(bottomImage);
    ASSERT_TRUE(bottomBounds.isValid());
    EXPECT_LT(bottomBounds.bottom(), bottomImage.height() - 1);
    EXPECT_GT(pixelAlpha(bottomImage, bottomBounds.center().x(), bottomBounds.bottom()), 0);
    EXPECT_EQ(pixelAlpha(bottomImage, bottomBounds.left(), bottomBounds.bottom()), 0);
    EXPECT_EQ(pixelAlpha(bottomImage, bottomBounds.right(), bottomBounds.bottom()), 0);
}

// ─── Design-language × theme sweep ──────────────────────────────────────────
//
// ScrollBar is quiet chrome: the Fluent path is byte-for-byte unchanged, while Material 3 and macOS
// only swap the THUMB color to a theme-aware neutral veil (no accent) and suppress the resting track.
// Geometry/animation are identical across languages. Design language + theme are GLOBAL singletons,
// so the fixture restores both in TearDown. zh_CN: ScrollBar 是安静的 chrome:Fluent 路径逐字节不变,
// Material 3 与 macOS 仅把 THUMB 颜色换成主题感知中性薄层(无强调色)并抑制静息轨道;几何/动画跨语言一致。
// 设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。
class ScrollBarDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a fully-opaque vertical scrollbar with a real scrollable range and grab its thumb.
    // setOpacity(1.0) defeats the fade-in so the painted thumb is actually visible.
    // zh_CN: 构建完全不透明、具备真实可滚动范围的垂直滚动条并抓取 thumb。setOpacity(1.0) 绕过淡入,使绘制的 thumb 可见。
    static QImage grabThumb() {
        ScrollBar bar(Qt::Vertical);
        bar.setThickness(9);
        bar.setFixedSize(9, 120);
        bar.setRange(0, 100);
        bar.setPageStep(20);
        bar.setValue(50);
        bar.setOpacity(1.0);
        return renderScrollBarImage(&bar);
    }

    static bool hasPaintedContent(const QImage& img) {
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (QColor::fromRgba(img.pixel(x, y)).alpha() > 0)
                    return true;
        return false;
    }
};

TEST_F(ScrollBarDesignLanguageTest, AllLanguagesAndThemesPaintAThumbWithoutOpaqueBlack) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const std::string ctx = std::string(lang.name) + "/" + th.name;
            const QImage img = grabThumb();

            // 1. A valid, non-empty image with a painted thumb. zh_CN: 有效、非空且绘制出 thumb 的图像。
            ASSERT_FALSE(img.isNull()) << ctx;
            EXPECT_GT(img.width(), 0) << ctx;
            EXPECT_GT(img.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(img)) << "thumb painted nothing: " << ctx;

            // 2. The painted thumb area must NOT be an opaque near-#000 surface (the invalid-QColor
            // trap: a default QColor is invalid yet alpha()==255 → solid black). Sample the thumb's
            // center; the thumb sits mid-track at value 50. zh_CN: 绘制的 thumb 区域不得为不透明近黑表面
            //(无效 QColor 陷阱:默认 QColor 无效却 alpha==255 → 纯黑)。采样轨道中部 value=50 的 thumb 中心。
            const QRect bounds = alphaBounds(img);
            ASSERT_TRUE(bounds.isValid()) << ctx;
            const QColor c = img.pixelColor(bounds.center());
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "ScrollBar painted an opaque black thumb: " << ctx << " rgba=(" << c.red() << ","
                << c.green() << "," << c.blue() << "," << c.alpha() << ")";
        }
    }
}

TEST_F(ScrollBarTest, TransparentRestingOverlayPreservesParentSurface) {
    QWidget parent;
    parent.resize(72, 144);
    parent.setAutoFillBackground(true);

    const QColor parentColor("#4A90E2");
    QPalette palette = parent.palette();
    palette.setColor(QPalette::Window, parentColor);
    parent.setPalette(palette);

    ScrollBar bar(Qt::Vertical, &parent);
    bar.setGeometry(60, 0, 12, parent.height());
    bar.setRange(0, 100);
    bar.setPageStep(20);
    bar.setOpacity(0.0);
    bar.show();

    EXPECT_FALSE(bar.testAttribute(Qt::WA_NoSystemBackground));
    EXPECT_FALSE(bar.testAttribute(Qt::WA_OpaquePaintEvent));

    QPixmap pixmap(parent.size());
    pixmap.fill(Qt::black);
    bar.render(&pixmap, bar.pos());

    const QColor sample = pixmap.toImage().pixelColor(bar.geometry().center());
    EXPECT_NEAR(sample.red(), parentColor.red(), 1);
    EXPECT_NEAR(sample.green(), parentColor.green(), 1);
    EXPECT_NEAR(sample.blue(), parentColor.blue(), 1);
}

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
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
