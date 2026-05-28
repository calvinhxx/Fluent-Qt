# label Specification

## Purpose

系统 SHALL 提供 `view::textfields::Label` 作为静态文本显示组件的公共名称，用于替代旧的 `TextBlock` 公共入口，并覆盖 Fluent typography、主题刷新与可视化测试场景。

## Requirements

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

### Requirement: Label supports configurable single-line elision

`Label` SHALL expose configurable text elision using Qt text elide modes. The default elide mode MUST be `Qt::ElideNone`, preserving existing Label behavior unless a caller opts into elision.

#### Scenario: Default labels are not elided
- **WHEN** a `Label` is constructed without changing its elide mode
- **THEN** its elide mode MUST be `Qt::ElideNone`
- **AND** its displayed text MUST remain equal to the full text

#### Scenario: Elide mode truncates overflowing text
- **WHEN** a `Label` has long plain text, a constrained width, and elide mode `Qt::ElideRight`
- **THEN** the displayed text MUST be the font-metric elided form
- **AND** the full text MUST remain available through `Label::text()`

### Requirement: Elided Label shows full text in Fluent ToolTip on hover

`Label` SHALL show a `view::status_info::ToolTip` containing the full unelided text when hovered if and only if the current text is actually elided in the rendered label.

#### Scenario: Hovering an actually elided label shows full text
- **WHEN** a `Label` with elide mode enabled renders truncated text and receives hover enter
- **THEN** a Fluent `ToolTip` MUST be shown
- **AND** the tooltip text MUST equal the full `Label::text()`

#### Scenario: Hovering a non-truncated label does not show tooltip
- **WHEN** a `Label` has enough width to render the full text
- **AND** the label receives hover enter
- **THEN** no elide tooltip MUST be shown

#### Scenario: Leaving the label hides the tooltip
- **WHEN** an elide tooltip is visible for a `Label`
- **AND** the label receives hover leave
- **THEN** the tooltip MUST hide

### Requirement: Label keeps elide tooltip state current

`Label` SHALL recompute rendered elision and tooltip eligibility whenever text, font, typography, geometry, elide mode, or theme refresh can affect text width.

#### Scenario: Resize removes tooltip eligibility
- **WHEN** a previously elided `Label` is resized wide enough to fit its full text
- **THEN** the displayed text MUST return to the full text
- **AND** hover MUST NOT show an elide tooltip

#### Scenario: Text changes update tooltip content
- **WHEN** a Label with an active elide tooltip changes its full text
- **THEN** the displayed elided text MUST be recomputed
- **AND** the tooltip content MUST use the updated full text when shown again

#### Scenario: Typography changes recompute elision
- **WHEN** `setFluentTypography()` or `onThemeUpdated()` changes the effective Label font
- **THEN** the elision state MUST be recomputed using the updated font metrics
