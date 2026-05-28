## Context

`src/view/status_info/` 当前包含 `ToolTip` 和新近新增的 `ProgressRing`，组件遵循 Qt Widgets 自绘、`FluentElement` Design Token 访问和 GTest/VisualCheck 覆盖模式。缺口是 WinUI 3 的 `InfoBadge`：它不是全局通知 badge，也不是 `InfoBar` 横幅，而是一个可嵌入导航项、按钮、列表项或任意控件角落的轻量状态标记。

Figma Windows UI Kit 节点 `27739:129449` 将 Badge 变体拆为 `Beacon`、`Icon Only`、`Number`、`Numbers Wide`，尺寸事实为：beacon 约 `4x4`，普通 icon/number 为 `16x16`，宽数字为内容驱动的 pill（示例 `19x16`）。同一节点覆盖 Light/Dark、Informational、Attention、Caution、Success、Critical 状态。变量定义提供了 Light/Dark 语义色：Attention `#005FB7/#60CDFF`、Caution `#9D5D00/#FCE100`、Success `#0F7B0F/#6CCB5F`、Critical `#C42B1C/#FF99A4`，以及 accent/on-accent text。

WinUI Gallery 的 `InfoBadgePage` 展示四类行为：

- `NavigationViewItem.InfoBadge` 中设置 `Value="5"` 并用 opacity 切换可见性。
- `Attention/Informational/Success/Critical` 四组样式在 Icon/Value/Dot 三种形态间切换。
- `InfoBadge` 可放入另一个控件内部，例如按钮右上角使用自定义红色背景和 icon source。
- 动态 `ValueNumberBox` 将 `Value` 更新到 `DynamicInfoBadge`，并允许 `-1` 作为无数值 sentinel。

## Goals / Non-Goals

**Goals:**

- 新增 `InfoBadge : public QWidget, public FluentElement, public view::QMLPlus`，保持 `status_info` 模块轻量自绘风格。
- 支持 Dot/Beacon、Icon、Value 三类显示形态，并在 `Auto` 模式下根据 `value` 和 `iconGlyph` 自动选择。
- 按 Figma 尺寸绘制 `4x4` beacon、`16x16` icon/number badge，以及内容驱动的 wide number pill。
- 支持 Informational、Attention、Caution、Success、Critical 语义状态和 Light/Dark 主题响应。
- 支持动态 value、opacity、custom background/text colors，以覆盖 WinUI Gallery 的 NavigationView 和 button corner 示例。
- 提供聚焦自动化测试和 VisualCheck。

**Non-Goals:**

- 不实现 `NavigationView` 或自动把 badge 挂到导航项；`InfoBadge` 只提供可组合的 QWidget。
- 不实现 Windows app tile / taskbar badge notification API。
- 不实现复杂数据绑定、动画计数器或数字变化过渡。
- 不引入 SVG/Lottie/图片资源；icon 使用 Segoe Fluent Icons 字体 glyph。
- 不改变 `ToolTip`、`ProgressRing`、`InfoBar` 相关 change 或其它 status_info 控件 API。

## Decisions

### 1. 使用 `InfoBadge` 命名而不是泛化 `Badge`

用户请求中提到“Badge”，但研究来源和 WinUI 控件名是 `InfoBadge`。使用 `InfoBadge` 能明确区分它与系统通知徽章、应用图标 badge、`BadgeNotificationManager` 和未来可能的营销/标签类 badge。文件命名为 `InfoBadge.h/.cpp`，测试目标命名为 `test_info_badge`。

### 2. 显示模式使用 `Auto / Dot / Icon / Value`

`InfoBadgeDisplayMode::Auto` 为默认模式：`value >= 0` 时显示 value badge；否则 `iconGlyph` 非空时显示 icon badge；否则显示 dot/beacon。显式 `Dot`、`Icon`、`Value` 用于测试和业务强制形态。`value=-1` 作为无数值 sentinel，沿用 WinUI Gallery 动态 value 示例的最小值语义。

