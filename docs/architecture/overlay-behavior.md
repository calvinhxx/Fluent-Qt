# Overlay Behavior Contract

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Window Chrome Architecture](window-chrome.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Typography Resolution →](typography-resolution.md)
<!-- docs-nav:top:end -->

本项目的 transient overlay（`Popup`、`Flyout`、`ComboBox` / `MultiSelectComboBox` dropdown、`DrawerView`、`Dialog` / `ContentDialog`、`CoachMark`、`TeachingTip`）使用 **same-window overlay** 模型：打开时挂载到 owning top-level `QWidget`，保持 `Qt::Widget` 子控件语义，不创建独立 `Qt::Window` / `Qt::Dialog` / `Qt::Tool`。这与 WinUI Gallery 的 ContentDialog / Flyout / TeachingTip（绑在当前窗口 `XamlRoot`）对齐。

相关 helper 的 canonical 位置是 `src/components/foundation/overlay/`，命名空间是 `fluent::overlay`。`OverlayCoordinator` 是 UILib 内部协调器，集中处理 top-level 挂载、宿主 resize、scrim 生命周期与 stacking；它不会进入安装头文件或成为应用层 API。

## Startup content cover

`fluent::status_info::SplashScreen` 是父控件内容区的启动遮罩。它跟随父控件尺寸，不管理任务、标题栏或启动流程。将它放在 `Window::contentHost()` 上可保留窗口按钮；也可以覆盖应用选定的局部容器。

默认 `Presentation::Branded` 使用完整图标显影、柔和背景光、可选品牌文字和底部进度条。`Presentation::Simple` 保留居中图标、加载环和状态文字的简洁效果，供应用显式选配。Gallery 启动使用默认效果，组件示例可切换两种模式并重播。

```cpp
#include <FluentQt/FluentQt.h>

// host is the QWidget content area; appIcon is supplied by the application.
auto* splash = new fluent::status_info::SplashScreen(host);
splash->setIcon(appIcon);
splash->setIconSize(QSize(96, 96));
splash->setTitle(QStringLiteral("My App"));
splash->setSubtitle(QStringLiteral("Your workspace, ready to go"));
splash->setText(QStringLiteral("正在加载工作区"));
// Optional: an application-owned icon holder in the same window.
splash->setTransitionTarget(titleBarIcon);
splash->show();

// Deliver worker progress on the GUI thread.
splash->setProgress(3, 5);
// The application calls this when startup has actually finished.
splash->dismiss();
```

```python
splash = fluentqt.SplashScreen(host)
splash.setIcon(app_icon)
splash.setTitle("My App")
splash.setSubtitle("Your workspace, ready to go")
splash.setText("正在加载工作区")
splash.setTransitionTarget(title_bar_icon)
splash.show()
splash.setProgress(3, 5)
splash.dismiss()
```

选用简洁效果时，C++ 调用 `splash->setPresentation(SplashScreen::Presentation::Simple)`，Python 调用 `splash.setPresentation(fluentqt.SplashScreen.Presentation.Simple)`。切换模式保留当前状态文字和进度。

`setProgress(done, total)` 切换为确定进度并归一化到 0–100%；`total <= 0` 或 `setIndeterminate(true)` 显示不确定进度并隐藏百分比。Branded 使用进度条，Simple 使用加载环。状态文字独立保留。达到 100% 不自动关闭，避免把任务计数当成启动完成。长文字显示省略，辅助功能可读取完整文本；小窗口会缩小图标并压缩间隔。

`setTransitionTarget(QWidget*)` 借用应用的图标容器。Full 动效退场时，完整图标缩小并移入该控件的内容矩形；目标可以位于内容宿主外的同窗口标题栏。组件不改变目标的所有权、外观或可见性，目标图标的显示时机由应用协调。没有目标、目标不可见或不在同一窗口时使用淡出；目标销毁后指针归零，进行中的图标从最后有效位置淡出。传入遮罩自身或其子控件会被忽略。

