# 系统能力路线图

中文 | [English](system-capability-roadmap.md)

> 阅读提示：文中保留类名、API 名和 `Capability Phase N` 标识，便于与代码、测试和
> 英文提案对应。“审核门”表示进入下一阶段前必须完成并由项目方确认的检查点。

## 目的

本路线图区分可复用控件行为、应用层策略和操作系统集成。每个能力阶段都应当能够
独立审核，也能够单独回退。只有前一阶段的 API、行为、测试和范围通过审核后，
才会进入下一阶段。

命名说明：本文中的能力阶段编号与
[组件合同基线](component-contract-baseline.md)里的历史 UILib Phase 编号不是
同一套编号。引用本文时，请使用“Capability Phase N”或“能力阶段 N”，避免混淆。

当前组件库已经包含 `InfoBar`、`InfoBadge`、`Avatar` 和 `Toast` 等应用窗口内
界面组件。后续重点是跨组件命令能力和可选的平台集成，而不是继续增加视觉上等价的
通知控件。

## 当前状态

| 能力阶段 | 状态 |
|---|---|
| 1 共享文本编辑右键菜单 | 已在 `release/1.5.x` 完成；Qt 5.15 和 Qt 6 编辑菜单合同通过 |
| 2 编辑命令统一入口与路由器 | 已完成；2026-07-28 已通过审核门 |
| 3 CommandBar / CommandBarFlyout | Capability Phase 3A 至 3D 已完成；自动化与 Computer Use 聚焦桌面回归通过，等待项目方统一发布回归 |
| 4 通知无障碍与生命周期 | 尚未开始 |
| 5 可选系统通知与应用角标 | 尚未开始；属于可选打包路线 |

## 架构边界

| 层级 | 负责 | 不负责 |
|---|---|---|
| 组件库 | 编辑菜单、命令呈现、窗口内通知视觉、无障碍合同 | 业务未读状态、业务路由、通知策略 |
| 应用层 | 当前编辑目标、通知数据、前后台策略、本地化可见文本 | 平台专用通知底层实现 |
| 可选平台模块 | 系统通知投递、激活、权限、任务栏或 Dock 角标能力 | 窗口内 Fluent 绘制、推送服务业务逻辑 |

核心 `FluentQt` 目标继续保持为跨平台 Qt Widgets 库。原生通知 SDK 不得成为核心
库的无条件依赖。`*/private/` 下的私有实现头不得进入
[FluentQtInstallHeaders.cmake](../../cmake/FluentQtInstallHeaders.cmake)。

## Capability Phase 1：共享文本编辑右键菜单

范围：

- 将 `TextEdit` 已有的 Fluent 右键菜单实现提取为
  `menus_toolbars/private/` 下的私有共享基础设施。
- 保留 Qt 标准动作的所有权、本地化文本、快捷键、启用状态以及 Undo/Redo
  分发语义。
- 将共享菜单接入 `LineEdit`；`PasswordBox`、`NumberBox` 和
  `AutoSuggestBox` 等派生输入框通过继承获得该能力，不新增公共命令 API。
- 辅助实现保持私有。现有已安装 `LineEdit` 声明只增加实现该行为所需的受保护
  （protected）事件重写，不增加新的公共可调用 API。

本阶段不做：

- 不提供公共编辑命令统一入口或窗口级动作。
- 不实现 CommandBar 表面。
- 除 Qt echo mode 已有标准菜单策略外，不新增 PasswordBox 专用脱敏策略。
- 私有 helper 不进入安装包。

验收标准：

- `TextEdit` 行为和对象名保持兼容：`FluentTextEdit.ContextMenu`。
- `LineEdit` 用 `FluentMenu` 呈现标准编辑动作：
  `FluentLineEdit.ContextMenu`。
- Undo 和 Redo 通过键盘快捷键及 Fluent 菜单都能正常工作。
- Copy、Delete 和 Select All 保留可识别图标。
- 聚焦测试目标 `test_line_edit` 和 `test_text_edit` 通过。

审核门：

- 进入 Capability Phase 2 前，审核私有所有权策略、动作代理、视觉密度、对象名
  以及 Qt 5.15/Qt 6 行为。
- 保留自动化合同，证明至少一个 `LineEdit` 派生控件仍能通过继承打开 Fluent
  菜单，但不借此扩大 PasswordBox 策略范围。

2026-07-28 已完成验证：

- Windows Qt 6.9.3 已构建 `test_line_edit`、`test_text_edit` 和
  `test_password_box`，所有非手工测试通过。
- Linux Qt 5.15.2 已构建相同三个目标；5 项右键菜单及 Undo/Redo 行为测试
  通过，其中包括隐藏模式 PasswordBox 能力。

## Capability Phase 2：编辑命令统一入口与路由器

