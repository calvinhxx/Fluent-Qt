# AutoSuggestBox Specification

## Purpose

规范 textfields 模块中的 Fluent `AutoSuggestBox` 控件：它继承自 `LineEdit`，提供 suggestions、query 提交、建议选择、文本变更原因、可选 header、右侧 query/clear 按钮、suggestion Flyout 生命周期，以及对应的主题与测试要求。

## Requirements

### Requirement: AutoSuggestBox 公共 API
系统 SHALL 在 textfields 模块中提供 `AutoSuggestBox` 控件。该控件继承自 `LineEdit`，并通过 Qt 属性和信号暴露 suggestions、header、query icon 和 suggestion flyout 状态。

#### Scenario: 默认构造 AutoSuggestBox
- **WHEN** 应用构造 `AutoSuggestBox`
- **THEN** 控件 MUST 表现为一个 `LineEdit`，suggestion list 为空，query icon 默认可见，header 默认为空，并且 suggestion list 状态为关闭

#### Scenario: 配置公共属性
- **WHEN** 应用设置 suggestions、header 文本、query icon glyph 或 query icon 可见性
- **THEN** 控件 MUST 保存新值，并且仅在存储值发生变化时发射对应 change signal

### Requirement: 文本变更原因
系统 SHALL 区分用户编辑、程序化文本变更和由建议项驱动的文本变更。

#### Scenario: 用户输入原因
- **WHEN** 用户通过键盘编辑文本
- **THEN** 控件 MUST 发射 `textChangedWithReason(text, UserInput)`

#### Scenario: 程序化变更原因
- **WHEN** 控件在浏览建议项时内部修改文本，但尚未提交某个建议项
- **THEN** 控件 MUST 发射 `textChangedWithReason(text, ProgrammaticChange)`

#### Scenario: 建议选择原因
- **WHEN** 用户通过点击或键盘确认选择某个建议项
- **THEN** 控件 MUST 发射 `textChangedWithReason(text, SuggestionChosen)`，而不是 `UserInput`

### Requirement: Suggestions API 与 Flyout 生命周期
当当前用户输入存在可用建议时，系统 SHALL 在与 `AutoSuggestBox` 对齐的 Flyout 中显示建议。

#### Scenario: 建议可用
- **WHEN** 控件拥有焦点、收到用户输入，并且 suggestion list 非空
- **THEN** 控件 MUST 打开 suggestion flyout，flyout 宽度与输入框宽度一致，且顶部边缘与输入框底部边缘对齐

#### Scenario: 清空建议
- **WHEN** 调用 `clearSuggestions()` 或 `setSuggestions({})`
- **THEN** 控件 MUST 保存为空 suggestion list，并关闭 suggestion flyout

#### Scenario: 用户文本为空
- **WHEN** 用户输入导致文本变为空字符串
- **THEN** 控件 MUST 隐藏 clear button，并关闭 suggestion flyout

### Requirement: 查询提交
当用户通过支持的交互提交当前查询时，系统 SHALL 发射 query-submitted 信号。

#### Scenario: 通过 Enter 提交
- **WHEN** `AutoSuggestBox` 拥有焦点，且没有选中建议项时用户按下 Enter
- **THEN** 控件 MUST 发射 `querySubmitted(queryText, QVariant{})` 并关闭 suggestion flyout

#### Scenario: 通过查询按钮提交
- **WHEN** 用户触发 query/search button
- **THEN** 控件 MUST 发射 `querySubmitted(queryText, QVariant{})` 并关闭 suggestion flyout

#### Scenario: 提交选中的建议
- **WHEN** 有效建议项被选中时用户按下 Enter
- **THEN** 控件 MUST 发射 `querySubmitted(queryText, chosenSuggestion)`，其中 `chosenSuggestion` 表示被选中的建议项

### Requirement: 建议选择
系统 SHALL 将建议选择行为与查询提交行为分开暴露。

#### Scenario: 点击建议项
- **WHEN** 用户点击 flyout 中的有效建议项
- **THEN** 控件 MUST 将文本更新为该建议值，发射 `suggestionChosen(chosenSuggestion)`，发射 `querySubmitted(queryText, chosenSuggestion)`，并关闭 flyout

#### Scenario: 键盘高亮建议项
- **WHEN** 用户通过 Down 或 Up 高亮有效建议项
- **THEN** 控件 MUST 发射 `suggestionChosen(selectedSuggestion)`，并使用建议变更原因更新文本

### Requirement: 键盘导航
系统 SHALL 为 suggestion flyout 支持 WinUI 风格键盘导航。

#### Scenario: Down 打开或推进选中项
- **WHEN** suggestions 可用时用户按下 Down
- **THEN** 如果 flyout 关闭，控件 MUST 打开 flyout；如果 flyout 已打开，控件 MUST 推进选中行

