# info-bar Specification

## Purpose

规范 `view::status_info::InfoBar`：提供 Fluent 应用状态消息条，支持 open/close、severity、单行/多行布局、icon/action/close 区、主题与禁用态，以及 WinUI Gallery 风格测试样例。

## Requirements

### Requirement: InfoBar 公共 API
系统 SHALL 提供一个 `InfoBar` 组件，继承 `QWidget`、`FluentElement` 和 `view::QMLPlus`，并暴露应用状态消息条所需的 open、title、message、severity、icon、close、action 和 layout 属性。

#### Scenario: 默认构造
- **WHEN** 创建一个 `InfoBar`
- **THEN** `isOpen` MUST 为 true、`title` MUST 为 `Title`、`message` MUST 为空、`severity` MUST 为 `Informational`、`isClosable` MUST 为 true、`isIconVisible` MUST 为 true、`singleLine` MUST 为 true、`preferredWidth` MUST 为 600，且 `actionWidget()` MUST 为 `nullptr`

#### Scenario: 属性变更信号
- **WHEN** 调用任一属性 setter 且新值与旧值不同
- **THEN** 组件 MUST 更新布局或绘制状态，并触发对应 changed 信号一次

#### Scenario: 重复设置相同值
- **WHEN** 调用任一属性 setter 且新值与旧值相同
- **THEN** 组件 MUST 不触发 changed 信号且 MUST 不产生冗余布局更新

#### Scenario: preferredWidth 防护
- **WHEN** 调用 `setPreferredWidth()` 且传入 0 或负数
- **THEN** 组件 MUST 保持上一个有效 preferred width 不变

#### Scenario: 可配置布局与图标 token
- **WHEN** 调用方设置 `contentMargins`、`closeButtonSize`、`singleLineHeight`、`multiLineMinHeight`、`multiLineActionMinHeight`、`iconTextSpacing`、`titleMessageSpacing`、`cornerRadius`、`severityIconSize`、`severityIconGlyphSize`、`titleFontRole`、`messageFontRole` 或 severity icon glyph 属性
- **THEN** 组件 MUST 更新布局或绘制状态并发出对应 changed 信号，且重复设置相同值 MUST 不重复发信号

### Requirement: Open 与关闭行为
系统 SHALL 通过 `isOpen` 表达 InfoBar 是否展示，并在 close button 点击时只关闭控件状态，不销毁控件实例。

#### Scenario: 设置关闭状态
- **WHEN** 调用 `setIsOpen(false)`
- **THEN** `isOpen()` MUST 返回 false，组件 MUST 隐藏或返回空尺寸语义，并触发一次 `isOpenChanged(false)` 信号

#### Scenario: 重新打开
- **WHEN** 已关闭的 `InfoBar` 调用 `setIsOpen(true)`
- **THEN** `isOpen()` MUST 返回 true，组件 MUST 恢复可见布局语义，并触发一次 `isOpenChanged(true)` 信号

#### Scenario: close button 点击
- **WHEN** `isClosable` 为 true 且用户点击 close button
- **THEN** 组件 MUST 调用 `setIsOpen(false)`，发出 `closed()` 信号，并隐藏 close button 所在的可交互状态

#### Scenario: 不可关闭
- **WHEN** `isClosable` 为 false
- **THEN** close button MUST 不显示，且用户无法通过内部 close button 改变 `isOpen`

### Requirement: Severity 视觉语义
系统 SHALL 支持 Informational、Success、Warning 和 Error 四种 severity，并为每种 severity 绘制对应 badge 图标与颜色。

#### Scenario: Informational severity
- **WHEN** `severity` 为 `Informational` 且 `isIconVisible` 为 true
- **THEN** 组件 MUST 显示 informational badge，badge 色 MUST 使用 accent 或 informational 语义色，并在 Light 主题下接近 Figma `#005FB7`