已接受、实现并审核的
[编辑命令路由器 API 提案](editing-command-router-proposal.md)已经完成。它不会暴露
任何内部 Qt 控件指针。Capability Phase 2 审核门已于 2026-07-28 通过，因此可以
开始 Capability Phase 3 的合同工作。

已交付职责：

- 暴露语义化编辑命令及能力变化，同时不公开 `TextEdit` 私有的 `QTextEdit`。
- 提供窗口作用域的 Undo、Redo、Cut、Copy、Paste、Delete 和 Select All 动作。
- 跟踪当前获得焦点且受支持的编辑器，不抢占无关控件或其他窗口的快捷键。
- 同一组动作可以复用于 `MenuBar`、右键菜单和后续命令表面。
- 为 `PasswordBox`、只读编辑器、建议输入框和数字输入框定义明确的能力收缩规则。

验收覆盖焦点与菜单恢复、剪贴板变化、只读编辑器、密码输入框、多窗口、原生快捷键、
调用方持有的呈现、作用域销毁和动作生命周期。同一窗口的第二个 Router 会被硬拒绝，
不会安装快捷键；PasswordBox 在按住 Peek 时请求右键菜单，会先结束临时显示，再以
Cut/Copy 禁用状态呈现菜单。15 项聚焦合同已在 Windows Qt 6.9.3 和 Linux
Qt 5.15.2 上通过；源码消费者和安装包消费者也能编译导出的分类 API。

## Capability Phase 3：CommandBar 与 CommandBarFlyout

只有 Capability Phase 2 建立稳定命令语义后，才增加可视命令呈现。

已接受的公共 API 和行为定义在
[Command Bar API 与行为提案](command-bar-proposal.md)中。Capability Phase 3A
已经提供：

- 已安装的公共类型声明；
- 共享的借用式 `QAction` 模型；
- 保持默认行为兼容的 Popup 焦点设置；
- 日志类别和安装包探针；
- 非视觉合同测试。

Capability Phase 3A 审核门已于 2026-07-28 通过。Capability Phase 3B 提供
私有动作 presenter、响应式测量、复合键盘焦点、确定且支持 RTL 的溢出规则，以及
可滚动的同窗口溢出层。Capability Phase 3C 完成上下文 Flyout 的呈现与交互合同。
Capability Phase 3D 补齐三种设计语言、无障碍、Gallery 示例、
EditingCommandRouter 集成和安装包边界验证。自动化与 Windows 聚焦桌面回归均已
完成；项目方的统一发布回归仍是最后一个外部审核门。

已接受组件：

- `CommandBar`：主命令、次级溢出、分隔符和响应式测量。
- `CommandBarFlyout`：遵循同窗口
  [浮层行为合同](../architecture/overlay-behavior.md)。
- 直接组合 Capability Phase 2 的编辑动作，不创建第二套编辑命令门面，也不自动
  替换右键菜单。

完整合同定义动作所有权、不支持的动作类型、确定性的溢出顺序、键盘导航、焦点恢复、
RTL 定位、触摸与鼠标行为、无障碍，以及 Fluent、Material、Cupertino 三种设计
语言下的行为。`Popup` 只新增一个默认开启的受保护（protected）焦点设置，使 Transient
Flyout 打开时不会发生哪怕短暂的焦点转移；该变化不修改 Popup 对象布局，也不增加
面向应用的属性。提案还将实现拆成四个可独立审核的部分。

2026-07-28 已完成 Capability Phase 3A 验证：

- Windows Qt 6.9.3 已构建 `test_command_bar` 和
  `test_command_bar_flyout`；12 项非视觉合同全部通过。
- Linux Qt 5.15.2 已构建相同目标；12 项合同全部通过。
- Windows Popup、Flyout、TeachingTip、MenuBar 和 EditingCommandRouter
  回归集合共发现 77 项测试，0 失败；6 项既有无头环境或手工测试按预期跳过。
- 源码子项目消费者和安装包消费者都能通过 `<FluentQt/MenusToolbars.h>` 编译并
  链接两个公共类型。
- 安装树包含两个公共头，不包含 `CommandActionModel_p` 和
  `TextEditingMenu_p`。
- 在 3A 审核门时，内联绘制、响应式溢出、命令 presenter、完整 Flyout 交互、
  无障碍适配和 Gallery 示例明确留在 Capability Phase 3B 至 3D；其中 3B 内容
  已在下方交付。

2026-07-28 已完成 Capability Phase 3B 验证：

- Windows Qt 6.9.3 与 Linux Qt 5.15.2 均通过 `test_command_bar` 当前全部
  17 项聚焦合同，其中包括内联呈现和响应式溢出合同。
- 覆盖内容包括优先级与逻辑尾部溢出、分隔符规范化、动作状态同步、折叠标签、
  RTL 视觉顺序、复合键盘焦点、焦点修复、同窗口浮层关闭、精确触发和删除安全。
