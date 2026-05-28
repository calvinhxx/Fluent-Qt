## Context

当前 `Dialog` 基类实现了入场/退场对称的 "resize 96%↔100% + opacity 0↔1" 动画（`kScaleFrom = 0.96`），以及 `SmokeOverlay`（纯 `QWidget`，`paintEvent` 填充固定半透明黑）。
`ContentDialog` 派生实现了 Title / Content / ButtonBar 三区布局，按钮条使用 `QHBoxLayout` 配合 `stretch=1` 将三个按钮等宽撑满。

参考 GIF（WinUI 3 Gallery ContentDialog 演示）关键观察：
- **入场**：Dialog 有明显的 pop-in 放大感（幅度比当前 0.96 更大），透明度同步渐显。
- **退场**：Dialog 有明显的 pop-out 缩小感，透明度同步渐隐；蒙层同步淡出而非瞬间消失。
- **按钮条**：三个按钮 `Save / Don't Save / Cancel` 宽度各约 ~100px，**左对齐**排列，按钮之间 ~8px 间距，右侧为留白。
- **按钮条高度**：约 68px，按钮本身 32px，上下内边距 ~18px。

### 关键约束：顶层窗口不支持 QGraphicsOpacityEffect

`Dialog` 继承自 `QDialog`，作为独立的顶层 `Qt::Window`。Qt 官方明确：
> `QGraphicsEffect` is not supported on top-level windows.

实际表现为：在顶层 window 上 `setGraphicsEffect` 会被静默忽略，且可能输出 `QWidget::setGraphicsEffect, effects on widgets that are window are not supported` 警告。
因此：
- **Dialog 本体的淡入淡出**只能依赖 `setWindowOpacity` + compositor 合成（当前实现方式保留）。
- **Smoke 蒙层**同样是独立顶层 window（frameless + Tool hint），不能使用 `QGraphicsOpacityEffect`。因此蒙层淡入淡出改为在 `paintEvent` 中按一个自定义 `progress (0..1)` 属性插值 `bg.alpha`，由 `QPropertyAnimation` 驱动。

## Goals / Non-Goals

**Goals:**
- 将 Dialog 入/出场 scale 幅度从 0.96 加大到 0.90，使 pop-in/pop-out 更明显，对齐 GIF 观感。
- Smoke 蒙层在入场/退场期间与 Dialog 同步淡入淡出（通过自定义 `progress` 属性 + `paintEvent` alpha 插值），duration 与 Dialog 主动画一致。
- ContentDialog 按钮条改为左对齐紧凑布局，按钮最小宽度 96px。
- ContentDialog 按钮条高度从 80 收紧到 68。
- 所有变更可被单元测试/VisualCheck 验证。

**Non-Goals:**
- 不尝试给顶层窗口加 `QGraphicsOpacityEffect`（Qt 不支持，见 Context）。
- 不改动 Dialog 公共 API（`setAnimationEnabled`、`done` 等签名保持不变）。
- 不改动 ContentDialog 的 Title / Content 布局结构。
- 不引入新的动画曲线或 duration 常量（复用 `themeAnimation().normal` / `entrance`）。
- 不调整 Button 基类；按钮 min-width 仅通过 `setMinimumWidth` 在 ContentDialog 内生效。

## Decisions

### Decision 1: 加大 scale 幅度至 `kScaleFrom = 0.90`

**选择**：在 `Dialog.cpp` 匿名 namespace 中，将 `constexpr double kScaleFrom = 0.96` 改为 `0.90`。入场/退场均对称使用该缩放范围（90%↔100%），配合 opacity 0↔1。`setAnimationProgress(p)` 的 resize + move 逻辑保持不变。

**理由**：
- GIF 观察的入场"弹出感"和退场"缩回感"明显大于 4% 形变；10% 形变更接近参考。
- 保留对称 scale 避免入/出场行为割裂（WinUI 3 的 Popup/ContentDialog 也是对称的 scale transform）。
- 顶层窗口无法使用 `QGraphicsOpacityEffect`，因此 pop 效果只能靠 `resize` + `move` 实现；加大幅度是成本最低的视觉提升手段。

**备选（已淘汰）**：
- 退场改为纯 opacity（移除 scale）→ 与 GIF 观感不一致（参考仍有可见的缩小）。
- 用 `QTransform` 渲染缩放 → 顶层 window 不适用，且会破坏子控件命中测试。