#### Scenario: Up 在首项前恢复原始文本
- **WHEN** 用户从第一条建议项按下 Up
- **THEN** 控件 MUST 清除 flyout 选中状态，并恢复用户原始键入文本

#### Scenario: Escape 关闭 flyout
- **WHEN** suggestion flyout 打开时用户按下 Escape
- **THEN** 控件 MUST 关闭 flyout，且不清空文本

### Requirement: 按钮布局与可见性
系统 SHALL 按 Windows UI Kit 布局绘制右侧 query 和 clear 控件。

#### Scenario: Search button 默认状态
- **WHEN** `AutoSuggestBox` 显示且 `queryIconVisible == true`
- **THEN** 控件 MUST 在输入框内部显示 query button，尺寸为 24x24 px，并距离 32 px 高输入框右边缘 4 px 对齐且垂直居中

#### Scenario: Clear button 可见性
- **WHEN** 输入文本非空
- **THEN** 控件 MUST 在 query button 左侧紧贴显示 24x24 px clear button，并为两个按钮预留足够的右侧内容边距

#### Scenario: 隐藏 query icon
- **WHEN** `queryIconVisible` 被设置为 false
- **THEN** 控件 MUST 隐藏 query button，并移除其预留文本边距

### Requirement: 建议弹层保留 clear button 交互
系统 SHALL 在 suggestion popup 打开期间保持 `AutoSuggestBox` 内建 clear/close button 可交互。popup MUST NOT 阻止该按钮接收 hover、press、release 或 click 输入。

#### Scenario: popup 打开时悬停 clear button
- **WHEN** 已启用的 `AutoSuggestBox` 包含非空文本并打开 suggestion popup
- **AND** 指针移动到内建 clear/close button 上方
- **THEN** clear/close button MUST 进入 hover visual state
- **AND** suggestion popup MUST 保持打开，直到发生 dismissing action

#### Scenario: popup 打开时点击 clear button 清空文本
- **WHEN** 已启用的 `AutoSuggestBox` 包含非空文本并打开 suggestion popup
- **AND** 用户点击内建 clear/close button
- **THEN** 控件文本 MUST 变为空字符串
- **AND** clear/close button MUST 隐藏
- **AND** suggestion popup MUST 关闭

#### Scenario: 修复后点击建议项仍然生效
- **WHEN** 已启用的 `AutoSuggestBox` 打开 suggestion popup
- **AND** 用户点击有效建议项
- **THEN** 控件 MUST 选择该建议项
- **AND** MUST 发射既有的 `suggestionChosen(...)` 和 `querySubmitted(...)` 信号
- **AND** suggestion popup MUST 关闭

#### Scenario: 外部按下仍然 light-dismiss 建议
- **WHEN** 已启用的 `AutoSuggestBox` 打开 suggestion popup
- **AND** 用户在 suggestion popup 和所属输入控件之外按下
- **THEN** suggestion popup MUST 关闭

### Requirement: Header 绘制
系统 SHALL 支持输入框上方的可选 header 标签。

#### Scenario: Header 为空
- **WHEN** `header` 为空字符串
- **THEN** 控件 MUST 使用标准 32 px 输入框高度，并且不预留 header 空间

#### Scenario: Header 非空
- **WHEN** `header` 非空
- **THEN** 控件 MUST 使用 Fluent typography 在输入框上方绘制 header 文本，并增大 size hint，使其包含 header 高度、间距和 32 px 输入框

### Requirement: Fluent 视觉适配
系统 SHALL 使用现有 Fluent Design Token 渲染 `AutoSuggestBox`，并在主题变化时更新视觉。

#### Scenario: 聚焦输入视觉
- **WHEN** `AutoSuggestBox` 拥有输入焦点
- **THEN** 输入框 MUST 使用 Token 驱动的控件背景、顶部/侧边描边，以及匹配 Windows UI Kit focused 状态的 2 px accent 底部描边

#### Scenario: 主题更新
- **WHEN** 全局 Fluent 主题发生变化
- **THEN** `AutoSuggestBox`、其按钮和 suggestion flyout MUST 从 Design Token 刷新颜色、字体和描边

### Requirement: 测试与视觉验证
系统 SHALL 为 `AutoSuggestBox` 行为和视觉状态提供聚焦测试。

#### Scenario: 运行单元测试
- **WHEN** 构建 `test_auto_suggest_box` 目标，并在 `SKIP_VISUAL_TEST=1` 下运行
- **THEN** 测试 MUST 在不打开可视化窗口的情况下验证属性、信号、suggestions、query submission、键盘导航和主题更新行为

#### Scenario: 运行 VisualCheck
- **WHEN** 在没有 `SKIP_VISUAL_TEST` 的情况下运行 VisualCheck 测试
- **THEN** 测试 MUST 展示有代表性的 Rest、focused/typing、flyout、with-header 和 disabled `AutoSuggestBox` 状态，供人工检查