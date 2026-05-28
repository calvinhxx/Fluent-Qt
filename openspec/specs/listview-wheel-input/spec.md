# ListView Wheel Input

## Purpose

规范 ListView 对跨平台滚轮、触控板和 RDP 转发输入的处理：按 phase/pixel/discrete 三路分类事件，Windows fallback 输入以正常滚动优先，使用 cluster 状态抑制 NoScrollPhase 高频边界尾巴，保护 overscroll bounce 生命周期，并保持 Qt5/Qt6 与 macOS 本地体验不回归。

## Requirements

### Requirement: 跨平台 wheelEvent 三路分类

ListView SHALL 在 `wheelEvent` 入口将事件分类为三类并分别处理：
- **PhaseBased**：`event->phase() != Qt::NoScrollPhase`（macOS 本地触控板）。
- **NoPhasePixel**：`NoScrollPhase` 且 `pixelDelta()` 非空（部分 Windows 精密触控板驱动 / Linux 触控板）。
- **NoPhaseDiscrete**：`NoScrollPhase` 且 `pixelDelta()` 为空（鼠标滚轮 / Mac RDP → Windows / Qt5 默认路径）。

#### Scenario: macOS 触控板事件走 PhaseBased 路径
- **WHEN** ListView 收到 `phase = ScrollUpdate` 且 `pixelDelta` 非空的 wheelEvent
- **THEN** 事件按 PhaseBased 路径处理，保留现有 overscroll/bounce 触发能力，可被后续手势打断 bounce

#### Scenario: Mac RDP → Windows 事件走 NoPhaseDiscrete 路径
- **WHEN** ListView 收到 `phase = NoScrollPhase`、`pixelDelta` 为空、`angleDelta = ±120` 的 wheelEvent
- **THEN** 事件按 NoPhaseDiscrete 路径处理，应用 cluster 与边界尾巴保护逻辑

#### Scenario: 鼠标滚轮事件走 NoPhaseDiscrete 路径
- **WHEN** ListView 收到 `phase = NoScrollPhase`、`pixelDelta` 为空、`angleDelta = ±120` 倍数的 wheelEvent
- **THEN** 事件与 RDP 路径共用同一处理逻辑（不可区分）

### Requirement: NoPhaseDiscrete cluster 节流

ListView SHALL handle NoPhaseDiscrete input (`NoScrollPhase` with empty `pixelDelta()`) as normal scrolling first. Cluster state SHALL be used only to suppress boundary-tail jitter and stale residual events; it MUST NOT preload overscroll pressure while the scrollbar is still moving through content.

#### Scenario: 普通鼠标滚轮单次滚动
- **WHEN** ListView is not at the relevant scroll boundary and receives one NoPhaseDiscrete wheelEvent with `angleDelta = ±120`
- **THEN** ListView MUST perform normal content scrolling through the underlying scrollbar
- **AND** ListView MUST reset NoPhaseDiscrete boundary cluster state
- **AND** ListView MUST NOT enter overscroll bounce

#### Scenario: 普通鼠标滚轮快速连续滚动
- **WHEN** ListView is not at the relevant scroll boundary and receives multiple NoPhaseDiscrete wheelEvents within `kClusterGapMs`
- **THEN** ListView MUST continue normal content scrolling for each event that can move the scrollbar
- **AND** ListView MUST NOT carry the accumulated cluster amount into overscroll when content reaches a boundary

#### Scenario: Windows 触控板高频 cluster 正常滚动
- **WHEN** ListView receives a high-frequency NoPhaseDiscrete sequence representing a Windows precision touchpad fallback while the scrollbar can still move
- **THEN** ListView MUST scroll content normally and keep the Fluent scrollbar synchronized
- **AND** ListView MUST NOT treat the sequence as boundary bounce pressure until a new event arrives while already at the boundary

#### Scenario: 边界处 NoPhaseDiscrete 同向事件触发有界回弹
- **WHEN** ListView is already at the start or end boundary and receives a NoPhaseDiscrete event requesting further movement beyond that boundary
- **THEN** ListView MUST accept and consume the event
- **AND** ListView MAY start one bounded overscroll bounce for visual feedback
- **AND** repeated same-direction NoPhaseDiscrete tail events MUST NOT extend or repeatedly restart that bounce

#### Scenario: 跨 cluster 重置累积量
- **WHEN** 距上一 NoPhaseDiscrete event 已超过 `kClusterGapMs`
- **THEN** ListView MUST reset the NoPhaseDiscrete boundary cluster state before processing the new event

