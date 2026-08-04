# PySide6 兼容性路线图

[English](ROADMAP.md)

本路线图按照风险由低到高，逐步扩展 FluentQt 的 Python API。Python 源代码
可以跨平台，并不代表原生 `_fluentqt` 扩展天然跨平台：每个受支持的操作系统、
CPU 架构、Qt 运行时和 CPython ABI 都需要独立构建和验证。

## 兼容性契约

- C++ 组件库继续支持 Qt 5.15+ 和 Qt 6.2+。
- 可选 PySide6 target 支持 Qt/PySide6/Shiboken6 6.2+，并保持默认关闭。
- PySide6、Shiboken6 runtime、Shiboken6 generator 和 C++ Qt SDK 必须使用
  相同版本。
- 所有 Python 分类模块都从同一个原生 `_fluentqt` 模块重新导出，避免重复创建
  主题、资源、Qt 对象身份和其他进程级状态。
- 导入 `fluentqt` 时不得创建 `QApplication`、初始化资源或修改当前主题。
- 本路线图不包含 PySide2/Shiboken2 兼容。

## 当前状态

| 里程碑 | 状态 | 交付内容 |
|---|---|---|
| M0 — 绑定基础设施 | 已完成 | 可选 CMake target、Qt 6.2 生成路径、版本门禁、单一原生模块、启动 API 和测试 |
| M1 — 核心控件 API | 已完成 | Basic Input、Text Fields、Window、主题/字体 API、ownership 和 `nativeEvent` 契约 |
| M2 — 低风险控件覆盖 | 已完成 | 所有计划叶子控件均已绑定或记录明确边界，并具备属性、信号、示例、manifest 检查和 wheel smoke 验证 |
| M3 — 托管控件 ownership | 已完成 | 计划内托管控件边界均具备固定语义适配器、ownership 与 GC 测试 |
| M4 — 模型与导航 | 已完成 | 计划内 model/navigation 组件已覆盖 Python model/delegate、虚函数分派、选择与生命周期 |
| M5 — Overlay 与原生窗口 | 进行中 | 本机 Windows 11 DWM 材质/布局及指针驱动 move/resize 验收已通过，自动化 XCB/Wayland/Windows/Cocoa 验收也已通过；仅剩实体 KWin/Wayland compositor 审查 |
| M6 — 可发布 Python 分发 | 进行中 | 已实现类型/API 治理、六目标 wheel 矩阵、分架构 manylinux repair/audit 路径，以及覆盖全部 88 个原生路由、199 个 SampleCard、除有意保留的 Python/C++ API 文本外像素一致的 wheel 内置 Gallery；仍需完整容器 CI 证据、签名和发布 |

## 公共 API 覆盖台账

下表是组件覆盖的事实来源。不能因为当前小批次的复选框全部完成，就把整个里程碑
标记完成；每个公开组件都必须完成绑定，或保留明确的边界决策。M0 至 M4 已完成该
审计。当前 manifest 记录了 77 个必需类、值类型及支持类型、11 个枚举、14 个函数和 2 个版本变量；
M5、M6 的剩余边界继续在下表及各自章节中记录。

| 分类 | 已绑定 | 剩余边界 |
|---|---|---|
| Basic Input | `Button`、`CheckBox`、`ColorPicker`、`ComboBox`、`CompoundButton`、`DropDownButton`、`HyperlinkButton`、`RadioButton`、`RatingControl`、`RepeatButton`、`Slider`、`SplitButton`、`ToggleButton`、`ToggleSplitButton`、`ToggleSwitch` | — |
| Collections | `DrawerView`、`FlipView`、`FlowView`、`GridView`、`ListView`、`SplitView`、`SplitViewPaneOptions`、`StackView`、`TreeView` | 当前公开组件与支持类型已覆盖完整 |
| Date & Time | `CalendarDatePicker`、`CalendarView`、`DatePicker`、`TimePicker` | 当前公开组件已覆盖完整 |
| Dialogs & Flyouts | `CoachMark`、`ContentDialog`、`Dialog`、`Flyout`、`Popup`、`TeachingTip` | 当前公开组件已覆盖完整 |
| Foundation | `FontIcon`、主题/字体包级 API、ownership 枚举 | `FluentElement`、`QMLPlus`、registry 与 overlay helper 保持实现层能力，不直接作为 Python mixin 发布 |
| Layout | `Accordion`、`Card`、`Divider`、`Expander` | 当前公开组件已覆盖完整 |
| Menus & Toolbars | `CommandBar`、`CommandBarFlyout`、`FluentMenu`、`FluentMenuBar`、`FluentMenuItem` | 当前公开组件已覆盖完整；原生 CI 验证已通过 |
| Navigation | `Breadcrumb`、`BreadcrumbItem`、`NavigationView`、`Pivot`、`PivotItem`、`SelectorBar`、`SelectorBarItem`、`StackContentHost`、`TabView`、`TabViewItem` | 当前公开组件已覆盖完整 |
| Scrolling | `AnnotatedScrollBar`、`AnnotatedScrollBarLabel`、`PipsPager`、`ScrollBar`、`ScrollView`、`ScrollViewZoomAwareWidget` | 当前公开组件与支持类型已覆盖完整 |
| Status & Info | `Avatar`、`InfoBadge`、`InfoBar`、`ProgressBar`、`ProgressRing`、`Shimmer`、`Toast`、`ToolTip` | 当前公开组件已覆盖完整；原生 CI 验证已通过 |
| Text Fields | `AutoSuggestBox`、`EditingCommandRouter`、`Label`、`LineEdit`、`NumberBox`、`PasswordBox`、`TextEdit` | 当前公开组件与支持类型已覆盖完整 |
| Windowing | `Window`、`TitleBar` 与 backdrop 值类型 | Windows 11 DWM 材质/布局及指针驱动 system move/resize 审查已完成；M5 仍需实体 KWin/Wayland compositor 行为审查 |

## M0 — 绑定基础设施

- [x] `FLUENT_QT_BUILD_PYSIDE6_BINDINGS` 为可选开关，并且只支持 Qt 6。
- [x] 生成路径兼容 Shiboken 6.2+，不强制依赖新版 Shiboken CMake helper。
- [x] 配置阶段拒绝 Qt、PySide6、Shiboken6 runtime 和 Shiboken6 generator
      版本不一致的工具链。
- [x] Python 包提供 `QApplication` 创建前、创建后的显式初始化 API。
- [x] 对生成的 `Window.nativeEvent()` 做目标函数级检查，不使用宽泛的整个
      wrapper 文件匹配。
- [x] Linux、Windows 和 macOS 原生 CI lane 均已定义 wheel 构建和 smoke
      测试。

## M1 — 核心控件 API

目前已经实现的核心 API 包含：

- Basic Input：`Button`、`CheckBox`、`RadioButton`、`Slider`、
  `ToggleButton` 和 `ToggleSwitch`。
- Text Fields：`Label`、`LineEdit`、`NumberBox` 和 `PasswordBox`。
- Windowing：`Window`、`TitleBar`、backdrop 枚举和 backdrop value type。
- Foundation：Light/Dark 主题、设计语言预设、accent 色、字体角色、字体缩放
  和构建信息。

该里程碑的合入门槛包括 Python 子类分派、Qt 属性与信号、`Window` 子控件
ownership、安全的双参数 `nativeEvent()` 契约、API manifest 检查和干净环境
wheel smoke 测试。

## M2 — 低风险控件覆盖

已完成范围：

- [x] 通过 `fluentqt.status_info` 分类增加 `ProgressBar` 和
      `ProgressRing`。
- [x] 覆盖范围/数值属性、组件枚举、信号、分类重新导出、API manifest、
      示例和已安装 wheel smoke 测试。
- [x] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认当前批次。
- [x] 增加 `RepeatButton`、`HyperlinkButton` 和 `Divider`，覆盖属性、
      信号、分类导出、manifest、wheel smoke 和可运行验收窗口。
- [x] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认第二批叶子控件。
- [x] 增加 `InfoBadge` 和 `Shimmer` 内置模板，覆盖属性、信号、分类导出、
      manifest、wheel smoke 和确定性验收截图。
- [x] 在稳定的 Python value type 契约设计完成前，不公开
      `ShimmerPainter::Element` 集合。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地第三批验证，包括原生组件测试和
      干净 wheel 运行检查。
- [x] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认第三批控件。
- [x] 将 `Avatar`、`RatingControl` 和 `ScrollBar` 审计为第四批叶子控件；
      它们不跨越 model、overlay、平台窗口或托管子控件 ownership 边界。
- [x] 为第四批补齐分类导出、嵌套枚举、属性/信号覆盖、manifest 检查、已安装
      wheel smoke 和可见验收窗口。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成第四批本地验证，包括生成 wrapper
      编译、全部 PySide 测试、干净安装 wheel、35 个原生组件测试和截图审查。
- [x] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认第四批叶子控件；
      原生 CI 验证已通过生成契约检查、完整绑定测试、可迁移 wheel
      和干净环境 smoke。
- [x] 单独审计 `PipsPager`，并复现生成器会把仅供动画使用的
      `selectedVisualOffset` 和 `visibleWindowOffset` 泄漏成 Python
      构造参数的问题。
- [x] 将两个内部动画改为 `QVariantAnimation` 回调，在保持 C++ 动效不变的
      同时，从 Qt 元对象和生成的 Python API 中移除实现偏移量。
- [x] 补齐 `PipsPager` 分类导出、枚举、属性/信号/导航测试、manifest、
      生成代码隐私契约、wheel smoke 和可见验收覆盖。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成第五批本地验证：17 个原生专项测试、
      全部 15 个绑定 CTest、29 个契约验证器测试、干净安装 wheel、依赖路径
      检查和截图审查均已通过。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `PipsPager`；原生 CI 验证已通过生成/编译、契约检查、
      完整绑定测试、可迁移 wheel、干净虚拟环境 smoke 和 C++ 回归门禁。
