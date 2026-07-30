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
| M3 — 托管控件 ownership | 计划中 | 为接管或释放子控件的容器增加 Python 安全适配器和 GC 测试 |
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
- [ ] 在原生 Linux 和 Windows Qt 6.2.4 CI lane 确认第三批控件。

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

候选范围包括 `ScrollView`、`StackView`、`DrawerView`、`TabView` 以及其他
接收托管 `QWidget` 的 API。

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

1. 让第三批 M2 控件通过原生 Linux 和 Windows Qt 6.2.4 CI lane。
2. 将 `ScrollView` 作为首个 M3 托管控件候选进行审计，先设计并验证显式
   Python ownership 适配器，再扩展到其他容器。