### Decision 2: Smoke 蒙层通过 `progress` 属性 + `paintEvent` alpha 插值联动淡入淡出

**选择**：
1. `SmokeOverlay` 增加 `Q_PROPERTY(double progress ...)` 与成员 `m_progress`（0..1），setter 调用 `update()`。
2. `SmokeOverlay::paintEvent` 中 `painter.fillRect(rect(), QColor(0,0,0, int(baseAlpha * m_progress)))`，`baseAlpha` 为主题定义。
3. `Dialog` 新增成员 `QPropertyAnimation* m_smokeAnim`，target 为 `m_smokeOverlay` 的 `progress` 属性。
4. `showSmokeOverlay()` 启动 `m_smokeAnim` 0→1（duration = `themeAnimation().fast`，150ms）。
5. `hideSmokeOverlay()` 启动 `m_smokeAnim` 1→0（duration = `themeAnimation().fast`），`finished` 回调中 `deleteLater()` overlay 并置 `m_smokeOverlay = nullptr`。
6. 若 overlay 正在淡出过程中 Dialog 被再次 `open()`，重用现有 overlay，将动画反向。
7. **Dialog 析构时同步清理 overlay**：`~Dialog()` 中若 `m_smokeOverlay` 仍在，直接 `hide()` + `delete`，避免栈上 Dialog（如 `ContentDialog dialog(window); dialog.exec();`）exec 返回后立即析构导致 `m_smokeAnim` 随之死亡、`finished` 不再触发、overlay 孤儿残留在屏幕。

**理由**：
- 顶层 window 不支持 `QGraphicsOpacityEffect`（见 Context），因此必须在 `paintEvent` 层面自行插值 alpha。
- `QPropertyAnimation` 直接 animate 自定义 `double progress` 属性简单可靠，复用已有动画基建。
- `finished` 回调延迟销毁 overlay 保证淡出完整播放。

**备选（已淘汰）**：
- `QGraphicsOpacityEffect` → **Qt 不支持顶层 window**，直接否决。
- 用 `setWindowOpacity` 控制蒙层 → 与 `paintEvent` 风格不统一，且在某些 Linux WM 下 compositor 合成质量不一致。
- 合并进 Dialog 主 `animationProgress` → 破坏 `hideSmokeOverlay()` 的独立调用语义；`hideEvent` 路径中 Dialog 动画可能已结束但 smoke 仍需独立淡出。

### Decision 3: 按钮条 QHBoxLayout 去掉 stretch，末尾加 addStretch()

**选择**：将 `btnLayout->addWidget(btn, 1)` 改为 `btnLayout->addWidget(btn, 0)`，并在最后一个按钮之后 `btnLayout->addStretch(1)`。同时在 `setupInternalLayout()` 中对三个按钮统一 `setMinimumWidth(kButtonMinWidth)`，其中 `kButtonMinWidth = 96`。

**理由**：
- `addStretch(1)` 使按钮集合左对齐，右侧留白。
- `setMinimumWidth(96)` 保证短文案按钮（如 "OK"）仍具合理触达面积。
- 按钮不使用固定宽度 → 文案较长（如 "Don't Save"）时仍可自然扩展。

**备选（已淘汰）**：
- 右对齐 (`addStretch` 放最前) → GIF 明显是左对齐。
- 全部用 `setFixedWidth(96)` → 无法容纳长文案。

### Decision 4: 按钮条高度 80 → 68

**选择**：`kButtonBarHeight` 从 `80` 改为 `68`。vPad 自动由 `(68 - ControlHeight::Standard(32)) / 2 = 18` 推出。

**理由**：观察 GIF 按钮条比例约为按钮高度 2.1 倍，68 更贴近。

### Decision 5: 动画时长统一为 `themeAnimation().fast`(150ms)

**选择**：Dialog 入场主动画（`m_animation`）以及 Smoke 淡入动画（`m_smokeAnim`）将 duration 从 `themeAnimation().normal`(250ms) 改为 `themeAnimation().fast`(150ms)。退场时长保持 `fast`。

**理由**：
- 250ms 入场与 WinUI 3 GIF 实际节奏比偏慢，pop-in 给人洞恶感。
- WinUI 3 ContentDialog 的实际过渡时间更接近 150ms；保留 `OutBack` 弹性指令曲线仍保留视觉纯度。
- 入/出场 duration 一致更符合用户心理预期（“开多快就关多快”）。

