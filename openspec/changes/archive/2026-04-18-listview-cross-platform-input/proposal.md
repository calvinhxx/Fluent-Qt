## Why

`ListView` 的滚轮/触控板/RDP 输入处理目前只在 macOS 本地能正常工作。在以下场景出现问题：
- **Mac RDP → Windows**：单次触控板手势会被转发为大量高频 `NoScrollPhase` 事件（无 `pixelDelta`），与 `bounce`/`overscroll` 逻辑冲突，出现"卡边界、反复回弹、滚动方向混乱"等异常。
- **Windows 本地 Qt6**：精密触控板（WM_MOUSEWHEEL / NoScrollPhase + ±120 angleDelta）与鼠标滚轮事件无法区分，当前缺乏 cluster 节流，连续滚动手势触发不正确的 overscroll/bounce。
- **Windows 本地 Qt5**：运行 crash。Qt5 缺失 `QEnterEvent` 的 Qt6 派生型（已用 `FluentEnterEvent` 兼容），但 `wheelEvent` 中部分逻辑（点位/Pixel API、bounce 动画初始化时序、空指针守卫缺失）在 Qt5 路径上未验证。

刚刚完成的 `flipview-cross-platform-input` 已沉淀了一套 phase-based vs NoScrollPhase 的输入分类方法论，本次将该方法论迁移到 ListView 的"连续滚动 + overscroll bounce"场景，并补齐 Qt5/Qt6 双版本的兼容守卫。

## What Changes

- **统一输入分类**：在 `wheelEvent` 入口将事件归为三类 — phase-based（macOS 触控板，含 `ScrollBegin/Update/Momentum/End`）、NoScrollPhase + pixelDelta（Windows 精密触控板部分驱动 / Linux 触控板）、NoScrollPhase + 仅 angleDelta（鼠标滚轮 / Mac RDP → Windows / Qt5 默认路径）。
- **NoScrollPhase cluster 节流**：参考 FlipView，引入 `kClusterGapMs = 120ms` 与累积阈值，避免 RDP 高频转发触发反复 overscroll/bounce。
- **Bounce 动画跨平台守卫**：
  - bounce 运行期间消费 `NoScrollPhase` 事件（已部分实现）+ 同时消费"无 phase 信号"的高频事件。
  - `m_bounceAnim` / `m_bounceTimer` 使用前增加 null 守卫；初始化保证在 `wheelEvent` 首次触发前完成。
  - NoScrollPhase 路径的 overscroll 触发改为基于 cluster 累积量（而非单事件 delta），消除 RDP 下"轻微一拨就触发 bounce"。
- **Qt5/Qt6 兼容**：
  - 所有 `event->position()` / `event->pos()` 等 API 通过 `QtCompat.h` 抽象（如必要新增 `fluentWheelPos()`）。
  - 鼠标 / 滚轮事件中所有 Qt6-only API（`QPointF` 重载、`pixelDelta` 行为差异）增加版本守卫。
  - 修复 Qt5 crash：定位崩溃栈（推测为 bounce 动画/timer 在 polish 前被访问，或事件中虚函数调用顺序差异），加固生命周期管理。
- **测试覆盖**：新增模拟 Mac RDP / Windows 触控板 / 鼠标滚轮 / Qt5 路径的 `TestListView` 用例；保留现有 macOS 本地手势行为不回归。

## Capabilities

### New Capabilities
- `listview-wheel-input`: ListView 跨平台滚轮/触控板事件分类、cluster 节流、overscroll bounce 触发条件，以及 Qt5/Qt6 兼容守卫。

### Modified Capabilities
（无 — 现有 specs/ 目录中无与 ListView 输入相关的 capability。）

## Impact

- **代码**：`src/view/collections/ListView.cpp` 的 `wheelEvent` / `startBounceBack` / 构造函数初始化顺序；可能新增 `src/common/QtCompat.h` 的 wheel 事件辅助函数。
- **API**：无公共 API 变更（`ListView` 头文件 signal/property 不动）。
- **测试**：新增 `tests/views/collections/TestListView.cpp` 用例（模拟 RDP / 鼠标滚轮 / Windows 触控板事件序列）。
- **依赖**：无新增依赖；继续支持 Qt5（`fluent_qt_lib` 当前可在 Qt5/Qt6 双版本编译，需保持）。
- **回归风险**：macOS 本地 phase-based 路径的 bounce 行为必须保持不变；现有 `TestListView` 全部用例不允许回归。