#### Scenario: 方向反转立即恢复滚动
- **WHEN** ListView consumed NoPhaseDiscrete boundary-tail events at one boundary
- **AND** the next NoPhaseDiscrete event requests movement back into the scrollable range
- **THEN** ListView MUST reset stale boundary state
- **AND** ListView MUST allow normal content scrolling in the reverse direction

### Requirement: bounce 运行期间消费高频事件

ListView SHALL protect a running `m_bounceAnim` from stale same-direction NoScrollPhase tail events while still allowing input that can move content back into range to recover promptly. NoPhasePixel and same-direction NoPhaseDiscrete tail events MUST be consumed without modifying overscroll, restarting the bounce timer, or restarting the running animation. PhaseBased events SHALL remain able to interrupt bounce.

#### Scenario: bounce 期间 NoPhaseDiscrete 同向 tail 被吞
- **WHEN** bounce animation is running and ListView receives a same-direction NoPhaseDiscrete wheelEvent that still requests movement beyond the active boundary
- **THEN** ListView SHALL `event->accept()` and return
- **AND** ListView MUST NOT modify overscroll
- **AND** ListView MUST NOT stop, restart, or extend the running bounce

#### Scenario: bounce 期间 NoPhasePixel tail 被吞
- **WHEN** bounce animation is running and ListView receives a NoPhasePixel wheelEvent that represents stale boundary-tail input
- **THEN** ListView SHALL `event->accept()` and return
- **AND** ListView MUST NOT modify overscroll
- **AND** ListView MUST NOT stop, restart, or extend the running bounce

#### Scenario: bounce 期间 NoPhaseDiscrete 反向恢复
- **WHEN** bounce animation is running and ListView receives a NoPhaseDiscrete wheelEvent requesting movement back into the scrollable range
- **THEN** ListView MUST clear stale NoPhaseDiscrete boundary-bounce state
- **AND** ListView MUST allow normal content scrolling instead of swallowing the event

#### Scenario: bounce 期间 PhaseBased 事件可打断
- **WHEN** bounce animation is running and ListView receives `phase = ScrollUpdate` with continuous gesture information
- **THEN** ListView SHALL stop the bounce animation
- **AND** ListView SHALL continue phase-based overscroll processing

### Requirement: bounce 动画与 timer 生命周期守卫

ListView SHALL 对 `m_bounceAnim` 与 `m_bounceTimer` 的所有访问点进行 null 守卫，确保在构造未完成或析构期间访问不会 crash。

#### Scenario: wheelEvent 在 bounceAnim 未初始化时不 crash
- **WHEN** ListView 构造期间被父窗口转发 wheelEvent，且此时 `m_bounceAnim == nullptr`
- **THEN** wheelEvent SHALL 安全跳过 bounce 相关分支，按普通滚动处理或调用基类实现

#### Scenario: 析构时停止动画与 timer
- **WHEN** ListView 析构
- **THEN** ListView SHALL stop `m_bounceAnim` 与 `m_bounceTimer`，不允许 pending tick 在析构后触发

### Requirement: Windows 键鼠与触控板兼容

ListView SHALL support Windows keyboard/mouse and touchpad usage without making one input class regress the other. Wheel handling changes MUST NOT affect keyboard navigation, selection, drag reorder, or model/delegate behavior.

#### Scenario: 鼠标滚轮不被触控板 cluster 逻辑吞掉
- **WHEN** a user scrolls ListView with a physical mouse wheel on Windows
- **THEN** each wheel tick that can move content MUST change the scrollbar value normally
- **AND** the event MUST NOT be consumed solely because it belongs to a recent NoPhaseDiscrete cluster

#### Scenario: 高分辨率滚轮小角度事件仍然滚动
- **WHEN** ListView is not at a boundary and receives a NoPhaseDiscrete wheelEvent with a sub-notch angle delta such as `angleDelta = ±60`
- **THEN** ListView MUST move the relevant scrollbar by a proportional pixel amount
- **AND** the event MUST NOT feel inert because the platform default scroll handler ignored the partial tick

#### Scenario: 触控板连续滚动不触发边界 flap
- **WHEN** a Windows touchpad emits a high-frequency NoPhaseDiscrete sequence that reaches the bottom boundary
- **THEN** ListView MUST remain pinned at the boundary after content is exhausted
- **AND** ListView MUST NOT repeatedly enter bounce, reverse direction, or leave the Fluent scrollbar out of sync

