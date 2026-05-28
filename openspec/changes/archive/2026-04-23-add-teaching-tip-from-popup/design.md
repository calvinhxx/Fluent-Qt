## Context

当前仓库已经有两层可复用弹层基建：

- `Popup`：自由坐标 overlay 基类，负责 top-level reparent、shadow margin、outside click / Escape 关闭、scrim、进出场动画
- `Flyout`：在 `Popup` 之上补了 `anchor + placement` 语义，但仍然假设自身是普通矩形卡片，不承担尾巴、引导内容和关闭原因等 TeachingTip 专属语义

用户现在要重新生成 `TeachingTip` 方案，并要求以 `Popup` 为基类做自定义派生，而不是继续扩展 `Popup` 本身。外部参考面已经比较明确：

- **WinUI Gallery** 样例确认了公开行为面：`Target`、`PreferredPlacement`、`PlacementMargin`、`IsLightDismissEnabled`、`ActionButtonContent`、`CloseButtonContent`、`HeroContent`
- **Figma Windows UI Kit** 的 TeachingTip 变体确认了视觉结构：
  - 基础款为浅色圆角卡片，带阴影和尾巴，标题/正文均为 14px、20px line-height
  - Hero 变体为顶部 180px 图像 + 文本区 + 按钮区，关闭按钮为 40x40 命中区域
  - 按钮区横向间距为 8px，按钮最小宽度约 96px
  - Tail 存在 `Top/Bottom/Left/Right` 不同边与 `Left/Center/Right` 或 `Top/Center/Bottom` 对齐点的组合，另有 `Light dismiss=True/False` 变体

另一个已知项目约束来自仓库经验：这类带阴影 margin 的浮层如果直接把内容挂在 `Popup` 根上，布局和视觉卡片会脱节，导致内容错位。因此 TeachingTip 需要把真实内容放进内部 card 容器，并让 card 与尾巴一起决定可见几何。

## Goals / Non-Goals

**Goals:**
- 以 `Popup` 为唯一基类实现 `TeachingTip`，不修改 `Popup` 的既有公共 API
- 实现 frame-only 架构：仅包含 target 锚定、placement、placement margin、light dismiss、tail 绘制和关闭原因语义；内容通过 `contentHost()` 由调用方自组装
- 让布局与绘制围绕"内部 contentHost + 尾巴"展开，避免 shadow margin 造成内容错位
- 暴露足够清晰的关闭原因语义，便于测试 light-dismiss、target 销毁、programmatic close 等场景
- 为后续实际编码准备好可直接映射到 GTest 的规格与任务清单

**Non-Goals:**
- 不把 TeachingTip 专属语义回灌到 `Popup` 或 `Flyout` 基类
- 不实现 `MenuFlyout`、`CommandBarFlyout`、`ToolTip` 等其它弹层
- 不引入新的 Design Token 或新的第三方绘图库
- 不做 WinUI 所有交互细节的一比一复制，例如动画曲线与无障碍 announcement 细节

## Decisions

### Decision 1: `TeachingTip` 直接派生自 `Popup`，不派生自 `Flyout`

**选择**：`class TeachingTip : public Popup`，由 TeachingTip 自己重写定位计算、关闭策略映射与绘制。

**原因**：
- `Flyout` 已经把“普通矩形卡片 + placement”做成了一个较窄的上层抽象，但 TeachingTip 还要增加 tail、hero、按钮区、关闭原因和更复杂的 fallback 逻辑
- 如果继续从 `Flyout` 派生，TeachingTip 会被迫继承一套为无尾巴矩形卡片设计的几何假设，后续要么重复覆盖，要么反向修改 `Flyout`
- `Popup` 已经提供了真正稳定的公共基座：overlay 生命周期、outside click / Escape、opacity 动画、scrim、top-level 坐标系统

**替代方案**：
- 派生自 `Flyout`：复用 anchor/placement，但会把 tail 和复杂布局强行塞进“普通 flyout”抽象
- 直接修改 `Popup`：污染基类，破坏 `Popup` 作为自由坐标 overlay 的边界

### Decision 2: 使用"根 widget 负责绘制，`contentHost` 暴露内容区"的双层结构

**选择**：TeachingTip 根 widget 保留 `Popup` 的 shadow margin 和动画能力，不把实际内容直接挂在根上。新增内部裸容器 `m_contentHost`（`QWidget*`），其几何由 `cardRect()` 自动维护（= cardSize，与 card 完全重合），调用方通过 `contentHost()` 获取引用后自行在其上建立 `AnchorLayout` 并添加子控件。组件不内置任何固定 schema。尾巴、边框、背景和阴影由根 widget 的 `paintEvent` 统一绘制。

**原因**：
- shadow margin 是为了阴影，不应参与内容布局；`contentHost` 精确等于 card 可用区域，调用方不需要关心坐标系偏移
- 去掉固定 schema（title/subtitle/icon/hero/buttons）后，组件变为纯 frame，与业务层解耦——不同场景可以在 `contentHost` 上自由组装内容，组件本身不随业务变化而膨胀
- 尾巴是否显示只影响 widget 总尺寸和 card 的偏移，`contentHost` 几何自动跟随，调用方无感

**替代方案**：
- 内置固定 schema（title/subtitle/icon/hero/buttons）：最初的设计方向，但实现后发现内容结构与业务强耦合，组件过重，难以适应不同内容布局需求
- 继续在根 widget 上直接布局：最容易再次出现内容进入阴影区、尾巴与 child 不对齐的问题

