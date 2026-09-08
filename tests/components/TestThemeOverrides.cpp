#include <gtest/gtest.h>
#include <QApplication>
#include <QFontInfo>
#include <QJsonObject>
#include <QPointer>
#include <QWidget>
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/UserTheme.h"

namespace {
class ThemeProbe : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    int updates = 0;
    void onThemeUpdated() override { ++updates; }
};
class ThemeOverridesTest : public ::testing::Test {
    fluent::ThemeRegistry::ExtendedSnapshot saved;
    fluent::FluentElement::Theme theme;
    void SetUp() override
    {
        saved = fluent::ThemeRegistry::instance().extendedSnapshot();
        theme = fluent::FluentElement::currentTheme();
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
    void TearDown() override
    {
        fluent::ThemeRegistry::instance().applyExtendedSnapshot(saved);
        fluent::FluentElement::setTheme(theme);
    }
};

TEST_F(ThemeOverridesTest, Contract_GlobalPatchIsAtomicAndKeepsUnspecifiedFields)
{
    ThemeProbe probe;
    const auto before = probe.themeColors();
    auto& registry = fluent::ThemeRegistry::instance();
    const int revision = registry.revision();
    const QJsonObject spec{{"light", QJsonObject{{"accentDefault", "#C04080"}}},
                           {"font", QJsonObject{{"scale", 1.5}}}};
    EXPECT_TRUE(fluent::UserTheme::applyOverrides(spec));
    EXPECT_EQ(registry.revision(), revision + 1);
    EXPECT_EQ(probe.themeColors().accentDefault, QColor("#C04080"));
    EXPECT_EQ(probe.themeColors().textAccentPrimary, QColor("#C04080"));
    EXPECT_EQ(probe.themeColors().bgCanvas, before.bgCanvas);
    EXPECT_EQ(registry.fontScale(), 1.5);
    EXPECT_FALSE(fluent::UserTheme::applyOverrides(spec));
    EXPECT_FALSE(fluent::UserTheme::applyOverrides(
        {{"font", QJsonObject{{"scale", 100}}}, {"light", QJsonObject{{"bgCanvas", "#123456"}}}}));
    EXPECT_EQ(probe.themeColors().bgCanvas, before.bgCanvas);
    EXPECT_EQ(registry.revision(), revision + 1);
}

TEST_F(ThemeOverridesTest, Contract_LocalFieldsSurviveGlobalUpdatesAndClearToCurrentTheme)
{
    ThemeProbe local, sibling;
    const QJsonObject spec{{"light", QJsonObject{{"textPrimary", "#123456"}}},
                           {"dark", QJsonObject{{"textPrimary", "#FEDCBA"}}},
                           {"radius", QJsonObject{{"control", 12}}},
                           {"font", QJsonObject{{"scale", 1.5}}}};
    const int revision = fluent::ThemeRegistry::instance().revision();
    EXPECT_TRUE(local.setThemeOverrides(spec));
    EXPECT_EQ(local.updates, 1);
    EXPECT_EQ(fluent::ThemeRegistry::instance().revision(), revision);
    EXPECT_EQ(sibling.updates, 0);
    EXPECT_EQ(local.themeColors().textPrimary, QColor("#123456"));
    EXPECT_NE(local.themeColors().textPrimary, sibling.themeColors().textPrimary);
    EXPECT_EQ(local.themeRadius().control, 12);
    EXPECT_EQ(local.themeFont().size, qRound(sibling.themeFont().size * 1.5));
    EXPECT_FALSE(local.setThemeOverrides(spec));
    EXPECT_EQ(local.updates, 1);
    EXPECT_TRUE(fluent::UserTheme::applyOverrides(
        {{"dark", QJsonObject{{"bgCanvas", "#182430"}}}, {"font", QJsonObject{{"scale", 2.0}}}}));
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    EXPECT_EQ(local.themeColors().textPrimary, QColor("#FEDCBA"));
    EXPECT_EQ(local.themeColorsRef().bgCanvas, QColor("#182430"));
    EXPECT_EQ(local.themeFont().size, 21);
    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    EXPECT_EQ(local.themeColors().textPrimary, sibling.themeColors().textPrimary);
    local.clearThemeOverrides();
    EXPECT_TRUE(local.themeOverrides().isEmpty());
    EXPECT_EQ(local.themeFont().size, sibling.themeFont().size);
    EXPECT_EQ(local.themeRadius().control, sibling.themeRadius().control);
}

TEST_F(ThemeOverridesTest, Contract_InvalidLocalInputPreservesStateAndDoesNotNotify)
{
    ThemeProbe local;
    EXPECT_TRUE(local.setThemeOverrides({{"radius", QJsonObject{{"control", 9}}}}));
    const auto saved = local.themeOverrides();
    for (const auto& bad : {QJsonObject{{"colour", QJsonObject{}}},
                            QJsonObject{{"light", QJsonObject{{"textPrimary", "invalid-color"}}}},
                            QJsonObject{{"font", QJsonObject{{"scale", "2"}}}},
                            QJsonObject{{"radius", QJsonObject{{"control", -1}}}},
                            QJsonObject{{"radius", QJsonObject{{"control", 2.5}}}}}) {
        EXPECT_FALSE(local.setThemeOverrides(bad));
        EXPECT_EQ(local.themeOverrides(), saved);
        EXPECT_EQ(local.updates, 1);
    }
}

TEST_F(ThemeOverridesTest, Contract_ButtonFontPrecedenceAndPerElementBoundary)
{
    fluent::basicinput::Button button("Local"), other("Global");
    ThemeProbe child(&button);
    EXPECT_TRUE(button.setThemeOverrides({{"font", QJsonObject{{"scale", 1.5}}},
                                          {"light", QJsonObject{{"textPrimary", "#654321"}}}}));
    EXPECT_EQ(button.font().pixelSize(), 21);
    EXPECT_EQ(child.themeFont().size, other.themeFont().size);
    EXPECT_EQ(child.themeColors().textPrimary, other.themeColors().textPrimary);
    QFont explicitFont = button.font();
    explicitFont.setPixelSize(25);
    button.setFont(explicitFont);
    button.setThemeOverrides({{"font", QJsonObject{{"scale", 2.0}}}});
    EXPECT_EQ(button.font().pixelSize(), 25);
    button.setFontRole(button.fontRole());
    EXPECT_EQ(button.font().pixelSize(), 28);
    button.clearThemeOverrides();
    EXPECT_EQ(button.font(), other.font());
}

TEST_F(ThemeOverridesTest, Contract_LocalUpdateMayDestroyElement)
{
    class DeletingWidget : public ThemeProbe {
    public:
        bool armed = false;
        void onThemeUpdated() override
        {
            if (armed)
                delete this;
        }
    };
    const QJsonObject spec{{"radius", QJsonObject{{"control", 9}}}};
    for (bool clearing : {false, true}) {
        auto* widget = new DeletingWidget;
        QPointer<QWidget> guard(widget);
        if (clearing)
            ASSERT_TRUE(widget->setThemeOverrides(spec));
        widget->armed = true;
        if (clearing)
            widget->clearThemeOverrides();
        else
            EXPECT_TRUE(widget->setThemeOverrides(spec));
        EXPECT_TRUE(guard.isNull());
    }

    // FluentElement also supports non-QObject users; QPointer is not required.
    // zh_CN: FluentElement 也支持非 QObject 使用方，不依赖 QPointer。
    class DeletingElement : public fluent::FluentElement {
    public:
        explicit DeletingElement(bool& destroyed) : destroyed(destroyed) {}
        ~DeletingElement() override { destroyed = true; }
        void onThemeUpdated() override { delete this; }
        bool& destroyed;
    };
    bool destroyed = false;
    auto* element = new DeletingElement(destroyed);
    EXPECT_TRUE(element->setThemeOverrides(spec));
    EXPECT_TRUE(destroyed);
    // Destruction must also unregister the element from later theme broadcasts.
    // zh_CN: 销毁后也必须退出后续主题广播。
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
}

TEST_F(ThemeOverridesTest, Contract_CustomFontFamilyPreservesRoleWeight)
{
    ThemeProbe local, global;
    // Use a bundled family so the test does not depend on installed system fonts.
    // zh_CN: 使用内置字体族，避免测试依赖系统安装字体。
    const QJsonObject spec{{"font", QJsonObject{{"family", Typography::FontFamily::UIText}}}};
    ASSERT_TRUE(local.setThemeOverrides(spec));
    ASSERT_TRUE(fluent::UserTheme::applyOverrides(spec));
    for (auto role : {Typography::FontRole::BodyStrong, Typography::FontRole::Title}) {
        const QFont font = local.themeFont(role).toQFont();
        EXPECT_EQ(font, global.themeFont(role).toQFont());
        EXPECT_TRUE(font.styleName().isEmpty());
        EXPECT_EQ(font.weight(), QFont::DemiBold);
        EXPECT_EQ(QFontInfo(font).weight(), QFont::DemiBold);
        EXPECT_GT(QFontInfo(font).weight(), QFontInfo(local.themeFont().toQFont()).weight());
    }
}
} // namespace