**备选（已淘汰）**：
- 添加新常量如 `Duration::Dialog = 180` → 不必要，复用 `Fast` 已足够。

### Decision 6: `~Dialog()` 同步销毁 Smoke Overlay

**选择**：为 `Dialog` 添加 `override ~Dialog()`，若 `m_smokeOverlay` 非空则直接 `hide()` + `delete`（非 `deleteLater()`）。

**理由（Bug修复）**：
- VisualCheck 等典型用法中 Dialog 为栈对象：例如 `ContentDialog dialog(window); dialog.exec();`。`exec()` 返回后 lambda 出栈，dialog 立即析构。
- `m_smokeAnim` 以 Dialog 为 parent，随 Dialog 析构激发 `QObject` 级联销毁→ `finished` 信号不再触发 → `hideSmokeOverlay()` 里安装的清理 lambda 不会运行。
- overlay 本身以 `parentWidget()`（如主窗口）为 parent，比 Dialog 寿命长 → **孤儿残留在屏幕上表现为“关闭 dialog 后模态蒙层挂住”**。
- 析构中同步 `delete` 是最小侵入且确定性最强的兄弟方案：放弃淡出动画的最后几帧（无论如何 Dialog 已经消失，视觉上 overlay 立即消失不会让用户觉得突兵）。

**备选（已淘汰）**：
- 将 `m_smokeAnim` reparent 到 overlay 以继承它的寿命 → 仍不解决 Dialog 析构时直接访问已销毁 Dialog 成员的风险。
- 将 `finished` lambda 的接收者改为 overlay 而非 Dialog → 同样仍受 m_smokeAnim 销毁影响。
- 用 `deleteLater()` 而非 `delete` → 存在性能上没区别，但 `delete` 即刻移除绘制例实更清晰。

### Decision 7: VisualCheck 示例内容对齐 WinUI 3 Gallery 参考图

**选择**：将 `TestContentDialog::VisualCheck` 中第一个“Standard 2-Button Dialog”示例的内容从“长段落 Windows 11 marks...”替换为与参考图一致的组合：

- Dialog 尺寸：`454 x 232`（参考图比例）
- Title："Save your work?"
- Content（包装为 `QWidget` + `QVBoxLayout`）：
  - `TextBlock`："Lorem ipsum dolor sit amet, adipisicing elit."（单行短文）
  - `CheckBox`："Upload your content to the cloud."
- Buttons：Save（Accent）/ Don't Save / Cancel（三按钮）

**理由**：
- 旧示例用长段落填满了整个内容区，看不到按钮条高度收紧效果也看不到 "Content + CheckBox" 的复合布局效果——与用户期望对齐的 WinUI 3 Gallery 参考图不一致。
- 增加 CheckBox 验证了 `setContent(QWidget*)` 接受包装 widget（QVBoxLayout）的能力——实际业务常需在 Content 区放多个控件。
- `454 x 232` 比旧的 `572 x 300` 更贴近参考图的紧凑观感。

**备选（已淘汰）**：
- 保留旧长段落示例，另外新增第四个 VisualCheck 按钮 → 扩充 VisualCheck 规模，用户需点击更多按钮才能看到对齐参考图的版本，成本高。

### Decision 8: `isVisible()` → `text().isEmpty()` 布局决策

**背景（Bug）**：首次 VisualCheck 显示 Title 被遮盖、ButtonBar 消失、整个 dialog 变灰，与参考图严重不符。

**根因**：`QWidget::isVisible()` 返回"有效可视性"——**祖先窗口未 `show()` 时永远返回 `false`**，即使 `setVisible(true)` 已被调用。`ContentDialog` 的典型用法是：
```cpp
ContentDialog dialog(window);
dialog.setTitle("...");                // → updateContentAnchors() 判断 titleLabel->isVisible() == false
dialog.setPrimaryButtonText("...");    // → updateButtonBar() 判断 primaryBtn->isVisible() == false
dialog.exec();                         // 到这里才真正 show()
```
在所有 setter 调用阶段，dialog 尚未进入 shown 状态，`isVisible()` 全部返回 false，导致：
1. `updateContentAnchors()` 误判 title 不可见 → content 锚到 dialog 顶部，**覆盖 title**
2. `updateButtonBar()` 误判所有按钮不可见 → `anyVisible=false` → **整个 buttonBar 被隐藏**

