## ADDED Requirements

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

## MODIFIED Requirements

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
