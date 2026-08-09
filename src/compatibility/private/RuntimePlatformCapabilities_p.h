#ifndef FLUENT_RUNTIMEPLATFORMCAPABILITIES_P_H
#define FLUENT_RUNTIMEPLATFORMCAPABILITIES_P_H

namespace compatibility::detail {

// Runtime-selected surface behavior that cannot be inferred from a desktop OS
// family alone. Platform adapters set this once before constructing widgets;
// reusable components consume capabilities without knowing the host runtime.
// zh_CN: 仅靠桌面 OS 族无法推断的运行时表面能力。平台适配器在创建控件前
// 设置一次；可复用组件只消费能力，不感知具体宿主运行时。
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

} // namespace compatibility::detail

#endif // FLUENT_RUNTIMEPLATFORMCAPABILITIES_P_H