`dismiss()` 遵循 Full/Reduced/Disabled 动效策略，隐藏后发送一次 `dismissed()`，默认不销毁。完成后可以再次 `show()` 复用；直接 `hide()` 会取消退场且不发送完成信号。一次性使用可连接 `dismissed` 到 `deleteLater`。隐藏状态下或退场中的重复 `dismiss()` 不触发额外信号。窗口最小化不会取消已发起的退场，恢复窗口时不会重新显示已关闭的遮罩。

Reduced 使用短淡出，Disabled 立即完成；两者都不播放图标位移或装饰性入场。HighContrast 不绘制背景光。任务完成后可立即 `dismiss()`，无需等待入场动画结束。

Gallery 外壳单独控制启动节奏：Full 动效下的 Branded 至少展示 1400 ms，加载时间计入其中；预热完成后至少保留 250 ms，再以 700 ms 的平滑起落曲线连接标题栏图标。长加载只补足就绪停留，不重复等待整段展示时间。Simple、Reduced 和 Disabled 保留 120 ms 的合成等待；首启引导从 `dismissed()` 后开始计时。公共组件不附加最短展示等待，示例中的 3600 ms 是可重播的模拟加载时间。

同一窗口内，新显示的遮罩会直接隐藏同宿主或嵌套重叠宿主上已有的遮罩；互不重叠的容器可以各自显示。新遮罩就绪后，旧遮罩发送一次 `replaced()`，不发送 `dismissed()`，也不自动销毁。一次性使用应将 `dismissed` 和 `replaced` 都连接到 `deleteLater`；可复用的遮罩可以保留对象，稍后重新显示。直接 `hide()` 和窗口最小化不发送 `replaced()`。

显示及退场期间，遮罩拦截宿主内容内的鼠标和键盘输入，接管内容区焦点并阻止底层快捷键通过该焦点触发；隐藏后尽可能恢复之前的焦点。宿主外的标题栏、控件和其他窗口继续正常工作。独立弹层及宿主外命令仍由应用协调。加载工作应分批调度或在工作线程执行，不能阻塞 GUI 线程，否则动画和窗口操作也会停止。

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

模态或 dim overlay 使用同窗口 `OverlayScrim`：scrim 位于背景控件之上、overlay 卡片或抽屉之下。模态 scrim 阻止背景 pointer input；非模态或非 dim 场景不保留阻塞 scrim。overlay 打开、top-level resize、抽屉位置更新时都应显式维护 `scrim -> overlay` 的 z-order（`raiseOverlayStack`）。

关闭 overlay 时必须同步隐藏或销毁 scrim，避免 stale scrim 留在 top-level 上继续阻塞背景控件。`Dialog` 烟雾与 `Popup` / `DrawerView` 共用同一 `OverlayScrim` 实现（含可选圆角表面与 spotlight）。

## Rendering And Theme

Overlay surface、border、shadow、smoke/scrim 均使用 Fluent token 和自绘。visible card/panel 外应保持透明，圆角区域不能由嵌入子控件背景泄漏填充。主题切换只触发重绘和 hosted content 样式刷新，不改变 open state、placement、selected value 或 content ownership。

同窗口子控件不要用独立原生窗口的 `windowOpacity` 做淡入淡出；使用 `QGraphicsOpacityEffect`（或等价控件内 opacity）画进宿主共享后备缓冲。

## Animation

禁用动画时，open/close 的可见性、进度、scrim、geometry 和 lifecycle signals 必须同步落定。启用动画时，Popup/Flyout/Dialog 的 opacity-only transition 不应改变 visible card geometry；DrawerView 的 position animation 保持 normalized `position` 语义。

## Open State Machine

Overlay 组件统一可观察语义，不统一继承树。`Popup` / `Flyout` / `TeachingTip`、`CoachMark` 与 `Dialog` / `ContentDialog` 保持各自的 Qt 基类；`DrawerView`、`ComboBox` / `MultiSelectComboBox` dropdown、`SplitButton` / `DropDownButton`（QMenu）不并入同一基类。

