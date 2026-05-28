## Why

当前组件库已有 ToolTip 等 Status & Info 控件，但缺少 WinUI 3 常见长任务反馈中的 `ProgressRing`。业务侧如果用普通 `QWidget` 临时绘制加载环，需要重复处理 indeterminate 动画、determinate 进度、主题色、尺寸和可视化测试，容易偏离 Figma Windows UI Kit 与 WinUI Gallery 的行为。

本变更新增自绘 `ProgressRing`，对齐 Figma Progress 组件中的 Ring 变体，以及 WinUI Gallery 中 `IsActive`、`Value`、`Background` 的核心示例行为。

## What Changes

- 新增 `ProgressRing` 状态信息组件，位于 `src/view/status_info/`，遵循 `QWidget + FluentElement + view::QMLPlus` 三重继承模式。
- 支持 indeterminate 与 determinate 两种模式：未知时长任务使用旋转弧线动画，已知进度任务按 `minimum` / `maximum` / `value` 绘制固定进度弧。
- 暴露 `isActive`、`isIndeterminate`、`minimum`、`maximum`、`value`、`ringSize`、`strokeWidth`、`status`、`backgroundVisible` 等属性和对应变更信号。
- 支持 Figma 中的 16px、32px、64px 三档 ring size，以及 Running、Paused、Error 状态色；Light/Dark 主题下从 Design Token/语义色派生画笔。
- 支持可选背景轨道，覆盖 WinUI Gallery 示例里的 Transparent/LightGray 背景切换语义。
- 增加自动化测试和 VisualCheck，覆盖属性边界、动画启停、进度归一化、主题响应、三档尺寸和视觉状态组合。

## Capabilities

### New Capabilities

- `progress-ring`: 覆盖 Fluent Design Qt 组件库中的 WinUI 3 风格环形进度控件行为、视觉状态、动画和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/status_info/ProgressRing.h`、`src/view/status_info/ProgressRing.cpp`。
- 更新测试配置：`tests/views/status_info/CMakeLists.txt` 注册 `test_progress_ring`。
- 新增测试：`tests/views/status_info/TestProgressRing.cpp`。
- 依赖现有 Design Token、`FluentElement`、`QMLPlus`、Qt painting 和 `QVariantAnimation` / `QTimer` 类动画能力。
- 不引入新的第三方依赖，不改变已有 `ToolTip` 或其他 Status & Info 控件公共 API。