# TeachingTip Specification

## Purpose

规范 dialogs_flyouts 模块中的 Fluent `TeachingTip` 组件：它派生自 `Popup`，提供 target 锚定、contentHost 自组装内容、preferred placement、placement margin、tail 对齐、light-dismiss 映射、关闭原因语义，以及 visual card 内部布局边界要求。

## Requirements

### Requirement: TeachingTip SHALL be a target-anchored overlay derived from Popup

`TeachingTip` SHALL 作为 `Popup` 的派生组件存在，并提供 `target` 语义用于锚定目标控件。组件 MUST 支持通过目标控件打开，并在目标控件销毁后安全关闭，而不是留下悬空 overlay。

#### Scenario: showAt(target) 打开 TeachingTip
- **WHEN** 调用 `showAt(button)` 或等价设置 `target=button` 后打开组件
- **THEN** TeachingTip SHALL 打开并锚定到该目标控件，而不是退化为普通居中 Popup

#### Scenario: target 销毁时安全关闭
- **WHEN** TeachingTip 已经打开，随后其 `target` 控件被销毁
- **THEN** TeachingTip SHALL 自动关闭，且关闭原因 SHALL 为 `TargetDestroyed`

### Requirement: TeachingTip SHALL expose a bare content host for caller-assembled content

TeachingTip 不内置任何固定内容 schema（无 title/subtitle/icon/hero/buttons 属性）。组件 MUST 通过 `contentHost()` 暴露一个 `QWidget*` 容器，其几何精确等于 card 可用区域（`cardSize`），调用方通过 AnchorLayout 在其上自由组装内容。

#### Scenario: contentHost 尺寸等于 cardSize
- **WHEN** `setCardSize(QSize(300, 160))` 后调用 `showAt(target)`
- **THEN** `contentHost()->size()` SHALL 等于 `QSize(300, 160)`

#### Scenario: 用户子控件保持在 contentHost 边界内
- **WHEN** 调用方在 `contentHost()` 上添加 QLabel 并设置 geometry 在 host->rect() 内
- **THEN** 该 QLabel 的 geometry 在 host->rect() 内保持不变，不因 shadow margin 或 tail 偏移

#### Scenario: setCardSize 在打开后实时更新
- **WHEN** TeachingTip 已打开，随后调用 `setCardSize(newSize)`
- **THEN** widget 总尺寸、contentHost 几何和位置 SHALL 立即重新计算，不需要重新调用 showAt

### Requirement: TeachingTip SHALL support preferred placement, placement margin, and tail alignment

TeachingTip SHALL 提供 `preferredPlacement` 与 `placementMargin`。`preferredPlacement` MUST 支持 `Auto`、`Top`、`TopLeft`、`TopRight`、`Bottom`、`BottomLeft`、`BottomRight`、`Left`、`LeftTop`、`LeftBottom`、`Right`、`RightTop`、`RightBottom`。组件打开时 MUST 基于 target 与 top-level 窗口可用空间计算最终位置，并在启用尾巴时让尾巴指向 target 的对应边/对齐点。

#### Scenario: Bottom placement 显示在目标下方
- **WHEN** `preferredPlacement=Bottom` 且 target 下方空间充足
- **THEN** TeachingTip SHALL 出现在 target 下方，`contentHost()->mapToGlobal(QPoint(0,0)).y()` SHALL 等于 `targetGlobal.bottom() + placementMargin + kTailSize`

#### Scenario: Auto 在底部空间不足时回退
- **WHEN** `preferredPlacement=Auto` 且 target 下方空间不足以容纳卡片 + `placementMargin` + `kTailSize`
- **THEN** TeachingTip SHALL 自动回退到其它可用方向（优先 Top），而不是溢出 top-level 窗口边界

#### Scenario: 边角 placement 保持 tail 对齐点
- **WHEN** `preferredPlacement=RightTop`
- **THEN** TeachingTip SHALL 出现在 target 右侧，`contentHost` 左边缘 SHALL 等于 `targetGlobal.right() + placementMargin + kTailSize`，且 tail 对齐 target 上部区域而非居中

### Requirement: TeachingTip SHALL map light-dismiss behavior to close policy

TeachingTip SHALL 提供 `lightDismissEnabled`。当该值为 true 时，组件 MUST 响应点击外部和 Escape 关闭；当该值为 false 时，组件 MUST 保持打开，直到调用方显式关闭。

#### Scenario: 启用 light dismiss 后点击外部关闭
- **WHEN** `lightDismissEnabled=true` 且用户点击 TeachingTip 外部区域
- **THEN** TeachingTip SHALL 关闭，且关闭原因 SHALL 为 `LightDismiss`

#### Scenario: 禁用 light dismiss 后点击外部不关闭
- **WHEN** `lightDismissEnabled=false` 且用户点击 TeachingTip 外部区域
- **THEN** TeachingTip SHALL 保持打开

#### Scenario: 禁用 light dismiss 后 Escape 不关闭
- **WHEN** `lightDismissEnabled=false` 且用户按下 Escape
- **THEN** TeachingTip SHALL 保持打开

### Requirement: TeachingTip SHALL report semantic close reasons

TeachingTip SHALL 在关闭时产出语义化 `CloseReason`，至少覆盖 `Programmatic`、`LightDismiss`、`TargetDestroyed`。程序调用和 light-dismiss 触发的关闭 MUST 能被外部区分。

#### Scenario: programmatic close
- **WHEN** 调用方通过 `closeWithReason(CloseReason::Programmatic)` 主动关闭 TeachingTip
- **THEN** TeachingTip SHALL 关闭，`closing` 信号的 `reason` SHALL 为 `Programmatic`

#### Scenario: light dismiss 关闭
- **WHEN** `lightDismissEnabled=true` 且用户点击 TeachingTip 外部区域
- **THEN** TeachingTip SHALL 关闭，`closing` 信号的 `reason` SHALL 为 `LightDismiss`

#### Scenario: target 销毁关闭
- **WHEN** TeachingTip 已打开，随后其 `target` 控件被销毁
- **THEN** TeachingTip SHALL 自动关闭，`closing` 信号的 `reason` SHALL 为 `TargetDestroyed`

### Requirement: TeachingTip SHALL keep contentHost inside the visual card bounds

`contentHost()` 的 geometry MUST 精确等于视觉卡片矩形（不含 shadow margin 和 tail 区域）。调用方在 contentHost 上的任意子控件，只要 geometry 在 `host->rect()` 内，SHALL 不与 shadow margin 或 tail 区域重叠。

#### Scenario: contentHost 不进入尾巴区域
- **WHEN** TeachingTip 以任意 placement 显示
- **THEN** `contentHost()->geometry()` 在 widget 坐标系中 SHALL 等于 `cardRect()`，不与 tail polygon 有效区域重叠

#### Scenario: placement 切换后 contentHost 几何自动更新
- **WHEN** `preferredPlacement` 从 Bottom 切换到 Right 后重新打开
- **THEN** contentHost 的位置 SHALL 随 tail 方向变化自动更新，调用方子控件无需任何操作