#### Scenario: Success severity
- **WHEN** `severity` 为 `Success` 且 `isIconVisible` 为 true
- **THEN** 组件 MUST 显示 success badge，badge 色 MUST 使用 success 语义色，并使用成功图标

#### Scenario: Warning severity
- **WHEN** `severity` 为 `Warning` 且 `isIconVisible` 为 true
- **THEN** 组件 MUST 显示 warning badge，badge 色 MUST 使用 caution 语义色，并使用警告图标

#### Scenario: Error severity
- **WHEN** `severity` 为 `Error` 且 `isIconVisible` 为 true
- **THEN** 组件 MUST 显示 error badge，badge 色 MUST 使用 critical 语义色，并使用错误图标

#### Scenario: Severity 背景色
- **WHEN** severity 在 Informational、Success、Warning 和 Error 之间切换
- **THEN** InfoBar 的 bar 背景 MUST 分别使用 informational/success/caution/critical background 语义色，而不仅仅改变左侧 badge 颜色

#### Scenario: Severity iconfont 字形
- **WHEN** severity badge 可见
- **THEN** badge 内部 MUST 使用 Figma InfoBar Badge 参考的 Segoe Fluent Icons glyph（Informational=`AsteriskBadge12` U+E625、Success=`CheckmarkBadge12` U+E65F、Warning=`ImportantBadge12` U+EDB1、Error=`ErrorBadge12` U+EDAE）或调用方配置的 glyph，并 MUST 以 glyph path bounds 进行视觉居中而不是依赖普通文本 baseline 对齐

#### Scenario: 隐藏 icon
- **WHEN** `isIconVisible` 为 false
- **THEN** 组件 MUST 隐藏 severity badge，并让文本区域占用 icon slot 释放出的水平空间

### Requirement: 单行布局
系统 SHALL 按 Figma Windows UI Kit 的 single-line Info Bar 变体绘制默认 600x50 布局。

#### Scenario: 默认尺寸
- **WHEN** 创建一个默认 `InfoBar`
- **THEN** `sizeHint()` MUST 返回宽 600、高 50 的尺寸语义，并且 `minimumSizeHint().height()` MUST 至少保留 50px

#### Scenario: 单行文本布局
- **WHEN** `singleLine` 为 true、title 和 message 均非空
- **THEN** title 和 message MUST 在同一行显示，title 使用 Body Strong 14px/20px 字体，message 使用 Body 14px/20px 字体，二者之间 MUST 保留约 12px 间距

#### Scenario: 单行可用空间不足
- **WHEN** `singleLine` 为 true 且 title、message、action 和 close button 的总宽度超过可用宽度
- **THEN** 组件 MUST 优先保持 icon、title、action 和 close button 可见，并通过自定义 `Label` 对 message 进行 elide，保留完整原文用于 hover tooltip，且不得产生文本重叠

#### Scenario: 单行 close hit target
- **WHEN** `isClosable` 为 true
- **THEN** close button 的交互区域 MUST 约为 48x48，视觉 glyph MUST 居中绘制，距离 InfoBar 外边框 MUST 保留 `contentMargins.right()` 指定的右侧边距，message/action 内容与 close button 左边界之间 MUST 保留明确间距，且不改变 InfoBar 50px 默认高度

### Requirement: 多行布局与 action 区
系统 SHALL 支持 multi-line Info Bar，用于长消息以及 Button / Hyperlink action 示例。

#### Scenario: 多行无 action
- **WHEN** `singleLine` 为 false、message 为长文本且 `actionWidget()` 为 `nullptr`
- **THEN** title MUST 独立一行，message MUST 换行显示，`sizeHint().height()` MUST 至少为 110px，并随文本换行高度增加

#### Scenario: 多行 Button action
- **WHEN** `singleLine` 为 false 且 `setActionWidget()` 传入一个 `Button`
- **THEN** action widget MUST 显示在文本区域下方，布局 MUST 至少保留 158px 高度语义，并且 action widget MUST 可点击

