## Why

当前仓库已经有 `Popup` 作为轻量 overlay 基座，也有 `Flyout` 作为“锚点 + Placement”的上层派生，但仍缺少 WinUI 语义完整的 `TeachingTip`。这直接留下一个能力缺口：无法用 Fluent 风格实现带标题、副标题、可选图标、操作按钮、关闭按钮、Hero 内容以及锚点尾巴的引导提示，而 Figma Windows UI Kit 与 WinUI Gallery 都明确把它作为 dialogs & flyouts 的标准组件。

## What Changes

- 新增 `TeachingTip` 组件，基于现有 `Popup` 派生实现，而不是在 `Popup` 基类上继续堆叠 TeachingTip 专属语义。
- `TeachingTip` 仅暴露 **frame-only** 公开 API：`target`、`preferredPlacement`、`placementMargin`、`lightDismissEnabled`、`tailVisible`，以及 `contentHost()` + `setCardSize()`。内容由调用方在 `contentHost()` 上通过 `AnchorLayout` 自由组装，组件本身不内置任何 title/subtitle/icon/hero/buttons 固定 schema。
- 将 TeachingTip 的几何计算收敛为"card rect + tail insets + shadow margin"三层结构，`contentHost()` 几何随 placement/cardSize 自动同步，调用方无需手动管理。
- 基于 `Popup` 的 overlay 生命周期、动画、outside click / Escape 关闭能力，补齐 TeachingTip 自己的定位（12方向 + Auto 回退）、关闭原因语义和 target 生命周期追踪。
- 新增对应单元测试与 VisualCheck，覆盖默认属性、placement、Auto 回退、target 销毁、light-dismiss、contentHost 尺寸、用户子控件边界、关闭原因语义。

## Capabilities

### New Capabilities
- `teaching-tip`: 基于 Popup 的轻量引导提示外壳，提供带 tail 的卡片几何、12 方向 placement + Auto 回退、target 生命周期追踪、light-dismiss 和关闭原因语义；内容由调用方通过 `contentHost()` 自组装。

### Modified Capabilities

## Impact

- **新增源码**：`src/view/dialogs_flyouts/TeachingTip.h`、`src/view/dialogs_flyouts/TeachingTip.cpp`
- **新增测试**：`tests/views/dialogs_flyouts/TestTeachingTip.cpp`
- **测试注册**：`tests/views/dialogs_flyouts/CMakeLists.txt`
- **依赖面**：复用现有 `Popup`、`AnchorLayout`、Fluent theme tokens，不引入新第三方依赖
- **实现约束**：保持 `Popup` 公共 API 稳定，TeachingTip 专属逻辑留在派生层；无固定 schema，内容完全由调用方负责