- Windows CommandBarFlyout、Popup、Flyout、MenuBar 和
  EditingCommandRouter 关联回归共发现 69 项测试，0 失败；5 项既有本地桌面或
  手工测试按约定跳过。
- Linux Qt 5 的等价非桌面关联回归 63 项全部通过。
- 3B 没有新增公共 API，也没有修改安装头白名单；`CommandPresenter_p` 保持私有。
- Capability Phase 3C 和 3D 已在下方交付，聚焦桌面回归也已完成；项目方的
  发布级统一回归仍保留在本轮验证之外。

2026-07-28 已完成 Capability Phase 3C 与 3D 自动化验证：

- `CommandBarFlyout` 已实现 Standard 与 Transient 模式、折叠/展开和始终展开呈现、
  确定性的响应式溢出、宿主边界内滚动、RTL 导航、焦点恢复、精确触发和删除安全的
  动作更新。
- 私有无障碍适配器覆盖工具栏、命令、弹出菜单、菜单项、快捷键、选中、禁用、焦点
  和“更多”展开状态，不新增安装 API。
- `CommandBar` 与 `CommandBarFlyout` 在 Fluent、Material、Cupertino 的亮色和
  暗色主题下均有绘制合同；Qt 6.9.3 与 Qt 5.15.2 均运行绘制和无障碍测试。
- EditingCommandRouter 动作可以复用于内联及 Flyout 命令表面，不丢失当前编辑器和
  选区；跨窗口命令表面会拒绝这些窗口作用域动作。
- 当前 Windows Qt 6.9.3 的 CommandBar、CommandBarFlyout、
  EditingCommandRouter、Popup、Flyout、MenuBar 标签集合共发现 98 项测试：
  92 项通过、6 项桌面交互或手工测试按预期跳过、0 失败。
- Linux Qt 5.15.2 的 17 项 CommandBar 聚焦合同和 14 项 CommandBarFlyout
  自动化合同全部通过；受保护的 Flyout VisualCheck 按预期跳过。
- Windows Gallery 内容页套件 44 项、0 失败，2 项视觉测试按约定跳过；新增响应式
  溢出、EditingCommandRouter 复用、显示模式示例和 72 px 随包图片。Linux Qt 5
  专用测试配置按既有约定关闭 Gallery。
- 源码子项目和已安装包消费夹具均能编译、链接分类 API。安装树包含两个命令表面的
  公共头，不包含私有动作模型、presenter 和无障碍头。
- Computer Use 聚焦回归已经实际执行 CommandBarFlyout VisualCheck 和 Gallery，
  覆盖 Fluent 亮/暗主题、Material 切换、LTR/RTL、Standard/Transient/展开模式、
  窄宽度响应式溢出、鼠标与键盘路径以及焦点保留。
- 该回归发现 CommandBarFlyout 次级行和 CommandBar 溢出行在首次展开时可能沿用
  暴露前的旧滚动视口宽度。现在通过展开后立即加延迟二次校准避免文字被截断，并为
  两条路径补了聚焦几何合同。
- 高 DPI、触摸和发布范围的完整检查仍由项目方统一回归，本文不将该外部门禁标记为
  已通过。

## Capability Phase 4：通知无障碍与生命周期

增加新的通知视觉之前，先加固现有组件：

- 使用当前 Qt 版本能够提供的最佳无障碍事件播报新出现的 Toast 内容，并提供
  Qt 5.15 兼容回退。
- 定义独立 `InfoBadge` 的值和可见性变化如何通过可访问父级暴露。
- 将 Toast 关闭原因、可选动作、悬停暂停、托管堆栈的分组与淘汰作用域
  （宿主 + 位置 + `maximumVisible()`）、可选关联/更新键和原地更新语义分别
  审核。托管堆叠仍是默认行为；更新键不能把投递模型变成“只替换、不堆叠”。

应用可见文本和未读状态策略继续由调用方持有。除非后续合同明确改变，
`setMaximumVisible()` 仍是进程级启动配置，并且只影响之后的 `showToast` 调用。

## Capability Phase 5：可选系统通知与应用角标

该阶段属于单独的可选构建目标，需要独立的平台与打包提案。

候选服务：

- `SystemNotificationService`：能力与权限查询、显示、更新、移除、通知激活、
  动作激活和失败报告。
- `AppBadgeService`：设置数字、设置受支持的图形、清除，以及报告不支持的能力。

后端方向：

- Qt `QSystemTrayIcon::showMessage()` 可以作为低保真回退。
- Windows 可以使用 Windows App SDK 提供应用通知和任务栏角标。
- macOS 可以使用 User Notifications 和 Dock tile badge。
- Linux 可以使用 freedesktop 通知服务；由于不存在统一的跨桌面应用图标角标合同，
  角标必须按能力检测启用。

推送投递、云端注册、通知历史模型和业务未读规则继续留在组件库之外。
