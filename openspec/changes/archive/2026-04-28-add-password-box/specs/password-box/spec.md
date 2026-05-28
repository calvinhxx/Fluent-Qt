## ADDED Requirements

### Requirement: PasswordBox 公共 API
系统 SHALL 提供一个 `PasswordBox` 组件，继承 `LineEdit`，并暴露密码输入专用的 `password` 属性、`passwordChanged` 信号和 `PasswordRevealMode` 枚举。

#### Scenario: 默认构造
- **WHEN** 创建一个 `PasswordBox`
- **THEN** `password` MUST 为空、默认 reveal mode MUST 为 `Peek`，并且控件 MUST 以遮罩模式显示输入内容

#### Scenario: 程序设置密码
- **WHEN** 调用 `setPassword("secret")`
- **THEN** `password()` MUST 返回 `"secret"`，编辑器内容 MUST 与密码值保持一致，并且 `passwordChanged("secret")` MUST 被触发一次

#### Scenario: 用户编辑密码
- **WHEN** 用户在 `PasswordBox` 中输入或删除字符
- **THEN** `password` MUST 反映当前真实输入值，并且 `passwordChanged` MUST 携带新的真实密码值

### Requirement: 密码显示模式
系统 SHALL 支持 `Peek`、`Hidden`、`Visible` 三种密码显示模式，并根据模式控制遮罩、明文显示和 Reveal 按钮可见性。

#### Scenario: Peek 模式按住显示
- **WHEN** reveal mode 为 `Peek`、控件启用且可编辑、密码非空，并且用户按住 Reveal 按钮
- **THEN** 密码内容 MUST 临时以明文显示

#### Scenario: Peek 模式释放恢复遮罩
- **WHEN** reveal mode 为 `Peek` 且用户释放 Reveal 按钮、鼠标离开按钮、控件失焦或控件被禁用
- **THEN** 密码内容 MUST 恢复遮罩显示

#### Scenario: Hidden 模式始终隐藏
- **WHEN** reveal mode 设置为 `Hidden`
- **THEN** 密码内容 MUST 始终遮罩显示，并且 Reveal 按钮 MUST 不可见

#### Scenario: Visible 模式始终显示
- **WHEN** reveal mode 设置为 `Visible`
- **THEN** 密码内容 MUST 以明文显示，并且临时 Reveal 按钮 MUST 不可见

### Requirement: Reveal 按钮布局与交互
系统 SHALL 在需要临时显示密码时提供右侧 Reveal 按钮，并保证该按钮不遮挡文本、placeholder、选区或输入框边框。

#### Scenario: 有密码时显示 Reveal 按钮
- **WHEN** reveal mode 为 `Peek`、控件启用且可编辑、密码非空
- **THEN** Reveal 按钮 MUST 显示在输入框右侧，并且文本右边距 MUST 预留按钮空间

#### Scenario: 不可编辑时隐藏 Reveal 按钮
- **WHEN** 控件被禁用、只读或密码为空
- **THEN** Reveal 按钮 MUST 不可见，并且控件 MUST 保持正确的文本边距

#### Scenario: Reveal 按钮不抢占文本焦点
- **WHEN** 用户按下或释放 Reveal 按钮
- **THEN** `PasswordBox` MUST 保持文本输入焦点，后续键盘输入 MUST 继续进入该密码框

### Requirement: Fluent PasswordBox 视觉结构
系统 SHALL 按 WinUI 3 Text Fields/Password Box 视觉结构绘制输入框、Header、状态颜色和聚焦底部高亮。

#### Scenario: 无 Header 布局
- **WHEN** `header` 为空
- **THEN** 控件推荐高度 MUST 为 32px，输入框 MUST 占据整个控件高度

#### Scenario: 有 Header 布局
- **WHEN** `header` 非空
- **THEN** 控件推荐高度 MUST 为 60px，Header 文本 MUST 位于输入框上方，并且输入框高度 MUST 保持 32px

#### Scenario: 聚焦状态绘制
- **WHEN** `PasswordBox` 获得焦点且处于启用、可编辑状态
- **THEN** 输入框 MUST 使用主题 accent 色绘制底部聚焦高亮

#### Scenario: 禁用状态绘制
- **WHEN** `PasswordBox` 被禁用
- **THEN** 输入框文本、placeholder、边框和按钮状态 MUST 使用主题 disabled 语义色，并且 Reveal 按钮 MUST 不可交互

### Requirement: 主题与测试覆盖
系统 SHALL 保证 `PasswordBox` 在 Light/Dark 主题下响应 Design Token，并提供自动化与可视化测试覆盖核心行为。

#### Scenario: 主题切换
- **WHEN** 全局主题在 Light 和 Dark 之间切换
- **THEN** `PasswordBox` 和 Reveal 按钮 MUST 更新字体、文本颜色、placeholder、selection、输入框填充、边框和 accent 高亮

#### Scenario: 自动化测试
- **WHEN** 构建 `test_password_box`
- **THEN** 测试 MUST 覆盖默认属性、password 信号、reveal mode、Reveal 按钮交互、Header 尺寸和禁用/只读状态

#### Scenario: VisualCheck
- **WHEN** 运行 `test_password_box --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 Light/Dark、Rest/Hover/Focus/Disabled、有 Header、Peek/Hidden/Visible 等代表性状态，并遵循 `SKIP_VISUAL_TEST` 环境变量守卫