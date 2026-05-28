## Context

`src/view/collections/` 目前包含 `ListView`、`GridView`、`TreeView` 和 `FlipView`。其中 `FlipView` 已经提供单页内容展示、页面管理、导航按钮、页面指示器和滑动动画，但它表达的是线性翻页视图，不是导航栈。Qt Widgets 自带 `QStackedWidget` 可以保存多个 page 并切换当前 index，但它缺少 QML `StackView` 的 push/pop/replace 语义、深度、busy 状态、页面生命周期状态和方向感明确的转场动画。

本变更新增 `StackView`，用于向导流程、设置详情页、列表到详情钻取、子页面返回等常见 Fluent 应用场景。组件应保持项目惯例：公开控件继承 Qt Widgets 的 `QStackedWidget`、`FluentElement` 和 `view::QMLPlus`，视觉与动画通过自绘/Qt 动画实现，不使用 QSS/QPalette 做 Fluent 样式核心。

## Goals / Non-Goals

**Goals:**

- 新增 `view::collections::StackView`，从 Qt Widgets 的 `QStackedWidget` 派生，并提供 QML `StackView` 风格的页面栈 API。
- 保留 `QStackedWidget` 的 widget 容器能力，同时在上层维护显式 stack entries，支持 push/pop/replace/clear 和 depth/currentItem/initialItem/busy 属性。
- 提供 Fluent 默认转场：push 时新页从前进方向滑入，pop 时旧页向前进方向退出并露出上一页，replace 使用同方向切换；同时支持禁用动画和配置 duration/easing。
- 在 QWidget 中提供可测试的页面状态：Inactive、Activating、Active、Deactivating，并在转场前后发出信号。
- 明确页面所有权和移除策略，避免 QStackedWidget 中 widget 仍存在但栈语义已移除导致的泄漏或悬空引用。
- 提供自动化测试和 VisualCheck，覆盖 API、转场、信号、生命周期、键盘返回和主题刷新。

**Non-Goals:**

- 不移植 QML `StackView` 的 component/url 加载机制；首版只接收已创建的 `QWidget*`。
- 不实现 QML attached property 语法；Qt Widgets 版本提供 `itemStatus(QWidget*)`、`itemIndex(QWidget*)` 和状态信号作为等价能力。
- 不内置 NavigationView、BreadcrumbBar 或返回按钮 UI；StackView 只提供栈容器和返回行为，调用方决定外部 chrome。
- 不替换现有 `FlipView`；两者服务不同场景，FlipView 仍用于同级页面轮播/翻页。
- 不引入新的第三方动画或状态机依赖。

## Decisions

### 1. 继承 QStackedWidget，同时维护独立栈模型

`StackView` 继承 `QStackedWidget`，因此业务可以继续把它当作标准 QWidget 容器嵌入布局，也能复用 `addWidget()`/`insertWidget()` 的基础页面管理能力。与此同时，QML `StackView` 语义不能仅靠 `currentIndex` 表达，所以内部维护 `QVector<StackEntry>`：每个 entry 保存 `QPointer<QWidget> item`、页面状态、是否由 StackView 拥有、转场期间的临时 geometry 和可选名称。

公共 API 形状：

- `push(QWidget* item)` / `push(const QVector<QWidget*>& items)`
- `pop()` / `pop(QWidget* item)` / `popToRoot()` / `popToItem(QWidget* item)`
- `replace(QWidget* item)` / `replace(int index, QWidget* item)`
- `clear()`
- `depth()`、`currentItem()`、`initialItem()`、`canPop()`、`busy()`
- `itemAt(int index)`、`indexOf(QWidget*)`、`contains(QWidget*)`、`itemStatus(QWidget*)`
- 信号：`depthChanged(int)`、`currentItemChanged(QWidget*)`、`busyChanged(bool)`、`itemPushed(QWidget*)`、`itemPopped(QWidget*)`、`itemReplaced(QWidget*, QWidget*)`、`itemStatusChanged(QWidget*, StackViewItemStatus)`、`transitionStarted(...)`、`transitionFinished(...)`

替代方案是从 `QWidget` 重新实现容器，但那会放弃 `QStackedWidget` 的成熟 index/widget 管理，也偏离用户语境中“从 Qt 栈页面容器派生”的要求。

### 2. 只让栈操作改变 StackView 栈语义，原始 QStackedWidget API 保持兼容但不作为主入口

继承 `QStackedWidget` 意味着调用方理论上可直接 `addWidget()` 或 `setCurrentIndex()`。为了避免两套状态互相打架，首版策略是：

- 推荐并测试 `push/pop/replace/clear` 作为主入口。
- 覆盖/包装 `addWidget()` 不可行，因为 Qt 方法非 virtual；因此文档和测试不把原始 addWidget 作为栈语义入口。
- 对构造时或外部已加入的 widget，提供 `adoptWidget(QWidget*)` 或在 `setInitialItem()` 中将其纳入栈模型。
- `setCurrentIndex()` 若由外部调用，应同步 current item 并更新状态，但不伪造 push/pop 历史。

这样可以保持 Qt 兼容性，同时让 Fluent StackView 的行为合同集中在新增 API 上。

