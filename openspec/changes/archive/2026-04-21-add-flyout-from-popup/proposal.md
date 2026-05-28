## Why

WinUI 3 中 `Flyout` 是与 `Dialog` 并列的轻量浮层控件，与 anchor 控件绑定、light-dismiss、按 Placement 定位。当前 `dialogs_flyouts/` 目录只有 `Popup`（自由坐标定位）和 `Dialog`（modal 居中），缺少「锚点 + 方向」语义，导致 Button、HyperlinkButton、ColorPicker 等控件无法以 WinUI 风格挂载浮层。

## What Changes

- 新增 `view::dialogs_flyouts::Flyout` 类，派生自现有 `Popup`
- 新增 `Placement` 枚举：`Top / Bottom / Left / Right / Full / Auto`
- 新增 `showAt(QWidget* anchor)` 接口，对标 WinUI `Flyout.ShowAt(...)`
- 新增 `anchorOffset`（默认 8px，对齐 WinUI 间距）与 `clampToWindow`（默认 true，避免越界）属性
- 重写虚函数 `Popup::computePosition()`，基于 anchor 几何 + Placement + 自身尺寸实时计算位置（含阴影 margin 偏移补偿）
- 默认行为对齐 WinUI Flyout：non-modal、不变暗、CloseOnPressOutside | CloseOnEscape

## Capabilities

### New Capabilities
- `flyout`: 锚定到目标 widget 的 light-dismiss 浮层，提供 6 种 Placement、anchorOffset、自动越界 clamp，以及 Auto 模式下的空间不足反转

### Modified Capabilities
<!-- 不修改任何现有能力，Popup 行为保持向后兼容 -->

## Impact

- **新增源文件**: `src/view/dialogs_flyouts/Flyout.h` / `Flyout.cpp`
- **新增测试文件**: `tests/views/dialogs_flyouts/TestFlyout.cpp`
- **CMake**: `tests/views/dialogs_flyouts/CMakeLists.txt` 注册 `test_flyout`
- **依赖**: 仅依赖现有 `Popup`、`Spacing` Token，不引入新依赖
- **不修改**: `Popup.h/.cpp`（仅复用 `computePosition()` 虚函数与 `setAnchor` 之外的全部基类机制）
- **下游收益**: Button / HyperlinkButton / ColorPicker / SplitButton 等可以以 `flyout->showAt(anchorBtn)` 模式挂载内容
