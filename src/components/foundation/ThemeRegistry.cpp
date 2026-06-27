#include "components/foundation/ThemeRegistry.h"

#include "design/CornerRadius.h"
#include "design/ThemeColors.h"

namespace fluent {

ThemeRegistry& ThemeRegistry::instance()
{
    static ThemeRegistry registry;
    return registry;
}

ThemeRegistry::ThemeRegistry()
{
    seedDefaults();
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
    (dark ? m_dark : m_light) = colors;
    ++m_revision;
}

void ThemeRegistry::setRadius(int none, int control, int overlay)
{
    m_radiusNone = none;
    m_radiusControl = control;
    m_radiusOverlay = overlay;
    ++m_revision;
}

void ThemeRegistry::setDesignLanguage(FluentElement::DesignLanguage language)
{
    if (m_designLanguage == language)
        return;
    m_designLanguage = language;
    ++m_revision;
}

void ThemeRegistry::setFontFamilyOverride(const QString& family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    ++m_revision;
}

void ThemeRegistry::setFontScale(qreal scale)
{
    if (scale <= 0.0 || qFuzzyCompare(m_fontScale, scale))
        return;
    m_fontScale = scale;
    ++m_revision;
}

void ThemeRegistry::resetToDefaults()
{
    seedDefaults();
    ++m_revision;
}

} // namespace fluent
