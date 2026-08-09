#include "RuntimePlatformCapabilities_p.h"

namespace compatibility::detail {
namespace {

RuntimePlatformCapabilities& mutableRuntimePlatformCapabilities()
{
    static RuntimePlatformCapabilities capabilities;
    return capabilities;
}

} // namespace

const RuntimePlatformCapabilities& runtimePlatformCapabilities()
{
    return mutableRuntimePlatformCapabilities();
}

void setRuntimePlatformCapabilities(const RuntimePlatformCapabilities& capabilities)
{
    mutableRuntimePlatformCapabilities() = capabilities;
}

} // namespace compatibility::detail
