## 为什么

当前 textfields 模块已有 `LineEdit`、`TextEdit` 和 `TextBlock`，但还缺少 WinUI 3 的 `AutoSuggestBox` 模式：用户输入时应用可以提供建议，控件可以提交查询，并且能区分“用户输入”和“由建议选择导致的文本变化”。新增这个组件可以补齐 Fluent 文本输入体系中常见的搜索/自动补全能力，并让业务代码复用与 Windows UI Kit、WinUI Gallery 行为一致的输入控件。

## 变更内容

- 在 `src/view/textfields/` 下新增 `AutoSuggestBox` 组件，继承自 `LineEdit`。
- 提供建议数据 API、Header / QueryIcon 配置、建议列表打开状态，以及 WinUI 风格的文本变更原因。
- 在输入框右侧加入 Search 和 Clear 按钮，布局匹配 Figma Windows UI Kit。
- 添加建议列表 Flyout，支持键盘/鼠标选择、查询提交，以及主题自适应 Fluent 绘制。
- 添加聚焦单元测试和 VisualCheck 测试，覆盖 Rest、Typing、Flyout、Header、Disabled 等状态。

## 能力范围

### 新增能力
- `auto-suggest-box`：定义 Fluent `AutoSuggestBox` 文本输入控件的公共 API 和行为，包括 suggestions、文本变更原因、建议选择、查询提交、Header 显示、QueryIcon、Flyout 生命周期、键盘导航和主题适配。

### 修改能力
- 无。

## 影响范围

- 新增 `src/view/textfields/` 下的组件源码文件。
- 新增 `tests/views/textfields/` 下的测试文件和一个 CMake 测试目标。
- 复用现有 `LineEdit`、`Button`、Fluent Design Token 以及已有 popup/dialog flyout 模式；预计不引入新的第三方依赖。
