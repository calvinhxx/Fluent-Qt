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
| M0 — 绑定基础设施 | 已实现 | 可选 CMake target、Qt 6.2 生成路径、版本门禁、单一原生模块、启动 API 和测试 |
| M1 — 核心控件 API | 已实现 | Basic Input、Text Fields、Window、主题/字体 API、ownership 和 `nativeEvent` 契约 |
| M2 — 低风险控件覆盖 | 进行中 | 增加具备属性、信号、示例、manifest 检查和 wheel smoke 验证的叶子 QWidget 控件 |
| M3 — 托管控件 ownership | 进行中 | 为接管或释放子控件的容器增加 Python 安全适配器和 GC 测试 |
| M4 — 模型与导航 | 计划中 | Python model/delegate、虚函数分派、选择状态和导航生命周期 |
| M5 — Overlay 与原生窗口 | 计划中 | 同窗口 overlay 行为以及原生桌面窗口/backdrop 验证 |
| M6 — 可发布 Python 分发 | 计划中 | wheel 支持矩阵、类型存根、API 兼容策略、签名和发布 |

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
- Windowing：`Window`、backdrop 枚举和 backdrop value type。
- Foundation：Light/Dark 主题、设计语言预设、accent 色、字体角色、字体缩放
  和构建信息。

该里程碑的合入门槛包括 Python 子类分派、Qt 属性与信号、`Window` 子控件
ownership、安全的双参数 `nativeEvent()` 契约、API manifest 检查和干净环境
wheel smoke 测试。

## M2 — 低风险控件覆盖

当前批次：

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
      CI run `30553990409` 已通过生成契约检查、完整绑定测试、可迁移 wheel
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
      确认 `PipsPager`；CI run `30598949551` 已通过生成/编译、契约检查、
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
      确认 `TextEdit` 批次。CI run `30601608042` 已通过生成/编译、生成代码
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
      确认 `CompoundButton` 批次。CI run `30603864933` 已通过生成/编译、
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
      确认 `FontIcon` 批次。CI run `30604556341` 已通过生成/编译、生成代码
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
      确认 `ColorPicker` 批次。CI run `30605260392` 已通过生成/编译、生成代码
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
      确认 `CalendarView` 批次。CI run `30607530481` 已通过生成/编译、生成代码
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
      确认 `AnnotatedScrollBar` 批次。CI run `30609069504` 已通过生成/编译、
      生成代码契约检查、全部绑定测试、可迁移 wheel、干净环境 smoke、验收
      截图、C++ 回归门禁和最终 CI Gate。

后续批次需要先进行 API 审计。只有符合以下条件的组件才属于本里程碑：

- 是不包含运行时 ownership 模式的叶子 `QWidget`；
- 不暴露 model/delegate 契约；
- 不创建 popup 或同窗口 overlay；
- 不依赖平台原生窗口行为；
- 不需要 Python 专用 façade 即可保持现有 C++ 语义。

每个批次都必须补齐生成 wrapper 输入、分类模块重新导出、
`api-manifest.json`、属性/信号测试、已安装 wheel smoke 测试；有可见行为的
控件还必须提供可运行示例。

## M3 — 托管控件 ownership

候选范围包括 `ScrollView`、`Accordion`、`StackView`、`DrawerView`、
`TabView` 以及其他接收托管 `QWidget` 的 API。

当前原型：

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
- [x] 在 CI run `30599841356` 的原生 Linux/Windows Qt 6.2.4 绑定 lane 和
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
      确认 `Accordion` ownership 批次。CI run `30610740405` 已通过生成与
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
      确认 `StackView` ownership/导航批次。CI run `30613428314` 已通过生成
      与编译、全部生成代码契约、28 个绑定 CTest、Qt 运行库路径校验、可迁移
      wheel、干净环境 smoke、验收截图、源码包检查、C++ 回归 lane 和最终
      CI Gate。

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

集合和导航组件只有在 Python model 边界设计完成后才能进入本里程碑。验证范围
必须包括：

- `QAbstractItemModel` 和 delegate 生命周期；
- Python 虚函数覆盖和 `super()` 分派；
- 选择、reset、行插入/删除和 persistent index 行为；
- 从 Python 和 C++ 两侧替换、销毁 model；
- 键盘、焦点、RTL 和与可访问性相关的导航行为。