- [x] 将 `TextEdit` 审计并接入为下一项叶子控件。Python API 包含纯文本编辑、
      可见行布局参数、样式属性/信号、滚动链以及现有 Qt-owned Fluent
      `ScrollBar` 的版本稳定 getter，不暴露私有 `QTextEdit`。
- [x] 统一不同生成器的 `verticalScrollBar()` API：Shiboken 6.2 会静默忽略
      这个跨命名空间指针返回值，因此 typesystem 统一移除不稳定 wrapper，再由
      Python 模块提供不改变 parent 或 ownership 的同名 getter。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `TextEdit` 批次验证：19 个原生
      专项测试、生成 wrapper 编译、全部 16 个绑定 CTest、新建干净环境 wheel
      smoke、依赖路径检查和可见截图审查均已通过。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `TextEdit` 批次。原生 CI 验证已通过生成/编译、生成代码
      契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收截图和 C++
      回归门禁。
- [x] 将 `CompoundButton` 审计并接入为下一项 M2 叶子控件。它只在已绑定的
      `Button` 上增加一个次级文本属性，不跨越 model、overlay、平台窗口或
      托管控件边界。
- [x] 覆盖全部构造重载、原生 `Button` 继承、`secondaryText` 及其重复安全
      信号、无障碍说明同步、mixin 隔离、分类导出、API manifest、已安装 wheel
      smoke 和可见验收渲染。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `CompoundButton` 批次验证：
      5 个原生专项测试、生成 wrapper 编译、全部 16 个绑定 CTest、新建干净
      环境 wheel smoke、依赖路径检查和可见截图审查均已通过。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `CompoundButton` 批次。原生 CI 验证已通过生成/编译、
      生成代码契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收
      截图和 C++ 回归门禁。
- [x] 将 `FontIcon` 审计并接入为下一项 M2 foundation 叶子控件。它仍是
      原生、主题感知的 `QWidget`，接收稳定的上游目录 key，不跨越 ownership、
      model、overlay 或平台边界。
- [x] 覆盖默认/字形构造函数、四个属性及其重复安全信号、目录 key 的光学尺寸
      渲染、mixin 隔离、foundation/根模块导出、API manifest、已安装 wheel
      smoke 和可见验收图标。示例有意使用
      `ic_fluent_settings_20_regular`；`Settings` 这样的显示文本不是目录 key。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `FontIcon` 批次验证：3 个原生
      专项测试、生成 wrapper 编译、全部 16 个绑定 CTest、新建干净环境 wheel
      smoke、依赖路径检查和可见截图审查均已通过。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `FontIcon` 批次。原生 CI 验证已通过生成/编译、生成代码
      契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收截图和 C++
      回归门禁。
- [x] 将 `ColorPicker` 审计并接入为下一项 M2 叶子控件。它仍是使用 `QColor`
      和透明度开关表达值语义的原生 `QWidget`，不跨越 ownership、model、
      overlay 或平台窗口边界。
- [x] 仅发布预期的 `color`/`alphaEnabled` 属性和信号，明确隐藏 7 个光谱及
      通道实现辅助方法；同时覆盖根模块/分类导出、API manifest、重复安全信号、
      已安装 wheel smoke 和独立的可见验收示例。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `ColorPicker` 批次验证：原生
      专项契约、生成 wrapper 编译、全部 17 个绑定 CTest、新建干净环境 wheel
      smoke、依赖路径检查和可见截图审查均已通过。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `ColorPicker` 批次。原生 CI 验证已通过生成/编译、生成代码
      契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收截图、C++
      回归门禁和最终 CI Gate。
- [x] 将 `CalendarView` 审计并接入为下一项 M2 叶子控件。它只交换 Qt 日期、
      locale、枚举和几何值，不跨越 ownership、model、overlay 或平台窗口边界。
- [x] 覆盖日期范围与钳制、重复安全属性信号、嵌套内容层级枚举、日期命中测试、
      mixin 隔离、新增的 `fluentqt.date_time` 分类、根模块导出、API manifest、
      已安装 wheel smoke 和独立的可见验收示例。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `CalendarView` 批次验证：46 个
      自动化原生测试通过，交互 VisualCheck 按设计跳过；生成 wrapper 编译、
      全部 18 个绑定 CTest、新建干净环境 wheel、依赖路径、已安装包直接导入和
      可见截图审查均已通过。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `CalendarView` 批次。原生 CI 验证已通过生成/编译、生成代码
      契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收截图、C++
      回归门禁和最终 CI Gate。
- [x] 将 `AnnotatedScrollBar` 审计并接入为下一项 M2 控件。Python API 包含
      可变的 `AnnotatedScrollBarLabel` 值类型、range 与布局属性、标签/查询
      方法、静态详情文本、交互信号，以及与 borrowed `ScrollView` 的原生双向
      同步；该控件既不托管也不拥有所连接的 view。
- [x] 在 Python 分类模块中统一 `AnnotatedScrollBarLabel` 的值比较，因为
      Shiboken 6.2 不会发布新版生成器可识别的命名空间级 C++ 比较运算符。
      该可变值类型保持不可 hash，并由 build-tree 与已安装 wheel 测试共同锁定
      相同行为。
- [x] 在能为 Shiboken 6.2+ 提供保持同步返回语义的 Python callable 适配器前，
      不公开 C++ `std::function<QString(int)>` 详情 provider。生成代码契约会
      拒绝这组不完整 provider API，也会拒绝 borrowed ScrollView 链接上的
      parent、ownership 或 keep-reference 记账。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `AnnotatedScrollBar` 批次验证：
      11 个自动化原生测试通过，交互 VisualCheck 按设计跳过；全部 20 个绑定
      CTest 与 36 个契约验证器测试通过；新建干净虚拟环境也通过 wheel 安装、
      依赖路径、`pip check`、运行时 smoke 和截图审查。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `AnnotatedScrollBar` 批次。原生 CI 验证已通过生成/编译、
      生成代码契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收
      截图、C++ 回归门禁和最终 CI Gate。

M2 已对当前公开组件集闭环。未来新增叶子控件仍需先进行 API 审计；只有符合以下
条件的组件才属于这一类工作：

- 是不包含运行时 ownership 模式的叶子 `QWidget`；
- 不暴露 model/delegate 契约；
- 不创建 popup 或同窗口 overlay；
- 不依赖平台原生窗口行为；
- 不需要 Python 专用 façade 即可保持现有 C++ 语义。

每个批次都必须补齐生成 wrapper 输入、分类模块重新导出、
`api-manifest.json`、属性/信号测试、已安装 wheel smoke 测试；有可见行为的
控件还必须提供可运行示例。

## M3 — 托管控件 ownership

已完成范围包括 `ScrollView`、`Expander`、`InfoBar`、`Accordion`、
`StackView`、`FlipView`、`SplitView`、`NavigationView`/
`StackContentHost` 和 `DrawerView`。`TabView` 的应用页面由调用方管理，
因此其 metadata/navigation 契约归入 M4，而不是托管控件边界。

已完成 ownership 批次：

- [x] 选择 `ScrollView` 作为首个 ownership 宿主。
- [x] 隐藏依赖运行时参数的
      `setContentWidget(QWidget*, WidgetOwnership)` 重载，并公开语义固定的
      `setOwnedContentWidget()`、`setBorrowedContentWidget()` 和
      `setReparentedContentWidget()` facade。
- [x] 在本地验证三种模式的替换、`None`、宿主销毁、显式 take、原父对象
      恢复、Python 子类身份以及重复 GC/析构。
- [x] 检查生成的私有适配入口不会隐式修改 Shiboken ownership、parent 或
      keep-reference 表。
- [x] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认完整 `ScrollView`
      ownership facade 与干净 wheel。
- [x] 将 `Expander` 审计为第二个 ownership 宿主，并绑定其公共基类 `Card`，
      但不暴露内部 header 控件。
- [x] 为 `Expander` 公开语义固定的 owned、borrowed、reparented 方法，同时
      保留 `setContentWidget()` 和构造参数 `contentWidget=` 与 C++ 一致的
      borrowed 默认语义。
- [x] 在本地验证 `Expander` 的替换、`None`、take、宿主销毁、原 parent
      保活/恢复、Python 子类身份、重复自然 GC、生成代码契约、干净安装 wheel
      以及可见兼容展示。
- [x] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认完整
      `Card`/`Expander` 批次与干净 wheel。
- [x] 将 `InfoBar` 审计为第三个 ownership 宿主；其 action 在挂载期间进入
      InfoBar 的 Qt 父子链，替换或清空时释放为无父控件。
- [x] 将 C++ action 裸指针改为可观察指针，并在 action 被外部销毁时清空属性、
      更新布局并发送 `actionWidgetChanged(nullptr)`，避免悬空访问。
- [x] 增加 Python `InfoBar` facade，截获 `actionWidget=` 构造参数、保留 Python
      子类身份、拒绝宿主/祖先循环，并提供返回 Python ownership 的
      `takeActionWidget()`。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地验证：14 个 InfoBar 原生专项测试、
      全部 16 个绑定 CTest、33 个契约验证器测试、干净安装 wheel、动态依赖路径
      检查和可见截图审查均已通过。
- [x] 在原生 CI 验证的原生 Linux/Windows Qt 6.2.4 绑定 lane 和
      Qt 5.15 C++ 兼容 lane 确认 `InfoBar` ownership 批次，包括生成代码契约、
      运行时测试、可迁移 wheel、干净环境 smoke 和验收截图。
- [x] 将 `Accordion` 审计为第四个 ownership 宿主。它组合已经绑定的
      `Expander` 条目并保留 C++ 的 Borrowed 默认语义，同时让每个条目的 Owned
      和 Reparented 模式保持显式。
- [x] 从原生公开面移除运行时 ownership 重载，发布固定语义的
      `addOwnedItem()`、`addBorrowedItem()`、`addReparentedItem()` 及对应
      insert 方法。Python facade 不借助 Shiboken parent/keep-reference 表，
      也能保留条目子类和原 parent。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `Accordion` 批次验证：全部 6 个
      Accordion 原生测试、24 个绑定 CTest 和 41 个契约验证器测试通过；新建
      干净虚拟环境也通过 wheel 安装、`pip check`、依赖路径、运行时 smoke、
      GC 压力和可见截图审查。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `Accordion` ownership 批次。原生 CI 验证已通过生成与
      编译、全部 41 项生成代码契约、24 个绑定 CTest、可迁移 wheel、干净环境
      smoke、验收截图、C++ 回归门禁、源码包验证和最终 CI Gate。
