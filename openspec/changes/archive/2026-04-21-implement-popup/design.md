## Context

当前仓库里已有的 dialogs_flyouts 组件主要是 Dialog 和 ContentDialog。它们的语义是：

- 基于 QDialog
- 可进入模态流程
- 具备 result / done(int) 语义
- 更接近 WinUI ContentDialog，而不是 Popup

Popup 需要解决的是另一类问题：在同一 top-level widget 内显示一个轻量的 overlay，不引入额外窗口，也不把 API 做成对话框模型。

本 change 的实际实现与最初草案相比做了两次重要收敛：

1. 去掉了 placementTarget / Placement / Auto flip 之类的定位系统
2. 去掉了 scale 动画，只保留 opacity

原因不是裁剪功能，而是为了让 Popup 保持足够底层、足够稳定。复杂的 placement 和视觉变换，更适合作为未来 Flyout 类的上层策略，而不是固化在 Popup 这个基类里。

## Goals / Non-Goals

### Goals

- 提供一个真正的 Popup 基类，而不是再包一层 QDialog
- API 尽量保持 QML Popup 的简单心智模型：设置位置，open，close
- 支持 closePolicy、modal、dim、动画、主题自绘
- 支持在同一个 top-level widget 中以 overlay 形式显示
- 为未来派生组件预留 computePosition() 这类扩展点

### Non-Goals

- 不实现 placement 枚举和自动避让算法
- 不实现 ToolTip / Flyout 的延迟显示、箭头、target 对齐策略
- 不实现 native popup window 模式
- 不实现拖拽
- 不尝试兼容 QDialog 式的 result / exec 语义

## Decisions

### Decision 1: Popup 继承 QWidget，而不是 QDialog

选择：

- Popup : public QWidget, public FluentElement, public view::QMLPlus
- open() 时 reparent 到最初 parent 的 top-level widget
- 使用 QWidget 子层方式显示，不创建新的 OS 顶层窗口

为什么：

- 这和 WinUI Popup 以及 QML Popup.Item 的 overlay 模型更接近
- 跟随宿主窗口移动、隐藏和 z-order，更符合轻量浮层预期
- 避免 QDialog 带来的 result、模态 loop 和窗口管理副作用

### Decision 2: 定位 API 收敛为一个 setPosition(QWidget*, QPoint)

选择：

- Popup 对外只保留一个位置接口：setPosition(QWidget* relativeTo, const QPoint& localPos)
- relativeTo 为任意 widget，localPos 为该 widget 局部坐标系中的点
- 传入 top-level widget 本身时，可表达顶层窗口内的固定位置
- 若未设置位置，则 open() 默认居中

为什么：

- Popup 保持基类角色，不承担 placement 系统的复杂性
- API 心智模型和 QML Popup 的 x / y 更接近
- 更高层组件如果需要 target 对齐、箭头或自动翻转，应在派生层先算好局部坐标，再调用 Popup::setPosition()

补充约束：

- setPosition 的语义针对可见卡片左上角
- Popup 内部有 shadow margin，因此 open() 会在 move() 前自动减去 margin 补偿

### Decision 3: 内容依赖型定位交给调用侧处理

选择：

- Popup 本身不提供“锚点对齐”或“根据最终高度反推坐标”的 public API
- 当调用侧的定位依赖 popup 最终内容尺寸时，应先激活 layout、必要时 resize 到 totalSizeHint()，再调用 setPosition()

为什么：

- 这是高层布局策略，而不是 Popup 基类职责
- 避免为单一示例场景向 Popup 公共接口注入额外概念
- VisualCheck 里的 Relative Pos 示例已经采用这种方式验证可行性

### Decision 4: 动画改为 opacity-only

选择：

- popupProgress 仍作为单一动画属性保留
- 动画通过 QGraphicsOpacityEffect 统一作用到背景和子控件
- 去掉早期草案中的 scale 动画

为什么：

- QWidget 子控件是各自独立绘制的，单纯在 paintEvent 中做 QPainter scale 无法真实缩放子控件
- 试验结果表明 scale 会导致背景和子控件错位，视觉不稳定
- opacity-only 更简单，也与当前 Dialog 的修正方向一致

### Decision 5: modal + dim 使用内部 PopupScrim

选择：

- 在 Popup.cpp 中定义内部 PopupScrim
- 当 modal 为 true 时在 top-level widget 上创建 scrim，拦截背景鼠标事件
- 当 dim 为 true 时，scrim 同时绘制 smoke 填充

为什么：

- 这个方案和 Dialog 的 smoke overlay 思路一致
- 能清晰地区分：
    - modal 但不 dim：拦截交互，不加遮罩视觉
    - modal 且 dim：拦截交互并加遮罩视觉

### Decision 6: 视觉 token 直接对齐现有 FluentElement 能力

选择：

- 圆角使用 themeRadius().overlay
- 阴影使用 themeShadow(Elevation::High)
- 背景使用 themeColors().bgLayer
- 描边使用 themeColors().strokeDefault
- 不使用 QSS 或 QPalette

为什么：

- 这和项目现有自绘组件风格保持一致
- 后续主题切换只需要 onThemeUpdated() -> update()

### Decision 7: 测试采用单文件、单 target、单个 VisualCheck 窗口

选择：

- TestPopup.cpp 包含所有行为测试
- 一个统一的 VisualCheck 窗口展示六种场景：
    - 固定位置 Info Popup
    - 默认居中 Center Popup
    - Modal + Dim
    - Notification fixed position
    - Sticky NoAutoClose
    - Relative Position 示例

为什么：

- 贴合仓库现有 dialogs_flyouts 测试组织方式
- 相比拆成多个 VisualCheck 用例，一个展示窗口更利于手工回归

## Risks / Trade-offs

- Popup 不内建 placement 系统，后续 Flyout 需要在自身实现 target 对齐和翻转策略
- 若业务定位依赖 popup 最终尺寸，调用侧必须先激活 layout 再计算坐标
- 同窗口 overlay 无法飞出 top-level widget 边界；如果以后确实需要跨窗口或跨屏 popup，需要另起新方案
- opacity-only 动画比 scale+opacity 更保守，但稳定性更高，且不会造成子控件错位