## M5 — Overlay 与原生窗口

Popup、Flyout、ContentDialog、TeachingTip、dropdown 和其他 overlay 组件
需要验证 scrim 层级、外部点击、Escape、焦点恢复、顶层窗口 resize 和关闭策略。

Window、TitleBar、Mica、Acrylic、vibrancy、compositor blur、拖动和 resize
必须在原生桌面环境验证。Offscreen 渲染适用于普通控件，但不能作为平台窗口
集成的验收依据。

## M6 — 可发布 Python 分发

- 定义受支持的 CPython、平台和架构 wheel 矩阵。
- 每个 wheel 都在原生目标或有明确说明的等价工具链中构建。
- 生成 `.pyi` 类型存根，并在 CI 中比较公共 API 变化。
- 增加依赖、许可证、wheel repair/audit、干净安装和导入检查。
- 建立 Python API 版本与弃用规则。
- 所有必需矩阵 lane 通过后，才签名并发布 wheel。

首个发布矩阵应有意小于 C++ 包矩阵。只有具备原生构建和运行验证时，才增加新的
Python 版本及 ARM64/x64 目标。

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

因此，本项目只有达到第三层，才称为“Python 支持完成并可正式发布”。这不包含
PySide2、Qt 5 Python 绑定，也不要求 Python 重写 C++ 绘制逻辑；Python 使用的
仍是同一套原生 FluentQt 控件。

## 如何验证效果

- **自动契约**：运行 `ctest --test-dir build/pyside6 -L '^pyside$'
  --output-on-failure`，检查属性、信号、继承、ownership、生成代码和验收窗口。
- **肉眼交互**：运行 `examples/compatibility_showcase.py`，切换 Light/Dark、
  Fluent/Material/macOS 和 accent，拖动 Slider、按住 RepeatButton，并检查文字、
  分隔线、进度控件和信号反馈。
- **可留档截图**：给验收窗口传入 `--snapshot <png>`；该模式也可在
  `QT_QPA_PLATFORM=offscreen` 下运行。
- **安装真实性**：从新建虚拟环境安装 wheel，再运行
  `tests/test_wheel_smoke.py`，确认没有借用源码目录或加载第二套 Qt。
- **原生窗口**：Window、TitleBar 和 backdrop 必须在真实桌面手工验证；
  offscreen 截图不能证明拖动、resize、Mica、vibrancy 或 compositor blur。

## 后续交付顺序

1. 原生 Linux/Windows Qt 6.2.4 CI run `30552580180` 已通过，将
   `Card`/`Expander` 视为第二个完成的 M3 批次。
2. 原生 Linux/Windows Qt 6.2.4 CI run `30553990409` 已通过，将
   `Avatar`/`RatingControl`/`ScrollBar` 叶子控件批次视为完成。
3. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30598949551`
   已通过，将 `PipsPager` 批次视为完成。
4. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30599841356`
   已通过，将 `InfoBar` ownership 批次视为完成。
5. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30601608042`
   已通过，将 `TextEdit` 叶子控件批次视为完成。
6. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30603864933`
   已通过，将 `CompoundButton` 叶子控件批次视为完成。
7. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30604556341`
   已通过，将 `FontIcon` foundation 叶子控件批次视为完成。
8. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30605260392`
   已通过，将 `ColorPicker` 叶子控件批次视为完成。
9. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30607530481`
   已通过，将 `CalendarView` 叶子控件批次视为完成。
10. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30609069504`
    已通过，将 `AnnotatedScrollBar` 值类型、静态详情和 borrowed
    ScrollView 链接批次视为完成。
11. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30610740405`
    已通过，将 `Accordion` ownership 批次视为完成。
12. 原生 Linux/Windows Qt 6.2.4 和 Qt 5.15 C++ CI run `30613428314`
    已通过，将 `StackView` ownership/导航批次视为完成。
13. 只有在 model/navigation 或 overlay 边界同时设计完成后，才推进 `TabView`
    和 `DrawerView`，避免仅为增加绑定数量而提前公开不稳定 API。