替代方案是把 Figma 的 `Number` 与 `Numbers Wide` 做成两个 public enum。它们本质上都是 Value 形态，宽度由文本内容自然决定；拆成两个 API 会让调用方承担不必要的布局判断。

### 3. 尺寸由内容形态计算，不暴露任意 diameter API

首版尺寸固定为 Figma 关键尺寸：dot/beacon `4x4`，icon/单字符/单数字 `16x16`，wide value 仍高 `16px`，宽度为 `max(16, textWidth + horizontalPadding)`。这样能稳定对齐 Figma，又避免业务传入任意尺寸后破坏 WinUI 风格。

如后续确有业务需要超大 badge，可另行扩展 `scale` 或 `badgeSize`，不放入首版。

### 4. 状态色从 Design Token 派生，允许自定义颜色覆盖

状态色映射：

- `Informational` → `themeColors().systemInfo`
- `Attention` → `themeColors().accentDefault` 或 Figma attention/accent 等价值
- `Caution` → `themeColors().systemCaution`
- `Success` → `themeColors().systemSuccess`
- `Critical` → `themeColors().systemCritical`

文字/icon 默认使用 `themeColors().textOnAccent`。当 `customBackgroundColor` 有效时优先使用自定义背景；当 `customTextColor` 有效时优先使用自定义前景。这样覆盖 WinUI Gallery 中 `Background="#C42B1C"` 的按钮角标示例，同时常规状态仍由 token 驱动。

### 5. 绘制全部在 `paintEvent()` 中完成

`InfoBadge` 直接自绘背景圆/胶囊、数字文本和 icon glyph。数值字体使用一个成员化 font role（如 `m_valueFontRole`）访问 token，避免在实现里硬编码 `themeFont("Caption")` 类字符串；icon 使用 Segoe Fluent Icons 字体。控件不使用 QSS/QPalette 作为视觉来源。

### 6. Opacity 只影响绘制，不改变布局

`badgeOpacity` 默认为 `1.0`。设置为 `0.0` 时 badge 不可见但保留 `sizeHint()`，对齐 WinUI Gallery 中 NavigationView 示例通过 opacity 隐藏/显示 InfoBadge 而不重排导航项的行为。值会 clamp 到 `[0.0, 1.0]`。

### 7. 不提供承载布局 API

InfoBadge 只是可嵌入子控件。按钮角标、导航行角标由调用方把 `InfoBadge` 放进自己的布局或 overlay 容器中。首版测试和 VisualCheck 会演示“按钮右上角”和“导航行右侧”组合方式，但不把这些组合做成组件 API。

## Risks / Trade-offs

- **[Risk] WinUI 的内部 InfoBadge 样式资源比首版 API 更细** → 首版用状态 enum + displayMode 覆盖 Gallery 主要行为；复杂 resource-style 替换可后续增加样式对象或属性组。
- **[Risk] 数字文本在 16px badge 中容易挤压** → 使用内容驱动宽度，单字符维持 16px，双位数自动扩展为 wide pill。
- **[Risk] 自定义背景色可能与默认文字色对比不足** → 提供 `customTextColor`，默认仍使用 `textOnAccent`；测试覆盖自定义背景和前景。
- **[Risk] `badgeOpacity=0` 保留布局可能让某些业务误以为控件隐藏** → 文档和测试明确它是绘制透明，不等同 `setVisible(false)`。

## Migration Plan

1. 新增 `InfoBadge` 源码并由现有 source glob 纳入 `fluent_qt_lib`。
2. 新增并注册 `test_info_badge`。
3. 增量验证：构建并运行 `test_info_badge`，VisualCheck 按需单独运行。
4. 若视觉审查发现数字/圆角与 Figma 偏差，优先调整内部 padding/font/radius，不扩大 public API。

## Open Questions

- `Informational` 是使用 `systemInfo` 还是中性灰 `Solid Neutral` 更贴近 Figma 的 “Informational Safe” 变体？首版以项目现有 `systemInfo` 为准，后续可根据 VisualCheck 调整或新增 `Neutral` 状态。