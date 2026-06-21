#include "components/foundation/FluentElement.h"
#include "components/foundation/private/FluentElement_p.h"
#include "design/ThemeColors.h"
#include "design/Typography.h"
#include "design/Spacing.h"
#include "design/CornerRadius.h"
#include "design/Elevation.h"
#include "design/Animation.h"
#include "design/Material.h"
#include "design/Breakpoints.h"

namespace fluent {

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

// --- Token accessors. zh_CN: 数据获取实现。---

FluentElement::Colors FluentElement::themeColors() const {
    Colors c;

    // One fill lambda keeps the Light/Dark blocks from duplicating each other.
    // zh_CN: 用 lambda 统一填充，避免 Light/Dark 两块完全重复。
    auto fill = [&](
        const QColor& accentDef, const QColor& accentSec, const QColor& accentTer, const QColor& accentDis,
        const QColor& ctrlDef, const QColor& ctrlSec, const QColor& ctrlTer, const QColor& ctrlDis,
        const QColor& ctrlAltSec, const QColor& ctrlAltTer,
        const QColor& subtleTrans, const QColor& subtleSec, const QColor& subtleTer,
        const QColor& strokeDef, const QColor& strokeSec, const QColor& strokeStr,
        const QColor& strokeCrd, const QColor& strokeDiv, const QColor& strokeSurf,
        const QColor& strokeFocOut, const QColor& strokeFocIn,
        const QColor& txtPri, const QColor& txtSec, const QColor& txtTer, const QColor& txtDis,
        const QColor& txtOnAcc, const QColor& txtAccPri,
        const QColor& bgCvs, const QColor& bgLyr, const QColor& bgLyrAlt, const QColor& bgSld,
        const QColor& g10, const QColor& g20, const QColor& g30, const QColor& g40,
        const QColor& g50, const QColor& g60, const QColor& g90,
        const QColor& g130, const QColor& g160, const QColor& g190,
        const QColor& sysCrit, const QColor& sysCritBg,
        const QColor& sysCau, const QColor& sysCauBg,
        const QColor& sysInfo, const QColor& sysInfoBg,
        const QColor& sysSucc, const QColor& sysSuccBg,
        const std::vector<QColor>& charts)
    {
        c.accentDefault = accentDef; c.accentSecondary = accentSec;
        c.accentTertiary = accentTer; c.accentDisabled = accentDis;
        c.controlDefault = ctrlDef; c.controlSecondary = ctrlSec;
        c.controlTertiary = ctrlTer; c.controlDisabled = ctrlDis;
        c.controlAltSecondary = ctrlAltSec; c.controlAltTertiary = ctrlAltTer;
        c.subtleTransparent = subtleTrans; c.subtleSecondary = subtleSec; c.subtleTertiary = subtleTer;
        c.strokeDefault = strokeDef; c.strokeSecondary = strokeSec; c.strokeStrong = strokeStr;
        c.strokeCard = strokeCrd; c.strokeDivider = strokeDiv; c.strokeSurface = strokeSurf;
        c.strokeFocusOuter = strokeFocOut; c.strokeFocusInner = strokeFocIn;
        c.textPrimary = txtPri; c.textSecondary = txtSec;
        c.textTertiary = txtTer; c.textDisabled = txtDis;
        c.textOnAccent = txtOnAcc; c.textAccentPrimary = txtAccPri;
        c.bgCanvas = bgCvs; c.bgLayer = bgLyr; c.bgLayerAlt = bgLyrAlt; c.bgSolid = bgSld;
        c.grey10 = g10; c.grey20 = g20; c.grey30 = g30; c.grey40 = g40;
        c.grey50 = g50; c.grey60 = g60; c.grey90 = g90;
        c.grey130 = g130; c.grey160 = g160; c.grey190 = g190;
        c.systemCritical = sysCrit; c.systemCriticalBg = sysCritBg;
        c.systemCaution = sysCau;   c.systemCautionBg = sysCauBg;
        c.systemInfo = sysInfo;     c.systemInfoBg = sysInfoBg;
        c.systemSuccess = sysSucc;  c.systemSuccessBg = sysSuccBg;
        c.charts = QList<QColor>(charts.begin(), charts.end());
    };

    if (currentTheme() == Dark) {
        using namespace ThemeColors::Dark;
        fill(Fill::AccentDefault, Fill::AccentSecondary, Fill::AccentTertiary, Fill::AccentDisabled,
             Fill::ControlDefault, Fill::ControlSecondary, Fill::ControlTertiary, Fill::ControlDisabled,
             Fill::ControlAltSecondary, Fill::ControlAltTertiary,
             Fill::SubtleTransparent, Fill::SubtleSecondary, Fill::SubtleTertiary,
             Stroke::ControlDefault, Stroke::ControlSecondary, Stroke::ControlStrong,
             Stroke::CardDefault, Stroke::DividerDefault, Stroke::SurfaceDefault,
             Stroke::FocusOuter, Stroke::FocusInner,
             Text::Primary, Text::Secondary, Text::Tertiary, Text::Disabled,
             Text::OnAccentPrimary, Text::AccentPrimary,
             BackgroundCanvas, BackgroundLayer, BackgroundLayerAlt, BackgroundSolid,
             Grey10, Grey20, Grey30, Grey40, Grey50, Grey60, Grey90, Grey130, Grey160, Grey190,
             System::Critical, System::CriticalBackground,
             System::Caution, System::CautionBackground,
             System::Informational, System::InfoBackground,
             System::Success, System::SuccessBackground,
             Charts);
    } else {
        using namespace ThemeColors::Light;
        fill(Fill::AccentDefault, Fill::AccentSecondary, Fill::AccentTertiary, Fill::AccentDisabled,
             Fill::ControlDefault, Fill::ControlSecondary, Fill::ControlTertiary, Fill::ControlDisabled,
             Fill::ControlAltSecondary, Fill::ControlAltTertiary,
             Fill::SubtleTransparent, Fill::SubtleSecondary, Fill::SubtleTertiary,
             Stroke::ControlDefault, Stroke::ControlSecondary, Stroke::ControlStrong,
             Stroke::CardDefault, Stroke::DividerDefault, Stroke::SurfaceDefault,
             Stroke::FocusOuter, Stroke::FocusInner,
             Text::Primary, Text::Secondary, Text::Tertiary, Text::Disabled,
             Text::OnAccentPrimary, Text::AccentPrimary,
             BackgroundCanvas, BackgroundLayer, BackgroundLayerAlt, BackgroundSolid,
             Grey10, Grey20, Grey30, Grey40, Grey50, Grey60, Grey90, Grey130, Grey160, Grey190,
             System::Critical, System::CriticalBackground,
             System::Caution, System::CautionBackground,
             System::Informational, System::InfoBackground,
             System::Success, System::SuccessBackground,
             Charts);
    }
    return c;
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

    return { s.family, s.styleName, s.size, s.weight, s.lineHeight };
}

FluentElement::Radius FluentElement::themeRadius() const {
    return { ::CornerRadius::None, ::CornerRadius::Control, ::CornerRadius::Overlay };
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
    return Material::Acrylic::get(currentTheme() == Dark);
}

Material::MicaToken FluentElement::themeMica() const {
    return Material::Mica::get(currentTheme() == Dark);
}

Material::SmokeToken FluentElement::themeSmoke() const {
    return Material::Smoke::get(currentTheme() == Dark);
}

Elevation::ShadowParams FluentElement::themeShadow(Elevation::Level level) const {
    return Elevation::getShadow(level, currentTheme() == Dark);
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

} // namespace fluent