#### Scenario: 多行 Hyperlink action
- **WHEN** `singleLine` 为 false 且 `setActionWidget()` 传入一个 `HyperlinkButton`
- **THEN** action widget MUST 显示在文本区域下方，保持 hyperlink 自身 accent 文本语义，并且 InfoBar MUST 不接管其 url 点击逻辑

#### Scenario: 替换 action widget
- **WHEN** `setActionWidget()` 在已有 action widget 后传入新的 widget 或 `nullptr`
- **THEN** 组件 MUST 从布局中移除旧 action widget，托管新 action widget 或清空 action 区，并触发一次 `actionWidgetChanged()` 信号

### Requirement: 主题与禁用状态
系统 SHALL 根据当前 Fluent 主题和 enabled 状态选择 InfoBar 背景、stroke、文本、badge 与 close/action 的颜色。

#### Scenario: Light 主题 chrome
- **WHEN** 当前主题为 Light
- **THEN** InfoBar 背景 MUST 接近 Figma `#F6F6F6` 半透明 card 背景，stroke MUST 为浅色 card stroke，文本 MUST 使用 primary text 色

#### Scenario: Dark 主题 chrome
- **WHEN** 当前主题为 Dark
- **THEN** InfoBar MUST 使用 Dark 主题下的 card/background/stroke/text 语义色，并保持 severity badge 与文本可读

#### Scenario: disabled 状态
- **WHEN** 组件被禁用
- **THEN** 文本、badge 和 close/action 视觉 MUST 使用 disabled 语义色或 disabled 子控件状态，且 close button MUST 不触发关闭

#### Scenario: 主题切换
- **WHEN** 全局主题在 Light 和 Dark 之间切换
- **THEN** `InfoBar` MUST 在 `onThemeUpdated()` 中刷新缓存颜色、内部字体/调色板和子控件状态，并请求重绘

### Requirement: WinUI Gallery 风格交互语义
系统 SHALL 支持 WinUI Gallery `InfoBarPage` 中的 severity、message length、action button、open、icon visible 和 closable 示例。

#### Scenario: Severity selector
- **WHEN** 外部控件将 severity 依次设置为 Informational、Success、Warning 和 Error
- **THEN** InfoBar MUST 更新 badge 图标与颜色，并保持 title/message/open 状态不变

#### Scenario: Message length selector
- **WHEN** 外部控件在短消息和长消息之间切换
- **THEN** InfoBar 的 message MUST 更新，短消息可在 single-line 中完整显示，长消息在 multi-line 中 MUST 换行显示

#### Scenario: Action selector
- **WHEN** 外部控件在 None、Button 和 Hyperlink 三种 action 之间切换
- **THEN** InfoBar MUST 分别清空 action、显示 Button action、显示 HyperlinkButton action，并重新计算 size hint

#### Scenario: Open/icon/closable controls
- **WHEN** 外部控件切换 `isOpen`、`isIconVisible` 或 `isClosable`
- **THEN** InfoBar MUST 同步显示状态、icon slot 和 close button 状态，并触发对应 changed 信号

### Requirement: 测试与可视化覆盖
系统 SHALL 为 `InfoBar` 提供自动化测试和 VisualCheck，覆盖行为、主题和 Figma / WinUI Gallery 关键视觉变体。

#### Scenario: 自动化测试
- **WHEN** 构建并运行 `test_info_bar`
- **THEN** 测试 MUST 覆盖默认属性、属性信号、open/close、severity、icon visibility、closable、action widget、single-line/multi-line size hint、theme update 和 disabled 行为

#### Scenario: VisualCheck
- **WHEN** 运行 `test_info_bar --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 Light/Dark、Single Line/Multi Line、Informational/Success/Warning/Error、action none/button/hyperlink、icon visible/hidden、closable/non-closable，以及 WinUI Gallery 三组核心示例，并遵循 `SKIP_VISUAL_TEST` 环境变量守卫
