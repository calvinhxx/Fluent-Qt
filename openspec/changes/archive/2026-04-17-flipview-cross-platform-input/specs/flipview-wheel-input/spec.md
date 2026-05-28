## ADDED Requirements

### Requirement: macOS 触控板 phase-based 翻页

FlipView SHALL 使用 phase-based 路径处理 macOS 触控板事件（ScrollBegin → ScrollUpdate → ScrollMomentum → ScrollEnd）。每次完整手势（Begin…End）最多触发一次翻页。

#### Scenario: macOS 触控板向左滑动翻到下一页
- **WHEN** FlipView 水平模式，收到 ScrollBegin 后连续 ScrollUpdate 事件，累积 pixelDelta.x 为负且绝对值 ≥ kGestureThreshold
- **THEN** FlipView SHALL 调用 goNext() 翻到下一页，后续 ScrollUpdate 事件在同一手势内不再触发翻页

#### Scenario: macOS 触控板惯性滚动不触发额外翻页
- **WHEN** 一次手势已触发翻页后，收到 ScrollMomentum 和 ScrollEnd 事件
- **THEN** FlipView SHALL 忽略这些事件，不触发额外翻页

### Requirement: Windows 精密触控板 NoScrollPhase 翻页

FlipView SHALL 正确处理 Windows 精密触控板产生的 NoScrollPhase 事件序列（WM_MOUSEWHEEL 映射），每次手势最多触发一次翻页。

#### Scenario: Windows 触控板单次两指滑动翻一页
- **WHEN** FlipView 收到一组高频（间隔 < kClusterGapMs）NoScrollPhase 事件，angleDelta 累积绝对值 ≥ kGestureThreshold
- **THEN** FlipView SHALL 翻一页，且同一 cluster 内后续事件不再触发额外翻页

#### Scenario: Windows 触控板快速连续两次手势
- **WHEN** 第一组 cluster 触发翻页后，间隔 > kClusterGapMs 收到新的 cluster
- **THEN** 若动画已结束，FlipView SHALL 立即执行翻页
- **THEN** 若动画仍在播放，FlipView SHALL 消费该事件但不设置 pending、不触发新翻页（NoScrollPhase 无法可靠区分"新手势"与"同一手势延迟到达的尾巴"）

#### Scenario: Windows 触控板 RDP 转发事件间隔较大
- **WHEN** 事件间隔 20-100ms（RDP 抖动下仍小于 kClusterGapMs=120ms）
- **THEN** FlipView SHALL 将这些事件识别为同一 cluster，不误拆为多个手势

#### Scenario: Mac RDP → Windows 触控板单次轻拨
- **WHEN** Mac 用户通过 RDP 在 Windows 上拨动触控板一次（事件特征：NoScrollPhase + 无 pixelDelta + angleDelta=±120，高频，持续 400-600ms）
- **THEN** FlipView SHALL 仅翻一页，不链式翻页到底

### Requirement: 鼠标滚轮单格翻页

FlipView SHALL 对鼠标滚轮的单次事件（angleDelta = ±120，NoScrollPhase）响应一次翻页。

#### Scenario: 鼠标滚轮向下滚动一格
- **WHEN** FlipView 收到单个 NoScrollPhase 事件，angleDelta.y = -120，无 pixelDelta
- **THEN** FlipView SHALL 调用 goNext()（|angleDelta|=120 ≥ kGestureThreshold=50，单事件即满足阈值）

#### Scenario: 鼠标滚轮快速连续滚动（动画期间）
- **WHEN** 鼠标滚轮在翻页动画播放期间产生新的 ±120 事件（与上次事件间隔 > kClusterGapMs）
- **THEN** FlipView SHALL 消费该事件但不触发翻页、不设 pending（NoScrollPhase 路径与 RDP 触控板共用，为避免链式翻页采用保守策略）
- **THEN** 动画结束后用户可再次滚动触发下一次翻页

### Requirement: 动画期间手势排队（仅 phase-based）

FlipView SHALL 在翻页动画播放期间，仅对 phase-based 路径（有明确 ScrollBegin 边界的触控板手势）排队手势输入，动画结束后执行排队的翻页。NoScrollPhase 路径因无法区分"新手势"与"同一手势延迟事件"，不参与 pending 机制。

#### Scenario: phase-based 动画期间收到新手势
- **WHEN** 翻页动画正在播放，phase-based 路径（macOS 原生触控板 ScrollBegin → ScrollUpdate 累积超阈值）触发翻页
- **THEN** FlipView SHALL 将翻页方向记录到 m_pendingFlipDir

#### Scenario: NoScrollPhase 动画期间不设 pending
- **WHEN** 翻页动画正在播放，NoScrollPhase 路径（鼠标滚轮 / Windows 触控板 WM_MOUSEWHEEL / RDP）收到事件并累积超阈值
- **THEN** FlipView SHALL 标记 m_npConsumed=true 消费事件，但不设置 m_pendingFlipDir

#### Scenario: 动画结束后执行 pending
- **WHEN** 翻页动画结束，m_pendingFlipDir ≠ 0
- **THEN** FlipView SHALL 立即执行 pending 方向的翻页并清零 m_pendingFlipDir

### Requirement: 跨平台行为一致

FlipView 的翻页行为 SHALL 在 macOS 和 Windows 上保持一致：相同的手势产生相同的翻页结果。

#### Scenario: 两指向左滑动在两个平台上都翻到下一页
- **WHEN** 用户在 macOS 或 Windows 上两指向左滑动
- **THEN** FlipView SHALL 翻到下一页（水平模式）
