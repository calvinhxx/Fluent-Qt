#include "WindowBackdrop.h"
#include "components/windowing/private/WindowBackdrop_p.h"

#include <QCoreApplication>
#include <QEvent>
#include <QVariant>
#include <QWidget>

namespace fluent::windowing {
namespace {

constexpr char kBackdropStateProperty[] = "fluentWindowBackdropState";

const QWidget* topLevelFor(const QWidget* widget)
{
    return widget ? widget->window() : nullptr;
}

QEvent::Type reevaluationEventType()
{
    static const int type = QEvent::registerEventType();
    return static_cast<QEvent::Type>(type);
}

} // namespace

bool BackdropCapabilities::supportsNative(BackdropEffect effect) const
{
    if (effect == BackdropEffect::Mica)
        return nativeMica;
    if (effect == BackdropEffect::Acrylic)
        return nativeAcrylic;
    return true;
}

bool BackdropCapabilities::supportsCompositor(BackdropEffect effect) const
{
    // A generic blur-behind protocol represents Acrylic's live background
    // sampling, not Mica's stable wallpaper-tinted material. Platforms with a
    // real Mica implementation advertise it through nativeMica instead.
    return effect == BackdropEffect::Acrylic && compositorBlur;
}

bool BackdropCapabilities::supportsTransparentMaterial(BackdropEffect effect) const
{
    return effect != BackdropEffect::Solid
        && alphaSurfaceSupported
        && (supportsNative(effect) || supportsCompositor(effect));
}

bool BackdropState::operator==(const BackdropState& other) const
{
    return requestedEffect == other.requestedEffect
        && effectiveEffect == other.effectiveEffect
        && backend == other.backend
        && fidelity == other.fidelity
        && surfaceMode == other.surfaceMode
        && platformApplied == other.platformApplied
        && reason == other.reason;
}

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
    return windowBackdropState(widget).surfaceMode
        == BackdropSurfaceMode::CompositedTransparent;
}

bool windowBackdropUsesPaintedMaterial(const QWidget* widget)
{
    return windowBackdropState(widget).surfaceMode == BackdropSurfaceMode::PaintedOpaque;
}

bool windowHasMaterialBackdrop(const QWidget* widget)
{
    return windowBackdropState(widget).effectiveEffect != BackdropEffect::Solid;
}

void requestWindowBackdropReevaluation(QWidget* widget)
{
    QWidget* topLevel = widget ? widget->window() : nullptr;
    if (!topLevel || !QCoreApplication::instance())
        return;
    QCoreApplication::postEvent(topLevel, new QEvent(reevaluationEventType()));
}

bool isWindowBackdropReevaluationEvent(const QEvent* event)
{
    return event && event->type() == reevaluationEventType();
}

} // namespace fluent::windowing
