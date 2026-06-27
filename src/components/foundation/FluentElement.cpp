#include "components/foundation/FluentElement.h"
#include "components/foundation/private/FluentElement_p.h"
#include "components/foundation/ThemeRegistry.h"

#include <QVariant>
#include <QWidget>
#include "design/ThemeColors.h"
#include "design/Typography.h"
#include "design/Spacing.h"
#include "design/CornerRadius.h"
#include "design/Elevation.h"
#include "design/Animation.h"
#include "design/Material.h"
#include "design/Breakpoints.h"

namespace fluent {

namespace {

constexpr char kThemeOverrideProperty[] = "fluentThemeOverride";

bool themeFromProperty(const QVariant& value, FluentElement::Theme& theme)
{
    if (!value.isValid())
        return false;

    bool ok = false;
    const int rawTheme = value.toInt(&ok);
    if (ok) {
        if (rawTheme == FluentElement::Light || rawTheme == FluentElement::Dark) {
            theme = static_cast<FluentElement::Theme>(rawTheme);
            return true;
        }
        return false;
    }

    const QString text = value.toString().trimmed();
    if (text.compare(QStringLiteral("Light"), Qt::CaseInsensitive) == 0) {
        theme = FluentElement::Light;
        return true;
    }
    if (text.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0) {
        theme = FluentElement::Dark;
        return true;
    }
    return false;
}

} // namespace

// --- FluentElement lifetime management. zh_CN: FluentElement 生命周期管理。---

FluentElement::FluentElement() : d_ptr(new FluentElementPrivate(this)) {
    FluentThemeManager::instance()->elements.insert(this);
}

FluentElement::~FluentElement() {
    FluentThemeManager::instance()->elements.remove(this);
    delete d_ptr;
}

// --- Static global management. zh_CN: 静态全局管理。---

void FluentElement::setTheme(Theme theme) {
    auto* mgr = FluentThemeManager::instance();
    if (mgr->currentTheme != theme) {
        mgr->currentTheme = theme;
        mgr->notifyAll();
    }
}

void FluentElement::setThemeDeferred(Theme theme) {
    auto* mgr = FluentThemeManager::instance();
    if (mgr->currentTheme != theme) {
        mgr->currentTheme = theme;
        mgr->notifyVisibleThenDeferred();
    }
}


FluentElement::Theme FluentElement::currentTheme() {
    return FluentThemeManager::instance()->currentTheme;
}

void FluentElement::refreshTheme() {
    FluentThemeManager::instance()->notifyVisibleThenDeferred();
}

int FluentElement::themeGeneration() {
    return FluentThemeManager::instance()->generation();
}

FluentElement::Theme FluentElement::effectiveTheme() const {
    const auto* widget = dynamic_cast<const QWidget*>(this);
    for (const QWidget* node = widget; node; node = node->parentWidget()) {
        Theme overriddenTheme = currentTheme();
        if (themeFromProperty(node->property(kThemeOverrideProperty), overriddenTheme))
            return overriddenTheme;
    }
    return currentTheme();
}

// --- Token accessors. zh_CN: 数据获取实现。---

FluentElement::Colors FluentElement::themeColors() const {
    // Colors now come from the runtime ThemeRegistry (seeded with the built-in Fluent palette, so the
    // default result is identical to the former compile-time construction). The app layer can install a
    // brand preset or user-file overrides without touching any control. effectiveTheme() still honors a
    // per-subtree fluentThemeOverride. zh_CN: 颜色改由运行时 ThemeRegistry 提供(以内置 Fluent 调色板播种,
    // 默认结果与原编译期构造完全一致)。应用层可安装品牌预设或用户文件覆盖而不动任何控件;effectiveTheme()
    // 仍尊重子树级 fluentThemeOverride。
    return ThemeRegistry::instance().colors(effectiveTheme() == Dark);
}

FluentElement::FontStyle FluentElement::themeFont(const QString& role) const {
    Typography::FontStyle s;
    if      (role == Typography::FontRole::Caption)         s = Typography::Styles::Caption;
    else if (role == Typography::FontRole::BodyStrong)      s = Typography::Styles::BodyStrong;
    else if (role == Typography::FontRole::BodyLarge)       s = Typography::Styles::BodyLarge;
    else if (role == Typography::FontRole::BodyLargeStrong) s = Typography::Styles::BodyLargeStrong;
    else if (role == Typography::FontRole::Subtitle)        s = Typography::Styles::Subtitle;
    else if (role == Typography::FontRole::Title)           s = Typography::Styles::Title;
    else if (role == Typography::FontRole::TitleLarge)      s = Typography::Styles::TitleLarge;
    else if (role == Typography::FontRole::Display)         s = Typography::Styles::Display;
    else                                                    s = Typography::Styles::Body;

    // Apply the registry's optional family override + size scale so a brand preset (Roboto / SF) or a
    // user config can restyle text without touching every control. A family override clears the Segoe
    // optical styleName, which would not apply to a different family. Defaults (empty family, scale 1.0)
    // reproduce the original style byte-for-byte. zh_CN: 套用注册表的可选字族覆盖与字号缩放,使品牌预设
    //(Roboto / SF)或用户配置无需改控件即可重塑文字;覆盖字族时清空 Segoe 光学 styleName(对异族无效)。
    // 默认(空字族、缩放 1.0)逐字节复现原样式。
    const ThemeRegistry& registry = ThemeRegistry::instance();
    const QString familyOverride = registry.fontFamilyOverride();
    const qreal scale = registry.fontScale();
    const QString family = familyOverride.isEmpty() ? s.family : familyOverride;
    const QString styleName = familyOverride.isEmpty() ? s.styleName : QString();
    return { family, styleName, qRound(s.size * scale), s.weight, qRound(s.lineHeight * scale) };
}

FluentElement::Radius FluentElement::themeRadius() const {
    return ThemeRegistry::instance().radius();
}

FluentElement::DesignLanguage FluentElement::themeDesignLanguage() const {
    return ThemeRegistry::instance().designLanguage();
}

FluentElement::Spacing FluentElement::themeSpacing() const {
    Spacing s;
    s.padding = {
        ::Spacing::Padding::ControlHorizontal,  ::Spacing::Padding::ControlVertical,
        ::Spacing::Padding::Card,               ::Spacing::Padding::Dialog,
        ::Spacing::Padding::TextFieldHorizontal, ::Spacing::Padding::TextFieldVertical,
        ::Spacing::Padding::ListItemHorizontal,  ::Spacing::Padding::ListItemVertical
    };
    s.gap = {
        ::Spacing::Gap::Tight,  ::Spacing::Gap::Normal,
        ::Spacing::Gap::Loose,  ::Spacing::Gap::Section
    };
    s.controlHeight = {
        ::Spacing::ControlHeight::Small,
        ::Spacing::ControlHeight::Standard,
        ::Spacing::ControlHeight::Large
    };
    s.xSmall   = ::Spacing::XSmall;
    s.small    = ::Spacing::Small;
    s.medium   = ::Spacing::Medium;
    s.standard = ::Spacing::Standard;
    s.large    = ::Spacing::Large;
    s.xLarge   = ::Spacing::XLarge;
    s.xxLarge  = ::Spacing::XXLarge;
    return s;
}

FluentElement::Animation FluentElement::themeAnimation() const {
    using namespace ::Animation;
    return {
        Duration::Fast, Duration::Normal, Duration::Slow, Duration::VerySlow,
        getEasing(EasingType::Standard),
        getEasing(EasingType::Accelerate),
        getEasing(EasingType::Decelerate),
        getEasing(EasingType::Entrance),
        getEasing(EasingType::Exit)
    };
}

Material::AcrylicToken FluentElement::themeAcrylic() const {
    return Material::Acrylic::get(effectiveTheme() == Dark);
}

Material::MicaToken FluentElement::themeMica() const {
    return Material::Mica::get(effectiveTheme() == Dark);
}

Material::SmokeToken FluentElement::themeSmoke() const {
    return Material::Smoke::get(effectiveTheme() == Dark);
}

Elevation::ShadowParams FluentElement::themeShadow(Elevation::Level level) const {
    return Elevation::getShadow(level, effectiveTheme() == Dark);
}

int FluentElement::themeBreakpoint(const QString& size) const {
    if (size == "Small") return Breakpoints::Small;
    if (size == "Large") return Breakpoints::Large;
    return Breakpoints::Medium;
}

QColor FluentElement::themeBackdrop(bool active) const {
    const Colors c = themeColors();
    if (active)
        return c.bgCanvas;  // Standard chrome tint, consistent with the rest of the surfaces.
    // Inactive: wash the canvas tint most of the way toward the content layer so the chrome
    // visibly flattens/lightens when the window loses focus (cross-platform stand-in for
    // Mica's inactive fallback — no wallpaper tint, but a clear active/inactive cue).
    // zh_CN: 非激活：把 canvas 色调大幅推向内容层，使窗口失焦时 chrome 明显变扁/变浅
    //（跨平台替代 Mica 非激活回退——没有壁纸着色，但有清晰的激活/非激活区分）。
    constexpr qreal t = 0.7;
    return QColor::fromRgbF(c.bgCanvas.redF()   + (c.bgLayer.redF()   - c.bgCanvas.redF())   * t,
                            c.bgCanvas.greenF() + (c.bgLayer.greenF() - c.bgCanvas.greenF()) * t,
                            c.bgCanvas.blueF()  + (c.bgLayer.blueF()  - c.bgCanvas.blueF())  * t);
}

QColor FluentElement::chromeBackdropFill(const QWidget* hostWindow, bool active) const {
    // The host window publishes "fluentMicaBackdrop" when it carries a real OS-composited backdrop
    // (Win11 Mica / macOS vibrancy). The single read of that contract lives here, so chrome surfaces
    // never re-derive it: when present, paint transparent (invalid color) to show it through;
    // otherwise paint the solid themeBackdrop for the activation state.
    // zh_CN: 宿主窗口带真实系统合成背景（Win11 Mica / macOS vibrancy）时会发布 "fluentMicaBackdrop"。该契约
    // 的唯一读取点集中在此，chrome 表面不再各自推导：有则画透明（无效色）透出背景；否则按激活态画纯色 themeBackdrop。
    if (hostWindow && hostWindow->property("fluentMicaBackdrop").toBool())
        return QColor();  // invalid => caller erases to transparent
    return themeBackdrop(active);
}

} // namespace fluent
