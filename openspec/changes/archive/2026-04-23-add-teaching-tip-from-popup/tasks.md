## 1. Public API 与骨架

- [x] 1.1 创建 `src/view/dialogs_flyouts/TeachingTip.h` / `TeachingTip.cpp`，建立 `TeachingTip : public Popup` 基础结构
- [x] 1.2 定义 `PreferredPlacement`（13值含Auto）与 `CloseReason`（5值）枚举，公开属性仅保留 frame 语义：`target`、`preferredPlacement`、`placementMargin`、`lightDismissEnabled`、`tailVisible`；新增 `contentHost()` 和 `setCardSize()`，不内置固定 schema
- [x] 1.3 提供 `showAt(QWidget*)`、`setTarget(QWidget*)`、`closeWithReason(CloseReason)` 或等价内部关闭入口，并声明 `closing(CloseReason)` 信号

## 2. 内部布局与视觉结构

- [x] 2.1 创建内部裸容器 `m_contentHost`，几何由 `cardRect()` 自动维护，通过 `contentHost()` 暴露给调用方，调用方自行在其上建立 AnchorLayout 并添加子控件
- [x] 2.2 `updateWidgetSize()` 自动计算 widget 总尺寸 = cardSize + 2×shadowMargin + tailInsets；`showAt()` 在 `open()` 前调用 `updateWidgetSize()` 确保 tail 已计入
- [x] 2.3 在 `paintEvent()` 中自绘 TeachingTip 的卡片 + tail 合并路径（tail 底边内缩 2px 确保 `united()` 真实融合无接缝），使用现有 Fluent overlay token，保持阴影 margin 与 card 几何一致

## 3. 定位、生命周期与关闭语义

- [x] 3.1 基于 `target` 几何实现 12 个显式 placement 与 `Auto` 的位置计算；`placementMargin` 语义为 tail 尖端到 target 边缘距离，卡片起点 = target ± (placementMargin + kTailSize)
- [x] 3.2 Auto 只检查主轴方向空间决定回退，横轴溢出由 `clampCardTopLeft()` 修正；tail 对齐点随 placement 变化
- [x] 3.3 用 `QPointer` + event filter 监听 `target` 与 top-level window，在 move/resize/hide 时重新定位
- [x] 3.4 `target` 销毁时自动关闭，并产出 `CloseReason::TargetDestroyed`
- [x] 3.5 将 `lightDismissEnabled` 映射到 `Popup::closePolicy`，并让 outside click / Escape / programmatic close 都产出正确 `CloseReason`

## 4. 测试与 VisualCheck

- [x] 4.1 创建 `tests/views/dialogs_flyouts/TestTeachingTip.cpp`，覆盖默认属性（含 `contentHost` 非 null、`cardSize=360×200`、`placementMargin=4`）、`showAt(target)`、target 销毁、light-dismiss、`closeWithReason` 关闭原因
- [x] 4.2 为 placement 与 fallback 增加用例：`Bottom`（contentHost.top == anchorRect.bottom + margin + 12）、`Auto` 近底边回退至 Top、`RightTop`（contentHost.left == anchorRect.right + margin + 12）
- [x] 4.3 为 contentHost 增加用例：`ContentHostMatchesCardSizeAndPlacement`（`contentHost()->size() == cardSize`）、`UserChildrenStayInsideContentHost`（QLabel 在 host->rect() 内）
- [x] 4.4 添加 VisualCheck：4 个 anchor 按钮触发 4 款 tip（Simple / Rich / Top / RightTop），每款 tip 的 `contentHost` 上用 AnchorLayout + TextBlock/Button 自组装内容
- [x] 4.5 在 `tests/views/dialogs_flyouts/CMakeLists.txt` 注册 `test_teaching_tip`

## 5. 验证与回归

- [x] 5.1 增量构建并运行 `test_teaching_tip`：10/10 PASSED
- [x] 5.2 VisualCheck 可视确认：位置正确（tail 尖端到 target 边 = placementMargin）、无灰色背景、无接缝线
- [x] 5.3 回归运行 `test_popup` 与 `test_flyout`，确认不破坏现有 Popup/Flyout 行为