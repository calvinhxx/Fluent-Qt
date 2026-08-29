#include "components/basicinput/HyperlinkButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QSignalSpy>
#include <QVBoxLayout>

#include <gtest/gtest.h>

using namespace fluent;
using namespace fluent::basicinput;

class HyperlinkButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class HyperlinkButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        ThemeRegistry::instance().resetToDefaults();
        window = new HyperlinkButtonTestWindow();
        window->setFixedSize(600, 800);
        window->setWindowTitle("Fluent HyperlinkButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic HyperlinkButton
        layout->addWidget(new QLabel("1. Basic HyperlinkButton:", window));
        auto* btn1 = new HyperlinkButton("Microsoft home page", window);
        btn1->setUrl(QUrl("https://www.microsoft.com"));
        layout->addWidget(btn1);

        // 2. HyperlinkButton with different sizes
        layout->addWidget(new QLabel("2. Different Sizes:", window));
        auto* h1 = new QHBoxLayout();
        auto* small = new HyperlinkButton("Small", window);
        small->setFluentSize(Button::Small);
        auto* normal = new HyperlinkButton("Standard", window);
        normal->setFluentSize(Button::StandardSize);
        auto* large = new HyperlinkButton("Large", window);
        large->setFluentSize(Button::Large);
        h1->addWidget(small);
        h1->addWidget(normal);
        h1->addWidget(large);
        h1->addStretch();
        layout->addLayout(h1);

        // 3. HyperlinkButton with click handler
        layout->addWidget(new QLabel("3. Click to navigate (Check Console):", window));
        auto* navBtn = new HyperlinkButton("Go to GitHub", QUrl("https://github.com"), window);
        layout->addWidget(navBtn);

        // 4. Disabled state
        layout->addWidget(new QLabel("4. Disabled state (No underline by default):", window));
        auto* disabledBtn = new HyperlinkButton("Disabled link", window);
        disabledBtn->setEnabled(false);
        layout->addWidget(disabledBtn);

        // 5. With underline (Explicitly enabled)
        layout->addWidget(new QLabel("5. With underline on hover (Explicitly enabled):", window));
        auto* withUnderline = new HyperlinkButton("Underline on hover", window);
        withUnderline->setShowUnderline(true); 
        layout->addWidget(withUnderline);

        // 6. Default behavior (No underline)
        layout->addWidget(new QLabel("6. Default behavior (No underline on hover):", window));
        auto* defaultBtn = new HyperlinkButton("Default no underline", window);
        layout->addWidget(defaultBtn);

        layout->addStretch();

        // Theme switch button
        auto* themeBtn = new QPushButton("Switch Theme", window);
        layout->addWidget(themeBtn);
        QObject::connect(themeBtn, &QPushButton::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light 
                                    ? fluent::FluentElement::Dark 
                                    : fluent::FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        ThemeRegistry::instance().resetToDefaults();
    }

    HyperlinkButtonTestWindow* window = nullptr;
};

TEST_F(HyperlinkButtonTest, Contract_PropertiesSignalOnlyOnRealChanges)
{
    HyperlinkButton link(QStringLiteral("Docs"));
    EXPECT_TRUE(link.url().isEmpty());
    EXPECT_FALSE(link.showUnderline());

    QSignalSpy urlSpy(&link, &HyperlinkButton::urlChanged);
    QSignalSpy underlineSpy(&link, &HyperlinkButton::showUnderlineChanged);
    const QUrl url(QStringLiteral("https://example.com/docs"));

    link.setUrl(url);
    link.setUrl(url);
    link.setShowUnderline(true);
    link.setShowUnderline(true);

    EXPECT_EQ(link.url(), url);
    EXPECT_TRUE(link.showUnderline());
    EXPECT_EQ(urlSpy.count(), 1);
    EXPECT_EQ(underlineSpy.count(), 1);
}

TEST_F(HyperlinkButtonTest, Contract_LightAndDarkRestPaintsWithoutOpaqueBlackFill)
{
    const FluentElement::Theme themes[]{FluentElement::Light,
                                        FluentElement::Dark};
    for (const auto theme : themes) {
        FluentElement::setTheme(theme);
        HyperlinkButton link(QStringLiteral("FluentQt"));
        link.setUrl(QUrl(QStringLiteral("https://example.com")));
        link.resize(120, 32);
        const QImage image = link.grab().toImage();
        ASSERT_FALSE(image.isNull()) << "theme=" << theme;

        const QRgb background = image.pixel(0, 0);
        bool painted = false;
        for (int y = 0; y < image.height() && !painted; ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixel(x, y) != background) {
                    painted = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(painted) << "theme=" << theme;

        const QColor center = image.pixelColor(image.width() / 2,
                                                image.height() / 2);
        const int luminance = qRound(0.299 * center.red()
                                     + 0.587 * center.green()
                                     + 0.114 * center.blue());
        EXPECT_FALSE(center.alpha() > 200 && luminance < 16)
            << "theme=" << theme;
    }

    ThemeRegistry::instance().resetToDefaults();
    FluentElement::setTheme(FluentElement::Light);
}

TEST_F(HyperlinkButtonTest, Contract_ThemeTypographyUsesButtonBaseContract)
{
    HyperlinkButton link(QStringLiteral("Docs"));
    link.setAttribute(Qt::WA_DontShowOnScreen);
    link.setFontRole(Typography::FontRole::BodyStrong);
    link.show();

    auto& registry = ThemeRegistry::instance();
    auto themed = registry.snapshot();
    themed.fontScale = 1.5;
    ASSERT_TRUE(registry.applySnapshot(themed));

    const QFont expected = link.themeFont(Typography::FontRole::BodyStrong).toQFont();
    EXPECT_EQ(link.fontRole(), Typography::FontRole::BodyStrong);
    EXPECT_EQ(link.font().family(), expected.family());
    EXPECT_EQ(link.font().pixelSize(), expected.pixelSize());
    EXPECT_EQ(link.font().weight(), expected.weight());
}

TEST_F(HyperlinkButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