- [x] 将 `StackView` 审计为第五个 ownership 宿主，并在不引入
      model/delegate 契约的前提下完成导航边界设计。保留原生
      push/pop/replace/clear 转场、状态信号、键盘返回导航和 indexed 查询。
- [x] 发布固定语义的 Owned、Borrowed、Reparented initial/push/批量 push/
      replace 方法；从原生公开面移除默认策略和指针 current wrapper，阻止继承
      `QStackedWidget` 的插入/移除绕行，并保活页面子类与恢复目标直到原生转场
      完成。
- [x] 使用 macOS Qt/PySide6 6.9.3 完成本地 `StackView` 批次验证：22 项
      自动化原生测试通过，交互 VisualCheck 按设计跳过；全部 28 个绑定 CTest
      和 44 个契约验证器测试通过。新建干净虚拟环境也通过 wheel 安装、
      `pip check`、依赖路径检查、运行时 smoke、GC 压力、源码包验证和可见截图
      审查。
- [x] 在原生 Linux/Windows Qt 6.2.4 绑定 lane 和 Qt 5.15 C++ 兼容 lane
      确认 `StackView` ownership/导航批次。原生 CI 验证已通过生成
      与编译、全部生成代码契约、28 个绑定 CTest、Qt 运行库路径校验、可迁移
      wheel、干净环境 smoke、验收截图、源码包检查、C++ 回归 lane 和最终
      CI Gate。
- [x] 将 `FlipView` 审计为第六个 ownership 宿主。在保留传统
      `addPage()` 默认由宿主拥有的同时，为每个页面增加显式 Owned、Borrowed、
      Reparented 安装/释放、`takePage()` 转移、原 parent 恢复以及 C++ 外部析构
      清理。
- [x] Python 仅发布语义固定的 add/insert 方法，在 facade 中保活页面子类与
      恢复目标，并隐藏旧式转移重载及运行时 ownership 参数。生成代码检查要求
      私有适配器不得隐式改变 Shiboken parent/reference 记账，并要求
      `takePage()` 返回 Python ownership。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地 `FlipView` 批次验证：
      30 项自动化原生测试通过，1 项人工 VisualCheck 按设计跳过；全部 39 个
      绑定 CTest、119 个 Python 绑定测试和 87 个契约验证器测试通过。后续
      item-view 析构强化将当前规模扩展到 43 个绑定 CTest 和 123 个 Python
      绑定测试；新建干净
      环境也通过 wheel 安装、`pip check`、已加载依赖路径检查、运行时 smoke、
      3 项 GC 压力、源码包集成构建和可见截图审查。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 `FlipView` ownership/导航批次，包括 Qt 5.15/6.2 C++
      回归、可迁移干净 wheel、源码包集成、验收截图和最终 CI Gate。Windows
      lane 还通过了四类 item view 各 25 轮的无 `close()` model/delegate/
      selection GC 压力，以及此前失败位置对应的已安装 wheel 完整 smoke。
- [x] 将 `SplitView` 审计为第七个 ownership 宿主。保留传统 add/insert
      默认由宿主拥有以及 C++ remove 转移语义，同时增加逐 pane 显式释放策略、
      `takePaneAt()` 转移、原 parent 恢复和外部析构清理。
- [x] 发布固定语义的 Owned、Borrowed、Reparented add/insert 方法以及可变的
      `SplitViewPaneOptions` 值类型。facade 在 remove 时应用已记录策略，保活
      pane 子类与恢复目标，并隐藏运行时 ownership 参数和传统转移 remove。
      生成代码检查要求私有适配器不得隐式修改 Shiboken parent/reference
      记账，并要求 `takePaneAt()` 返回 Python ownership。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地 `SplitView` 批次验证：
      16 项自动化原生测试通过，1 项人工 VisualCheck 按设计跳过；全部 47 个
      绑定 CTest、131 个 Python 绑定测试和 92 个契约验证器测试通过。新建
      干净环境也通过 wheel 安装、`pip check`、依赖路径检查、运行时 smoke、
      3 项 GC 压力、源码包集成和可见截图审查。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 `SplitView` ownership 批次，包括生成代码契约、全部
      47 个绑定 CTest、可迁移干净 wheel、源码包集成、验收截图、Qt 5.15/6.2
      C++ 回归和最终 CI Gate。
- [x] 将 `NavigationView` 及其 C++ 所有的 `StackContentHost` 审计为第八个
      托管控件边界。在保留传统 C++ 转移行为的同时，为页面和 header/main/footer
      chrome 增加显式释放策略、take 操作、原 parent 恢复、重复/祖先拒绝以及外部
      析构清理。
- [x] 通过 `fluentqt.navigation` 发布固定语义的 Owned、Borrowed、Reparented
      页面/chrome 方法。内部 content host 与直接构造的 `StackContentHost` 使用
      同一套 Python facade；无需生成器侧 parent 或 keep-reference 记账即可保活
      Python 子类与恢复目标。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地确认：24 项自动化
      NavigationView 原生测试通过，1 项人工 VisualCheck 按设计跳过；全部 54 个
      绑定 CTest、144 个 Python 绑定测试和 100 个契约验证器测试通过。新建干净
      环境也通过 wheel 安装、`pip check`、已加载依赖路径检查、运行时 smoke、
      6 项隔离 GC 压力、源码包集成和可见截图审查。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 `NavigationView`/`StackContentHost` 批次，包括全部
      54 个绑定 CTest、生成代码契约、Qt 5.15/6.2 C++ 回归、可迁移干净
      wheel、源码包集成、验收截图和最终 CI Gate。
- [x] 将 `DrawerView` 审计为第九个托管控件边界，也是首个 M5
      same-window overlay 批次。保留 C++ 的 Borrowed 默认语义，公开固定的
      Owned、Borrowed、Reparented 内容方法和 `takeContentWidget()`，并覆盖
      `CloseFlag`、scrim、outside press、Escape、开关生命周期和 Python 虚函数分派。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地确认：DrawerView 原生
      24 项中 22 项通过，2 项桌面/人工测试按设计跳过；全部 58 个绑定 CTest、
      152 个 Python 绑定测试和 104 个契约验证器测试通过。新建的
      `.venv-pyside69-drawer-wheel` 也通过 wheel 安装、`pip check`、已加载依赖
      路径、完整 smoke、3 项隔离 GC 压力、源码包重新生成/编译和截图审查。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 `DrawerView` 批次，包括生成代码契约、全部绑定测试、
      三平台可迁移干净 wheel、验收截图、源码包集成、Qt 5.15/6.2 C++ 回归
      和最终 CI Gate。

`ScrollView` 的支持契约以普通 Python GC 和 Qt parent 析构为准。托管期间的
`Shiboken.ownedByPython()` 标志会随 Shiboken 版本变化，不属于公共 API；
PySide6 6.2.4/Windows 下反复显式 `Shiboken.delete(host)` 甚至会让原生
`QScrollArea` wrapper fast-fail，因此不把这种低层调试操作当作兼容性门槛。

`Expander` 使用相同的自然 GC 契约，但保留 C++ 默认行为：普通
`setContentWidget()` 为 borrowed。Python facade 会截获构造参数
`contentWidget=`，避免绕过已审计的 ownership 路径。

`Accordion` 保留相同的逐条目 C++ 策略。Python facade 会保活托管条目的
wrapper 与 Reparented 恢复目标，在释放恢复目标前先同步 Shiboken parent
记账，并在 Qt 从外部销毁条目时移除记录。只有 `takeItem()` 会将条目以无父、
Python-owned 状态转回调用方。

`StackView` 的普通 `push()`、`replace()` 和 `setInitialItem()` 保留 C++
Owned 默认语义，其余页面策略都通过显式方法固定。Borrowed 与 Reparented
wrapper 在转场仍引用页面时继续保活，仅在原生清理完成后释放。继承自
`QStackedWidget` 的直接插入/移除会绕过导航栈，因此不属于 Python 契约。
`setCurrentWidget()` 通过只传 index 的适配路径继续公开，避免 Shiboken 根据
指针参数名称推断新的 QObject parent。

`FlipView` 的普通 `addPage()` 与 `insertPage()` 保留 C++ 的 Owned 默认语义，
其余策略由显式方法固定。`removePage()` 应用已记录的策略，`takePage()` 始终
返回无父、Python-owned 页面。facade 保活 Python 子类与 Reparented 恢复目标；
页面被外部销毁时，C++ 宿主和 facade 都会移除对应记录。

`SplitView` 的普通 `addPane()` 与 `insertPane()` 保留 C++ 的 Owned 默认语义。
Python 公开固定语义的 Owned、Borrowed、Reparented 入口，
`removePane()`/`removePaneAt()` 应用已记录策略，`takePaneAt()` 专用于无条件转移
无父 pane。facade 保活 Python 子类与 Reparented 恢复目标；pane 被外部销毁时，
原生层与 facade 都会清理对应记录。

`DrawerView` 保留 C++ 普通 `setContentWidget()` 的 Borrowed 默认语义，Python
同时公开固定语义的 Owned、Borrowed、Reparented 入口以及
`takeContentWidget()`。facade 保活托管 wrapper 与 Reparented 恢复目标，并在
替换、外部析构和显式转移时清理记录。宿主自身及其祖先不能作为内容控件；切换
同一控件的 ownership 模式前必须先显式取回，避免隐式改写生命周期契约。

