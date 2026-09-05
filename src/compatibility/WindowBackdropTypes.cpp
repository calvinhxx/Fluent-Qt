#include "WindowBackdropTypes.h"

namespace fluent::windowing {

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
    return effect != BackdropEffect::Solid && alphaSurfaceSupported &&
           (supportsNative(effect) || supportsCompositor(effect));
}

bool BackdropState::operator==(const BackdropState& other) const
{
    return requestedEffect == other.requestedEffect && effectiveEffect == other.effectiveEffect &&
           backend == other.backend && fidelity == other.fidelity &&
           surfaceMode == other.surfaceMode && platformApplied == other.platformApplied &&
           reason == other.reason;
}

} // namespace fluent::windowing
