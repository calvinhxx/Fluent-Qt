# Overlay Behavior Contract

本项目的 transient overlay（`Popup`、`Flyout`、`ComboBox` dropdown、`DrawerView`）默认使用 same-window overlay 模型：打开时挂载到 owning top-level `QWidget`，保持 `Qt::Widget` 子控件语义，不创建独立 `Qt::Window` 或 `Qt::Dialog`。相关 helper 的 canonical 位置是 `src/components/foundation/overlay/`，命名空间是 `fluent::overlay`，属于 component foundation 通用能力，不是一个独立组件分类。

## Geometry

Overlay 实现必须区分三层几何：

- outer widget geometry：包含阴影预留边距的实际 `QWidget::geometry()`。
- visible card/panel geometry：用户看到、定位和 hit-test 的逻辑卡片或抽屉区域。
- content geometry：承载 ListView、viewport 或任意子控件的内容区域；内容应保持 inset 或裁剪，避免方形 viewport 背景泄漏到圆角外。

阴影 margin 不参与调用方语义。`setPosition()`、anchor placement、edge placement 和测试断言都以 visible card/panel 为准。

## Light Dismiss

`CloseOnPressOutside` 只把 visible card/panel 外的按下视为 outside press；shadow margin 不是交互区域。`CloseOnEscape` 支持 overlay 自身以及 owning top-level 上下文中的 Escape。`NoAutoClose` 禁止 outside press 和 Escape 的隐式关闭。

非模态 overlay 关闭后允许原始 outside press 继续传递给背景目标。`DrawerView` 的 Escape 关闭仍保持原有“吞掉 Escape”的行为，以避免背景快捷键在抽屉关闭时同时触发。

## Scrim And Stacking

模态或 dim overlay 使用 same-window scrim：scrim 位于背景控件之上、overlay 卡片或抽屉之下。模态 scrim 阻止背景 pointer input；非模态或非 dim 场景不保留阻塞 scrim。overlay 打开、top-level resize、抽屉位置更新时都应显式维护 `scrim -> overlay` 的 z-order。

关闭 overlay 时必须同步隐藏或异步销毁 scrim，避免 stale scrim 留在 top-level 上继续阻塞背景控件。

## Rendering And Theme

Overlay surface、border、shadow、smoke/scrim 均使用 Fluent token 和自绘。visible card/panel 外应保持透明，圆角区域不能由嵌入子控件背景泄漏填充。主题切换只触发重绘和 hosted content 样式刷新，不改变 open state、placement、selected value 或 content ownership。

## Animation

禁用动画时，open/close 的可见性、进度、scrim、geometry 和 lifecycle signals 必须同步落定。启用动画时，Popup/Flyout 的 opacity-only transition 不应改变 visible card geometry；DrawerView 的 position animation 保持 normalized `position` 语义。

## Preserved Differences And Deferred Work

`ComboBox` dropdown 保持非模态、非 dim，并保留当前 index、editable text 和 ListView selection 行为。`DrawerView` 保留 edge drag、normalized position、content widget ownership 和现有 public `ClosePolicy` API。

暂不把 `Popup::ClosePolicy` 与 `DrawerView::ClosePolicy` 合并，也不让 `DrawerView` 继承 `Popup`。这些 public API consolidation 若需要，应通过后续 OpenSpec 变更单独评估。
