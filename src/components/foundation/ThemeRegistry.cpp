#include "components/foundation/ThemeRegistry.h"

#include "design/CornerRadius.h"
#include "design/ThemeColors.h"

namespace fluent {
namespace {

using Colors = FluentElement::Colors;
using ColorMember = QColor Colors::*;

constexpr ColorMember kColorMembers[] = {
    &Colors::accentDefault,
    &Colors::accentSecondary,
    &Colors::accentTertiary,
    &Colors::accentDisabled,
    &Colors::controlDefault,
    &Colors::controlSecondary,
    &Colors::controlTertiary,
    &Colors::controlDisabled,
    &Colors::controlAltSecondary,
    &Colors::controlAltTertiary,
    &Colors::subtleTransparent,
    &Colors::subtleSecondary,
    &Colors::subtleTertiary,
    &Colors::strokeDefault,
    &Colors::strokeSecondary,
    &Colors::strokeStrong,
    &Colors::strokeCard,
    &Colors::strokeDivider,
    &Colors::strokeSurface,
    &Colors::strokeFocusOuter,
    &Colors::strokeFocusInner,
    &Colors::textPrimary,
    &Colors::textSecondary,
    &Colors::textTertiary,
    &Colors::textDisabled,
    &Colors::textOnAccent,
    &Colors::textAccentPrimary,
    &Colors::bgCanvas,
    &Colors::bgLayer,
    &Colors::bgLayerAlt,
    &Colors::bgSolid,
    &Colors::grey10,
    &Colors::grey20,
    &Colors::grey30,
    &Colors::grey40,
    &Colors::grey50,
    &Colors::grey60,
    &Colors::grey90,
    &Colors::grey130,
    &Colors::grey160,
    &Colors::grey190,
    &Colors::systemCritical,
    &Colors::systemCriticalBg,
    &Colors::systemCaution,
    &Colors::systemCautionBg,
    &Colors::systemInfo,
    &Colors::systemInfoBg,
    &Colors::systemSuccess,
    &Colors::systemSuccessBg
};

bool colorsEqual(const Colors& lhs, const Colors& rhs)
{
    for (ColorMember member : kColorMembers) {
        if (lhs.*member != rhs.*member)
            return false;
    }
    return lhs.charts == rhs.charts;
}

bool snapshotsEqual(const ThemeRegistry::Snapshot& lhs,
                    const ThemeRegistry::Snapshot& rhs)
{
    return colorsEqual(lhs.lightColors, rhs.lightColors)
        && colorsEqual(lhs.darkColors, rhs.darkColors)
        && lhs.radius.none == rhs.radius.none
        && lhs.radius.control == rhs.radius.control
        && lhs.radius.overlay == rhs.radius.overlay
        && lhs.designLanguage == rhs.designLanguage
        && lhs.fontFamilyOverride == rhs.fontFamilyOverride
        && qFuzzyCompare(lhs.fontScale, rhs.fontScale);
}

bool snapshotIsValid(const ThemeRegistry::Snapshot& snapshot)
{
    return snapshot.radius.none >= 0
        && snapshot.radius.control >= 0
        && snapshot.radius.overlay >= 0
        && snapshot.fontScale > 0.0
        && qIsFinite(snapshot.fontScale);
}

} // namespace

ThemeRegistry& ThemeRegistry::instance()
{
    static ThemeRegistry registry;
    return registry;
}

ThemeRegistry::ThemeRegistry()
{
    seedDefaults();
}

ThemeRegistry::ThemeRegistry(UninitializedTag)
{
}

ThemeRegistry::Snapshot ThemeRegistry::snapshot() const
{
    Snapshot result;
    result.lightColors = m_light;
    result.darkColors = m_dark;
    result.radius = radius();
    result.designLanguage = m_designLanguage;
    result.fontFamilyOverride = m_fontFamily;
    result.fontScale = m_fontScale;
    return result;
}

ThemeRegistry::Snapshot ThemeRegistry::defaultSnapshot()
{
    ThemeRegistry defaults(UninitializedTag{});
    defaults.seedDefaults();
    return defaults.snapshot();
}

bool ThemeRegistry::applySnapshot(const Snapshot& next)
{
    if (!snapshotIsValid(next) || snapshotsEqual(snapshot(), next))
        return false;

    m_light = next.lightColors;
    m_dark = next.darkColors;
    m_radiusNone = next.radius.none;
    m_radiusControl = next.radius.control;
    m_radiusOverlay = next.radius.overlay;
    m_designLanguage = next.designLanguage;
    m_fontFamily = next.fontFamilyOverride;
    m_fontScale = next.fontScale;
    ++m_revision;

    FluentElement::refreshTheme();
    return true;
}

void ThemeRegistry::seedDefaults()
{
    // Build the two default palettes directly from the design tokens. buildColors<> can't take a
    // namespace as a template argument, so resolve each namespace's members inline here.
    // zh_CN: 直接用设计 token 构建两套默认调色板。命名空间不能作模板实参,故在此就地解析各命名空间成员。
    {
        using namespace ThemeColors::Light;
        FluentElement::Colors& c = m_light;
        c.accentDefault = Fill::AccentDefault;
        c.accentSecondary = Fill::AccentSecondary;
        c.accentTertiary = Fill::AccentTertiary;
        c.accentDisabled = Fill::AccentDisabled;
        c.controlDefault = Fill::ControlDefault;
        c.controlSecondary = Fill::ControlSecondary;
        c.controlTertiary = Fill::ControlTertiary;
        c.controlDisabled = Fill::ControlDisabled;
        c.controlAltSecondary = Fill::ControlAltSecondary;
        c.controlAltTertiary = Fill::ControlAltTertiary;
        c.subtleTransparent = Fill::SubtleTransparent;
        c.subtleSecondary = Fill::SubtleSecondary;
        c.subtleTertiary = Fill::SubtleTertiary;
        c.strokeDefault = Stroke::ControlDefault;
        c.strokeSecondary = Stroke::ControlSecondary;
        c.strokeStrong = Stroke::ControlStrong;
        c.strokeCard = Stroke::CardDefault;
        c.strokeDivider = Stroke::DividerDefault;
        c.strokeSurface = Stroke::SurfaceDefault;
        c.strokeFocusOuter = Stroke::FocusOuter;
        c.strokeFocusInner = Stroke::FocusInner;
        c.textPrimary = Text::Primary;
        c.textSecondary = Text::Secondary;
        c.textTertiary = Text::Tertiary;
        c.textDisabled = Text::Disabled;
        c.textOnAccent = Text::OnAccentPrimary;
        c.textAccentPrimary = Text::AccentPrimary;
        c.bgCanvas = BackgroundCanvas;
        c.bgLayer = BackgroundLayer;
        c.bgLayerAlt = BackgroundLayerAlt;
        c.bgSolid = BackgroundSolid;
        c.grey10 = Grey10; c.grey20 = Grey20; c.grey30 = Grey30; c.grey40 = Grey40;
        c.grey50 = Grey50; c.grey60 = Grey60; c.grey90 = Grey90;
        c.grey130 = Grey130; c.grey160 = Grey160; c.grey190 = Grey190;
        c.systemCritical = System::Critical;     c.systemCriticalBg = System::CriticalBackground;
        c.systemCaution = System::Caution;        c.systemCautionBg = System::CautionBackground;
        c.systemInfo = System::Informational;     c.systemInfoBg = System::InfoBackground;
        c.systemSuccess = System::Success;        c.systemSuccessBg = System::SuccessBackground;
        c.charts = QList<QColor>(Charts.begin(), Charts.end());
    }
    {
        using namespace ThemeColors::Dark;
        FluentElement::Colors& c = m_dark;
        c.accentDefault = Fill::AccentDefault;
        c.accentSecondary = Fill::AccentSecondary;
        c.accentTertiary = Fill::AccentTertiary;
        c.accentDisabled = Fill::AccentDisabled;
        c.controlDefault = Fill::ControlDefault;
        c.controlSecondary = Fill::ControlSecondary;
        c.controlTertiary = Fill::ControlTertiary;
        c.controlDisabled = Fill::ControlDisabled;
        c.controlAltSecondary = Fill::ControlAltSecondary;
        c.controlAltTertiary = Fill::ControlAltTertiary;
        c.subtleTransparent = Fill::SubtleTransparent;
        c.subtleSecondary = Fill::SubtleSecondary;
        c.subtleTertiary = Fill::SubtleTertiary;
        c.strokeDefault = Stroke::ControlDefault;
        c.strokeSecondary = Stroke::ControlSecondary;
        c.strokeStrong = Stroke::ControlStrong;
        c.strokeCard = Stroke::CardDefault;
        c.strokeDivider = Stroke::DividerDefault;
        c.strokeSurface = Stroke::SurfaceDefault;
        c.strokeFocusOuter = Stroke::FocusOuter;
        c.strokeFocusInner = Stroke::FocusInner;
        c.textPrimary = Text::Primary;
        c.textSecondary = Text::Secondary;
        c.textTertiary = Text::Tertiary;
        c.textDisabled = Text::Disabled;
        c.textOnAccent = Text::OnAccentPrimary;
        c.textAccentPrimary = Text::AccentPrimary;
        c.bgCanvas = BackgroundCanvas;
        c.bgLayer = BackgroundLayer;
        c.bgLayerAlt = BackgroundLayerAlt;
        c.bgSolid = BackgroundSolid;
        c.grey10 = Grey10; c.grey20 = Grey20; c.grey30 = Grey30; c.grey40 = Grey40;
        c.grey50 = Grey50; c.grey60 = Grey60; c.grey90 = Grey90;
        c.grey130 = Grey130; c.grey160 = Grey160; c.grey190 = Grey190;
        c.systemCritical = System::Critical;     c.systemCriticalBg = System::CriticalBackground;
        c.systemCaution = System::Caution;        c.systemCautionBg = System::CautionBackground;
        c.systemInfo = System::Informational;     c.systemInfoBg = System::InfoBackground;
        c.systemSuccess = System::Success;        c.systemSuccessBg = System::SuccessBackground;
        c.charts = QList<QColor>(Charts.begin(), Charts.end());
    }

    m_radiusNone = ::CornerRadius::None;
    m_radiusControl = ::CornerRadius::Control;
    m_radiusOverlay = ::CornerRadius::Overlay;
    m_designLanguage = FluentElement::DesignFluent;
    m_fontFamily.clear();
    m_fontScale = 1.0;
}

void ThemeRegistry::setColors(bool dark, const FluentElement::Colors& colors)
{
    Snapshot next = snapshot();
    (dark ? next.darkColors : next.lightColors) = colors;
    applySnapshot(next);
}

void ThemeRegistry::setRadius(int none, int control, int overlay)
{
    Snapshot next = snapshot();
    next.radius = {none, control, overlay};
    applySnapshot(next);
}

void ThemeRegistry::setDesignLanguage(FluentElement::DesignLanguage language)
{
    Snapshot next = snapshot();
    next.designLanguage = language;
    applySnapshot(next);
}

void ThemeRegistry::setFontFamilyOverride(const QString& family)
{
    Snapshot next = snapshot();
    next.fontFamilyOverride = family;
    applySnapshot(next);
}

void ThemeRegistry::setFontScale(qreal scale)
{
    Snapshot next = snapshot();
    next.fontScale = scale;
    applySnapshot(next);
}

void ThemeRegistry::resetToDefaults()
{
    applySnapshot(defaultSnapshot());
}

} // namespace fluent
