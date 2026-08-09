# WebAssembly 路线图

[English](webassembly-roadmap.md)

本文档用于跟踪 Fluent-Qt 浏览器版本的实现与验证进度，并作为当前状态的
唯一事实来源。只有获得对应的完成证据后，里程碑才能标记为完成。桌面端
Qt 5.15+ 与 Qt 6.2+ 的支持范围保持不变。

## 当前状态

| 里程碑 | 状态 | 交付内容 | 完成证据 |
| --- | --- | --- | --- |
| M0 - 可行性与架构 | 已完成 | 浏览器约束、Qt/Emscripten 基线、部署与许可证决策 | 已于 2026-08-09 完成基于仓库现状的设计审查 |
| M1 - 核心库与 Hello World | 已完成 | Qt WebAssembly 预设、导出的 Asyncify 辅助目标、浏览器安全的窗口边框、Hello World 目标 | Qt 6.9.3 / Emscripten 3.1.70 配置和构建通过；Hello World canvas 已于 2026-08-09 在 Chromium 中启动 |
| M2 - C++ Web Gallery | 已完成 | 浏览器运行时、控制台日志、WebLocalStorage 设置、页面懒加载、客户端标题栏、Retina 性能档位 | 2026-08-09 在 DPR 2 Chromium 中以 11.70 秒通过全部 88 个路由、存储和异步对话框/菜单检查；最慢冷路由为 185 ms |
| M2.1 - 浏览器产品化 | 已完成 | 响应式窗口化外壳、自适应像素预算、有界路由缓存、简体中文字体回退、已整理的最小 UILib 应用、安装后消费验证 | 在 1920x1080 / DPR 2 下，自适应 1x 快速冒烟用时 1.06 秒，原生 2x 为 2.15 秒；完整 88 路由冒烟把堆容量稳定在 128 MiB；安装后的 `FluentQt::FluentQt` 消费方于 2026-08-09 链接并构建通过 |
| M2.2 - 浏览器一致性加固 | 已完成 | 单一且可聚焦的 Qt 桌面表面、可移动/缩放 Fluent 窗口、最大化/还原与 OpenWindow、不透明活动标题栏、软件材质缓存、空闲回弹兜底、字体状态隔离、不透明多行菜单、浏览器键盘输入 | 真实浏览器缩放已跨越导航断点，最大化/还原及 OpenWindow 通过视觉检查；浏览器物理按键已能到达 Fluent 文本控件；最新 1.25x 的 88 路由冒烟用时 6.89 秒，完整 1378 项原生 CTest 清单于 2026-08-09 无剩余失败 |
| M3 - CI 与 GitHub Pages | 进行中 | 可复用 WebAssembly CI 模块、浏览器冒烟、部署至 `/gallery/`、网站入口 | 远端 fast/full Actions 与 Pages 制品均已通过；仍待同步至 `main` 后验证部署 URL |
| M4 - 分发与维护 | 已完成 | 工作流、许可证、源码包文档及受支持工具链策略 | 2026-08-10 的集成前完整 CI 已通过全部 44 个任务、`CI Gate` 与 `Release ready`；临时分支运行历史在合入后清理 |

当前 7 个里程碑中有 6 个已完成。M3 的仓库实现、远端 CI 与 Pages 制品已经
就绪；在同步至 `main` 并验证公开 `/gallery/` URL 前，M3 仍保持“进行中”。

## 支持目标

首个受支持的浏览器目标有意保持较窄范围：

- Qt 6.9.3 `wasm_singlethread` 与 Emscripten 3.1.70；
- 启用了 WebAssembly、WebGL 和本地存储的现代浏览器；
- 可复用的 `FluentQt` 库以及 C++ Gallery；
- 单线程执行；仅仍保留嵌套模态事件循环的消费方选择启用 Asyncify 兼容目标；
- 静态部署至 `https://calvinhxx.github.io/Fluent-Qt/gallery/`。

本项目不把 Qt 5.15 或 Qt 6.2 的 WebAssembly 构建列为受支持浏览器目标；
这些版本仍作为桌面端兼容性基线。

## 实现边界

- `FluentQt::FluentQt` 本身不携带 Emscripten 专属链接参数。
- `FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT` 添加可选的
  `platforms/webassembly/` 集成，与 PySide6 的显式启用边界保持一致。
