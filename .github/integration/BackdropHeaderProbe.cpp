#include <compatibility/WindowBackdropTypes.h>
#include <compatibility/WindowChromeCompat.h>
#include <components/windowing/WindowBackdrop.h>

#include <QMetaEnum>
#include <QVariant>
#include <type_traits>

static_assert(std::is_same<compatibility::BackdropEffect, fluent::windowing::BackdropEffect>::value,
              "The compatibility layer and Window must share one backdrop protocol");

// Link the moved implementations and namespace metaobject through public headers.
void verifyBackdropHeaders()
{
    const fluent::windowing::BackdropCapabilities capabilities;
    const bool supported = capabilities.supportsNative(compatibility::BackdropEffect::Solid);
    const auto state = fluent::windowing::windowBackdropState(nullptr);
    const auto value = QVariant::fromValue(state);
    const auto meta = QMetaEnum::fromType<fluent::windowing::BackdropEffect>();
    Q_UNUSED(supported);
    Q_UNUSED(value);
    Q_UNUSED(meta);
}
