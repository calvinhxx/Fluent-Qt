#ifndef FLUENT_RUNTIMEPLATFORMCAPABILITIES_P_H
#define FLUENT_RUNTIMEPLATFORMCAPABILITIES_P_H

namespace compatibility::detail {

// Runtime-selected platform capabilities set once before widget creation.
// zh_CN: 运行时确定的平台能力配置，在控件创建前初始化一次。
struct RuntimePlatformCapabilities {
    bool customWindowChromePreferred = false;
    int clientSideFrameMargin = 0;
    bool manualMoveResizeFallback = true;
    bool translucentPopupSurfaces = true;
    bool hostsApplicationWindowsInDesktopSurface = false;
    bool opaqueClientTitleBarSurface = false;
    bool cachePaintedWindowSurfaces = false;
};

const RuntimePlatformCapabilities& runtimePlatformCapabilities();
void setRuntimePlatformCapabilities(const RuntimePlatformCapabilities& capabilities);

inline bool effectiveWindowActivation(bool nativeActive)
{
    return nativeActive || runtimePlatformCapabilities().hostsApplicationWindowsInDesktopSurface;
}

} // namespace compatibility::detail

#endif // FLUENT_RUNTIMEPLATFORMCAPABILITIES_P_H