- 浏览器应用链接 `FluentQt::WebAssembly`，在创建 Fluent 控件前调用
  `configureRuntime()`，并通过 `showWindow()` 展示窗口。适配层只创建一个 Qt
  桌面画布并在其中承载应用窗口，可复用控件无需感知浏览器运行时。
- 浏览器消费方通过链接 `FluentQt::WasmAsyncify` 选择启用嵌套事件循环支持；
  原生构建不定义该适配目标。
- 核心 `src/`、Gallery 视图/视图模型和共享示例代码不包含 WebAssembly 条件
  分支。CMake 在平台能力与启动接口背后选择窄化的桌面或浏览器实现。
- Hello World 与 Gallery 均不链接 Asyncify。Gallery 的对话框和菜单使用异步
  `open()` / `popup()` 流程；无法立即移除嵌套循环的外部消费方仍可显式选择
  Asyncify 目标。
- 浏览器 Gallery 不包含单实例 IPC、系统托盘、原生窗口位置恢复、更新检查、
  文件系统主题目录和桌面打包。
- 浏览器设置使用 `QSettings::WebLocalStorageFormat`；日志输出到浏览器控制台；
  路由按需创建，不在启动时批量预热。浏览器适配层最多保留最近使用的 16 个
  路由，桌面端仍保持不设上限的页面常驻策略。
- Web 外壳在 160 万像素预算下自适应选择 1x–1.25x QWidget backing store；
  页脚提供平衡/原生档位，`?render-scale=native` 仍可显式恢复原生分辨率。
  该策略完全位于可复用 C++ 代码之外。显式 1x 会标注为 HiDPI 下偏软的
  性能档；1.25x 作为日常交互和视觉检查档位。
- 在桌面尺寸视口中，WebAssembly 运行时会把同一套 frameless QWidget 树作为
  单一浏览器尺寸 Qt 桌面表面的居中圆角子窗口；小尺寸视口自动最大化。可用
  `?window-mode=windowed|maximized` 显式选择两种展示方式。窗口几何与页面外框
  中，HTML 外壳负责浏览器 stage 与请求模式，可选运行时适配层负责控件几何、
  手动移动/缩放及最大化/还原，不向组件层加入 WebAssembly 条件分支。
- Web 构建会嵌入由 Noto Sans SC 生成的 GB2312 简体中文回退子集，并通过可选
  资源契约完成注册。原生包不携带该字体，组件代码也不包含 WebAssembly 字体
  分支。
- 当前受支持制品仍为单线程。未来多线程实验必须先拆出可测量的非 GUI 工作，
  并作为独立制品发布；QWidget 布局/绘制仍在浏览器 GUI 线程，线程版部署还要求
  SharedArrayBuffer 与 COOP/COEP 响应头。
- WebAssembly 制品通过 Pages 渠道发布，不属于桌面安装包、vcpkg 矩阵、
  Python wheel 或 Release 资产。

## CI 分层

`ci-wasm.yml` 是原生 C++ 与 PySide6 模块之外的第三个可复用验证模块。
快速验证会构建并启动 Hello World 和 Gallery 外壳；完整验证会在模拟 DPR 2
下遍历 Gallery 路由，执行异步对话框与菜单路径，验证持久化与渲染档位，并
生成 Pages 制品。顶层
`CI Gate` 继续作为稳定的分支保护检查项。

## 集成路径

WebAssembly 工作已隔离到直接基于 `release/1.6.x` 创建的
`codex/web-sup`。后续按以下线性路径完成：

1. 实现、聚焦本地验证和评审修复均保留在 `codex/web-sup`。
2. 推送分支并创建目标为 `release/1.6.x` 的拉取请求；将 `CI Gate` 作为
   必需检查，并在合入前运行完整 CI 层级。
3. 将评审后的提交 rebase 合入 `release/1.6.x`，随后删除本地和远端临时分支。
4. 按发布治理流程将已发布的 `release/1.6.x` 提交同步到 `main`。Pages 自动
   部署仍有意只绑定 `main`。
5. 验证部署后的 `/gallery/` URL，并同步更新中英文 Roadmap。M3 以 Pages
   部署证据关单；M4 以对应的远端 `Release ready` 证据关单。

## 验证记录