### Decision 3: `PreferredPlacement` 覆盖 12 个定向位置 + `Auto`

**选择**：TeachingTip 提供 `Auto` 与 12 个显式方位值：
- `Top`、`TopLeft`、`TopRight`
- `Bottom`、`BottomLeft`、`BottomRight`
- `Left`、`LeftTop`、`LeftBottom`
- `Right`、`RightTop`、`RightBottom`

其中显式值决定卡片相对 target 的主边和 tail 对齐点；`Auto` 默认优先 `Bottom`，空间不足时按 `Top -> Right -> Left` 顺序回退。

**原因**：
- WinUI Gallery 明确存在 `PreferredPlacement` 与 `PlacementMargin`
- WinUI Gallery Sample 3 还明确存在 `Content` 正文槽位，不能只靠 `subtitle` 覆盖全部正文结构
- Figma 明确提供了多种 tail 对齐点，而不仅仅是四个主方向
- 只做四向 placement 会让视觉和语义都明显弱于 Figma 资产

**替代方案**：
- 仅实现 `Top/Bottom/Left/Right/Auto`：实现较小，但无法表达 Figma 中的边角 tail 变体
- 完全复制 Figma 每个变体为独立 API：过于笨重，枚举更适合 Qt 侧公开接口

### Decision 4: `IsLightDismissEnabled` 直接映射到 `Popup::closePolicy`

**选择**：
- `isLightDismissEnabled = true` 时，TeachingTip 启用 `CloseOnPressOutside | CloseOnEscape`
- `isLightDismissEnabled = false` 时，TeachingTip 切换为 `NoAutoClose`

**原因**：
- 现有 `Popup` 已经内建这两种关闭触发面，不需要额外发明一套平行机制
- Figma 也明确把 `Light dismiss=True/False` 作为变体维度

**替代方案**：
- 保留 Escape、仅关闭 outside click：会引入一套和 `Popup` 不一致的 close policy 映射，增加理解成本

### Decision 5: 关闭行为统一产出 `CloseReason`

**选择**：TeachingTip 暴露 `CloseReason` 枚举，并在关闭前发出 `closing(CloseReason)`。最少覆盖以下原因：
- `Programmatic`
- `ActionButton`
- `CloseButton`
- `LightDismiss`
- `TargetDestroyed`

**原因**：
- WinUI Gallery 的样例虽然只展示 `IsOpen` 切换，但项目里的 Qt 版本更需要信号来驱动测试和业务逻辑
- 没有关闭原因的话，light-dismiss、按钮点击和 target 销毁只能靠外部猜测，测试成本会明显升高

**替代方案**：
- 只暴露 `closed()`：过于贫弱，不利于回归测试

### Decision 6: Hero、按钮区、关闭按钮采用 Figma 已有几何约束

**选择**：
- Hero 内容在顶部，占据单独区域，默认高度按 Figma 样例使用 `180px`
- 标题与正文延续 Figma 14px / 20px 节奏，默认使用 `TextBlock` + `themeFont` 对应角色
- 额外 `content` widget 位于 subtitle 之后、按钮区之前，用于承载 WinUI Sample 3 的正文内容
- 关闭按钮为右上 40x40 点击区域
- 按钮区横向排列，gap = 8，按钮最小宽度 = 96
- 无可见按钮时整段按钮区收起，无多余空白

**原因**：
- 这些约束都来自已经读取的 Figma 节点，能明显减少“看起来像个气泡，但不像 TeachingTip”的风险
- 按钮最小宽度和 40x40 close hit target 也便于做稳定断言

### Decision 7: 对 target 和 top-level 安装 event filter，保持定位与生命周期稳定

**选择**：TeachingTip 对 `target` 与其 top-level window 使用 `QPointer` + event filter：
- target 销毁时自动关闭，并以 `TargetDestroyed` 作为原因
- target 移动、resize、hide，以及 top-level resize 时重新计算位置

**原因**：
- TeachingTip 是锚定型组件，不能只在 `open()` 时算一次位置
- `QPointer` 能保证 target 消失时不会悬空引用

## Risks / Trade-offs

- **[Risk] 12 个 placement + Auto 回退会增加几何分支复杂度** → Mitigation：把“主方向计算”“tail 对齐点计算”“窗口边界 clamp”拆成独立辅助函数，并用单测逐项覆盖
- **[Risk] 根 widget 继承自 Popup，而 Popup 自身也会绘制矩形卡片，可能与 TeachingTip 自绘重复** → Mitigation：在 TeachingTip 中覆盖绘制入口，显式接管卡片 + tail 的可见路径，不依赖 Popup 默认矩形背景
- **[Risk] Hero 内容若直接接受任意 QWidget，尺寸策略可能不稳定** → Mitigation：首版优先支持 `QPixmap`/图片型 hero；若支持任意 QWidget，则要求 caller 自己给出期望尺寸
- **[Trade-off] 直接派生 Popup 会重复一部分 Flyout 的 placement 经验** → 接受：TeachingTip 的 tail 与 card 几何和 Flyout 已不相同，共用过多反而会耦合
- **[Trade-off] `isLightDismissEnabled=false` 时把 Escape 一并关闭，行为更保守** → 接受：这与当前 `Popup` close policy 模型完全一致，且更容易解释和测试

## Migration Plan

无迁移成本。本 change 只新增 `TeachingTip` 能力，不修改 `Popup`、`Flyout`、`Dialog` 的现有 public API。