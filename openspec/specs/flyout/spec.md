# flyout Specification

## Purpose

`Flyout` 组件 — 派生自 `Popup`，在 WinUI 3 风格的轻量浮层场景中提供「锚定到目标控件 + Placement 定向」的能力，对标 `Microsoft.UI.Xaml.Controls.Flyout`。

## Requirements

### Requirement: Flyout 派生自 Popup 并提供 anchor 语义

`Flyout` SHALL 派生自 `view::dialogs_flyouts::Popup`，新增 `setAnchor(QWidget*)` 与 `showAt(QWidget*)` 接口，提供「锚定到目标控件」的浮层弹出能力。`Popup` 公共 API SHALL NOT 因此变更。

#### Scenario: showAt 等价于 setAnchor + open
- **WHEN** 调用 `flyout->showAt(button)`
- **THEN** Flyout SHALL 先记录 button 为 anchor，然后调用 `open()`，最终 isOpen() 为 true

#### Scenario: 无 anchor 时退化到基类居中
- **WHEN** 未调用 `setAnchor()` 直接 `open()`
- **THEN** Flyout SHALL 回退到 `Popup::computePosition()` 的居中行为，不 crash

#### Scenario: anchor 在打开期间被销毁
- **WHEN** Flyout 已打开，anchor 控件被 delete
- **THEN** Flyout SHALL 通过 QPointer 安全检测 anchor 失效，重新计算位置时回退到基类居中，不 crash

### Requirement: Placement 枚举与位置计算

`Flyout` SHALL 提供 `Placement` 枚举：Top / Bottom / Left / Right / Full / Auto。`computePosition()` SHALL 基于 anchor 在 top-level 中的几何 + Placement + 自身可见尺寸，返回 `move()` 目标坐标（已扣除 shadow margin）。

#### Scenario: Bottom 放置
- **WHEN** anchor 在窗口中央，Placement = Bottom，anchorOffset = 8
- **THEN** Flyout 卡片 SHALL 出现在 anchor 下方，左右与 anchor 水平居中对齐，垂直方向距 anchor 底部 8px

#### Scenario: Top 放置
- **WHEN** anchor 在窗口中央，Placement = Top
- **THEN** Flyout 卡片 SHALL 出现在 anchor 上方，左右与 anchor 水平居中对齐，垂直方向距 anchor 顶部 8px

#### Scenario: Left 放置
- **WHEN** anchor 在窗口中央，Placement = Left
- **THEN** Flyout 卡片 SHALL 出现在 anchor 左侧，上下与 anchor 垂直居中对齐，水平方向距 anchor 左边 8px

#### Scenario: Right 放置
- **WHEN** anchor 在窗口中央，Placement = Right
- **THEN** Flyout 卡片 SHALL 出现在 anchor 右侧，上下与 anchor 垂直居中对齐，水平方向距 anchor 右边 8px

#### Scenario: Full 放置等价于 top-level 居中
- **WHEN** Placement = Full
- **THEN** Flyout 位置 SHALL 与 `Popup::computePosition()` 居中结果一致

### Requirement: Auto 模式根据空间自动反转

Placement = Auto 时，Flyout SHALL 优先 Bottom；若 anchor 下方剩余空间小于 (cardHeight + anchorOffset)，且上方空间足够，则切换到 Top；若两侧都不足，选择空间较大的一侧。

#### Scenario: 下方空间充足，Auto 选 Bottom
- **WHEN** anchor 距 top 顶部 50px，Placement = Auto，cardHeight + offset = 100
- **THEN** Flyout SHALL 出现在 anchor 下方

#### Scenario: 下方空间不足，上方充足，Auto 反转到 Top
- **WHEN** anchor 距 top 底部 30px，距顶部 400px，Placement = Auto，cardHeight + offset = 100
- **THEN** Flyout SHALL 出现在 anchor 上方

### Requirement: clampToWindow 边界裁剪

`clampToWindow` SHALL 默认为 true。当为 true 时，`computePosition()` 计算结果 SHALL 被裁剪到 top-level 窗口范围内，左右上下各保留至少 4px 呼吸空间，避免 Flyout 越界。

#### Scenario: anchor 靠近右边缘时 Flyout 不越界
- **WHEN** anchor 紧贴 top-level 右边缘，Placement = Bottom，clampToWindow = true
- **THEN** Flyout 卡片右边 SHALL 不超过 top-level 右边缘 - 4px

#### Scenario: clampToWindow=false 时不裁剪
- **WHEN** clampToWindow = false，且计算位置超出 top-level
- **THEN** Flyout SHALL 按原始计算位置 move（可见区域可越界）

### Requirement: 默认行为对齐 WinUI Flyout

`Flyout` 构造时 SHALL 默认设置：modal = false、dim = false、closePolicy = CloseOnPressOutside | CloseOnEscape、anchorOffset = 8、clampToWindow = true、placement = Bottom。

#### Scenario: 构造后默认属性
- **WHEN** `Flyout f(parent);` 构造完成
- **THEN** `f.isModal()` SHALL 为 false，`f.isDim()` SHALL 为 false，`f.placement()` SHALL 为 Bottom，`f.anchorOffset()` SHALL 为 8，`f.clampToWindow()` SHALL 为 true

#### Scenario: 点击外部关闭
- **WHEN** Flyout 打开后，鼠标按下 Flyout 卡片外的位置
- **THEN** Flyout SHALL 关闭（继承 Popup 的 light-dismiss 机制）

#### Scenario: Esc 键关闭
- **WHEN** Flyout 打开且获得焦点，按 Escape 键
- **THEN** Flyout SHALL 关闭
