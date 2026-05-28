## ADDED Requirements

### Requirement: Dialog 入/出场 SHALL 使用 scale 0.90↔1.0 + opacity 0↔1 对称动画

`Dialog` 在入场（`open()` / `exec()`）和退场（`done(r)`）期间 MUST 同时应用：
- **尺寸缩放**：入场从 `90%` 过渡到 `100%`，退场从 `100%` 过渡到 `90%`（基于实际 `m_targetSize` 计算，保持窗口中心不变）。
- **透明度**：入场 `windowOpacity` 从 `0.0` 过渡到 `1.0`，退场从 `1.0` 过渡到 `0.0`。

实现上 MUST 通过一个 `kScaleFrom = 0.90` 常量控制缩放起点，入/出场使用对称曲线（复用主题 `entrance` easing）。
MUST NOT 在顶层 window 上使用 `QGraphicsOpacityEffect`（Qt 不支持，会被忽略）。

#### Scenario: 入场起始帧尺寸为目标的 90%
- **WHEN** 已 `show()` 的 `Dialog` 正处于入场动画的起始帧（`animationProgress ≈ 0`）
- **THEN** 当前 `size().width() ≈ targetSize.width() * 0.90` 且 `windowOpacity() ≈ 0.0`

#### Scenario: 退场末尾帧尺寸为目标的 90%
- **WHEN** `Dialog` 调用 `done(r)` 后进入退场动画，动画接近结束（`animationProgress ≈ 0`）
- **THEN** 当前 `size().width() ≈ targetSize.width() * 0.90` 且 `windowOpacity() ≈ 0.0`

#### Scenario: 入/出场期间窗口中心保持稳定
- **WHEN** Dialog 入场或退场动画播放中
- **THEN** 相邻两帧的窗口几何中心偏移 ≤ 1px（由 `resize` + `move` 联动保证）

### Requirement: Smoke 蒙层 SHALL 随 Dialog 同步淡入淡出

启用 `setSmokeEnabled(true)` 的 `Dialog`，其 smoke overlay MUST 在入场时通过自定义 `progress` 属性从 `0.0` 过渡到 `1.0`，
在退场时从 `1.0` 过渡到 `0.0`，`paintEvent` 按 `progress` 线性插值填充色 alpha。
Smoke 的动画 duration MUST 与 Dialog 主动画的 duration 相同（复用 `themeAnimation().normal`）。
Smoke overlay MUST 在退场淡出完成后才被销毁/隐藏，而不是在 Dialog `done` 的瞬间消失。

MUST NOT 使用 `QGraphicsOpacityEffect`（顶层 window 不支持）。

#### Scenario: 入场时 smoke 从完全透明开始
- **WHEN** 启用 smoke 的 `Dialog` 调用 `open()`
- **THEN** smoke overlay 在显示的第一帧 `progress ≈ 0.0`（`paintEvent` 绘制 alpha ≈ 0），随后逐帧递增直至 `1.0`

#### Scenario: 退场时 smoke 延迟销毁
- **WHEN** 启用 smoke 的 `Dialog` 调用 `done()`
- **THEN** smoke overlay 不会立即隐藏/销毁，而是在其 `progress` 动画完成（到达 `0.0`）后才被 `deleteLater()`

#### Scenario: smoke 动画 duration 与 Dialog 一致
- **WHEN** 启用 smoke 的 `Dialog` 入场或退场
- **THEN** `m_smokeAnim->duration() == themeAnimation().normal`（与 Dialog 主 `m_animation` 一致）

### Requirement: ContentDialog 按钮条 SHALL 为左对齐紧凑布局

`ContentDialog` 的按钮条 MUST 使用 `QHBoxLayout`，三个按钮从左向右依次排列（可见的按钮按 Primary/Secondary/Close 的源代码顺序），
布局末尾 MUST 存在一个 `stretch` 项以使按钮集合左对齐、右侧留白。
任一可见按钮 MUST 满足 `minimumWidth() >= 96`。按钮之间的水平间距 MUST 等于 `Spacing::Gap::Normal (8)`。

#### Scenario: 按钮左对齐
- **WHEN** `ContentDialog` 设置了 `primaryButtonText`、`secondaryButtonText`、`closeButtonText` 并显示
- **THEN** 最右侧可见按钮的 `geometry().right()` 严格小于 `buttonBar->contentsRect().right()`（右侧存在留白）

#### Scenario: 按钮最小宽度
- **WHEN** 任一按钮文案为短字符串（如 "OK"）
- **THEN** 该按钮的实际 `width()` >= `96`

### Requirement: ContentDialog 按钮条高度 SHALL 为 68px

`ContentDialog` 的按钮条容器 MUST 固定高度为 `68px`。按钮 vertical padding MUST 对称，
使按钮（`Spacing::ControlHeight::Standard = 32`）在按钮条中垂直居中。

#### Scenario: 按钮条高度等于 68
- **WHEN** `ContentDialog` 显示任意可见按钮组合
- **THEN** 其内部 `buttonBar` widget 的 `height() == 68`
