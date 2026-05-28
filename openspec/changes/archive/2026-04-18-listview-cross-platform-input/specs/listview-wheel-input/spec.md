# ListView Wheel Input

## ADDED Requirements

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
- **THEN** 事件按 NoPhaseDiscrete 路径处理，应用 cluster 节流逻辑

#### Scenario: 鼠标滚轮事件走 NoPhaseDiscrete 路径
- **WHEN** ListView 收到 `phase = NoScrollPhase`、`pixelDelta` 为空、`angleDelta = ±120` 倍数的 wheelEvent
- **THEN** 事件与 RDP 路径共用同一处理逻辑（不可区分）

### Requirement: NoPhaseDiscrete cluster 节流

ListView SHALL 在 NoPhaseDiscrete 路径上以 `kClusterGapMs = 120` 毫秒为间隔归并事件，并以 cluster 累积量 `m_clusterAccum` 作为 overscroll 触发判据。

#### Scenario: 同一 cluster 内多个事件归并
- **WHEN** 在 120ms 内收到多个连续 NoPhaseDiscrete 事件
- **THEN** ListView SHALL 累积 scrollPx 到 `m_clusterAccum`，最多在边界触发一次 overscroll 进入

#### Scenario: 跨 cluster 重置累积量
- **WHEN** 距上一 NoPhaseDiscrete 事件已超过 120ms
- **THEN** ListView SHALL 重置 `m_clusterAccum = 0`，开启新 cluster

#### Scenario: 微小累积量不触发 overscroll
- **WHEN** cluster 内累积绝对值 < `kClusterMinPx = 8` 且滚动条已在边界
- **THEN** ListView SHALL NOT 进入 overscroll 状态，事件被消费

#### Scenario: Mac RDP 单次轻拨不触发 bounce
- **WHEN** Mac RDP → Windows 转发的单次触控板轻拨产生 5 个 NoPhaseDiscrete 事件，每个 angleDelta=±60，间隔 30ms
- **THEN** ListView SHALL 在 cluster 内累积像素，仅当累积量超过 `kClusterMinPx` 且在边界时进入 overscroll，否则按普通滚动处理

### Requirement: bounce 运行期间消费高频事件

ListView SHALL 在 `m_bounceAnim` 运行期间消费所有 NoScrollPhase 事件（含 NoPhasePixel 与 NoPhaseDiscrete），仅允许 PhaseBased 事件打断 bounce。

#### Scenario: bounce 期间 NoPhaseDiscrete 事件被吞
- **WHEN** bounce 动画运行中，收到 NoPhaseDiscrete wheelEvent
- **THEN** ListView SHALL `event->accept()` 并 return，不修改 overscroll，不停止 bounce

#### Scenario: bounce 期间 NoPhasePixel 事件被吞
- **WHEN** bounce 动画运行中，收到 NoPhasePixel wheelEvent
- **THEN** ListView SHALL `event->accept()` 并 return，不修改 overscroll，不停止 bounce

#### Scenario: bounce 期间 PhaseBased 事件可打断
- **WHEN** bounce 动画运行中，收到 phase = ScrollUpdate 且 pixelDelta 非空的 wheelEvent
- **THEN** ListView SHALL `m_bounceAnim->stop()` 并继续后续 overscroll 处理逻辑

### Requirement: bounce 动画与 timer 生命周期守卫

ListView SHALL 对 `m_bounceAnim` 与 `m_bounceTimer` 的所有访问点进行 null 守卫，确保在构造未完成或析构期间访问不会 crash。

#### Scenario: wheelEvent 在 bounceAnim 未初始化时不 crash
- **WHEN** ListView 构造期间被父窗口转发 wheelEvent，且此时 `m_bounceAnim == nullptr`
- **THEN** wheelEvent SHALL 安全跳过 bounce 相关分支，按普通滚动处理或调用基类实现

#### Scenario: 析构时停止动画与 timer
- **WHEN** ListView 析构
- **THEN** ListView SHALL stop `m_bounceAnim` 与 `m_bounceTimer`，不允许 pending tick 在析构后触发

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
