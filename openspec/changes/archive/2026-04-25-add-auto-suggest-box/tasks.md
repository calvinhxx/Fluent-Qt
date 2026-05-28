## 1. 公共 API 与文件注册

- [x] 1.1 新建 `src/view/textfields/AutoSuggestBox.h`，在 `view::textfields` 命名空间中声明 `AutoSuggestBox : public LineEdit`
- [x] 1.2 添加 `TextChangeReason { UserInput, ProgrammaticChange, SuggestionChosen }` 并使用 `Q_ENUM` 注册
- [x] 1.3 添加 `Q_PROPERTY`：`suggestions`、`header`、`queryIconGlyph`、`queryIconVisible`，以及只读 `isSuggestionListOpen`
- [x] 1.4 声明 `textChangedWithReason`、`suggestionChosen`、`querySubmitted` 以及所有属性变更通知信号
- [x] 1.5 声明 public slots：`setSuggestions(const QStringList&)` 和 `clearSuggestions()`
- [x] 1.6 声明组件需要的 size hint、主题更新、绘制、resize、key、focus、move、enter、leave 事件重写

## 2. LineEdit 基础集成

- [x] 2.1 实现 `AutoSuggestBox.cpp` 构造函数和析构函数
- [x] 2.2 使用 `setClearButtonEnabled(false)` 禁用 `LineEdit` 内置清除按钮
- [x] 2.3 必要时抑制继承的 frame 绘制，并按 header 感知几何绘制 AutoSuggestBox 输入框
- [x] 2.4 通过 `setContentMargins()` 更新文本内容边距，确保文本不会与右侧按钮重叠
- [x] 2.5 实现默认 32 px 输入框高度，以及支持 header 的 `sizeHint()` / `minimumSizeHint()`

## 3. 右侧按钮区

- [x] 3.1 创建 search/query `view::basicinput::Button` 子控件，使用 Subtle 风格、IconOnly 布局、24x24 px 几何和默认搜索 glyph
- [x] 3.2 创建 clear `view::basicinput::Button` 子控件，使用 Subtle 风格、IconOnly 布局、24x24 px 几何和 clear glyph
- [x] 3.3 将 search button 放在距离输入框右边缘 4 px 处，并让 clear button 紧贴在它左侧
- [x] 3.4 点击 search button 时发射 `querySubmitted(text(), QVariant{})` 并关闭 flyout
- [x] 3.5 点击 clear button 时清空文本、隐藏 clear button、更新边距并关闭 flyout
- [x] 3.6 实现 `queryIconGlyph` 和 `queryIconVisible` setter，并发射信号、更新几何和边距

## 4. 文本变更原因处理

- [x] 4.1 将 `QLineEdit::textChanged` 连接到内部处理器，并发射 `textChangedWithReason(text, reason)`
- [x] 4.2 在内部 `setText()` 调用前记录下一次文本变更原因
- [x] 4.3 在键盘导航改变输入文本前保存用户原始键入文本
- [x] 4.4 确保建议驱动的文本变更发射为 `SuggestionChosen`，而不是 `UserInput`
- [x] 4.5 确保导航预览驱动的文本变更发射为 `ProgrammaticChange`

## 5. Suggestion Flyout

- [x] 5.1 在 `AutoSuggestBox.cpp` 中实现内部 `SuggestionListPopup` 类，遵循现有 Popup/Flyout 派生模式
- [x] 5.2 使用 `QListView` 和 `QStringListModel` 驱动 popup
- [x] 5.3 实现 `QStyledItemDelegate`，包含 40 px item 高度、Fluent Body 字体、文本内边距、hover/selected 背景和 4 px item 圆角
- [x] 5.4 使用 Token 驱动背景、描边、8 px overlay radius、透明窗口清理和现有 dialog shadow 绘制 flyout
- [x] 5.5 将 flyout 定位到 AutoSuggestBox 底部边缘，宽度匹配输入框，并进行屏幕边界夹取
- [x] 5.6 当 AutoSuggestBox move/resize 且 flyout 打开时，重新定位 flyout
- [x] 5.7 在 `onThemeUpdated()` 中刷新 popup 颜色、字体和几何

## 6. Suggestions 与 Flyout 生命周期

- [x] 6.1 实现 `setSuggestions()`，更新已存储 suggestions 和 popup model
- [x] 6.2 仅当列表发生变化时发射 `suggestionsChanged()`
- [x] 6.3 控件有焦点、用户输入非空且 suggestions 可用时打开 flyout
- [x] 6.4 suggestions 被清空或用户输入变为空时关闭 flyout
- [x] 6.5 flyout 可见状态变化时发射 `suggestionListOpenChanged(bool)`

## 7. 选择、查询提交与键盘导航

- [x] 7.1 实现建议项点击处理：更新文本、发射 `suggestionChosen`、发射 `querySubmitted(queryText, chosenSuggestion)` 并关闭 flyout
- [x] 7.2 实现 Down 键行为：打开 flyout 或推进选中行
- [x] 7.3 实现 Up 键行为：向上移动选中项，并在第一项之前恢复用户原始文本
- [x] 7.4 键盘导航高亮有效建议项时发射 `suggestionChosen(selectedSuggestion)`
- [x] 7.5 实现 Enter/Return 行为：提交选中建议或提交普通查询
- [x] 7.6 实现 Escape 行为：关闭 flyout 且不清空文本

## 8. Header 与主题绘制

- [x] 8.1 实现 `header` setter，并以 `header` 是否为空作为 Header 显示状态，发射变更信号、更新几何
- [x] 8.2 使用 Fluent typography 和文本颜色 Token 在输入框上方绘制可选 header 文本
- [x] 8.3 使用现有 Token 颜色绘制 Rest、Hover、Focused、Disabled 和 read-only 输入框状态
- [x] 8.4 确保 focused 状态使用 2 px accent 底部描边和 1 px 顶部/侧边描边
- [x] 8.5 确保 `onThemeUpdated()` 刷新文本样式、按钮、header、输入框 frame 和 popup

## 9. 测试与构建接线

- [x] 9.1 新建 `tests/views/textfields/TestAutoSuggestBox.cpp`，使用现有 FluentTestWindow 和 SetUpTestSuite 模式
- [x] 9.2 添加默认构造、属性 setter、属性通知信号测试
- [x] 9.3 添加 `textChangedWithReason` 测试，覆盖用户输入、程序化导航预览和建议选择路径
- [x] 9.4 添加 suggestions 显示/隐藏生命周期和 `suggestionListOpenChanged` 测试
- [x] 9.5 添加通过 Enter、query button 和选中建议提交查询的测试
- [x] 9.6 添加键盘 Down、Up 恢复、Enter、Escape 行为测试
- [x] 9.7 添加 VisualCheck 测试，展示 Rest、focused typing with flyout、with-header、disabled 和 pre-filled 状态，并加入 `SKIP_VISUAL_TEST` 守卫
- [x] 9.8 在 `tests/views/textfields/CMakeLists.txt` 中注册 `test_auto_suggest_box`
- [x] 9.9 构建并运行 `cmake --build build --target test_auto_suggest_box` 和 `SKIP_VISUAL_TEST=1 ./build/tests/views/textfields/test_auto_suggest_box`
