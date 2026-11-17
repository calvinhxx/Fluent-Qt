#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/private/ThemeSpec_p.h"
#include "components/foundation/private/FluentElement_p.h"
#include "design/Animation.h"
#include "design/Breakpoints.h"
#include "design/CornerRadius.h"
#include "design/Elevation.h"
#include "design/Material.h"
#include "design/Spacing.h"
#include "design/ThemeColors.h"
#include "design/Typography.h"

#include <QVariant>
#include <QWidget>

namespace fluent {

class FluentElementPrivate {
public:
    QJsonObject overrides;
    ThemeRegistry::ExtendedSnapshot resolved;
    int registryRevision = -1;
};

namespace {

constexpr char kThemeOverrideProperty[] = "fluentThemeOverride";

bool themeFromProperty(const QVariant& value, FluentElement::Theme& theme)
{
    if (!value.isValid())
        return false;

    bool ok = false;
    const int rawTheme = value.toInt(&ok);
    if (ok) {
        if (rawTheme == FluentElement::Light || rawTheme == FluentElement::Dark
            || rawTheme == FluentElement::HighContrast) {
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
    if (text.compare(QStringLiteral("HighContrast"), Qt::CaseInsensitive) == 0) {
        theme = FluentElement::HighContrast;
        return true;
    }
    return false;
}

} // namespace

FluentElement::FluentElement() : d_ptr(nullptr)
{
    FluentThemeManager::instance()->elements.insert(this);
}

FluentElement::~FluentElement()
{
    FluentThemeManager::instance()->elements.remove(this);
    delete d_ptr;
}

void FluentElement::setTheme(Theme theme)
{
    auto* mgr = FluentThemeManager::instance();
    if (mgr->currentTheme != theme) {
        mgr->currentTheme = theme;
        mgr->notifyAll();
    }
}

void FluentElement::setThemeDeferred(Theme theme)
{
    auto* mgr = FluentThemeManager::instance();
    if (mgr->currentTheme != theme) {
        mgr->currentTheme = theme;
        mgr->notifyVisibleThenDeferred();
    }
}

FluentElement::Theme FluentElement::currentTheme()
{
    return FluentThemeManager::instance()->currentTheme;
}

bool FluentElement::themeUsesDarkAppearance(Theme theme)
{
    switch (theme) {
    case Light:
        return false;
    case Dark:
    case HighContrast:
        return true;
    }
    return false;
}

void FluentElement::refreshTheme()
{
    FluentThemeManager::instance()->notifyVisibleThenDeferred();
}

int FluentElement::themeGeneration()
{
    return FluentThemeManager::instance()->generation();
}

FluentElement::Theme FluentElement::effectiveTheme() const
{
    const auto* widget = dynamic_cast<const QWidget*>(this);
    for (const QWidget* node = widget; node; node = node->parentWidget()) {
        Theme overriddenTheme = currentTheme();
        if (themeFromProperty(node->property(kThemeOverrideProperty), overriddenTheme))
            return overriddenTheme;
    }
    return currentTheme();
}

bool FluentElement::effectiveThemeUsesDarkAppearance() const
{
    return themeUsesDarkAppearance(effectiveTheme());
}

FluentElement::Colors FluentElement::themeColors() const
{
    return themeColorsRef();
}

bool FluentElement::setThemeOverrides(const QJsonObject& overrides)
{
    if (!detail::validateThemeSpec(overrides) || themeOverrides() == overrides)
        return false;
    if (!d_ptr)
        d_ptr = new FluentElementPrivate;
    d_ptr->overrides = overrides;
    d_ptr->registryRevision = -1;
    // onThemeUpdated() may destroy this element; do not access members afterward.
    if (auto* widget = dynamic_cast<QWidget*>(this)) {
        widget->updateGeometry();
        widget->update();
    }
    onThemeUpdated();
    return true;
}

QJsonObject FluentElement::themeOverrides() const
{
    return d_ptr ? d_ptr->overrides : QJsonObject();
}

void FluentElement::clearThemeOverrides()
{
    setThemeOverrides({});
}

void FluentElement::resolveThemeOverrides() const
{
    const auto& registry = ThemeRegistry::instance();
    if (d_ptr->registryRevision == registry.revision())
        return;
    d_ptr->resolved = registry.extendedSnapshot();
    detail::applyThemeSpec(d_ptr->resolved, d_ptr->overrides);
    d_ptr->registryRevision = registry.revision();
}

const FluentElement::Colors& FluentElement::themeColorsRef() const
{
    const Theme theme = effectiveTheme();
    if (!d_ptr || d_ptr->overrides.isEmpty())
        return ThemeRegistry::instance().colors(theme);
    resolveThemeOverrides();
    if (theme == Dark)
        return d_ptr->resolved.base.darkColors;
    if (theme == HighContrast)
        return d_ptr->resolved.contrastColors;
    return d_ptr->resolved.base.lightColors;
}

FluentElement::FontStyle FluentElement::themeFont(Typography::FontRole role) const
{
    if (!d_ptr || d_ptr->overrides.isEmpty())
        return ThemeRegistry::instance().resolvedFontStyle(role);
    resolveThemeOverrides();
    const auto& snapshot = d_ptr->resolved.base;
    const auto& base = Typography::fontStyle(role);
    const bool defaultFamily = snapshot.fontFamilyOverride.isEmpty();
    return {defaultFamily ? base.family : snapshot.fontFamilyOverride, defaultFamily ? base.styleName : QString(),
        qRound(base.size * snapshot.fontScale), base.weight, qRound(base.lineHeight * snapshot.fontScale)};
}

FluentElement::Radius FluentElement::themeRadius() const
{
    if (!d_ptr || d_ptr->overrides.isEmpty())
        return ThemeRegistry::instance().radius();
    resolveThemeOverrides();
    return d_ptr->resolved.base.radius;
}

FluentElement::Spacing FluentElement::themeSpacing() const
{
    Spacing s;
    s.padding = {::Spacing::Padding::ControlHorizontal, ::Spacing::Padding::ControlVertical, ::Spacing::Padding::Card,
        ::Spacing::Padding::Dialog, ::Spacing::Padding::TextFieldHorizontal, ::Spacing::Padding::TextFieldVertical,
        ::Spacing::Padding::ListItemHorizontal, ::Spacing::Padding::ListItemVertical};
    s.gap = {::Spacing::Gap::Tight, ::Spacing::Gap::Normal, ::Spacing::Gap::Loose, ::Spacing::Gap::Section};
    s.controlHeight = {::Spacing::ControlHeight::Small, ::Spacing::ControlHeight::Standard,
        ::Spacing::ControlHeight::Large};
    s.xSmall = ::Spacing::XSmall;
    s.small = ::Spacing::Small;
    s.medium = ::Spacing::Medium;
    s.standard = ::Spacing::Standard;
    s.large = ::Spacing::Large;
    s.xLarge = ::Spacing::XLarge;
    s.xxLarge = ::Spacing::XXLarge;
    return s;
}

FluentElement::Animation FluentElement::themeAnimation() const
{
    using namespace ::Animation;
    return {Duration::Fast, Duration::Normal, Duration::Slow, Duration::VerySlow, getEasing(EasingType::Standard),
        getEasing(EasingType::Accelerate), getEasing(EasingType::Decelerate), getEasing(EasingType::Entrance),
        getEasing(EasingType::Exit)};
}

Material::AcrylicToken FluentElement::themeAcrylic() const
{
    return Material::Acrylic::get(effectiveThemeUsesDarkAppearance());
}

Material::MicaToken FluentElement::themeMica() const
{
    return Material::Mica::get(effectiveThemeUsesDarkAppearance());
}

Material::SmokeToken FluentElement::themeSmoke() const
{
    return Material::Smoke::get(effectiveThemeUsesDarkAppearance());
}

Elevation::ShadowParams FluentElement::themeShadow(Elevation::Level level) const
{
    return Elevation::getShadow(level, effectiveThemeUsesDarkAppearance());
}

int FluentElement::themeBreakpoint(Breakpoints::Breakpoint breakpoint) const
{
    return Breakpoints::value(breakpoint);
}

QColor FluentElement::themeBackdrop(bool active) const
{
    const Colors& c = themeColorsRef();
    if (active)
        return c.bgCanvas;
    // Inactive windows blend 70% toward bgLayer so chrome visibly dims on focus loss (Mica inactive fallback).
    constexpr qreal t = 0.7;
    return QColor::fromRgbF(c.bgCanvas.redF() + (c.bgLayer.redF() - c.bgCanvas.redF()) * t,
        c.bgCanvas.greenF() + (c.bgLayer.greenF() - c.bgCanvas.greenF()) * t,
        c.bgCanvas.blueF() + (c.bgLayer.blueF() - c.bgCanvas.blueF()) * t);
}

} // namespace fluent
