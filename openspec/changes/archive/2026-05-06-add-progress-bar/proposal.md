## Why

`status_info` 目录已有 `ProgressRing`，但还缺少 WinUI / Fluent 中更常用的横向 `ProgressBar`。业务侧如果直接使用 `QProgressBar` 或临时自绘，会难以对齐 Figma Windows UI Kit 的 Progress Bar 变体、WinUI Gallery 示例里的 indeterminate/determinate 行为，以及本项目现有 Fluent 主题和 VisualCheck 规范。

## What Changes

- 新增自定义 `ProgressBar` 状态信息组件，位于 `src/view/status_info/`，遵循 `QWidget + FluentElement + view::QMLPlus` 的本项目控件模式。
- 支持 determinate 与 indeterminate 两种表现：已知进度时按 `minimum` / `maximum` / `value` 绘制填充段，未知进度时绘制居中移动的活动段。
- 暴露 WinUI 风格状态属性，覆盖 Running、Paused、Error、disabled 和可选背景 rail 的视觉语义。
- 对齐 Figma Progress / Bar 变体：默认 220px 宽、32px 高布局语义，居中 1px rail，3px 圆角 progress track，Light/Dark 主题下使用 Running、Paused、Error 对应颜色。
- 对齐 WinUI Gallery `ProgressBarPage` 行为：indeterminate 示例支持 `ShowPaused` / `ShowError` 状态，determinate 示例由数值输入驱动 `Value`，并提供可用于自动化测试的进度文本/可访问语义。
- 增加 GTest 与 VisualCheck，覆盖属性边界、进度归一化、indeterminate 动画启停、状态色、主题切换、尺寸和 WinUI Gallery 风格示例。

## Capabilities

### New Capabilities

- `progress-bar`: 覆盖 Fluent Design Qt 组件库中的 WinUI 3 风格横向进度条控件行为、视觉状态、动画和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/status_info/ProgressBar.h`、`src/view/status_info/ProgressBar.cpp`。
- 更新测试配置：`tests/views/status_info/CMakeLists.txt` 注册 `test_progress_bar`。
- 新增测试：`tests/views/status_info/TestProgressBar.cpp`。
- 依赖现有 Design Token、`FluentElement`、`QMLPlus`、Qt painting 和轻量 timer 动画能力。
- 不引入新的第三方依赖，不改变已有 `ProgressRing`、`ToolTip` 或其他控件公共 API。