**选择**：两处布局决策改用"逻辑可见性"—— `!text().isEmpty()`：
- `updateContentAnchors()`: `if (!m_titleLabel->text().isEmpty())`
- `updateButtonBar()`: `anyVisible = !m_primaryBtn->text().isEmpty() || !m_secondaryBtn->text().isEmpty() || !m_closeBtn->text().isEmpty()`

**理由**：
- 与 `setVisible()` 参数等价（`setVisible(!text.isEmpty())`），语义一致。
- 不依赖窗口生命周期状态，任何时候调用结果稳定。
- 避免通过 `show() → relayout` 的隐式时序耦合。

**备选（已淘汰）**：
- 把布局决策延迟到 `showEvent()` → 增加时序耦合，且 `setContent()` 等链式调用期间无法立即校验。
- 在 setter 中临时 `show()/hide()` dialog 来激活 `isVisible()` → 产生可见闪烁。

### Decision 9: 测试主窗口背景改用 QPalette 而非 QSS（避免污染 Dialog 子控件）

**背景（Bug）**：第二轮 VisualCheck 显示 Dialog 的 Title 和 Content 区域为灰色（应为白色 bgLayer），CheckBox / TextBlock 看起来都自带灰底。

**根因**：`TestContentDialog.cpp` 的 `FluentTestWindow::onThemeUpdated()` 之前用 `setStyleSheet("background-color: <bgCanvas>")` 设置主窗口背景。**Qt QSS 会沿 widget 父子链向所有子孙传播**——包括弹出的 ContentDialog 内的所有子控件——导致 TextBlock / CheckBox / contentHost 被全部染成 `bgCanvas` 灰色，覆盖了 ContentDialog `paintEvent` 绘制的白色 bgLayer。

**选择**：测试主窗口改用 `QPalette::Window` + `setAutoFillBackground(true)`：
```cpp
QPalette pal = palette();
pal.setColor(QPalette::Window, c.bgCanvas);
setPalette(pal);
setAutoFillBackground(true);
```

**理由**：
- `QPalette` 配合 `autoFillBackground` **仅作用于自身**，不向子控件传播。
- 主窗口的 background 行为完全等价（视觉一致），不影响其它测试。
- 此修复仅在测试侧，不污染产品代码。

**备选（已淘汰）**：
- 在 ContentDialog 内对所有子控件递归调用 `setStyleSheet("background-color: transparent")` → 侵入产品代码且脆弱（任何新增子控件都需要补此调用）。
- 使用 `setStyleSheet("FluentTestWindow { background: ... }")` 限定 selector → 仍可能在某些 Qt 版本下传播到子控件的 `QWidget` 选择器，且要求测试 widget 类名稳定。

## Risks / Trade-offs

- **[风险] 加大 scale 到 0.90 可能在低端 compositor 下出现 resize+move 不同步的抖动** → Mitigation：当前动画依赖 `QPropertyAnimation` 驱动单一 `animationProgress`，所有 resize/move/opacity 在同一帧内完成，理论同步；若出现抖动可回退至 0.93。
- **[风险] 150ms fast 动画在低端机可能漏帧** → Mitigation：fast 是主题 token 统一使用的“按钮点击等即时反馈”时长，已在其他组件验证过。若未来观察到折扁感可在 Decision 5 层面回退至 `normal`。
- **[风险] `SmokeOverlay::paintEvent` 在高频动画中可能造成 CPU 负载（每帧 `fillRect`）** → Mitigation：overlay 占全屏但 paint 为单一 `fillRect`，开销极低；60fps 下无感。
- **[风险] 按钮最小宽度 96px 可能与现有业务代码中的短按钮预期冲突** → Mitigation：`kButtonMinWidth` 仅作用于 ContentDialog 内部三个按钮，不影响 Button 基类的其它使用场景。
- **[Trade-off] 按钮条高度缩减后，若文案折行（非常罕见），可能显得拥挤** → 不处理：ContentDialog 按钮文案约定为一行；业务方若需多行应改用自定义内容。
- **[Trade-off] `~Dialog()` 中直接 `delete` overlay 而非等待淡出完成** → 可接受：Dialog 析构意味着 Dialog 本体已消失（无论之前是否播放完退场动画），overlay 同步消失在视觉上不会造成折扁感。

## Migration Plan

不涉及迁移：无 public API 变更。本次为内部视觉/动效调整，业务代码无需改动。测试需要更新以反映新预期（已在 tasks.md 中列出）。