`InfoBar` 使用更窄的现有 C++ 契约：当前 action 随宿主销毁，但替换、清空或
`takeActionWidget()` 会将其释放为无父、Python-owned 控件。Python facade
负责 wrapper 保活；生成代码不得修改 Shiboken ownership、parent 或
keep-reference 表。
action 的外部析构通过 Qt 支持的 deferred-delete 路径验证。对仍位于 parent
链中的 Python 子类直接调用 `Shiboken.delete()`，可能在 PySide6
6.2.4/Windows 中于 Qt 完成 destroyed 信号链前触发 fast-fail，因此这种低层
wrapper 操作不属于兼容性要求。

每个 API 暴露给 Python 前必须：

- 当静态 Shiboken ownership 规则无法描述依赖运行时参数的契约时，改用语义
  明确的 Python 方法；
- 验证 owned、borrowed、reparented、replaced、taken 和 `None` 转换；
- 重复执行创建、接管、释放、删除和 `gc.collect()`，检查双重析构、提前析构
  和失效 wrapper；
- 在 C++ 持有对象时保留 Python 子类状态。

## M4 — 模型与导航

本里程碑先接入公共契约中不包含 model 或页面 ownership 的导航组件；完成
Python 边界设计后，也已完成依赖 model 的集合组件 `ListView`、`GridView`、
`TreeView` 和 `FlowView`。
验证范围必须包括：

- `QAbstractItemModel` 和 delegate 生命周期；
- Python 虚函数覆盖和 `super()` 分派；
- 选择、reset、行插入/删除和 persistent index 行为；
- 从 Python 和 C++ 两侧替换、销毁 model；
- 键盘、焦点、RTL 和与可访问性相关的导航行为。

已完成范围：

- [x] 审计 `TabView`：它只拥有 `TabViewItem` 元数据、选择状态和导航行为，
      应用页面继续由调用方组合并管理。
- [x] 通过 `fluentqt.navigation` 绑定 `TabView` 和可变 `TabViewItem`，
      支持 QVariant 兼容的 Python 元数据与稳定值比较，同时保持内部
      `TabStrip` 私有。
- [x] 覆盖构造、元数据修改、属性、信号、关闭、重排、键盘快捷键、RTL、
      Python 虚函数覆盖与 `super()`、外部 `QStackedWidget` 页面宿主、
      API manifest、生成代码契约、wheel smoke 和可见验收示例。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地验证：9 项自动化
      TabView 原生测试通过，2 项桌面/人工用例按设计跳过；全部 29 个绑定
      CTest、75 个 Python 绑定测试和 49 个契约验证器测试通过。新建虚拟环境
      也通过 wheel 安装、`pip check`、依赖路径检查、运行时 smoke、源码包
      集成构建和可见截图审查。
- [x] 原生 CI 验证已完成生成代码、测试、源码包和干净 wheel
      验证：原生 Linux/Windows Qt 6.2.4 与 macOS Qt 6.9.3 绑定 lane
      全部通过生成、编译、契约检查、绑定测试、可迁移 wheel 构建和干净环境
      安装；Qt 5.15、Qt 6.2 C++ 集成 lane 与最终 CI Gate 也全部通过。
- [x] 审计 `ListView`：model、selection model 和 delegate 继续由调用方
      所有，但安装期间必须保留对应 Python wrapper。自定义 header/footer
      QWidget 托管以及 section 开关/同步 `std::function` 回调在具备明确
      Python 契约前继续保持私有。
- [x] 通过 `fluentqt.collections` 绑定 `ListView`，包括兼容 Qt 6.2 的
      `SelectionMode` 适配器、model/selection retention、delegate wrapper
      retention、由显式绑定适配器实现且保持原 Python 方法名的内部滚动条
      getter，以及纯文本 header/footer 便捷 API。
- [x] 覆盖 Python `QAbstractListModel` 的 insert/remove/reset 通知、
      persistent index、自定义 `QItemSelectionModel`、Python delegate 与
      view 虚函数分派、替换/析构生命周期、API manifest、生成代码契约、
      wheel smoke 和可见验收示例。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成整个批次的本地确认：
      101 项 ListView 原生测试通过，1 项人工 VisualCheck 按设计跳过；全部
      30 个绑定 CTest、81 个 Python 绑定测试和 58 个契约验证器测试通过。
      新建虚拟环境也通过 wheel 安装、`pip check`、依赖路径检查、运行时
      smoke、源码包集成构建和可见截图审查。
- [x] 原生 CI 验证已确认原生 Linux/Windows Qt 6.2.4、macOS
      Qt 6.9.3、Qt 5.15/6.2 C++ 回归、三平台可迁移干净 wheel 与最终
      CI Gate。该轮还实际覆盖了 Shiboken 6.2 无法发现跨命名空间滚动条
      成员 getter 的兼容分支。
- [x] 审计 `GridView`：普通 model、selection model 和 delegate 继续由
      调用方所有。原生拖拽重排明确只支持 `QStandardItemModel`；其他
      `QAbstractItemModel` 仍支持展示、选择和通知，但不虚构重排能力。
- [x] 通过 `fluentqt.collections` 绑定 `GridView`，复用稳定的 Qt 6.2
      `SelectionMode` 转换器，并暴露原生 cell 尺寸、选择、滚动行为、
      header/placeholder 文本、重排信号、delegate 虚函数分派和 borrowed
      内部滚动条适配器。
- [x] 覆盖 model insert/remove/reset 与 persistent index、调用方所有的
      model/selection/delegate 生命周期、外部析构、Python delegate/view
      虚函数分派、键盘/RTL/可访问性行为、API manifest、生成代码契约、
      已安装 wheel smoke 和可见的 `QStandardItemModel` 组拖拽示例。
- [x] 加固 Windows Shiboken 6.2 的 retained delegate 路径：Python facade
      返回 delegate 前先验证 wrapper，并在 Python `destroyed` 回调漏触发时
      丢弃失效引用；单元测试和已安装 wheel smoke 都强制覆盖该兼容分支。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地确认：66 项 GridView
      原生测试中 56 项通过，10 项桌面/人工用例按设计跳过；全部 31 个绑定
      CTest、88 个 Python 绑定测试和 61 个契约验证器测试通过。新建干净
      虚拟环境也通过 wheel 安装、`pip check`、依赖路径检查、运行时 smoke、
      源码包集成构建和可见截图审查。
- [x] 原生 CI 验证已确认原生 Linux/Windows Qt 6.2.4、macOS
      Qt 6.9.3、Qt 5.15/6.2 C++ 回归、三平台可迁移干净 wheel、验收截图
      与最终 CI Gate。
- [x] 审计 `TreeView`：层级 model、selection model 和 delegate 继续由
      调用方所有，安装期间由 facade 保活 Python wrapper。原生拖拽重排只对
      `QStandardItemModel` 承诺支持；实现细节型 `SelectionIndicatorStyle`
      保持私有。
- [x] 通过 `fluentqt.collections` 绑定 `TreeView`，包括稳定的 Qt 6.2
      `SelectionMode` 适配器、层级展开、check-state 选择、指示器可见性与
      运动标量 API、重排信号、Python 虚函数分派和 borrowed 内部滚动条适配器。
- [x] 覆盖层级 insert/remove/reset 通知与 persistent index、调用方所有的
      model/selection/delegate 替换和外部析构、失效 delegate wrapper、Python
      model/delegate/view 虚函数分派、键盘/RTL/可访问性行为、API manifest、
      生成代码契约、已安装 wheel smoke 和可见层级示例。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地确认：93 项原生
      TreeView 测试中 92 项通过，人工 VisualCheck 按设计跳过；全部 32 个
      绑定 CTest、95 个 Python 绑定测试和 66 个契约验证器测试通过。新建
      干净虚拟环境也通过 wheel 安装、`pip check`、依赖路径检查、运行时
      smoke、源码包集成构建和可见截图审查。
- [x] 原生 CI 验证已确认生成代码契约、原生 Linux/Windows
      Qt 6.2.4 行为、macOS Qt 6.9.3、Qt 5.15/6.2 C++ 回归、源码包集成、
      三平台可迁移干净 wheel 与验收截图，以及最终 CI Gate。Windows
      Shiboken 6.2 生命周期用例也已通过 Qt 支持的 model 延迟析构路径。
- [x] 审计并绑定 `Breadcrumb` 与可变 `BreadcrumbItem` 元数据。Python facade
      禁止混合序列，并将文本列表和元数据列表分派给两个独立原生适配器；这是因为
      部分 Shiboken 版本会把值 wrapper 错选成 `QStringList`，并静默生成空文字。
- [x] 覆盖 metadata/QVariant 往返、稳定值比较、属性、信号、插入/删除、overflow
      geometry、激活、键盘与 Python 虚函数分派、API manifest、生成适配器契约、
      已安装 wheel smoke，以及可见的完整路径/中间溢出示例。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成整个批次的本地确认：10 项
      Breadcrumb 自动原生测试通过，1 项人工 VisualCheck 按设计跳过；全部 33 个
      绑定 CTest、98 个 Python 绑定测试和 73 个契约验证器测试通过。新建虚拟
      环境也通过 wheel 安装、`pip check`、依赖路径检查、运行时 smoke、源码包
      集成构建和可见截图审查。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 `Breadcrumb` 批次，包括生成代码契约、绑定测试、干净
      wheel 安装、验收截图、Qt 5.15/6.2 C++ 集成、源码包集成和最终 CI Gate。
- [x] 审计 `Pivot` 与 `SelectorBar`：两者只持有可变导航元数据、选择和溢出
      状态，不接管应用页面、model、delegate、overlay 或调用方 QWidget。
- [x] 通过 `fluentqt.navigation` 绑定 `Pivot`、`PivotItem`、`SelectorBar`
      和 `SelectorBarItem`，覆盖文本/值类型双重载、可变且不可哈希的稳定值语义、
      QVariant 兼容数据、嵌套 overflow 枚举以及根模块/分类/原生类型同一性。
