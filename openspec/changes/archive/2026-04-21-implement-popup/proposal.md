## Why

WinUI 3 和 Qt Quick Controls 都把 Popup 视为 Dialog 之外的另一类基础原语：

- 不开启独立的模态事件循环
- 不返回 result
- 作为同窗口内的 overlay 浮在内容之上
- 可以 light-dismiss
- 可以由更高层的 Flyout、TeachingTip、DropDown 等组件复用

本仓库此前只有 Dialog 和 ContentDialog。两者都建立在 QDialog 之上，更接近模态对话框，而不是轻量浮层。缺少 Popup 基类会直接带来两个问题：

1. 后续 Flyout / MenuFlyout / TeachingTip 缺少统一的弹层基座
2. 业务侧若需要非模态浮层，只能临时拼 QWidget 或误用 QDialog

本次实现最终收敛到一个比最初提案更简单的方向：Popup 只提供 QML 风格的相对坐标定位能力，不内建复杂 placement 枚举。更高层的对齐、避让和箭头策略留给未来的 Flyout 类派生实现。

## What Changes

- 新增 view::dialogs_flyouts::Popup，继承 QWidget、FluentElement、view::QMLPlus
- Popup 在 open() 时挂载到原始 parent 的 top-level widget 上，不创建新的 OS 顶层窗口
- 提供最小但完整的浮层 API：
  - isOpen / setIsOpen / open / close
  - setPosition(QWidget* relativeTo, const QPoint& localPos)
  - closePolicy
  - modal / dim
  - animationEnabled / popupProgress
- 定位语义保持简单：
  - localPos 表示可见卡片左上角相对于 relativeTo 的局部坐标
  - 若 relativeTo 本身就是 top-level widget，则等价于顶层窗口内的绝对坐标
  - 若未设置位置，则默认在 top-level widget 中居中
- 视觉采用 Fluent token：overlay 圆角、阴影、背景和描边
- 动画采用 opacity-only 方案；原始的 scale 方案在实现过程中被移除，因为会导致背景与子控件渲染错位
- 支持可选 modal + dim scrim
- 增加完整单测和一个统一的 VisualCheck 展示窗口

## Capabilities

### New Capabilities
- popup-overlay: 提供 Fluent 风格的非模态 overlay popup 基类，具备相对定位、light-dismiss、可选 scrim 和基础动画能力，可作为未来 Flyout 体系的底座

### Modified Capabilities
- 无

## Impact

- 新增源码：
  - src/view/dialogs_flyouts/Popup.h
  - src/view/dialogs_flyouts/Popup.cpp
- 新增测试：
  - tests/views/dialogs_flyouts/TestPopup.cpp
  - tests/views/dialogs_flyouts/CMakeLists.txt 中的 test_popup 注册
- 更新文档：
  - readme.md 中 dialogs_flyouts 章节补充 Popup
  - .github/copilot-instructions.md 中组件目录补充 Popup
- 不修改现有 Dialog / ContentDialog 的 public API
- 为未来的 Flyout / TeachingTip / DropDownButton 弹层实现提供统一基类，但本 change 不负责实现这些派生组件