### 3. 转场使用临时 geometry 动画，不依赖 QStackedWidget 内部绘制

`QStackedWidget` 默认只显示当前页；为了做 push/pop 的双页面转场，需要在动画期间同时显示 from/to 两个 QWidget。实现时让两个 page 都设为 visible，并用 `QParallelAnimationGroup` 动画它们的 `pos` 与 `windowOpacity` 或内部 `QGraphicsOpacityEffect`。动画完成后再调用 `setCurrentWidget(to)`，隐藏 inactive 页面并恢复 geometry。

默认转场：

- Push：当前页向后退少量或淡出，新页从右侧/下侧滑入。
- Pop：旧页向右侧/下侧滑出，目标页从轻微反向偏移回到原位。
- Replace：按 push 方向切换，但旧页转为 Deactivating 后从栈移除。

方向由 `orientation` 控制，水平默认从右侧进入，垂直默认从下侧进入。动画时长默认使用 `themeAnimation().normal`，曲线使用 `themeAnimation().decelerate`；`transitionAnimationEnabled=false` 或 duration 为 0 时同步完成。

### 4. 页面生命周期状态是 StackView 的核心行为，不做纯视觉附属

QML `StackView` 的重要能力之一是页面知道自己正在进入或退出。Qt Widgets 没有 attached property，因此 `StackView` 提供 `StackViewItemStatus` 枚举：`Inactive`、`Activating`、`Active`、`Deactivating`。状态变化通过 `itemStatusChanged(QWidget*, status)` 发出，并可通过 `itemStatus(item)` 查询。

状态规则：

- push/replace 目标页进入 `Activating`，完成后变为 `Active`。
- 被覆盖或移除的当前页进入 `Deactivating`，完成后变为 `Inactive` 或从栈移除。
- 栈内非当前页保持 `Inactive` 且隐藏。
- 无动画时仍按同样顺序发出状态变化，保证业务逻辑可测试。

### 5. busy 期间串行化栈操作，避免页面状态错乱

转场期间 `busy=true`。默认首版不并行执行多次 push/pop/replace，后续操作会被拒绝并返回 false，或可选地在后续版本添加队列。选择“拒绝而不是排队”的原因是 Qt Widgets 页面所有权和销毁时机更容易被业务侧影响，显式失败更安全、更可测试。

键盘返回只在 `!busy && canPop()` 时生效；默认支持 Backspace、Alt+Left 和 macOS 的 Back/Forward 键等 Qt key code 中可用的返回输入。

### 6. 页面所有权采用明确策略

默认 `push(QWidget*)` 会将页面 parent 设置为 StackView，StackView 在 clear/析构时销毁仍在栈中的页面。对调用方仍需持有的外部页面，提供 `setItemOwnership(StackViewItemOwnership)` 或 push 重载参数，允许 `StackViewDoesNotOwnItem`。被 pop 的 owned 页面在转场结束后 `deleteLater()`；非 owned 页面会 `hide()`、`setParent(nullptr)` 或保留 parent 策略按 API 明确执行。

首版建议采用简单枚举：`OwnsItems` / `DoesNotOwnItems` 作为全局默认，再允许单次 push 覆盖。

## Risks / Trade-offs

- [Risk] `QStackedWidget` 原生 API 与新增栈 API 并存，外部直接调用 `removeWidget()` 可能绕过 StackView 状态。→ Mitigation: 明确主入口为 StackView API；在 `childEvent`/`event` 或关键路径中检测 widget 移除并重建栈模型；测试覆盖外部 remove 的安全降级。
- [Risk] 转场期间同时显示两个真实 QWidget，复杂子控件可能触发额外布局或 repaint 成本。→ Mitigation: 只动画 geometry/opacity，避免截图缓存；默认只保留两个可见页面，inactive 页面隐藏。
- [Risk] owned 页面 pop 后销毁时机可能影响业务持有的裸指针。→ Mitigation: 提供所有权策略和信号，在 `itemPopped` 后再 `deleteLater()`，并在文档/测试中明确生命周期。
- [Risk] QML StackView 的 attached properties 无法在 QWidget 中完全等价。→ Mitigation: 用查询 API 和信号表达同样信息，不模拟 QML 语法。
- [Risk] busy 时拒绝操作可能和 QML 的部分使用习惯不同。→ Mitigation: 首版行为简单且可预测；后续可在不破坏 API 的情况下增加可选 operation queue。

## Migration Plan

1. 新增 `StackView.h/.cpp`，通过现有源码 glob 纳入 `fluent_qt_lib`。
2. 新增 `TestStackView.cpp` 并在 `tests/views/collections/CMakeLists.txt` 注册 `test_stack_view`。
3. 先实现无动画同步路径和栈状态，再接入 transition 动画。
4. 增加 VisualCheck，展示 push/pop/replace、水平/垂直转场、返回键和 Light/Dark 主题。
5. 增量验证 `test_stack_view`；如改动公共 collection 头文件或共享行为，再运行相关 collection 测试。

## Open Questions

- 是否需要首版就支持 operation queue，还是保持 busy 时返回 false；当前设计倾向后者。
- 是否需要单页级别 transition 自定义回调，还是先用全局 transition 配置；当前设计倾向先提供全局配置。