## 上下文

当前 textfields 模块提供 `LineEdit`、`TextEdit` 和 `TextBlock`。其中 `LineEdit` 已经包含 Fluent 输入框的核心视觉能力：自绘控件背景/边框、聚焦时底部强调色线、内容边距、字体 Token 应用、主题处理，以及一个内部清除按钮。

本次要新增的是 WinUI 3 风格的 `AutoSuggestBox`。Windows UI Kit 的 Figma 节点显示：Auto Suggest Box 输入框高度为 32px，控件圆角为 4px，文本使用 14px Segoe UI Variable，placeholder 使用次级文本透明度；Focused typing 状态使用 1px 顶部/侧边描边和 2px accent 底部描边。结合当前库的 `Button::Small` 视觉，右侧查询/清除按钮最终收敛为 24x24，并在 32px 输入框内垂直居中。Flyout 变体会把 160px 宽的浮层直接放在输入框下方。

WinUI Gallery 确立了事件语义：`TextChanged` 处理器会检查 `args.Reason == AutoSuggestionBoxTextChangeReason.UserInput`；`QuerySubmitted` 会在按 Enter、点击查询按钮、选择建议时触发；`SuggestionChosen` 会在用户高亮或选择建议时触发，并暴露当前建议项。

## 目标 / 非目标

**目标：**

- 新增 `AutoSuggestBox` 作为 textfields 组件，并继承自 `LineEdit`。
- 保持现有 Fluent 组件架构和 Design Token 使用方式。
- 暴露一组 Qt 友好的小型 API，对齐 WinUI 核心行为：suggestions、文本变更原因、建议选择、查询提交。
- 布局尽量贴近 Windows UI Kit，便于 VisualCheck 验证：32px 输入框高度、24x24 右侧按钮、4px 输入框圆角、Focused accent 底部描边、与输入框等宽对齐的 Flyout。
- 测试聚焦在新组件本身，并注册独立 CMake 测试目标。

**非目标：**

- 第一版不引入完整的 model/delegate 自定义框架。
- 不实现异步建议提供器；应用侧通过 `textChangedWithReason` 响应并更新 `suggestions`。
- 不替换 `LineEdit` 公共 API，也不改变现有 textfield 行为。
- 除 `LineEdit` 已有的文本渲染样式外，不新增依赖 QSS/QPalette 的 Fluent 视觉实现。

## 设计决策

### 继承 `LineEdit`

`AutoSuggestBox` 直接继承 `LineEdit`，而不是重新实现一个裸 `QLineEdit`。这样可以复用字体、选区颜色、placeholder、输入行为和当前 textfields 模块的约定。

备选方案：在一个 wrapper widget 内组合一个 `LineEdit` 子控件。这个方案会让焦点处理、sizeHint、文本 API 转发和信号兼容更复杂。直接继承更简单，也更贴合现有 textfields 模块。

### 禁用 `LineEdit` 内置清除按钮，提供 AutoSuggestBox 自己的按钮区

`LineEdit` 的清除按钮是私有成员，并且尺寸/布局只适合普通文本框。`AutoSuggestBox` 需要按 Figma 精确放置查询按钮和清除按钮。因此组件会调用 `setClearButtonEnabled(false)`，并管理两个 `view::basicinput::Button` 子控件：

- Search button：默认可见，查询 glyph 默认使用 Fluent 搜索图标，几何尺寸 24x24，距离右边缘 4px，并在输入框内垂直居中。
- Clear button：仅文本非空时可见，几何尺寸 24x24，紧贴 Search button 左侧。

右侧内容边距通过公开的 `setContentMargins()` 更新，让 `LineEdit::applyThemeStyle()` 重新计算文本 padding，不暴露或修改 `LineEdit` 私有实现。

### 使用 WinUI 风格的文本变更原因

`AutoSuggestBox` 定义 `TextChangeReason { UserInput, ProgrammaticChange, SuggestionChosen }`。内部更新文本前会设置下一次变更原因，然后调用 `setText()`，最终在 `QLineEdit::textChanged` 连接中发射 `textChangedWithReason(text, reason)`。

这与 WinUI Gallery 的建议一致：业务代码只在原因是用户输入时刷新建议，避免因为高亮建议或选择建议导致文本变化时再次触发建议刷新循环。

### 建议列表使用 Flyout 派生内部类

建议列表遵循现有 Popup/Flyout 体系：使用轻量内部 `Flyout` 派生类，复用同窗口 overlay、light-dismiss、阴影边距和主题绘制能力，并由 `QStringListModel` 驱动 `QListView`。它应对齐到 `AutoSuggestBox` 底边，并与输入框等宽。

备选方案：手写 `Dialog` / native popup。该方案会把对话框语义带入建议列表，并需要重复处理定位、light-dismiss 和阴影边距；直接复用 `Flyout` 更简单，也与 ComboBox 迁移后的结构一致。

### 公共 suggestions API 保持简单

第一版暴露 `QStringList suggestions`，选中建议以 `QVariant` 发出。这足以覆盖测试和常见搜索框场景，同时避免过早公开 item model 的生命周期和 delegate 规则。

备选方案：从第一版就暴露 `QAbstractItemModel*`。它更灵活，但会扩大 API 面、delegate 职责和所有权规则。如果后续确有需求，可以在此基础上扩展 model 版本。

### Header 属于 AutoSuggestBox 自身

WinUI 文本输入支持可选 Header。`AutoSuggestBox` 仅暴露 `header` 文本作为单一事实来源；当 `header` 非空时，控件在 32px 输入框上方预留 Header 区域，并用 Fluent 字体和文本颜色 Token 绘制标签。`header` 为空时不预留 Header 空间。

## 风险 / 取舍

- Qt popup 窗口的焦点变化可能影响 Flyout 生命周期 -> 在查询提交、Escape、清空 suggestions、用户输入为空时显式关闭 Flyout，同时依赖 `Qt::Popup` 处理外部点击关闭。
- 键盘导航可能误触发用户输入的建议刷新 -> 使用 `TextChangeReason` 状态保护 programmatic 和 suggestion-driven 的 `setText()`。
- `LineEdit::applyThemeStyle()` 是私有函数 -> 使用现有公开 setter（`setClearButtonEnabled`、`setContentMargins`、`setFrameVisible`），不改动 `LineEdit` 内部实现。
- QWidget 绘制无法直接获得系统 acrylic blur -> 复用现有 Fluent 主题颜色/材质 Token 和 Flyout 阴影约定；规格聚焦在 Token 驱动的 Fluent 外观，不要求 OS compositor 级模糊。
- 仅支持 `QStringList` suggestions 可能对复杂业务数据不够 -> 选中值使用 `QVariant` 发出，并让内部结构保留后续扩展 model-backed 版本的空间。

## 迁移计划

- 新增组件，不改变现有 `LineEdit`、`TextEdit` 或 `TextBlock` 行为。
- 只注册新的 `test_auto_suggest_box` 测试目标。
- 回滚范围隔离：如不接受该组件，删除新增文件和 CMake 目标即可。

## 待确认问题

- 后续版本是否需要支持自定义 item delegate 或 `QAbstractItemModel` suggestions？
- 当建议数量超过最大可见高度时，Flyout 是否要使用项目内 Fluent `ScrollBar` 覆盖原生滚动条？
