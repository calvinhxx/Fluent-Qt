## Context

`src/view/status_info/` 目前已有 `ToolTip`、`ProgressBar` 和 `ProgressRing`。其中 `ProgressBar` / `ProgressRing` 已建立状态信息控件的首选实现模式：直接继承 `QWidget`，混入 `FluentElement` 与 `view::QMLPlus`，通过 `QPainter` 自绘、缓存主题色，并在测试中提供 WinUI Gallery 风格示例与 VisualCheck。

Figma Windows UI Kit 的 Info Bar 节点显示该控件的核心 master 变体：Light/Dark、Single Line/Multi Line、Informational/Warning/Critical/Success、Action=None/HyperLink Button/Button。默认 single-line informational 样式为 600x50，背景为 `#F6F6F6` 约 50% 透明，描边约 `rgba(0,0,0,0.06)`，圆角约 3px；左侧 16px severity badge 位于 x=15、y=16 附近，标题和正文均为 14px / 20px，title 使用 Body Strong。multi-line 无 action 时约 600x110，有 action button / hyperlink 时约 600x158。WinUI Gallery `InfoBarPage` 展示三个主要交互：通过 ComboBox 切换 `Severity`，长短 message 与 action button/hyperlink 切换，以及 `IsOpen`、`IsIconVisible`、`IsClosable` 控制。

## Goals / Non-Goals

**Goals:**

- 新增 `InfoBar : public QWidget, public FluentElement, public view::QMLPlus`，保持 `status_info` 组件一致继承与自绘风格。
- 支持 `isOpen`、`title`、`message`、`severity`、`isClosable`、`isIconVisible`、`actionWidget`、`preferredWidth`、`singleLine` 等 WinUI 风格行为。
- 按 Figma Info Bar 变体绘制默认 600x50 single-line 版式、多行 110px / 158px 高度语义、半透明 card 背景、1px stroke、3px 圆角、16px badge、48x48 close hit target。
- 支持 Informational、Success、Warning、Error/Critical 四种 severity 的图标、badge 色和 Light/Dark 主题刷新。
- 复刻 WinUI Gallery 示例：可切换 open/severity、长短消息、action none/button/hyperlink、icon visibility、closable。
- 提供 GTest 与 VisualCheck，覆盖行为、尺寸、主题和关键视觉变体。

**Non-Goals:**

- 不实现 toast、flyout、notification queue 或自动消失计时器；`InfoBar` 是内容区内的持久状态消息条。
- 不实现动画展开/收起，首版 `isOpen=false` 使用隐藏与空 size hint 语义。
- 不自研 action button 类型；首版通过 `QWidget* actionWidget` 组合现有 `Button` / `HyperlinkButton` 或业务自定义控件。
- 不暴露完整自定义颜色 API；首版颜色由 `FluentElement` 的 Design Token 和 severity 派生。
- 不修改已有 `ToolTip`、`ProgressBar`、`ProgressRing` 的公共 API。

## Decisions

### 1. 使用 QWidget 容器 + 子控件布局 + 自绘 chrome

`InfoBar` 作为 `QWidget` 容器，背景、stroke 和 severity badge 由 `paintEvent()` 自绘；title/message 使用项目自定义 `view::textfields::Label`，close 使用 `Button` 的 icon-only subtle 风格，action 使用外部传入的 `QWidget*`。纯手写全部文字绘制会增加换行、elide、可访问和 action 组合成本；完全依赖 QFrame/QSS 又会偏离项目“自绘 Fluent chrome，不使用 QSS/QPalette 作为视觉核心”的约定。因此采用“自绘 chrome + Fluent 子控件承载内容”的折中方案。

### 2. API 对齐 WinUI 命名，同时使用 Qt 可组合类型

公共枚举定义为 `InfoBarSeverity { Informational, Success, Warning, Error }`。`Error` 对应 WinUI Gallery 的 `InfoBarSeverity.Error`，视觉上匹配 Figma 的 Critical 变体。公共属性包括：

- `isOpen`：默认 true，false 时隐藏并返回空尺寸语义。
- `title` / `message`：默认分别为 `Title` 和空字符串，支持单行或多行布局。
- `severity`：默认 `Informational`。
- `isClosable`：默认 true，控制 close button 是否显示。
- `isIconVisible`：默认 true，控制 severity badge 是否显示。
- `singleLine`：默认 true；false 时 message 可换行并按内容/action 增高。
- `preferredWidth`：默认 600，作为 `sizeHint().width()` 和 VisualCheck 宽度基准。
- `actionWidget`：通过 `setActionWidget(QWidget*)` 托管一个可选 action 控件。

setter 重复设置相同值不发信号；布局相关 setter 调用 `updateGeometry()` 和 `update()`。

### 3. 布局常量直接来自 Figma 结构，并允许内部微调

默认宽度 600，高度 single-line 50。根容器内边距按 Figma 取 left 15、right 15、top 13、bottom 15；icon slot 为 16x22，badge 16x16，icon/text gap 13，title/message gap 12，右侧 close hit target 48x48，视觉 close glyph 使用 `Typography::Icons::ChromeClose`。多行无 action 的 `sizeHint()` 至少 110px，高度随 message wrapping 增长；有 action widget 时底部 action 区让高度至少 158px。