- [x] 覆盖 item 修改、去重的选择信号、激活、键盘/Python 虚函数分派、geometry
      与 MoreButton overflow、API manifest、生成代码重载/值/QVariant 契约、
      已安装 wheel smoke，以及连接调用方 `QStackedWidget` 的可见示例。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地确认：17 项自动化
      Pivot/SelectorBar 原生测试通过，2 项人工 VisualCheck 按设计跳过；全部
      34 个绑定 CTest、104 个 Python 绑定测试和 77 个契约验证器测试通过。
      新建干净虚拟环境也通过 wheel 安装、`pip check`、已加载依赖路径检查、
      运行时 smoke、源码包集成构建和可见截图审查。
- [x] 原生 CI 验证已确认原生 Linux/Windows Qt 6.2.4、macOS
      Qt 6.9.3、Qt 5.15/6.2 C++ 回归、干净 wheel、源码包集成、验收截图
      和最终 CI Gate。Windows lane 也通过了 retained `ListView` model 与
      delegate 所支持的 deferred-destruction 路径。
- [x] 审计 `FlipView`、`FlowView` 和 `SplitView`：`FlowView` 不包含托管
      QWidget 边界，可复用调用方所有的 item-model 契约；`FlipView` 已具备
      显式 M3 页面契约，`SplitView` 现在也具备明确的 M3 pane 释放与原 parent
      恢复契约。
- [x] 通过 `fluentqt.collections` 绑定 `FlowView`，包括稳定的 Qt 6.2
      `SelectionMode` 与 borrowed 滚动条适配器、对其重写 model/delegate
      setter 的生成 wrapper 保活，以及 facade 层失效 delegate 清理。
- [x] 覆盖 Python 可变尺寸 role、insert/remove/reset 与 persistent index、
      选择、geometry/hit testing、Python delegate paint/size 虚函数、view
      虚函数分派、依赖替换和延迟析构、API manifest、生成代码契约、已安装
      wheel smoke 和可见的自适应卡片示例。
- [x] 使用版本匹配的 macOS Qt/PySide6 6.9.3 完成本地确认：15 项自动化
      FlowView 原生测试通过，1 项人工 VisualCheck 按设计跳过；全部 35 个
      绑定 CTest、111 个 Python 绑定测试和 82 个契约验证器测试通过。新建
      干净环境也通过 wheel 安装、`pip check`、依赖路径检查、运行时 smoke、
      源码包集成构建和验收截图审查。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 `FlowView` 批次，包括生成代码契约、绑定测试、三平台
      可迁移干净 wheel、验收截图、Qt 5.15/6.2 C++ 回归、源码包集成和最终
      CI Gate。Windows 6.2.4 lane 还实际覆盖了确定性的 signal/view 隔离与
      已安装 wheel FlowView 释放顺序。

## M5 — Overlay 与原生窗口

Popup、Flyout、ContentDialog、TeachingTip、dropdown 和其他 overlay 组件
需要验证 scrim 层级、外部点击、Escape、焦点恢复、顶层窗口 resize 和关闭策略。

当前批次：

- [x] 绑定 `DrawerView` 的 edge、尺寸、modal/dim、交互、动画、`CloseFlag`、
      开关生命周期与内容 ownership API。
- [x] 在本地覆盖 same-window 挂载、scrim 外部点击、Escape、`NoAutoClose`、
      Python 虚函数覆盖、Owned/Borrowed/Reparented、显式 take、干净 wheel 和
      可见截图。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 lane 确认该批次，并通过三平台干净 wheel、源码包集成、
      Qt 5.15/6.2 C++ 回归与最终 CI Gate。
- [x] 绑定 `Popup` 的开关状态、modal/dim、动画、`CloseFlag`、锚点相对定位、
      局部主题源和 light-dismiss 穿透区域，并由 facade 保留依赖 wrapper。
- [x] 在匹配的本机 Qt/PySide6 6.9.3 上覆盖同窗口挂载、scrim 创建、Escape、
      `NoAutoClose`、焦点归还且不覆盖后续焦点移动、Python 虚函数覆盖、外部
      QWidget 删除、25 轮依赖 GC 压力、生成契约、已安装 wheel smoke 和可见截图。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 Popup 批次，包括生成代码契约、绑定测试、三平台干净
      wheel、源码包集成、验收截图、Qt 5.15/6.2 C++ 回归和最终 CI Gate。
- [x] 绑定 `Flyout` placement、anchor offset、窗口边界约束、继承的 Popup
      生命周期与 caller-owned anchor API，并复用依赖保留 facade。
- [x] 在匹配的本机 Qt/PySide6 6.9.3 上覆盖 Top/Bottom 定位、Auto 翻转、
      同窗口挂载、默认非模态/无 scrim、Escape 与焦点归还、Python 虚函数覆盖、
      外部 anchor 析构、25 轮依赖 GC 压力、生成契约、已安装干净 wheel、源码包
      集成和可见截图。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 Flyout 批次，包括生成代码契约、绑定测试、三平台干净
      wheel、源码包集成、验收截图、Qt 5.15/6.2 C++ 回归和最终 CI Gate。
- [x] 绑定 `Dialog` 与 `ContentDialog` 的原生同窗口模态、smoke scrim、
      动画/结果生命周期、命令信号、稳定结果常量、caller-owned 主题源保活，
      并通过 `setContent()` / `takeContent()` 明确内容安装与释放。
- [x] 用 `QPointer` 加固原生 `ContentDialog::content()`，禁止会让 Shiboken
      6.9 在模块启动时崩溃的静态字段生成，并在匹配的本机 Qt/PySide6 6.9.3
      上覆盖 Python 子类、原生命令结果、外部析构、宿主/祖先拒绝、25 轮 GC
      压力、生成契约、源码包、干净安装 wheel 与可见截图。本机 43 项原生
      Dialog 测试全部通过（2 项人工 VisualCheck 按设计跳过），166 项绑定测试、
      120 项 verifier 测试和全部 65 项 PySide CTest 也已通过。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 Dialog/ContentDialog 批次，包括生成代码契约、全部
      65 项绑定 CTest、三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2
      C++ 回归和最终 CI Gate。
- [x] 绑定 `ComboBox` 的条目/当前值 API、信号、可编辑 line editor 接管、
      caller-owned model 保活、model column/root index、Python popup 虚函数分派
      和原生 same-window dropdown。
- [x] 将 popup 内部的 `ListView` 与 delegate 保持为实现细节；拒绝只会修改 Qt
      未使用 fallback popup 的继承 `setView()`/`setItemDelegate()`，并用生成代码
      契约约束 model、editor ownership 与 popup 原生 fallback。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过 40 项原生 ComboBox 测试
      （1 项人工 VisualCheck 跳过）、170 项绑定测试、126 项 verifier 测试、
      全部 67 项 PySide CTest、源码包集成、干净 wheel 安装/运行时隔离，以及
      构建目录与安装 wheel 字节完全一致的可见截图。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 ComboBox 批次，包括生成代码契约、全部 67 项绑定
      CTest、三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2 C++
      回归和最终 CI Gate。
- [x] 绑定 `DropDownButton`、`SplitButton`、`ToggleSplitButton`，以及它们
      依赖的 `FluentMenu`、`FluentMenuItem`。菜单保持 caller-owned；
      `setMenu()` 在替换、`setMenu(None)` 或宿主销毁前保留 Python wrapper。
- [x] 用删除安全的 `QPointer`、可观察 `menu`/`isOpen` 属性、替换/外部析构
      信号、RTL 二级区域命中，以及主操作/二级菜单/toggle 严格分离，加固原生
      菜单生命周期；直接菜单测试覆盖字体通知与 QAction 触发语义。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过 25 项聚焦原生测试
      （3 项人工 VisualCheck 跳过）、174 项绑定测试、129 项生成代码契约
      verifier、全部 69 项 PySide CTest、解压源码包后的绑定重建、干净安装
      wheel smoke，以及字节完全一致的构建目录/安装 wheel 菜单按钮截图。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认菜单按钮批次，包括生成代码契约、全部 69 项绑定 CTest、
      三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2 C++ 回归和最终
      CI Gate；Windows lane 也通过了 parented `ContentDialog` fixture 的 Qt
      延迟销毁路径。
- [x] 绑定 `CalendarDatePicker`、`DatePicker` 与 `TimePicker`，公开原生 Qt
      `QDate`/`QTime`/locale 值、嵌套 field/format 枚举、重复赋值安全的属性信号、
      Python 虚函数分派，并复用现有原生同窗口 popup。内部 popup/flyout helper
      全部保持私有；Qt 所有的 `CalendarView` getter 不改变 Shiboken ownership、
      parent 或保活状态。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过 55 项聚焦原生测试
      （3 项人工 VisualCheck 按设计跳过）、178 项绑定测试、132 项生成代码契约
      verifier、全部 71 项 PySide CTest、解压源码包后的绑定重建，以及新建干净
      venv 的 wheel smoke。构建目录与安装 wheel 的选择器截图具有完全相同的
      SHA-256。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认日期/时间选择器批次，包括生成代码契约、全部 71 项绑定
      CTest、三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2 C++ 回归
      和最终 CI Gate。
- [x] 在原生 Fluent `LineEdit` 之上直接绑定 `AutoSuggestBox`，支持 Python
      字符串列表转换、两个嵌套枚举、重复赋值安全的属性、带类型的文本/建议/
      查询/打开态信号、Python 虚函数分派、键盘预览与提交，并复用现有同窗口
      建议 Flyout。内部 model、popup 和行 delegate 保持私有，同时从 Python
      文本输入继承链移除 C++ 主题刷新钩子。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过全部 15 项自动原生
      AutoSuggestBox 测试（1 项人工 VisualCheck 跳过）、181 项绑定测试、
      137 项生成代码契约 verifier、全部 73 项 PySide CTest、解压源码包后的
      绑定重建/测试，以及新建干净 venv 的 wheel smoke。构建目录与已安装 wheel
      的验收截图具有完全相同的 SHA-256。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 AutoSuggestBox 批次，包括生成代码契约、全部 73 项绑定
      CTest、三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2 C++ 回归
      和最终 CI Gate。
