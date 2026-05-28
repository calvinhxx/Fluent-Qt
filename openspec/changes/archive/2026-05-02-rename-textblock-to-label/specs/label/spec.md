## ADDED Requirements

### Requirement: Label 公共 API
系统 SHALL 提供 `view::textfields::Label` 作为原静态 `TextBlock` 组件的规范公共名称，并暴露静态文本、Fluent typography 和主题刷新 API。

#### Scenario: 默认构造
- **WHEN** 创建一个无文本 `Label`
- **THEN** `fluentTypography` MUST 默认为 `Body`
- **AND** 控件 MUST 是 `QLabel` 派生类型

#### Scenario: 文本构造
- **WHEN** 使用 `Label(const QString& text, QWidget* parent)` 创建控件
- **THEN** 控件的 `text()` MUST 等于构造传入的文本

#### Scenario: Fluent typography
- **WHEN** 调用 `setFluentTypography()` 设置新的 typography token
- **THEN** `fluentTypography()` MUST 返回新 token
- **AND** 控件 MUST 使用对应主题字体
- **AND** `typographyChanged` MUST 触发一次

#### Scenario: 主题刷新
- **WHEN** 调用 `onThemeUpdated()`
- **THEN** `Label` MUST 保持当前 typography token 并更新字体和 `QPalette::WindowText`

### Requirement: TextEdit 保持独立
系统 SHALL 保留 `view::textfields::TextEdit` 作为富文本/多行文本编辑组件，并且 SHALL NOT 将 `TextEdit` 重命名或别名为 `Label`。

#### Scenario: TextEdit 仍可构造
- **WHEN** 代码包含 `view/textfields/TextEdit.h` 并构造 `view::textfields::TextEdit`
- **THEN** 代码 MUST 编译通过，并保留文本编辑相关 API

### Requirement: TextBlock 移除
系统 SHALL 移除旧的 `TextBlock` 公共入口，使新代码只使用 `Label`。

#### Scenario: 不保留兼容别名
- **WHEN** 检查 `src/view/textfields`
- **THEN** MUST NOT 存在 `TextBlock.h` 或 `TextBlock.cpp`
- **AND** MUST NOT 存在 `using Label = TextBlock` 或 `using TextBlock = Label`

### Requirement: Label 主题与可视化覆盖
系统 SHALL 为 `Label` 提供自动化测试和 VisualCheck，覆盖重命名后的静态文本 API、Fluent typography 和 Light/Dark 主题展示。

#### Scenario: 自动化测试
- **WHEN** 构建并运行 `test_label`
- **THEN** 测试 MUST 覆盖 `Label` 构造、文本、`fluentTypography`、theme refresh 和 VisualCheck 路径

#### Scenario: TextEdit 自动化测试
- **WHEN** 构建并运行 `test_text_edit`
- **THEN** 测试 MUST 覆盖 `TextEdit` 仍作为编辑组件可用