这些尺寸不应只作为 `.cpp` 局部常量存在。首版公开 `contentMargins`、`closeButtonSize`、`singleLineHeight`、`multiLineMinHeight`、`multiLineActionMinHeight`、`iconTextSpacing`、`titleMessageSpacing`、`cornerRadius`、`severityIconSize` 和 `severityIconGlyphSize`，用于 VisualCheck 微调和业务嵌入场景。close button 到右边框的距离由 `contentMargins.right()` 控制，避免额外的 close-only margin 与整体内容边距语义重复。默认值仍保持 Figma/WinUI 语义。

`singleLine=true` 时 title 与 message 同行，message 按可用宽度交给自定义 `Label` 执行 `ElideRight`，保留完整原文以便 hover 时显示 `ToolTip`；close button 左侧额外保留内容间距，避免省略号或 action 贴近关闭按钮。`singleLine=false` 时 title 独立一行，message 换行并关闭 elide，action widget 位于文本下方并左对齐。

### 4. Severity 颜色从现有语义 token 派生

`updateThemeColors()` 缓存 background、stroke、text、icon foreground 和 badge 色：

- Informational: `themeColors().accentDefault` 或 `systemInformational` 等价语义色，Figma Light 参考 `#005FB7`。
- Success: `themeColors().systemSuccess`。
- Warning: `themeColors().systemCaution`。
- Error: `themeColors().systemCritical`。

badge 内图标使用 `textOnAccent` 或适合该 severity 的高对比前景色。bar 背景按 severity 使用 `systemInfoBg`、`systemSuccessBg`、`systemCautionBg`、`systemCriticalBg`，而不是所有状态共用 neutral card 背景；stroke 使用 card stroke token。

Figma InfoBar Badge 子节点显示 16x16 badge 组件中包含 14x14 `Base` 与 12px Segoe Fluent Icons glyph：Informational=`AsteriskBadge12` (U+E625)、Warning=`ImportantBadge12` (U+EDB1)、Critical=`ErrorBadge12` (U+EDAE)、Success=`CheckmarkBadge12` (U+E65F)。Qt 实现使用这些 iconfont glyph，并通过 `QPainterPath::addText()` 的实际 path bounds 将 glyph 视觉居中到 base rect，避免 `drawText(..., AlignCenter, ...)` 的 font baseline 差异导致 warning/error 看起来不居中。

`severityIconSize` 默认 16，`severityIconBackgroundInset` 默认 1，对应 Figma 的 14px Base；`severityIconGlyphSize` 默认 12，对应 Figma 的 Badge12 glyph。`onThemeUpdated()` 刷新缓存，更新内部 label/button 字体颜色并重绘。

title/message 的 typography role 公开为 `titleFontRole` 与 `messageFontRole`；severity glyph 公开为四个 glyph 属性，允许在项目后续确认更准确的 Segoe Fluent Icons 字形后直接替换默认值。

### 5. Close 行为是状态变更，不销毁控件

点击 close button 调用 `setIsOpen(false)`，发出 `isOpenChanged(false)` 和 `closed()`。控件本身不 delete，不从父布局移除，便于外部通过 `setIsOpen(true)` 重新打开。`isOpen=false` 时 `setVisible(false)` 或等效隐藏；测试只要求 `isOpen`、visibility 和 size hint 语义一致。

### 6. 测试使用行为断言 + 渲染像素抽样 + VisualCheck

自动化测试覆盖默认值、信号、close click、severity 切换、icon/close/action 可见性、single-line/multi-line 尺寸、theme update、action widget ownership。必要时通过渲染 image 抽样确认 badge/stroke/background 非透明且 severity 切换改变关键像素。VisualCheck 用 `SKIP_VISUAL_TEST` 守卫展示 Figma 和 WinUI Gallery 关键组合。

## Risks / Trade-offs

- [Risk] Figma 中 InfoBar 使用组件实例与 token 叠加，导出尺寸和实际 WinUI 运行时资源可能有 1px 差异 → [Mitigation] 以 600x50、110、158、15/13/12/48 等稳定结构为测试基准，视觉细节通过 VisualCheck 微调内部常量，不扩大 API。
- [Risk] 外部 action widget 的 size policy 各异，可能挤压 message → [Mitigation] `InfoBar` 对 action 区设置明确最大/最小布局规则，多行模式优先换行 message，单行模式优先 elide message。
- [Risk] `isOpen=false` 同时调用 `hide()` 可能与外部布局显隐控制交互 → [Mitigation] setter 只维护组件自身状态和 visibility，文档/测试明确重新打开用 `setIsOpen(true)`。
- [Risk] 主题语义 token 未完全覆盖 Figma 的 InfoBar 专用背景色 → [Mitigation] 首版内部从现有 card/system token 派生颜色，必要时只调整 `updateThemeColors()`。

## Migration Plan

1. 新增 `InfoBar` 源码，源文件由现有 CMake glob 纳入 `fluent_qt_lib`。
2. 新增并注册 `test_info_bar`。
3. 构建并运行 `test_info_bar`，默认通过 `SKIP_VISUAL_TEST=1` 跳过 VisualCheck。
4. 按需运行 VisualCheck，对比 Figma Info Bar master 和 WinUI Gallery 示例。
5. 若视觉审查不通过，优先调整内部布局常量、颜色映射和 label wrapping，不变更公共 API。

## Open Questions

- 后续是否需要公开 custom icon 或 custom severity color API，取决于业务是否需要超出 WinUI 四种 severity 的品牌化状态消息。