### 三个“打开”分别指什么

| 概念 | API | 含义 |
| --- | --- | --- |
| 逻辑请求态（公开 `isOpen`） | `isOpen()` / `setIsOpen(bool)` / `isOpenChanged` | 调用方请求的开关。`open()` / `setIsOpen(true)` 在 Opening 开始时即为 `true`；`close()` / `setIsOpen(false)` 在 Closing 开始时即为 `false`。 |
| 动画完成态 | `opened()` / `closed()` | 入场或退场动画结束（禁用动画时与请求同步落定）。 |
| 控件可见性 | `QWidget::isVisible()` | 实现细节。Opening 在 `show()` 前可暂不可见，Open 为可见；Closing 期间仍可见，退场完成后先 `hide()`，再发 `closed()`。 |

公开绑定应使用 `isOpen`，不要用 `isVisible()` 推断逻辑开关。`popupProgress` / `animationProgress` 只描述过渡，不代替 `isOpen`。

### 相位与信号顺序

相位：`Closed → Opening → Open → Closing → Closed`。

打开（canonical 在前，旧名为兼容别名，两者都发）：

1. `opening()`（别名 `aboutToShow()`）
2. `isOpenChanged(true)`（仅在逻辑态实际变化时）
3. 控件 `show()`，scrim / geometry 就位
4. `opened()`（动画结束；禁用动画时在同一次调用内同步发出）

关闭：

1. `closing(reason)`（别名 `aboutToHide()`；无参 `aboutToHide` 保持原签名；`Dialog` 的 canonical 信号仍为无参 `closing()`）
2. `isOpenChanged(false)`（仅在逻辑态实际变化时）
3. 退场动画（若启用）；此间 `isOpen() == false` 且 `isVisible() == true`
4. `hide()` 并释放 scrim，然后 `closed()`

`Dialog` / `ContentDialog` 使用同一顺序。`QDialog::finished(int)` / `accepted()` / `rejected()` 保留；它们不是 overlay 相位信号。`TeachingTip::closing(TeachingTip::CloseReason)` 保留为组件特定信号，数值 0–4 与 `Popup::CloseReason` 对齐。

`CoachMark` 保留既有 `open` 属性、`isOpen()` / `setOpen()` 和 `openChanged(bool)`，不在 1.7 中新增一组同义公开 API。`openChanged` 在逻辑请求态变化时发出；`opened` 在淡入完成后发出，`closed` 在淡出完成并隐藏后发出。打开中关闭或关闭中重开会反转当前过渡，不得为被取消的方向发出完成信号。

### 重入

| 调用 | 规则 |
| --- | --- |
| `open()` 在 Opening / Open | 忽略（no-op，不重复信号） |
| `close()` 在 Closing / Closed | 忽略 |
| `close()` 在 Opening | 取消入场，转入 Closing |
| `open()` 在 Closing | 取消退场，转入 Opening（从当前进度反向） |
| 相位信号处理里销毁 overlay | 允许；实现必须用 `QPointer` 防护，不得在销毁后发后续信号 |

`opening` / `closing` **不可取消**。需要阻止关闭时用 `NoAutoClose` 或不要调用 `close()`。

### Close reasons

`Popup::CloseReason`（`Q_ENUM`）：

| 值 | 何时 |
| --- | --- |
| `Programmatic` | `close()` / `setIsOpen(false)` / `Dialog::done()`，以及未标明原因的关闭 |
| `ActionButton` | TeachingTip / ContentDialog 主操作等显式动作 |
| `CloseButton` | TeachingTip 关闭按钮 |
| `LightDismiss` | `CloseOnPressOutside` 命中可见卡片外 |
| `TargetDestroyed` | 锚点 / target 销毁 |
| `Escape` | `CloseOnEscape`（TeachingTip 在 light-dismiss 开启时仍通过自己的 `closing` 报告 `LightDismiss`，以保持既有数值） |

未标明时默认为 `Programmatic`。重复 `close()` 不重复发 `closing`。