| 日期 | 范围 | 结果 | 证据或阻塞项 |
| --- | --- | --- | --- |
| 2026-08-09 | 本地工具链 | 通过 | 检测到 Qt 6.9.3 `wasm_singlethread`；确认 Qt 安装器未提供 Emscripten 编译器后，安装并启用了官方 emsdk 3.1.70 |
| 2026-08-09 | 配置与构建 | 通过 | `cmake --preset wasm` 和 `cmake --build --preset wasm --parallel 6` 已构建 FluentQt、Hello World 与 C++ Gallery |
| 2026-08-09 | 快速浏览器冒烟 | 通过 | CI Playwright runner 已通过 4 个路由、WebLocalStorage、异步对话框/菜单、渲染档位及 Hello World canvas 检查 |
| 2026-08-09 | 完整浏览器冒烟 | 通过 | 在模拟原生 DPR 2 下，CI Playwright runner 已通过全部 88 个路由、WebLocalStorage、异步对话框/菜单、渲染档位及 Hello World canvas 检查 |
| 2026-08-09 | 浏览器性能 | 通过 | Gallery 移除全程序 Asyncify 后，`fluent_qt_gallery.wasm` 从 33,109,235 字节降至 23,545,148 字节（-28.9%）。在 DPR 2 下，最终默认 1.25x 档位完整冒烟耗时 11.70 秒（最慢路由 185 ms），原生 2x 为 24.37 秒（最慢路由 449 ms） |
| 2026-08-09 | Pages 制品 | 通过 | `stage-wasm-pages.py` 已整理白名单应用文件、许可证与 `build-info.json`；CI 分类器和工作流边界测试通过 |
| 2026-08-09 | 源码包 | 通过 | 已生成 `FluentQt-1.6.1-source.zip`，使用 WebAssembly 工具链完成配置和 Hello World 构建，安装开发组件，并验证导出的 `FluentQt::WasmAsyncify` |
| 2026-08-09 | 适配层解耦 | 通过 | 已将浏览器生命周期、设置、主题持久化和分发行为收口到构建时选择的平台适配器；原生 Gallery/Hello World 构建、聚焦 Gallery 测试和完整 88 路由浏览器冒烟均通过 |
| 2026-08-09 | 浏览器窗口化外壳 | 通过 | 在 1280x720 浏览器视口中，视觉检查测得居中的 922x600 应用表面，具备 Fluent 客户端标题栏、圆角裁剪、阴影以及切换最大化模式的页脚入口。HTML 外壳负责 stage，`FluentQt::WebAssembly` 负责单一 Qt 桌面表面与被承载控件的几何 |
| 2026-08-09 | 简体中文字体回退 | 通过 | 从固定版本的 Noto Sans SC 生成 1,990,352 字节 GB2312 子集，仅嵌入 WebAssembly 构建，并在 CalendarView 中确认 `八月` 与 `周一` 至 `周日` 正常显示；自动冒烟同时验证 Han 回退注册和必需字形覆盖。最终 Gallery WASM 为 25,524,513 字节，比加入回退前的 23,545,148 字节增加 1,979,365 字节（+8.4%） |
| 2026-08-09 | 安装后 UILib 消费 | 通过 | 将 WebAssembly Development 组件安装到隔离前缀，以导出的 `FluentQt_DIR` 配置 `examples/hello_world`，并成功构建 `fluentqt_hello_world.wasm`；Gallery 页脚也把同一最小应用发布到 `hello-world/` |
| 2026-08-09 | 产品化完整浏览器冒烟 | 通过 | 在模拟 DPR 2、默认 1.25x 档位下，全部 88 个路由、WebLocalStorage、异步对话框/菜单、CJK 回退、窗口化外壳及已整理 Hello World 均在 8.22 秒内通过；最慢路由为 `list-view`，耗时 130 ms |
| 2026-08-09 | 自适应渲染与有界缓存 | 通过 | 在 1920x1080 / DPR 2 下，快速冒烟在自适应 1x 用时 1.06 秒（最慢路由 171 ms）、平衡 1.25x 用时 1.25 秒（207 ms）、原生 2x 用时 2.15 秒（403 ms）。完整 88 路由冒烟用时 8.27 秒，堆容量保持 128 MiB，program break 为 34 -> 93 MiB；此前无界缓存会把容量从 128 扩至 153 MiB。新增 LRU 契约后，原生 `test_gallery_shell_framework` 58/58 通过 |
| 2026-08-09 | 浏览器一致性回归集 | 通过 | 在 1280x720 真实浏览器中，922x600 的 Window 已缩放至 1095x600 并跨越响应式导航断点；最大化/还原返回缩放后的几何，OpenWindow 保持为有界 Fluent 子窗口。窗口化与最大化快速冒烟均通过，随后完整冒烟以 9.31 秒通过全部 88 个路由，并显式验证三动作通用菜单及七动作 PasswordBox 编辑菜单。排版、列表回弹、菜单、文本框与窗口相关的 199 项原生聚焦测试零失败；25 项跨平台或人工视觉用例按预期跳过 |
| 2026-08-09 | 标题清晰度与重绘开销 | 通过 | 已分别修复两个根因：显式 1x 现在会标记为 HiDPI 下偏软的性能档；浏览器宿主 chrome 强制按活动态显示，并绘制为不透明 token 表面。静态软件材质会在失效条件之间复用缓存。在 DPR 2 / 渲染 DPR 1.25 下，完整 88 路由冒烟用时 7.01 秒，标题栏/缓存契约、存储、窗口、对话框、菜单、CJK 回退及 Hello World 全部通过；最慢路由 103 ms，堆容量保持 128 MiB。200 项聚焦原生测试零失败，25 项按预期跳过。由于本机 Qt SDK 仅提供 `wasm_singlethread`，且线程版不会把 QWidget GUI 工作移出主线程，多线程仍作为后续实验项。 |
| 2026-08-09 | 浏览器文本输入 | 通过 | 已从单一 WebAssembly 桌面表面移除禁止激活的标志，使 Qt 能聚焦其隐藏 HTML input 桥。真实浏览器物理按键已更新 LineEdit、PasswordBox、TextEdit、NumberBox 与 AutoSuggestBox，后者正确发出 `UserInput` 并打开候选列表。自动化 DPR 2 / 渲染 DPR 1.25 完整冒烟现会向真实 Fluent LineEdit 发送物理按键，并以 6.89 秒通过全部 88 个路由、文本编辑菜单与 Hello World。该改动仍隔离在 `FluentQt::WebAssembly`，原生文本控件路径保持不变。 |
| 2026-08-09 | 集成前本地门禁 | 通过 | CI 分类测试（14/14）、模块化工作流边界、Gallery/UILib 边界、项目元数据、全部 9 个打包场景、CI C++ 矩阵测试（6/6）、锁定版本的 WebAssembly 字体可复现检查及 16 文件 Pages 制品均通过。完整 1378 项 macOS CTest 清单发现一处散落的 Qt 版本判断；将 Qt 6.8 字体回退 API 收口到 `QtCompat.h` 后，该测试已重建并通过，原生可运行用例无剩余失败。随后重新构建 WebAssembly，并以 6.89 秒通过全部 88 路由浏览器冒烟。 |
| 2026-08-09 | 集成分支 | 通过 | 已将评审后的 WebAssembly 范围推送至 `codex/web-sup`，并创建目标为 `release/1.6.x` 的[草稿 PR #27](https://github.com/calvinhxx/Fluent-Qt/pull/27) |
| 2026-08-10 | 远端 fast CI | 通过 | PR #27 的集成前 fast CI 已通过 WebAssembly 浏览器冒烟、3 条 PySide6 兼容/发布路径、5 条原生 C++/集成路径及 `CI Gate`。合入后清理对应临时分支 Actions 历史，Roadmap 保留结果摘要 |
| 2026-08-10 | 远端 full CI | 通过 | 集成前完整 CI 已通过全部 44 个任务，包括 Qt 5.15/6.2 原生兼容、x64/ARM64 安装包与测试、sanitizer 契约、Python 3.10-3.13 release wheels、`CI Gate` 与 `Release ready`。WebAssembly full 冒烟在 DPR 2 / 渲染 DPR 1.25 下以 9.705 秒遍历全部 88 个路由；浏览器总耗时 10.17 秒，最慢路由 168 ms，堆容量保持 128 MiB。合入后按策略删除临时分支运行记录 |
| 2026-08-10 | Pages 部署 | 进行中 | fast 与 full CI 均已整理并上传 `/gallery/` 制品。自动部署仍有意只绑定 `main`，因此公开 URL 验证是 M3 唯一剩余完成项 |

## 进度更新规则

里程碑状态发生变化时，必须在同一次变更中同时更新状态表与验证记录。
记录具体命令及可核验的通过或失败证据；不能仅凭原生构建结果宣称浏览器
支持已验证。英文版 `webassembly-roadmap.md` 与本文件也必须在同一次变更中
保持同步。
