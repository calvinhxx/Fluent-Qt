#include "WindowBackdrop.h"
#include "components/foundation/FluentElement.h"
#include "design/Material.h"

#include <algorithm>
#include <QVariant>
#include <QWidget>

namespace fluent::windowing {
namespace {

constexpr char kBackdropStateProperty[] = "fluentWindowBackdropState";
constexpr char kWindowBackdropEffectProperty[] = "fluentWindowBackdropEffect";
constexpr int kBackdropEffectSolid = 0;
constexpr int kBackdropEffectMica = 1;
constexpr int kBackdropEffectAcrylic = 2;

const QWidget* topLevelFor(const QWidget* widget)
{
    return widget ? widget->window() : nullptr;
}

QColor blendRgb(const QColor& from, const QColor& to, qreal amount)
{
    amount = std::max<qreal>(0.0, std::min<qreal>(1.0, amount));
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

int backdropEffectFromProperty(const QVariant& value)
{
    bool ok = false;
    const int effect = value.toInt(&ok);
    if (!ok)
        return kBackdropEffectSolid;
    if (effect == kBackdropEffectMica || effect == kBackdropEffectAcrylic)
        return effect;
    return kBackdropEffectSolid;
}

} // namespace

BackdropState windowBackdropState(const QWidget* widget)
{
    BackdropState state;
    tryWindowBackdropState(widget, &state);
    return state;
}

bool tryWindowBackdropState(const QWidget* widget, BackdropState* state)
{
    const QWidget* topLevel = topLevelFor(widget);
    if (!topLevel || !state)
        return false;

    const QVariant value = topLevel->property(kBackdropStateProperty);
    if (!value.isValid() || !value.canConvert<BackdropState>())
        return false;
    *state = value.value<BackdropState>();
    return true;
}

void publishWindowBackdropState(QWidget* window, const BackdropState& state)
{
    QWidget* topLevel = window ? window->window() : nullptr;
    if (!topLevel)
        return;
    topLevel->setProperty(kBackdropStateProperty, QVariant::fromValue(state));
}

bool windowBackdropRequiresTransparentClear(const QWidget* widget)
{
    return windowBackdropState(widget).surfaceMode == BackdropSurfaceMode::CompositedTransparent;
}

bool windowBackdropUsesPaintedMaterial(const QWidget* widget)
{
    return windowBackdropState(widget).surfaceMode == BackdropSurfaceMode::PaintedOpaque;
}

bool windowHasMaterialBackdrop(const QWidget* widget)
{
    return windowBackdropState(widget).effectiveEffect != BackdropEffect::Solid;
}

QColor windowChromeBackdropFill(const FluentElement& themeHost, const QWidget* hostWindow,
                                bool active)
{
    BackdropState typedState;
    const bool hasTypedState = tryWindowBackdropState(hostWindow, &typedState);
    if (hasTypedState && typedState.surfaceMode == BackdropSurfaceMode::CompositedTransparent) {
        return QColor();
    }
    if (!hasTypedState && hostWindow && hostWindow->property("fluentMicaBackdrop").toBool()) {
        return QColor();
    }

    const int requestedEffect =
        hasTypedState
            ? static_cast<int>(typedState.requestedEffect)
            : (hostWindow
                   ? backdropEffectFromProperty(hostWindow->property(kWindowBackdropEffectProperty))
                   : kBackdropEffectSolid);
    const bool dark = themeHost.effectiveThemeUsesDarkAppearance();
    const auto& colors = themeHost.themeColorsRef();

    if (requestedEffect == kBackdropEffectMica) {
        const Material::MicaToken mica = Material::Mica::get(dark);
        const QColor target = active ? colors.bgLayerAlt : colors.bgLayer;
        return blendRgb(mica.baseColor, target, active ? 0.10 : 0.35);
    }

    if (requestedEffect == kBackdropEffectAcrylic) {
        const Material::AcrylicToken acrylic = Material::Acrylic::get(dark);
        const QColor target = active ? colors.bgLayerAlt : colors.bgLayer;
        return blendRgb(acrylic.tintColor, target, active ? 0.22 : 0.45);
    }

    return themeHost.themeBackdrop(active);
}

} // namespace fluent::windowing
