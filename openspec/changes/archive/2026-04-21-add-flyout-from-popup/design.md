## Context

`dialogs_flyouts/` 已具备：
- `Popup`（基类）：reparent 到 top-level overlay、阴影绘制（`kShadowMargin = 16`）、Scrim/Dim、Light-dismiss、Esc 关闭、`opacity` 进出场动画、虚函数 `computePosition()`、`setPosition(relativeTo, localPos)`
- `Dialog`：modal 居中容器，独立实现（继承 `QDialog`，与 `Popup` 体系并行）
- `ContentDialog`：派生自 `Dialog` 的 WinUI3 风格内容对话框

现在需要补齐 `Flyout` 这一类「锚定到 anchor 控件、按方向定位」的轻量浮层（对标 WinUI 3 `Microsoft.UI.Xaml.Controls.Flyout`），主要由 Button / HyperlinkButton / SplitButton / ColorPicker 等基础输入控件作为承载点。

参考来源：
- WinUI Gallery `FlyoutPage.xaml`（`<Button.Flyout><Flyout><StackPanel>…`）
- MS Learn `flyouts` 文档（`Placement` 枚举、light-dismiss 行为、与 anchor 8px 间距）
- Figma Windows UI kit（仅参考视觉规范；MCP 401 未授权，未直接读取节点）

## Goals / Non-Goals

**Goals:**
- 提供与 anchor 控件解耦的、可复用的轻量浮层容器，开发者只需 `flyout->showAt(button)` 即可
- 通过 `Placement` 控制相对 anchor 的方位，支持 6 种语义：Top / Bottom / Left / Right / Full / Auto
- `Auto` 模式根据可用空间自动在 Top/Bottom 之间反转，避免越界
- 默认行为对齐 WinUI Flyout：non-modal、不变暗、light-dismiss
- 完全复用 `Popup` 的阴影、动画、scrim、light-dismiss 机制；本次不重写绘制与生命周期
- 不破坏 `Popup` 公共 API，`Popup` 既有测试与下游用法零回归

**Non-Goals:**
- 不实现 Flyout Tail/箭头（WinUI 3 默认无箭头；后续按需另开 change）
- 不实现 `MenuFlyout` / `CommandBarFlyout`（属 `menus_toolbars/` 范畴，单独 change）
- 不实现 `OverlayInputPassThroughElement`（首单击穿透）— 高级特性，视使用反馈再加
- 不引入新 Design Token；间距复用 `Spacing::Standard` 的语义（8px 锚点偏移与 16px 阴影 margin）
- 不实现 ShowMode / Hidden / Transient 区分；统一按 light-dismiss 处理

## Decisions

### Decision 1: 派生自 `Popup`，仅重写 `computePosition()`

**选择**: `class Flyout : public Popup`，重写虚函数 `QPoint computePosition() const override`。

**替代方案**:
- (A) 复制 `Popup` 实现并独立维护 → 重复代码、动画/scrim/light-dismiss 行为容易漂移
- (B) 在 `Popup` 上加 `Placement` 枚举与 `setAnchor()` → 污染基类，违反「Popup 是自由坐标定位」语义
- (C) 用 `setPosition(relativeTo, localPos)` 在 caller 端手算 → 每个 caller 都要重写一遍方位逻辑

**选 派生**：`Popup::computePosition()` 已经是设计时预留的扩展点；重写它只在 `open()` 流程中生效，对自由坐标 API 零影响。

### Decision 2: anchor 用 `QPointer<QWidget>` 弱引用

防止 anchor 在 flyout 打开期间被销毁导致悬空指针。`computePosition()` 中检查 `m_anchor && m_anchor->window()`，失效时回退到基类居中。

### Decision 3: 坐标系统一映射到 top-level

anchor 几何通过 `m_anchor->mapTo(top, QPoint(0,0))` 映射到 top-level 坐标系，因为 Popup 在 `open()` 时会被 reparent 到 top-level overlay。返回值需减去 `kShadowMargin`，对齐 `Popup::open()` 中 `move(m_targetPos - QPoint(kShadowMargin, kShadowMargin))` 的偏移补偿语义（`computePosition()` 路径不会自动减阴影 margin，子类必须自己处理）。

### Decision 4: anchorOffset 默认 8px

WinUI 3 默认 flyout 与 anchor 之间留 8px 间隙（`Spacing::Standard / 2`）。允许用户通过 `setAnchorOffset()` 调整。不引入新 Token，直接用整型属性。

### Decision 5: clampToWindow 默认 true

避免 Flyout 越出 top-level 窗口边界。clamp 边界留 4px 呼吸空间。可通过 `setClampToWindow(false)` 关闭（极少数全屏覆盖场景）。

### Decision 6: Auto 模式只在 Top/Bottom 之间反转

不在 Left/Right 间反转——大多数 anchor（按钮、链接）水平方向边距较大，垂直方向才容易越界。算法：先尝试 Bottom；若下方空间不足且上方足够，则改 Top；都不足则选空间较大的一侧。

### Decision 7: Full 复用基类居中

`Placement::Full` 直接调用 `Popup::computePosition()`，行为与不设 placement 的 Popup 一致（top-level 居中），与 WinUI `FlyoutPlacementMode::Full` 语义对齐（虽然 WinUI Full 会拉伸尺寸——本实现不拉伸，仅居中，避免破坏自适应布局）。

## Risks / Trade-offs

- **shadow margin 双重补偿** → Popup 的 `setPosition` 路径在 `open()` 减去 margin，`computePosition()` 路径需子类自己减。这是基类隐式契约，已在 `Flyout.cpp` 注释中明确。Mitigation: 测试覆盖 anchor 处于 top-level 各角落时的最终视觉位置。
- **anchor 在 flyout 打开期间被删除** → `QPointer` 自动置空，`computePosition()` 回退到居中。Mitigation: 单元测试 `AnchorDestroyedDuringOpen`。
- **Auto 在两侧空间都不足时选择「较大一侧」可能仍越界** → 由 `clampToWindow` 兜底。Mitigation: 测试 `AutoFallbackWhenBothInsufficient`。
- **不实现 Tail/箭头** → 与 WinUI 3 默认对齐，但 WinUI 2 / UWP 用户期待箭头。Mitigation: 文档明确说明，未来 `add-flyout-tail` change 单独引入。
- **Full 不拉伸尺寸** → 与 WinUI 行为不完全一致。Mitigation: 文档注明，需要拉伸的 caller 应自行设置 size 或使用 Dialog。

## Migration Plan

无破坏性变更。
- `Popup` 公共 API 不变，下游零回归
- 新增类 `view::dialogs_flyouts::Flyout`，按需 `#include "view/dialogs_flyouts/Flyout.h"` 即可
- 后续 Button / HyperlinkButton 如需挂载浮层，可在 caller 侧 `auto* fl = new Flyout(parent); fl->showAt(btn);`，不强制改造现有控件