- [x] 绑定 `CoachMark` 与 `TeachingTip`，覆盖 caller-owned target 保活、原生
      同窗口定位、content host 访问、语义化关闭原因、Python 虚函数分派，并明确
      禁止绕过 facade 的原始 target API 和内部主题钩子。`TeachingTip` 复用现有
      Popup 依赖保活 facade，不改变 QWidget ownership。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过全部 30 项自动原生
      CoachMark/TeachingTip 测试（2 项人工 VisualCheck 跳过）、184 项绑定测试、
      143 项生成代码契约 verifier 和全部 75 项 PySide CTest；同时完成解压源码包
      重建/测试、新建干净 venv 的 wheel smoke 与 `pip check`，构建目录和安装
      wheel 的验收截图字节完全一致。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 CoachMark/TeachingTip 批次，包括生成代码契约、全部
      75 项绑定 CTest、三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2
      C++ 回归和最终 CI Gate。
- [x] 绑定 `Toast` 与 `ToolTip` 的全部嵌套枚举、原生属性和信号、直接/托管
      展示、按键更新与淘汰、目标控件所有的 tooltip 附加、caller-owned
      theme source/QAction 保活，并明确移除动画进度和主题刷新实现钩子。Toast
      facade 将 `anchor.window()` 记录为真实 Python 父对象，同时保留原始子
      anchor 用于继承局部主题。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过全部 23 项自动原生
      Toast/ToolTip 测试（1 项人工 VisualCheck 跳过）、189 项绑定测试、157 项
      生成代码契约 verifier 和全部 77 项 PySide CTest；同时重建并测试解压后的
      源码包，通过新建干净 venv 的 wheel smoke 与 `pip check`，且构建目录和
      已安装 wheel 的 status-overlay 截图字节完全一致。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认 Status & Info 收口批次，包括生成代码契约、全部 77 项
      绑定 CTest、三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2 C++
      回归和最终 CI Gate。
- [x] 绑定 `CommandBar`、final `CommandBarFlyout` 与 `FluentMenuBar`，覆盖
      嵌套枚举、属性、信号、主/次命令变更、同窗口 flyout 调用，以及兼容
      Qt 6.2 Shiboken 生成器的 Python 可调用 `QWidget::addAction` 重载。
- [x] 让 caller-owned `QAction` 保持 borrowed，并仅在动作实际所属命令分区中
      保留 Python wrapper；覆盖 add、insert、分区移动、remove、clear、替换、
      外部析构与跨命令面共享动作生命周期，且不转移 QObject parent 或 Shiboken
      ownership。flyout 调用源控件保活到替换、显式清空或宿主销毁为止。
- [x] 在匹配的本机 macOS Qt/PySide6 6.9.3 上通过 23 项聚焦原生测试
      （4 项人工/交互检查按设计跳过）、194 项绑定测试、169 项生成代码契约
      verifier 和全部 79 项 PySide CTest；同时完成解压源码包重建/测试、干净
      venv wheel smoke 与 `pip check`，并得到字节完全一致的构建目录/安装 wheel
      命令面截图（`ba5b29a1f29575198bbc086204235cb268c7d91bf3372d0cd277eaabd2b3767e`）。
- [x] 原生 CI 验证已在原生 Linux/Windows Qt 6.2.4 与 macOS
      Qt 6.9.3 上确认命令面批次，包括生成代码契约、全部 79 项绑定 CTest、
      三平台干净 wheel、源码包集成、验收截图、Qt 5.15/6.2 C++ 回归和最终
      CI Gate。
- [x] 绑定 `TitleBar` 与既有 `Window` 的安全 chrome/backdrop API。保持
      `Window.titleBar()` 为 Qt-owned，使 TitleBar 内容替换时释放旧 Python
      子控件，移除内部主题刷新钩子，并保留安全的双参数
      `Window.nativeEvent()` 契约。
- [x] 覆盖 TitleBar 属性、信号、Python 子类分派，Window/TitleBar 内容
      生命周期、25 轮 GC stress、生成代码 parent/ownership 契约、manifest
      导出、干净 wheel smoke 和可见验收窗口。本机工作树和重新解压的源码包中，
      199 项绑定测试、172 项 verifier 测试和全部 82 项 PySide CTest 均通过。
- [x] 使用匹配的本机 Qt/PySide6 6.9.3 与原生 Cocoa 插件完成验收：Solid
      解析为 opaque 后端，Mica 和 Acrylic 都解析为原生 macOS vibrancy 与
      composited surface。新建 venv 的 wheel smoke 与 `pip check` 通过，工作树
      和安装 wheel 的 offscreen 截图字节完全一致；原生 JSON 记录两种系统材质
      后再保存可读的 Solid 截图。
- [x] 原生 CI 验证已在 Linux/Windows Qt 6.2.4 与 macOS Qt 6.9.3
      确认生成代码契约、全部 82 项绑定 CTest、干净 wheel 和原生
      XCB/Windows/Cocoa 验收报告，同时通过源码包集成、Qt 5.15/6.2 C++ 回归
      与最终 CI Gate。Qt 6.2 报告路径会将旧 Shiboken 的 byte string 规范化为
      UTF-8 文本。
- [x] 在本机实体 Windows 11 桌面验证 DWM 材质应用与 chrome 布局。Qt 6.9.3
      通过 `DwmSystemBackdrop` 成功应用 Mica 和 Acrylic；原生报告与可读的 Solid
      截图确认自定义 chrome、系统标题按钮预留区和 resize 传递均正常。
- [x] 在本机 Windows 11 桌面审查指针驱动的 system move 与边框 resize。真实
      标题栏拖动把窗口原点从 `(503,209)` 移到 `(623,289)`；真实右下边框拖动把
      尺寸从 `914x614` 调整为 `814x534`，对外发布的 chrome frame 同步更新。
- [ ] 在实体 Linux 桌面审查 KWin/Wayland compositor 材质与系统 move/resize。

自动化原生验收会拒绝 offscreen/minimal 插件，并验证原生 handle、chrome/内容
ownership、resize 传递和最终 backdrop 不变量。本机 WSLg Wayland 与 XCB 报告
也确认了 painted fallback，但不把它误报为 compositor blur。Windows DWM 材质
质量及指针驱动 system move/resize 现已有覆盖；实体 KWin blur 与指针行为是 M5
唯一剩余的桌面验收项。

## M6 — 可发布 Python 分发

- [x] 以 `bindings/pyside6/wheel-matrix.json` 定义并校验受支持的 CPython、
  平台和架构矩阵。首发覆盖 Linux、macOS、Windows 的 x64 与 ARM64；这里的
  x64 指 x86_64/AMD64，不包含 32 位 x86。
- [x] 在原生目标跑通全部首发 wheel lane。日常 fast CI 保留 Linux/Windows
  x64 的 Python 3.10 + Qt/PySide/Shiboken 6.2.4 最低兼容门禁和现有 macOS
  ARM64 lane；full CI 使用 Python 3.11 + 6.9.3 增加 Linux x64/ARM64、
  Windows x64/ARM64 和 macOS x64，连同现有 macOS ARM64 组成六目标首发集；
  原生 CI 验证已在全部原生目标通过。
- [x] 从 Shiboken 签名生成 `_fluentqt.pyi` 与 facade `.pyi`，用
  `api-manifest.json` 校验公共类、枚举、函数和必需方法，将存根纳入干净
  wheel smoke，并在 CI 中对已安装 wheel 运行严格 mypy 消费方检查。
- [x] 将精确 PySide/Shiboken 依赖、许可证和 notice 纳入 wheel，并在每个
  原生 lane 执行干净虚拟环境安装、导入、类型检查及原生依赖/架构检查。
- [x] 定义 Linux manylinux 构建、repair 和 `auditwheel` 审计策略。发布 lane 会在
  匹配的 PyPA policy image 中重新构建，固定 `auditwheel`，排除由元数据精确固定
  的 PySide6/Qt/Shiboken runtime，校验可重定位 RPATH 与 wheel 元数据，并生成带
  hash 的 JSON 审计报告；详见 `bindings/pyside6/MANYLINUX.md`。
- [x] 建立 Python API 版本与弃用规则。包公开 `__version__` 和
  `__api_version__`；manifest schema、SemVer、替代符号与“仅后续 major 可移除”
  规则由 stub 门禁和单元测试强制执行；详见
  `bindings/pyside6/API_COMPATIBILITY.md`。
- [x] 交付 wheel 内置的 Python Gallery，作为只消费公开包 API 的集成应用。
  生成契约以 C++ 目录为事实来源，锁定 12 个分类、顺序一致的 88 个路由、67 个
  组件页以及全部 199 个原生 SampleCard。路由组件加 10 个内嵌支持类型覆盖全部
  77 个 manifest 类型。每张卡片都用完整可执行的 `preview_source` 构建公开 API
  实时预览，而可见的 Source code 区只展示精简、与 C++ 规范方法对齐的 Python
  教学代码。验收门禁会执行完整预览源码、编译并按 C++ 契约检查可见代码语义，且
  拒绝 fallback 预览、Gallery 内部导入、预览 parent 泄漏或契约漂移。
  安装版应用复刻当前 C++ 视觉壳层和页面原型、打包同一批首页/控件图片，并以
  自动测试锁定标题栏、搜索、导航、Hero、响应式网格、组件分区、SampleCard 与
  快照几何。最终 Windows 全路由验收会同时以 `1440x900` 重新生成两个应用的
  快照：21 组路由整图字节一致，其余 67 个组件页只在有意区分 Python/C++ 的
  `Use` API 文本矩形内存在差异。共比较 114,048,000 个像素，其中
  113,592,073 个完全一致；455,927 个变化像素全部位于该矩形内，矩形外变化
  像素为零。快照入口会清除瞬时 hover 状态，因此结果不再取决于宿主鼠标位置。
- [ ] 所有必需矩阵 lane 通过后，签名并正式发布 wheel。

Qt 6.2.4 仍是绑定最低版本，而不是 ARM64 wheel 的构建版本：官方 PySide 6.2.4
没有 Linux/Windows ARM64 wheel，Linux ARM64 的 Qt/PySide 6.9.3 二进制还要求
glibc 2.39，因此该 lane 使用 `ubuntu-24.04-arm`。Windows ARM64 使用从 3.11
开始提供官方 ARM64 工具缓存的 CPython。这样既守住低版本兼容，也避免用版本号
相同但来源或架构不匹配的两套 Qt。

