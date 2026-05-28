## 1. 组件结构

- [x] 1.1 新增 `src/view/textfields/PasswordBox.h`，定义 `PasswordBox : public LineEdit`、`PasswordRevealMode`、`password`、`header`、`passwordRevealMode` 等 Q_PROPERTY 和信号
- [x] 1.2 新增 `src/view/textfields/PasswordBox.cpp`，构造时关闭 `LineEdit` 内置 clear button，初始化 EchoMode、Reveal 按钮和 AnchorLayout
- [x] 1.3 在 `PasswordBox` 中实现 `password()` / `setPassword()`，以 `LineEdit::text()` 作为单一真实值来源并转发 `passwordChanged`

## 2. 交互与布局

- [x] 2.1 实现 `PasswordRevealMode::Peek` / `Hidden` / `Visible`，用 `QLineEdit::EchoMode` 控制遮罩和明文显示
- [x] 2.2 实现 Reveal 按钮按住显示、释放/离开/失焦/禁用恢复遮罩的交互逻辑，并保持文本焦点
- [x] 2.3 使用 `AnchorLayout` 将 Reveal 按钮锚定在输入框右侧和输入框垂直中心，Header 变化时同步 anchor offset
- [x] 2.4 动态更新 contentMargins 和 textMargins，确保文本、placeholder、selection 不与 Reveal 按钮或 Header 重叠

## 3. Fluent 绘制与主题

- [x] 3.1 实现 Header 与输入框 frame 自绘，无 Header 高度 32px，有 Header 高度 60px
- [x] 3.2 覆盖 Rest、Hover、Focus、Pressed、Disabled、ReadOnly 状态的 fill/stroke/text/accent 颜色
- [x] 3.3 在 `onThemeUpdated()` 中同步 `LineEdit`、Reveal 按钮、字体、palette 和绘制状态
- [x] 3.4 如 Reveal/Hide 图标缺少集中常量，在 `Typography::Icons` 中补充命名 glyph 常量

## 4. 构建与测试

- [x] 4.1 确认 `fluent_qt_lib` 自动纳入新增 PasswordBox 源码，并更新需要显式注册的 CMake 配置
- [x] 4.2 新增 `tests/views/textfields/TestPasswordBox.cpp`，按项目测试规范初始化 QApplication、资源和字体
- [x] 4.3 注册 `test_password_box`，覆盖默认属性、password 信号、reveal mode、按钮交互、Header 尺寸、禁用/只读状态
- [x] 4.4 增加 VisualCheck，展示 Light/Dark、Header、Peek/Hidden/Visible、Focus/Disabled 等代表性状态，并加入 `SKIP_VISUAL_TEST` 守卫
- [x] 4.5 增量验证：构建并运行 `test_password_box`，必要时单独运行 VisualCheck