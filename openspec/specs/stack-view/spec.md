# StackView Specification

## Purpose

规范 collections 模块中的 Fluent `StackView` 控件：以 QML StackView 风格管理 QWidget 页面导航栈，支持 push/pop/replace/clear、页面生命周期状态、Fluent 转场动画、所有权策略、返回输入，以及对应的主题与测试要求。

## Requirements

### Requirement: StackView 公共组件
系统 SHALL 在 `src/view/collections/` 提供 `StackView` 公共组件，用于以 Fluent 风格管理 QWidget 页面导航栈。组件 MUST 从 Qt Widgets 的 `QStackedWidget` 派生，并同时继承 `FluentElement` 和 `view::QMLPlus`。

#### Scenario: 默认构造
- **WHEN** 创建一个 `StackView`
- **THEN** `depth()` MUST 为 0、`currentItem()` MUST 为 `nullptr`、`initialItem()` MUST 为 `nullptr`、`canPop()` MUST 为 false、`busy()` MUST 为 false，并且组件 MUST 可作为普通 QWidget 放入布局

#### Scenario: 组件符合项目继承模式
- **WHEN** 编译包含 `src/view/collections/StackView.h` 的测试或业务代码
- **THEN** `StackView` MUST 具备 `QStackedWidget` 页面容器能力、`FluentElement` 主题能力和 `view::QMLPlus` anchors/binding 能力

#### Scenario: 设置 initial item
- **WHEN** 调用 `setInitialItem(widget)`
- **THEN** StackView MUST 将该 widget 作为栈底页面，`depth()` MUST 为 1、`currentItem()` MUST 等于该 widget、`initialItem()` MUST 等于该 widget，且 widget 状态 MUST 为 `Active`

### Requirement: 页面栈操作
系统 SHALL 提供 QML StackView 风格的 push、pop、replace 和 clear 操作，并维护稳定的深度和当前页面状态。

#### Scenario: Push 页面
- **WHEN** 对空 StackView 调用 `push(page)`
- **THEN** 操作 MUST 成功，`depth()` MUST 增加到 1，`currentItem()` MUST 为 page，并发出 `itemPushed(page)`、`depthChanged(1)` 和 `currentItemChanged(page)`

#### Scenario: 连续 Push 页面
- **WHEN** StackView 已有一个当前页并调用 `push(nextPage)`
- **THEN** StackView MUST 将 nextPage 放到栈顶，旧当前页 MUST 保留在栈内但变为 inactive，`canPop()` MUST 为 true

#### Scenario: Pop 页面
- **WHEN** StackView 深度大于 1 且调用 `pop()`
- **THEN** StackView MUST 移除当前栈顶页面，恢复上一页为 current item，`depth()` MUST 减 1，并发出 `itemPopped(oldTop)` 和 `currentItemChanged(previousPage)`

#### Scenario: Pop 栈底页面被拒绝
- **WHEN** StackView 深度小于等于 1 且调用 `pop()`
- **THEN** 操作 MUST 返回失败，栈内容 MUST 不变，`canPop()` MUST 为 false

#### Scenario: Replace 当前页面
- **WHEN** StackView 已有当前页面并调用 `replace(newPage)`
- **THEN** StackView MUST 用 newPage 替换当前栈顶，`depth()` MUST 保持不变，并发出 `itemReplaced(oldPage, newPage)` 和 `currentItemChanged(newPage)`

#### Scenario: Clear 栈
- **WHEN** 调用 `clear()`
- **THEN** StackView MUST 移除所有栈内页面，`depth()` MUST 为 0、`currentItem()` MUST 为 `nullptr`、`initialItem()` MUST 为 `nullptr`，并发出 depth/current 变化信号

#### Scenario: Pop 到指定页面
- **WHEN** 栈中包含目标 page 且调用 `popToItem(page)`
- **THEN** StackView MUST 移除目标 page 之上的所有页面，并将 page 设为 current item

#### Scenario: Pop 到根页面
- **WHEN** StackView 深度大于 1 且调用 `popToRoot()`
- **THEN** StackView MUST 只保留 initial item，并将 initial item 设为 current item

### Requirement: 页面状态与生命周期
系统 SHALL 为栈内页面维护可查询的生命周期状态，并在转场前后发出状态变化信号。

#### Scenario: Push 状态变化
- **WHEN** 调用 `push(nextPage)` 并开始转场
- **THEN** nextPage 状态 MUST 变为 `Activating`，旧当前页状态 MUST 变为 `Deactivating`
- **WHEN** 转场完成
- **THEN** nextPage 状态 MUST 变为 `Active`，旧当前页状态 MUST 变为 `Inactive`

#### Scenario: Pop 状态变化
- **WHEN** 调用 `pop()` 并开始转场
- **THEN** 被移除页面状态 MUST 变为 `Deactivating`，目标上一页状态 MUST 变为 `Activating`
- **WHEN** 转场完成
- **THEN** 目标上一页状态 MUST 变为 `Active`，被移除页面 MUST 从栈模型中移除

#### Scenario: 无动画仍发出状态变化
- **WHEN** `transitionAnimationEnabled` 为 false 且执行 push/pop/replace
- **THEN** StackView MUST 同步完成操作，并仍按 Activating/Active、Deactivating/Inactive 顺序发出状态变化

#### Scenario: 查询非栈内页面状态
- **WHEN** 调用 `itemStatus(widget)` 且 widget 不属于 StackView 栈
- **THEN** 方法 MUST 返回 `Inactive` 或等价的非活动状态，且 MUST 不改变栈内容

### Requirement: Fluent 转场动画
系统 SHALL 提供可配置的 Fluent 页面转场，用于 push、pop 和 replace，并在转场期间报告 busy 状态。