Linux 原生 smoke 产物仍保留 `linux_*` 标签。发布 workflow 现在会在分架构 policy
image 中重新构建，只上传修复后的 manylinux wheel 与审计报告，但这条新增容器路径
仍需完整原生 CI 证据。在所有 lane 通过且显式启用签名/上传门禁之前，不发布任何
wheel。

## 完成标准

只有同时满足以下条件，里程碑才能标记为完成：

1. 公共 API 已记录并形成文档；
2. 生成结果能使用声明的最低工具链编译；
3. 属性、信号、枚举、Python 子类和 ownership 已按适用范围测试；
4. wheel 能在干净虚拟环境安装和运行；
5. 进程只加载一套版本一致的 Qt/PySide6/Shiboken6 runtime；
6. 必需的原生 Linux、Windows 和 macOS CI lane 全部通过。

macOS 本地验证可以作为开发证据，但不能代替 C++ Python 扩展在原生 Linux 或
Windows 上的确认。

## 什么叫“Python 兼容完成”

项目将完成程度分为三层，避免把“能 import”误认为全部完成：

1. **基础可用**：M0 和 M1 完成，已声明的核心控件能够构造、收发信号、读写
   属性、继承并从 wheel 运行。当前已经达到这一层。
2. **功能完整**：M2–M5 完成；所有计划公开的叶子控件、托管控件、模型/导航、
   overlay 和原生窗口契约都有 Python API 与对应生命周期/交互测试。未绑定的
   C++ 公共组件必须明确记录原因，而不能静默缺失。
3. **发布完成**：M6 完成；明确 CPython/操作系统/架构矩阵，提供类型存根、
   API 兼容和弃用规则，并发布经过干净环境验证的 wheel。

当前 M0 至 M4 已完成。达到功能完整现在只需完成 M5 的实体 Linux
KWin/Wayland compositor 审查；达到发布完成还需完成 M6 剩余分发工作。

因此，本项目只有达到第三层，才称为“Python 支持完成并可正式发布”。这不包含
PySide2、Qt 5 Python 绑定，也不要求 Python 重写 C++ 绘制逻辑；Python 使用的
仍是同一套原生 FluentQt 控件。

## 如何验证效果

- **自动契约**：运行 `ctest --test-dir build/pyside6 -L '^pyside$'
  --output-on-failure`，检查属性、信号、继承、ownership、生成代码和验收窗口。
- **Python Gallery**：从已安装 wheel 运行 `python -m fluentqt.gallery` 做交互审查；
  增加 `--verify-catalog --walk-routes --snapshot <png> --report <json>` 可生成确定性的
  manifest、路由与渲染证据。
- **肉眼交互**：运行 `examples/compatibility_showcase.py`，切换 Light/Dark、
  Fluent/Material/macOS 和 accent，拖动 Slider、按住 RepeatButton，并检查文字、
  分隔线、进度控件和信号反馈。
- **Model 边界**：运行 `examples/list_view_model.py`、
  `examples/flow_view_model.py`、`examples/grid_view_model.py` 和
  `examples/tree_view_model.py`，分别检查扁平列表通知、可变尺寸换行、网格
  选择/重排以及层级展开/选择/重排，同时审查 Python delegate 绘制。
- **托管页面**：运行 `examples/flip_view_ownership.py`，切换页面并分别移除三种
  ownership 页面，再调用 `takePage()`，确认析构、脱离、原 parent 恢复与显式
  转移行为；运行 `examples/navigation_view_ownership.py`，检查 C++ 所有的内容
  宿主、header/main/footer chrome 策略以及 Left/Top 响应式布局。
- **同窗口 Overlay**：运行 `examples/drawer_view_ownership.py`，检查右侧 drawer、
  dim scrim、外部点击关闭、开关信号，以及三种内容 ownership 的释放行为；运行
  `examples/popup_overlay.py` 检查 Popup 锚点、焦点归还、Escape/外部关闭与
  passthrough 行为；运行 `examples/flyout_overlay.py` 检查
  Top/Bottom/Left/Right/Auto 定位、窗口内约束、light dismiss 与 anchor 生命周期；
  运行 `examples/combo_box_dropdown.py` 检查 Python model 条目、可编辑文本、
  键盘选择、Escape 关闭与原生 same-window dropdown；运行
  `examples/date_time_pickers.py` 检查 Python 提供的 `QDate`、`QTime`、locale、
  field/format 枚举、值信号和三个原生同窗口 picker popup；运行
  `examples/auto_suggest_box.py` 检查 Python 字符串列表建议、带原因/查询值的
  信号、键盘预览、焦点保持和原生同窗口建议 Flyout；运行
  `examples/command_surfaces.py` 检查 FluentMenuBar 排版、CommandBar 主/次
  命令、同窗口 CommandBarFlyout，以及共享 caller-owned QAction 的行为；
  向任一示例传入
  `--snapshot <png>` 可生成可留档截图。
- **导航值类型**：运行 `examples/breadcrumb_navigation.py`，检查 Python
  `BreadcrumbItem` 元数据、激活信号、完整路径绘制和窄宽度中间溢出行为；运行
  `examples/selector_pivot_navigation.py`，检查调用方页面组合、元数据选择、
  Pivot 过滤和 MoreButton overflow。
- **可留档截图**：给验收窗口传入 `--snapshot <png>`；该模式也可在
  `QT_QPA_PLATFORM=offscreen` 下运行。
- **安装真实性**：从新建虚拟环境安装 wheel，再运行
  `tests/test_wheel_smoke.py`，确认没有借用源码目录或加载第二套 Qt。
- **原生窗口**：使用 Cocoa、Windows、XCB 或 Wayland 运行
  `examples/window_chrome.py --verify-native --snapshot <png> --report
  <json>`；预期有原生 Mica/Acrylic/vibrancy/compositor 时再增加
  `--require-platform-backdrop`。Offscreen 截图仍然只能证明布局。

## 历史验证记录

以下条目记录绑定开发期间实际覆盖过的契约。历史压缩完成后，各批次单独触发的
workflow run 会按计划删除；重写后保留的最终 full CI 是当前分支级验证证据。

1. 原生 Linux/Windows Qt 6.2.4 原生 CI 验证已通过，将
   `Card`/`Expander` 视为第二个完成的 M3 批次。
2. 原生 Linux/Windows Qt 6.2.4 原生 CI 验证已通过，将
   `Avatar`/`RatingControl`/`ScrollBar` 叶子控件批次视为完成。
3. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `PipsPager` 批次视为完成。
4. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `InfoBar` ownership 批次视为完成。
5. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `TextEdit` 叶子控件批次视为完成。
6. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `CompoundButton` 叶子控件批次视为完成。
7. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `FontIcon` foundation 叶子控件批次视为完成。
8. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `ColorPicker` 叶子控件批次视为完成。
9. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
   已通过，将 `CalendarView` 叶子控件批次视为完成。
10. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
    已通过，将 `AnnotatedScrollBar` 值类型、静态详情和 borrowed
    ScrollView 链接批次视为完成。
11. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
    已通过，将 `Accordion` ownership 批次视为完成。
12. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ 原生 CI 验证
    已通过，将 `StackView` ownership/导航批次视为完成。
13. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3 和 Qt 5.15 C++ CI
    原生 CI 验证已通过，将 `TabView` 元数据/导航批次视为完成。
14. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    三平台干净 wheel 与最终 CI Gate 已在原生 CI 验证中通过，
    将 `ListView` model/delegate 批次视为完成。
15. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    三平台干净 wheel、验收截图与最终 CI Gate 已在 原生 CI 验证
    通过，将 `GridView` model/delegate/reorder 批次视为完成。
16. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    源码包集成、三平台干净 wheel、验收截图与最终 CI Gate 已在原生 CI 验证中通过，将 `TreeView` hierarchy/model/delegate/reorder
    批次视为完成。
17. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    三平台干净 wheel、源码包集成、验收截图与最终 CI Gate 已在原生 CI 验证中全部通过，将 `DrawerView` overlay/ownership 批次视为完成。
18. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、干净 wheel smoke、
    验收截图、源码包集成与最终 CI Gate 已在原生 CI 验证中通过，
    将 `Breadcrumb` 元数据/导航批次视为完成。
19. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、干净 wheel、源码包集成、
    验收截图、Qt 5.15/6.2 C++ 与最终 CI Gate 已在 原生 CI 验证
    通过，将 `SelectorBar`/`Pivot` 元数据导航批次视为完成。
20. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、干净 wheel、源码包
    集成、验收截图、Qt 5.15/6.2 C++ 和最终 CI Gate 已在原生 CI 验证中全部通过，将 `FlowView` model/delegate 批次视为完成。
21. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    三平台干净 wheel、源码包集成、验收截图与最终 CI Gate 已在原生 CI 验证中全部通过，将 `FlipView` ownership/导航批次视为完成。
22. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    干净 wheel、源码包集成、验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将 `SplitView` ownership 批次视为完成。
23. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    全部 54 个绑定 CTest、干净 wheel、源码包集成、验收截图和最终 CI Gate
    已在原生 CI 验证中一起通过，将 `NavigationView`/
    `StackContentHost` 页面与 chrome ownership 批次视为完成。
24. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、绑定测试、三平台干净 wheel、源码包集成、验收截图和最终
    CI Gate 已在原生 CI 验证中一起通过，将 `Popup` 同窗口 overlay
    与 QWidget 依赖生命周期批次视为完成。
25. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、绑定测试、三平台干净 wheel、源码包集成、验收截图和最终
    CI Gate 已在原生 CI 验证中一起通过，将 `Flyout` 定位与
    caller-owned anchor 生命周期批次视为完成。
26. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 65 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将
    `Dialog`/`ContentDialog` 同窗口模态、结果和托管内容 ownership 批次视为完成。
27. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 67 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将
    `ComboBox` model/editor ownership 与同窗口 dropdown 批次视为完成。
28. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 69 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将菜单按钮
    与 Fluent menu 批次视为完成。
29. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 71 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将原生
    日期/时间选择器与 popup 生命周期批次视为完成。
30. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 73 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将
    AutoSuggestBox 字符串列表/信号与同窗口建议 Flyout 批次视为完成。
31. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 75 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将
    CoachMark/TeachingTip target 保活、同窗口引导面、content host 和语义化
    关闭原因批次视为完成。
32. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 77 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将包含
    Toast/ToolTip overlay 生命周期与 borrowed 依赖处理在内的 Status & Info
    公开组件批次视为完成。
33. 原生 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、Qt 5.15/6.2 C++、
    生成代码契约、全部 79 项绑定 CTest、三平台干净 wheel、源码包集成、
    验收截图和最终 CI Gate 已在原生 CI 验证中一起通过，将
    `CommandBar`/`CommandBarFlyout`/`FluentMenuBar` borrowed action 与
    同窗口命令面批次视为完成。
34. 原生 CI 验证已通过 Linux/Windows Qt 6.2.4、macOS Qt 6.9.3、
    生成代码契约、全部 82 项绑定 CTest、三平台干净 wheel、原生
    XCB/Windows/Cocoa 报告、源码包集成、Qt 5.15/6.2 C++ 回归和最终 CI Gate，
    将自动化 `Window`/`TitleBar` API、ownership、backdrop 状态与原生平台插件
    批次视为完成。实体 Windows 11 DWM 与 Linux KWin/Wayland 视觉/交互审查仍未完成。
35. 原生 CI 验证已一起通过 14 个生成 stub、覆盖 75 个类/11 个枚举/
    14 个函数的 manifest gate、全部 84 项绑定 CTest、安装后严格 mypy、原生
    Linux/Windows Qt 6.2.4 与 macOS Qt 6.9.3 的干净 wheel、源码包集成、
    Qt 5.15/6.2 C++ 回归和最终 CI Gate，将 M6 的首个类型/API 防回退批次视为
    完成。更完整的 wheel 矩阵、兼容策略、签名和发布工作仍属于 M6 待办。
36. 历史压缩前的原生 CI 验证已通过 Linux、macOS、Windows 的
    x64/ARM64 六个 Python 3.11 + Qt/PySide/Shiboken 6.9.3 首发 wheel lane，
    同时通过 Linux/Windows x64 的 Python 3.10 + 6.2.4 最低兼容门禁、全部绑定
    CTest、严格 mypy、干净安装、原生窗口 smoke、Qt 5.15/6.2 C++ 回归、
    sanitizer、CI Gate 与 Release ready。该轮还验证了 borrowed QAction 销毁后
    CommandBar 异步焦点重建不再解引用悬空地址，因此将 M6 的原生 wheel 矩阵与
    最低兼容策略批次视为完成；manylinux repair/audit、Python API 版本规则、签名
    和正式发布仍属于 M6 待办。
37. 根据公共 API 台账、本机全部 84 项 PySide CTest 与 原生 CI 验证
    对里程碑状态进行同步：M0 至 M4 已完成；M5 因实体 DWM/KWin/Wayland 审查
    保持进行中，M6 因 manylinux、API 版本治理、签名和正式发布保持进行中。
38. 实现 M6 的 API 治理与 manylinux 策略批次。包现已公开完整版本和 major/minor
    API 版本变量，manifest 包含机器校验的弃用台账；Linux 发布 lane 定义分架构
    PyPA policy image、固定 repair 输入、外置 PySide6/Qt/Shiboken 边界、可重定位
    RPATH 检查和 JSON 审计证据。本机 Windows Qt 6.9.3 与 Linux Qt 6.2.4 构建
    各自通过全部 86 项 PySide CTest、干净 wheel smoke、`pip check`、严格 mypy
    与运行时依赖解析。Windows 11 DWM 成功应用原生 Mica/Acrylic；WSLg
    Wayland/XCB 以预期 painted fallback 通过原生 chrome 验收。M5 仍需完成
    Windows 指针交互与实体 KWin 审查。由于本机没有 Docker，M6 仍需新增
    manylinux 容器 lane 在完整 CI 中通过后，才能进入签名和正式发布。
39. 将 wheel 内置的 Python Gallery 集成批次视为完成。应用只使用公开
    `fluentqt` API。生成契约现以 C++ Gallery 目录为事实来源，锁定同样的 12 个
    分类、顺序一致的 88 个路由、67 个组件页以及全部 199 个原生 SampleCard。
    Python 应用通过 67 个路由组件和 10 个内嵌支持类型覆盖全部 77 个 manifest
    类型；每张卡片都构建公开 API 实时预览并执行完全相同的展示源码，任何通用
    fallback 都会被拒绝。完成条件还包括原生视觉壳层，而不只是早期的目录原型：
    Python 应用现共用首页/控件图片，并复刻标题栏、居中搜索、响应式侧边导航、
    首页 Hero 与网格、组件分区、引用卡、实时 SampleCard 和 Source code 折叠区。
    几何测试会拒绝旧的原始 TreeView/按钮列表布局；PNG 生成会将透明 DWM/Mica
    像素合成到 Fluent fallback 底色。本机 Windows Qt/PySide 6.9.3 与 Linux
    Qt/PySide 6.2.4 源码构建各自通过全部 89 项 PySide CTest；干净 wheel 通过
    smoke、`pip check`、严格 mypy、88/88 安装版路由遍历、199/199 原生等价示例
    （fallback 为零），并校验全部 74 张控件图片和 7 张首页 tile。Windows
    `windows` 与 WSLg `wayland` 安装版快照也已审查。Qt 6.2 验收遍历会在路由
    之间让出事件循环，避免事件饥饿。该集成证据不能替代 M5 的实体
    KWin/Wayland 桌面审查，也不会关闭 M6 剩余的容器 CI、签名与正式发布工作。
40. 关闭本机 Windows 指针验收与 Gallery 对齐缺口。Windows 11 真实指针
    输入把自定义 chrome 窗口移动了 `120x80` 像素，并通过右下边框将尺寸从
    `914x614` 调整为 `814x534`；DWM Mica/Acrylic 仍保持原生后端。最终 C++ 与
    Python 全路由验收分别重新生成 88 张 `1440x900` 快照。当前如实记录的结果为
    21 组整图字节一致，另 67 个组件页的变化像素只位于 Python/C++ `Use` API
    文本内；该矩形外的像素全部一致。快照入口现会在渲染前清除瞬时 hover 状态。
    本机 Windows Qt/PySide 6.9.3 与 Linux Qt/PySide 6.2.4
    均通过 32 项 Gallery 测试、干净安装 wheel smoke 与 `pip check`；Linux 还
    通过全部 201 项绑定测试，以及 TreeView（92 通过/1 项可视化跳过）、
    ProgressRing（11/1）和 ScrollBar（4/1）聚焦 C++ 套件。原生 WSLg Wayland
    报告确认自定义交互 chrome 与如实上报的 Mica/Acrylic painted fallback。
    M5 现在只剩实体 KWin/Wayland compositor 与指针审查；M6 仍需容器 CI 证据、
    签名及发布授权。
41. 修正 Python Gallery 的启动遮罩、API 参考、示例源码与主题刷新回归。启动遮罩
    现在覆盖整个 Gallery 客户区，不再只覆盖右侧内容区。组件 `Use` 卡片展示
    Python import/type/module，不再出现 C++ 头文件或 CMake target；全部 199 个
    实时预览均由其展示的 Python 源码直接构造，ListView 源码也补齐了与预览一致
    的头像、delegate、model 设置顺序和选择状态。Icon Browser 的主题刷新不再调用
    绑定层有意不公开的 `ToolTip::onThemeUpdated()`。共享的非代码文案已改为语言
    无关，因此最终 Windows 88 路由对比为 21 组整图完全一致、67 组只在 `Use`
    文本矩形内不同，矩形外变化像素为零。Windows Qt/PySide 6.9.3 与 Linux 6.2.4
    均通过 33 项 Gallery 测试；两端重新构建并强制安装的 wheel 也通过源码/启动/
    主题循环聚焦测试、完整 wheel smoke 与 `pip check`。
42. 关闭 Python Gallery 的生命周期、教学源码、设置切换延迟与 Top→Left 导航状态
    缺口。交互启动现在使用每用户 `QLockFile`/`QLocalServer` 单实例；程序会在导入
    88 路由/199 示例的完整 UI 图之前完成加锁和激活握手，主控制器就绪前收到的激活
    会排队恢复窗口，自动化验收模式则按设计保持相互独立。Windows 真实第二次启动
    以 0 退出，耗时 1.067 秒，Gallery 窗口始终只有一个。此记录取代第 39、41 条中
    “展示源码与完整预览源码完全相同”的过度约束：每张卡片保留完整可执行的
    `preview_source` 来构建像素级预览，同时用独立、精简、只含公开 API 的 `source`
    作为可见教学代码。199 份完整源码全部成功执行，199 份可见代码全部可编译且保留
    C++ 契约中的规范 API 操作；可见 Python 总计 2,648 行，对应 C++ 2,369 行
    （1.118 倍），单块最长 68 行，且不含 Gallery 内部导入或 `gallery_parent`。
    主题/样式/accent 变化现在只立即刷新壳层与当前路由，隐藏页在真正导航到时再按代次
    懒刷新；88 页全部构建后，主题切换从 1,460.1 ms 降至 219.0 ms，样式切换从
    546.5 ms 降至 28.7 ms。左侧 pane 只有在回场动画完全结束后才解除 compact，
    Windows 真实 Top→Left 往返会恢复全部主导航与 footer 项。最终 Windows
    Qt/PySide 6.9.3 和 Linux 6.2.4 的源码与强制安装 wheel 均通过 Gallery 套件、
    wheel smoke、`pip check`、88/88 路由和 199/199 预览，失败数为零。M5 的实体
    KWin/Wayland 审查以及 M6 的容器 CI、签名和正式发布仍保持未完成。
