## Why

用户提供的 WinUI 3 Gallery 演示 GIF 展示了 ContentDialog 的标准交互形态与当前实现存在可观察的视觉偏差：
入场/退场的缩放幅度过小（当前 `kScaleFrom = 0.96`）视觉上不够"pop"，Smoke 蒙层瞬现瞬灭缺少过渡，
按钮条三个按钮被 `stretch=1` 拉伸撑满整行而非左对齐紧凑排列，按钮条高度偏大。
本次改动统一对齐 WinUI 3 Gallery 视觉与动效语言。

## What Changes

- **Dialog 入/出场缩放幅度加大**：`kScaleFrom` 从 `0.96` 改为 `0.90`，入场 90%→100%、退场 100%→90%，配合 opacity 0↔1，pop-in/pop-out 观感更明显。
- **Smoke 蒙层随 Dialog 同步淡入淡出**：在 `SmokeOverlay::paintEvent` 中按 `m_progress (0..1)` 插值 `bg.alpha`，由 Dialog 的 `m_smokeAnim`（`QPropertyAnimation`）驱动自定义 `Q_PROPERTY(double progress)`；入场 0→1、退场 1→0，duration 与 Dialog 动画一致。
- **ContentDialog 按钮条改为左对齐紧凑布局**：按钮不再使用 `stretch=1` 撑满，改为 `stretch=0` + 末尾 `addStretch(1)`，左侧起排列，右侧留白。
- **ContentDialog 按钮条高度收紧**：`kButtonBarHeight` 由 `80px` 降至 `68px`。
- **ContentDialog 增加按钮最小宽度常量 `kButtonMinWidth = 96`**：三个按钮统一 `setMinimumWidth(96)`，满足 WinUI 3 Gallery 观察到的短文案按钮保持合理触达面积。

> 注：顶层 `QDialog`（`Qt::Window` 窗口）**不能**使用 `QGraphicsOpacityEffect`（Qt 会忽略并输出警告），
> 因此 Dialog 本体的淡入淡出继续使用 `setWindowOpacity`，Smoke 蒙层的淡入淡出使用自定义 `progress` 属性驱动 `paintEvent` 的 alpha 插值，
> 而非 `QGraphicsOpacityEffect`。

## Capabilities

### New Capabilities
- `dialog-winui3-polish`: 统一 Dialog/ContentDialog 与 WinUI 3 Gallery 视觉与动效对齐的约束集合（入/出场加大 scale + opacity、蒙层联动淡入淡出、按钮条左对齐紧凑布局、按钮最小宽度）。

### Modified Capabilities
<!-- 无：openspec/specs/ 当前为空（flipview 是唯一现存 change，尚未 archive），故本次引入新 capability。 -->

## Impact

- **修改文件**：
  - `src/view/dialogs_flyouts/Dialog.h` / `Dialog.cpp`（`kScaleFrom` 0.96→0.90、Smoke 淡入淡出、新增 `m_smokeAnim` 与 `SmokeOverlay::progress` 属性）
  - `src/view/dialogs_flyouts/ContentDialog.cpp`（按钮条布局、高度、按钮 min-width）
- **测试文件**：
  - `tests/views/dialogs_flyouts/TestDialog.cpp`（新增入/出场 scale 幅度断言 + 蒙层 progress 插值断言）
  - `tests/views/dialogs_flyouts/TestContentDialog.cpp`（新增按钮 minWidth / 左对齐 / 按钮条高度断言）
- **API 兼容性**：无 public API 变更。所有改动限于私有实现与布局常量。
- **VisualCheck**：现有 VisualCheck 测试需手动复核动效与按钮外观，但不需修改代码。
