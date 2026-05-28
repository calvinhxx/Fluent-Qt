## Context

项目已有 `LineEdit` 作为 WinUI 3 风格单行文本输入基础控件，负责 Fluent frame、主题字体、placeholder/selection palette、contentMargins、focus 底部高亮和内置 clear button。`AutoSuggestBox` 已证明基于 `LineEdit` 派生并在派生类中自绘 Header/输入框、使用 `AnchorLayout` 布局内部按钮的模式可行。

Figma Windows UI Kit 的 Text Fields 页面中，Password Box 组件覆盖以下视觉事实：无 Header 输入框为 159/160 x 32，带 Header 高度为 60；状态覆盖 Light/Dark、Rest、Hover、Pressed、Disabled、Focus、Typing、Text entered、Text selected，并有 Show PW Rest/Hover/Pressed/Sticky 以及自定义 arrow/password-enter 按钮状态。变量定义中正文使用 Segoe UI Variable Body 14/20，输入框使用 Control fill/stroke、Accent bottom line、Text Primary/Secondary/Disabled 等语义色。

WinUI Gallery 中 PasswordBox 示例体现了核心行为：通过 `Password` 读取真实密码，`PasswordChanged` 响应输入变化，`PasswordRevealMode.Visible/Hidden` 可由外部控件切换；`ValidatedPasswordBox` 示例则把 `PasswordBox` 包装成带验证提示的更高阶控件，说明密码强度/规则验证应独立于基础 PasswordBox。

## Goals / Non-Goals

**Goals:**

- 新增 `PasswordBox : public LineEdit`，保留本项目三重继承链中 `LineEdit` 已提供的 Fluent 能力。
- 提供 `password` 属性、`passwordChanged` 信号和 `PasswordRevealMode` 枚举，对齐 WinUI Gallery 的主要使用方式。
- 支持隐藏、持续显示、按住显示三种密码显示策略。
- 支持可选 Header，并按 Figma 的 32px 输入框和 60px Header 组合高度布局。
- 使用 `AnchorLayout` 管理 Reveal 按钮位置，避免手写散落几何逻辑。
- 提供 GTest 与 VisualCheck，覆盖交互、属性、布局和主题状态。

**Non-Goals:**

- 不在首版实现密码强度、规则校验、验证消息或 `ValidatedPasswordBox` 组合控件。
- 不承诺安全内存、系统级 secure input 或剪贴板策略；Qt `QLineEdit::Password` 只提供显示遮罩。
- 不改造 `LineEdit` / `AutoSuggestBox` 公共 API。
- 不引入 QSS/QPalette 作为 Fluent 主视觉来源；派生控件仍以 `paintEvent()` 和 Design Token 自绘为准。

## Decisions

### 1. 从 LineEdit 派生并关闭默认 clear button

`PasswordBox` 继承 `LineEdit`，构造时关闭 `LineEdit` 内置 clear button，仅保留密码专用的 Reveal 按钮。这样可复用字体、palette、内容边距、selection 和主题更新逻辑，同时避免右侧 clear 与 reveal 两个按钮竞争有限的 32px 高输入框空间。

备选方案是直接继承 `QLineEdit` 并重新实现所有 Fluent 输入框行为。该方案会复制 `LineEdit` 已有逻辑，后续主题或字体修正容易分叉。

### 2. 以 QLineEdit 文本作为密码值的单一来源

`password()` 读取 `LineEdit::text()`，`setPassword()` 调用 `setText()`，`passwordChanged` 由 `QLineEdit::textChanged` 转发。组件不再维护一份额外的 `QString m_password`，从根上避免真实文本与编辑器文本不同步。

显示遮罩交给 `QLineEdit::setEchoMode()`：隐藏时使用 `QLineEdit::Password`，显示时使用 `QLineEdit::Normal`。这符合 Qt 控件模型，也让 selection、输入法、undo/redo、validator 等基础能力继续由 `QLineEdit` 处理。

### 3. 使用 WinUI 风格的 PasswordRevealMode

定义 `enum class PasswordRevealMode { Peek, Hidden, Visible }`：

- `Peek` 为默认模式：有密码、可编辑、启用时显示 Reveal 按钮；按住按钮临时切换为明文，释放、离开按钮、失焦或禁用时恢复遮罩。
- `Hidden`：始终遮罩，隐藏 Reveal 按钮。
- `Visible`：始终明文，隐藏临时 Reveal 按钮。

Figma 中 “Show PW Sticky (Custom)” 表达的是设计变体状态；首版不增加额外 sticky toggle 属性。持续显示由 `Visible` 模式承担，按住显示由 `Peek` 模式承担。

### 4. Header 和输入框由派生类自绘

带 Header 时控件总高度采用 60px：Header 文本区域 20px、间距 8px、输入框 32px。无 Header 时总高度为 32px。派生类关闭 `LineEdit` frame 自绘，然后在 `paintEvent()` 中按 `inputRect()` 绘制输入框 frame 和 focus bottom border；Header 使用 Body 字体和 Text Primary 颜色绘制。

Reveal 按钮按 Figma 的 Text Box Button primitive 预留 32 x 28 的 hit area，垂直居中到输入框区域。文本右边距按按钮可见性动态增加，避免输入内容、placeholder、选区与按钮重叠。

### 5. 内部按钮使用 AnchorLayout

Reveal 按钮创建为 `view::basicinput::Button`，`Subtle`、`IconOnly`、`Small`、`Qt::NoFocus`。通过 `AnchorLayout` 将按钮锚定到控件右侧和输入框垂直中心；Header 高度变化时只更新 anchor offset，而不是散落多处 `move()` 计算。

### 6. 测试采用现有 GTest / VisualCheck 模式

测试文件使用 `.github/instructions/test-patterns.instructions.md` 里的 `SetUpTestSuite()`、`FluentTestWindow` 和 `SKIP_VISUAL_TEST` 守卫。增量验证默认只构建并运行 `test_password_box`，VisualCheck 单独运行。

## Risks / Trade-offs

- Qt `QLineEdit::Password` 不是安全存储 → 在文档和设计中明确首版只解决 UI 行为，不宣称安全内存能力。
- Reveal 按钮图标可能没有现成 `Typography::Icons` 常量 → 实现时优先复用已有 Segoe Fluent Icons；若需新增 glyph 常量，只在 `Typography::Icons` 中集中定义。
- Header 自绘会与 `LineEdit` 默认 frame 逻辑有少量重复 → 复用 `AutoSuggestBox` 已验证的 `inputRect()` / `paintInputFrame()` 模式，并保持实现局部化。
- `Visible` 模式下明文显示可能影响用户预期 → 默认模式保持 `Peek`，只有业务显式设置 `Visible` 才持续明文。

## Migration Plan

1. 新增 `PasswordBox` 源码并纳入 `fluent_qt_lib`。
2. 新增并注册 `test_password_box`。
3. 构建并运行受影响测试目标。
4. 如后续业务需要密码强度验证，另行提出 `validated-password-box` 或表单验证能力，不塞入基础控件。

## Open Questions

- Reveal/Hide 的最终 glyph 是否需要新增命名常量，取决于现有 Segoe Fluent Icons 常量是否足够表达设计。