应用级事件过滤器只能处理所属顶层窗口内的 Escape。若按键来自原生菜单、其他顶层窗口，或当前事件位于另一个同窗口 overlay 内，应先交给该表面处理；CoachMark 不得跨窗口关闭，也不得抢先吞掉更上层菜单或 overlay 的 Escape。

### `modal`、`dim`、`closePolicy` 正交

三条轴独立，不要用其中一条去推断另一条：

- **`modal`**：是否用 scrim **挡住**背景指针。`true` 时 scrim `WA_TransparentForMouseEvents` 为 false。
- **`dim`**：是否绘制烟雾。`false` 时 scrim 不涂色（仍可按 `modal` 拦截输入）。
- **`closePolicy` / light-dismiss**：`NoAutoClose`、`CloseOnPressOutside`、`CloseOnEscape`。只约束隐式关闭，不决定是否有 scrim。

组合：

- 二者皆 false：无 scrim。
- 仅 `modal`：不可见的输入挡板。
- 仅 `dim`：可见但不拦截的烟雾；outside press 仍可按 `closePolicy` 关闭，并允许穿透到背景。
- 二者皆 true：挡输入的烟雾。

`Dialog::setSmokeEnabled(true)` 是历史兼容包：同时打开 `modal` 与 `dim`。`setSmokeEnabled(false)` 同时关掉二者；`isSmokeEnabled()` 仅在两条轴都为 `true` 时返回 `true`。正交组合请直接 `setModal` / `setDim`。`DrawerView` 继续用自己的 `ClosePolicy` 类型，不与 `Popup::ClosePolicy` 合并。

### NOTIFY 与 no-op

可绑定属性（`isOpen`、`modal`、`dim`、`closePolicy`、`animationEnabled`，以及各组件已公开的同类属性）必须：

- 提供 `NOTIFY` 信号；
- 写入当前值时为 no-op，不重复 `NOTIFY`。

主题切换只重绘 / 刷新 hosted content，不得改变 `isOpen`、placement、selected value 或 content ownership，也不得因此发出 open-state 信号。

### 兼容别名

旧名称保留到下一个 major 才考虑删除：

- `aboutToShow` ↔ `opening`
- `aboutToHide` ↔ `closing`（无参别名）
- `setIsOpen` / `isOpen` 为公开开关；`open()` / `close()` 为命令
- `Dialog::isSmokeEnabled` / `setSmokeEnabled` ↔ 历史烟雾包（见上）
- TeachingTip 的 `CloseReason` 与 `closing(TeachingTip::CloseReason)` 保留

### 不强制统一的继承

不要把 `DrawerView`、`ComboBox`、`MultiSelectComboBox`、`SplitButton`、`DropDownButton`、`Dialog` 收进同一个 overlay 基类。`SplitButton` / `DropDownButton` 的 `isOpen` 描述的是 QMenu 可见性，不是 same-window overlay 相位。`ComboBox` 与 `MultiSelectComboBox` dropdown 继续组合 `Flyout`。`fluent::overlay::OverlayCoordinator` 保持内部实现。

## Preserved Differences And Deferred Work

`ComboBox` 与 `MultiSelectComboBox` dropdown 保持非模态、非 dim；前者保留当前 index、editable text 和 ListView 单选行为，后者保留即时多选、搜索与 filtered select-all 行为。`DrawerView` 保留 edge drag、normalized position、content widget ownership 和现有 public `ClosePolicy` API。

暂不把 `Popup::ClosePolicy` 与 `DrawerView::ClosePolicy` 合并，也不让 `DrawerView` 继承 `Popup`。这些 public API consolidation 若需要，应通过后续独立设计与实现任务单独评估。

需要真正跨应用边界的系统对话框时，应使用独立 `Window`，不要让 `ContentDialog` / `Dialog` 再走原生顶层旁路。

<!-- docs-nav:bottom:start -->
---
[← Window Chrome Architecture](window-chrome.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Typography Resolution →](typography-resolution.md)
<!-- docs-nav:bottom:end -->
