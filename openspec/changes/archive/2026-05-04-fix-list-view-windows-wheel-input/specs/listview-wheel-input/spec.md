## MODIFIED Requirements

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
- **WHEN**距上一 NoPhaseDiscrete event 已超过 `kClusterGapMs`
- **THEN** ListView MUST reset the NoPhaseDiscrete boundary cluster state before processing the new event

#### Scenario: 方向反转立即恢复滚动
- **WHEN** ListView consumed NoPhaseDiscrete boundary-tail events at one boundary
- **AND** the next NoPhaseDiscrete event requests movement back into the scrollable range
- **THEN** ListView MUST reset stale boundary state
- **AND** ListView MUST allow normal content scrolling in the reverse direction

### Requirement: bounce 运行期间消费高频事件

ListView SHALL protect a running bounce animation from stale Windows/RDP fallback events, while still allowing input that can move back into content to recover promptly.

#### Scenario: bounce 期间 NoPhaseDiscrete tail 被吞
- **WHEN** bounce animation is running and ListView receives a same-direction NoPhaseDiscrete tail event that still requests movement beyond the active boundary
- **THEN** ListView MUST accept the event and return without modifying overscroll or stopping bounce

#### Scenario: bounce 期间 NoPhasePixel tail 被吞
- **WHEN** bounce animation is running and ListView receives a same-direction NoPhasePixel event that still requests movement beyond the active boundary
- **THEN** ListView MUST accept the event and return without modifying overscroll or stopping bounce

#### Scenario: bounce 期间 NoPhaseDiscrete 反向恢复
- **WHEN** bounce animation is running and ListView receives a NoPhaseDiscrete event requesting movement back into the scrollable range
- **THEN** ListView MUST stop or finish the stale boundary state
- **AND** ListView MUST allow normal content scrolling instead of swallowing the event

#### Scenario: bounce 期间 PhaseBased 事件可打断
- **WHEN** bounce animation is running and ListView receives `phase = ScrollUpdate` with continuous gesture information
- **THEN** ListView MUST stop the bounce animation and continue phase-based overscroll processing

## ADDED Requirements

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