#### Scenario: RDP 惯性尾巴不污染下一次手势
- **WHEN** RDP-forwarded touchpad tail events are consumed at a boundary
- **AND** a later gesture starts after the cluster gap or in the opposite direction
- **THEN** ListView MUST treat the later gesture as fresh input
- **AND** normal scrolling MUST resume if content can move

#### Scenario: 键盘导航不回归
- **WHEN** a user navigates ListView with keyboard selection keys after wheel input
- **THEN** ListView selection and current-index behavior MUST remain governed by the existing `QListView` selection model

### Requirement: Windows NoPhaseDiscrete 回弹动画连续性

ListView SHALL provide smooth boundary rebound for Windows-style NoPhaseDiscrete input by starting edge feedback from a stable overscroll value, running one continuous bounce-back to zero, and keeping the native and Fluent scrollbar positions pinned at the active boundary.

#### Scenario: 边界输入启动单次回弹反馈
- **WHEN** ListView is already at the relevant scroll boundary and receives a NoPhaseDiscrete wheelEvent requesting further movement beyond that boundary
- **THEN** ListView SHALL set one bounded overscroll offset on the active axis
- **AND** ListView SHALL arm or start one bounce-back sequence from that current overscroll value
- **AND** ListView SHALL keep the underlying scrollbar value at the boundary

#### Scenario: 同向尾部事件不重启回弹
- **WHEN** a NoPhaseDiscrete boundary rebound is armed or running
- **AND** ListView receives additional same-direction NoPhaseDiscrete tail events within the active cluster
- **THEN** ListView MUST accept and consume those events
- **AND** ListView MUST NOT restart the bounce timer
- **AND** ListView MUST NOT restart the running animation
- **AND** ListView MUST NOT increase the active overscroll beyond the configured bounded feedback value

#### Scenario: 回弹从当前位移连续归零
- **WHEN** bounce-back starts after Windows-style NoPhaseDiscrete boundary feedback
- **THEN** the animation start value MUST equal the current overscroll value on the active axis
- **AND** the animation SHALL drive overscroll continuously back to `0.0`
- **AND** ListView SHALL reset NoPhaseDiscrete boundary-bounce state after the animation finishes

#### Scenario: 反向输入立即恢复内容滚动
- **WHEN** ListView has consumed same-direction NoPhaseDiscrete boundary-tail events
- **AND** the next NoPhaseDiscrete wheelEvent requests movement back into the scrollable range
- **THEN** ListView MUST clear stale boundary-bounce state
- **AND** ListView MUST allow normal content scrolling instead of swallowing the event

#### Scenario: Fluent 滚动条不被回弹动画拖离边界
- **WHEN** Windows-style NoPhaseDiscrete rebound is armed, running, or settling
- **THEN** the native scrollbar and Fluent scrollbar SHALL remain synchronized at the relevant boundary
- **AND** only the viewport overscroll offset SHALL animate for the rebound effect

### Requirement: Qt5/Qt6 双版本兼容

ListView SHALL 在 Qt5 与 Qt6 上行为一致且不 crash。`wheelEvent` 内部 SHALL NOT 调用 Qt6-only API，时间戳 SHALL 通过 `QDateTime::currentMSecsSinceEpoch()` 获取（不依赖 `event->timestamp()`）。

#### Scenario: Qt5 编译通过
- **WHEN** 用 Qt5 编译 fluent_qt_lib
- **THEN** ListView 编译通过，不报缺失符号或 API 不存在错误

#### Scenario: Qt5 运行时滚动不 crash
- **WHEN** Qt5 运行时收到任意类型 wheelEvent（鼠标滚轮 / 触控板）
- **THEN** ListView 正常滚动，不 crash

#### Scenario: 时间戳跨平台一致
- **WHEN** wheelEvent 处理需要时间戳用于 cluster 判定
- **THEN** ListView SHALL 使用 `QDateTime::currentMSecsSinceEpoch()`，不依赖 `event->timestamp()`

### Requirement: macOS 本地体验不回归

ListView SHALL 在 macOS 本地触控板与鼠标滚轮场景下保持现有行为：
- 触控板手势的 overscroll bounce 触发与回弹动画曲线不变。
- 鼠标滚轮的连续滚动行为不变。
- 已有 `TestListView` 用例全部通过。

#### Scenario: macOS 触控板边界手势仍触发 overscroll
- **WHEN** macOS 触控板在列表底部继续向下滑动
- **THEN** ListView SHALL 进入 overscroll 状态，松手后 bounce-back

#### Scenario: 已有测试用例不回归
- **WHEN** 运行 `test_list_view`
- **THEN** 所有现有测试 SHALL 通过
