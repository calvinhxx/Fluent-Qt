#pragma once

#include "OpenGLTestEnvironment.h"
#include "components/spatial/SpatialRuntime.h"

namespace tests::support {
// Probe Qt independently so product regressions still fail on a hardware GPU host.
inline QString nativeOpenGLUnavailableReason()
{
    return nativeOpenGLContextUnavailableReason([]() -> QString {
        const auto renderer = fluent::spatial::SpatialRuntime::currentRendererName();
        if (!fluent::spatial::SpatialRuntime::isHardwareRenderer(renderer))
            return QStringLiteral("Requires hardware OpenGL; renderer: ") + renderer;
        return {};
    });
}
} // namespace tests::support