#include "QtTestEnvironment.h"
#include "components/textfields/Label.h"

TEST_F(ThemeOverridesTest, VisualCheckLocalThemeOverrides)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Visual review is opt-in";
    const bool dark = qEnvironmentVariableIntValue("LOCAL_THEME_DARK") != 0;
    fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Dark
                                         : fluent::FluentElement::Light);
    ThemeProbe window;
    window.resize(640, 340);
    window.setAutoFillBackground(true);
    QPalette palette = window.palette();
    palette.setColor(QPalette::Window, window.themeColors().bgCanvas);
    window.setPalette(palette);
    auto* layout = new fluent::AnchorLayout(&window);
    auto* title = new fluent::textfields::Label("Global theme / local overrides", &window);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    title->anchors()->top = {&window, fluent::AnchorLayout::Edge::Top, 24};
    title->anchors()->left = {&window, fluent::AnchorLayout::Edge::Left, 24};
    layout->addWidget(title);
    QWidget* previous = title;
    for (int index = 0; index < 3; ++index) {
        auto* button = new fluent::basicinput::Button(index == 0   ? "Global defaults"
                                                      : index == 1 ? "Local colors, font and radius"
                                                                   : "Cleared: global defaults",
                                                      &window);
        if (index != 0) {
            button->setThemeOverrides(
                {{"light", QJsonObject{{"controlDefault", "#FFF0F7"}, {"textPrimary", "#7A2454"}}},
                 {"dark", QJsonObject{{"controlDefault", "#382331"}, {"textPrimary", "#FFABD8"}}},
                 {"radius", QJsonObject{{"control", 12}}},
                 {"font", QJsonObject{{"scale", 1.5}}}});
        }
        if (index == 2)
            button->clearThemeOverrides();
        button->anchors()->top = {previous, fluent::AnchorLayout::Edge::Bottom, 24};
        button->anchors()->left = {&window, fluent::AnchorLayout::Edge::Left, 24};
        button->anchors()->right = {&window, fluent::AnchorLayout::Edge::Right, -24};
        button->setMinimumHeight(48);
        layout->addWidget(button);
        previous = button;
    }
    window.show();
    if (tests::support::isVisualSnapshotMode()) {
        tests::support::VisualSnapshotOptions options;
        options.windowSize = window.size();
        options.variant = dark ? "local-theme-dark" : "local-theme-light";
        options.theme = dark ? tests::support::VisualSnapshotTheme::Dark
                             : tests::support::VisualSnapshotTheme::Light;
        EXPECT_TRUE(tests::support::captureVisualSnapshot(&window, options));
        return;
    }
    qApp->exec();
}