#### Scenario: Push 转场
- **WHEN** StackView 水平方向执行 push 且动画启用
- **THEN** 新页面 MUST 从前进方向滑入并淡入，旧页面 MUST 淡出或轻微滑出，转场期间 `busy()` MUST 为 true

#### Scenario: Pop 转场
- **WHEN** StackView 水平方向执行 pop 且动画启用
- **THEN** 旧页面 MUST 向前进方向滑出，目标页面 MUST 从反向偏移回到原位，转场期间 `busy()` MUST 为 true

#### Scenario: 垂直方向转场
- **WHEN** `orientation` 为 `Qt::Vertical` 且执行 push 或 pop
- **THEN** StackView MUST 使用垂直方向的滑入/滑出几何，而不是水平几何

#### Scenario: 动画完成
- **WHEN** 转场动画完成
- **THEN** `busy()` MUST 变为 false，只有 current item MUST 保持可见和 active，inactive 页面 MUST 被隐藏

#### Scenario: Busy 时拒绝新操作
- **WHEN** `busy()` 为 true 且调用 push/pop/replace/clear
- **THEN** 操作 MUST 返回失败或保持栈内容不变，并 MUST 不启动第二个并发转场

#### Scenario: 配置动画参数
- **WHEN** 调用方设置 `transitionAnimationEnabled`、`transitionDuration` 或 `orientation`
- **THEN** 后续转场 MUST 使用新的配置，并发出对应 changed 信号

### Requirement: 页面所有权
系统 SHALL 明确 StackView 对页面 QWidget 的 parent 和销毁策略，确保 pop、replace、clear 后没有悬空栈项。

#### Scenario: 默认接管页面 parent
- **WHEN** 调用 `push(page)` 且 page 没有 parent 或 parent 不是 StackView
- **THEN** StackView MUST 将 page 的 parent 设置为自身，并将其纳入 QStackedWidget 管理

#### Scenario: Owned 页面被 Pop
- **WHEN** 默认 owned 页面被 `pop()` 移除
- **THEN** StackView MUST 在转场完成后从 QStackedWidget 移除该页面，并按所有权策略销毁或调度销毁该页面

#### Scenario: Non-owned 页面被 Pop
- **WHEN** 调用方以 non-owned 策略 push 页面并随后 pop
- **THEN** StackView MUST 从栈模型和 QStackedWidget 中移除该页面，但 MUST 不销毁该页面

#### Scenario: 外部页面销毁
- **WHEN** 栈内 QWidget 在外部被销毁
- **THEN** StackView MUST 移除对应栈项，更新 depth/current item，并且 MUST 不访问悬空指针

### Requirement: 输入与返回导航
系统 SHALL 提供返回导航辅助 API 和键盘输入行为，使业务可以将 StackView 用于设置页、详情页和向导流程。

#### Scenario: goBack 返回上一页
- **WHEN** `canPop()` 为 true 且调用 `goBack()`
- **THEN** StackView MUST 执行与 `pop()` 等价的返回操作

#### Scenario: goBack 无上一页
- **WHEN** `canPop()` 为 false 且调用 `goBack()`
- **THEN** 操作 MUST 返回失败，栈内容 MUST 不变

#### Scenario: 键盘返回
- **WHEN** StackView 拥有焦点、`canPop()` 为 true 且用户按下 Backspace 或平台可用的返回键组合
- **THEN** StackView MUST 执行返回操作

#### Scenario: Busy 时忽略键盘返回
- **WHEN** `busy()` 为 true 且用户触发返回输入
- **THEN** StackView MUST 忽略输入，当前转场 MUST 不被中断

### Requirement: 主题与布局
系统 SHALL 在主题和尺寸变化时保持页面布局、裁剪和转场视觉正确。

#### Scenario: 主题切换
- **WHEN** Fluent 主题在 Light 和 Dark 之间切换
- **THEN** StackView MUST 在 `onThemeUpdated()` 中刷新动画参数或视觉资源，并请求必要重绘，但 MUST 不改变栈内容

#### Scenario: 尺寸变化
- **WHEN** StackView resize
- **THEN** current item 和转场相关页面 MUST 重新布局到新的 content rect，inactive 页面 MUST 保持隐藏

#### Scenario: size hint
- **WHEN** StackView 包含页面
- **THEN** `sizeHint()` MUST 基于 QStackedWidget/当前页面合理返回非空尺寸，并可被布局系统使用

### Requirement: 测试与 VisualCheck
系统 SHALL 为 StackView 提供自动化测试和 VisualCheck，覆盖公共 API、页面生命周期、转场、输入和主题行为。

#### Scenario: 测试目标注册
- **WHEN** 配置 CMake 后构建测试
- **THEN** `tests/views/collections/CMakeLists.txt` MUST 注册 `test_stack_view`，并且目标 MUST 能链接 `fluent_qt_lib`

#### Scenario: 自动化测试
- **WHEN** 使用 `SKIP_VISUAL_TEST=1` 运行 `test_stack_view`
- **THEN** 非 VisualCheck 测试 MUST 覆盖默认属性、push/pop/replace/clear、popToRoot/popToItem、busy 拒绝、状态信号、所有权策略、键盘返回、禁用动画和主题刷新

#### Scenario: VisualCheck
- **WHEN** 手动运行 `test_stack_view --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 Fluent StackView 的 push、pop、replace、水平/垂直转场、返回导航和 Light/Dark 主题切换

#### Scenario: VisualCheck 自动化守卫
- **WHEN** `SKIP_VISUAL_TEST=1` 被设置
- **THEN** VisualCheck MUST skip 且 MUST 不打开交互式窗口
