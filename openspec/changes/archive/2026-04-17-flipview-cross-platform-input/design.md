## Context

FlipView 使用 `QWheelEvent` 处理鼠标滚轮和触控板输入。Qt 对不同平台/设备的事件上报方式不同：

| 平台 + 设备 | QWheelEvent::phase() | pixelDelta | angleDelta |
|---|---|---|---|
| macOS 触控板 | ScrollBegin → ScrollUpdate → ScrollMomentum → ScrollEnd | 精确像素 | 近似角度 |
| Windows 精密触控板 (WM_POINTER) | 同 macOS | 精确像素 | 近似角度 |
| Windows 精密触控板 (WM_MOUSEWHEEL) | **全部 NoScrollPhase** | null | 120 的整数倍 |
| Windows/macOS 鼠标滚轮 | NoScrollPhase | null | ±120 |

**问题核心**：Windows 上大部分精密触控板驱动将两指滚动映射为 `WM_MOUSEWHEEL`，所有事件都是 `NoScrollPhase`。Mac RDP → Windows 场景下触控板事件同样走 `WM_MOUSEWHEEL` 路径，且特征与物理鼠标滚轮完全一致（`NoScrollPhase` + 无 `pixelDelta` + `angleDelta=±120`），无法从事件本身区分。

事件间隔特征：
- Windows 触控板 WM_MOUSEWHEEL：8-16ms
- RDP 转发：20-30ms（网络抖动下可达 60-100ms）
- 鼠标滚轮单次刻意连击：>100ms
- 一次完整手势总时长：400-600ms（大于动画窗口 ~300ms）

当前已知问题：
1. Windows 触控板快速连续滑动时，`kClusterGapMs` 未能正确分隔手势，导致单次手势触发多次翻页
2. 动画播放期间触控板事件的 pending 机制与 cluster 检测交互不当
3. **Mac RDP → Windows 实测**：单次触控板轻拨触发链式翻页到底；即便修复链式问题后仍有概率一次拨动翻两页（动画结束后手势尾巴 + RDP 网络抖动 → 误判新 cluster）

## Goals / Non-Goals

**Goals:**
- 三种输入源（macOS trackpad、Windows Precision Touchpad、鼠标滚轮）均可正确翻页，每次手势恰好翻一页
- Windows 触控板高频 NoScrollPhase 事件序列被正确识别为单次手势
- 动画播放期间的手势排队（pending）机制对所有输入源一致工作
- 保持现有 macOS 触控板和鼠标滚轮的正确行为不退化

**Non-Goals:**
- 不增加触摸屏拖拽翻页（touch drag/swipe）支持
- 不修改 FlipView 的视觉渲染或动画曲线
- 不修改公开 API

## Decisions

### Decision 1: 统一 NoScrollPhase 的 cluster 检测策略

**选择**：保留 cluster gap 间隔检测作为基础策略，但引入 **velocity-based cooldown** 作为补充：当一个 cluster 触发翻页后，进入冷却期（cooldown），冷却期内的事件只更新 pending 方向，不触发新翻页。冷却期时长与动画时长对齐。

**替代方案**：
- 固定 debounce 时长（如 300ms）：简单但无法适配不同滚轮速度，快速翻页时体验差
- 用 angleDelta 总量判断：鼠标滚轮单次 ±120 就应翻页，无法区分触控板连续事件
- 检测事件频率（高频 = 触控板，低频 = 鼠标）：不可靠，RDP 转发会改变频率

**理由**：cluster gap 检测已经基本正确区分了手势边界，问题出在同一 cluster 内累积过快导致重复翻页。用 cooldown 保证每个 cluster 最多翻一页即可解决。

### Decision 2: cooldown 与动画排队统一

**选择**：将 cooldown 与 `m_slideAnimation` 状态绑定——当动画播放中时，自动处于 cooldown 状态。新的 cluster（gap 超过阈值）即使在动画期间到来，也只更新 `m_pendingFlipDir`。动画结束后检查 pending 并执行。

**理由**：避免引入独立的 cooldown timer，复用已有的动画状态机。

### Decision 3 (REVISED): 鼠标滚轮走统一 cluster 路径，移除离散快速路径

**初版选择**：NoScrollPhase + 无 pixelDelta + |angleDelta|=120 单次事件走快速路径，直接翻页。

**修订**：移除快速路径，鼠标滚轮和触控板走同一套 cluster 累积逻辑。

**原因（Mac RDP → Windows 实测）**：Mac 触控板经 RDP 转发给 Windows 时，事件特征为 `NoScrollPhase + angleDelta=±120 + 无 pixelDelta`，与物理鼠标滚轮完全相同。快速路径会在每个 ±120 事件到来时重置 `m_npConsumed=false` 并直接翻页，导致单次触控板轻拨触发链式翻页到底。

**新逻辑**：统一走 cluster 累积。由于 `|120| >= kGestureThreshold(50)`，鼠标滚轮单次事件仍在第一个事件时触发翻页（体验不变）；但 RDP 触控板的后续 ±120 事件被同 cluster 的 `m_npConsumed=true` 拦截。

**备选（已淘汰）**：
- 保留快速路径 + RDP 特征检测 → 无法可靠区分物理鼠标滚轮和 RDP 触控板（事件特征完全相同）

### Decision 4: NoScrollPhase 事件在动画期间不设置 pending

**初版（Decision 2）**：动画期间的 NoScrollPhase 新 cluster 设 `m_pendingFlipDir`，动画结束后执行。

**修订**：NoScrollPhase 动画期间只标记 `m_npConsumed`，**不设置 pending**。pending 机制仅保留给 phase-based 路径。

**原因（Mac RDP → Windows 实测）**：RDP 网络抖动可导致单次手势内部出现 >kClusterGapMs 的事件间隙，被误判为"新 cluster"。若此时设 pending，动画结束后执行 pending → 新动画 → 新事件又形成新 cluster 又设 pending → 链式翻页到底。

**权衡**：NoScrollPhase 无法区分"新手势"和"同一手势经网络延迟到达的事件尾巴"，因此保守策略是放弃 pending。phase-based 路径因有 `ScrollBegin` 显式边界，仍然可以可靠设 pending。

**副作用**：鼠标滚轮快速连转多格时，动画期间的事件被吞掉，直到动画结束后才响应下一格。这是可接受代价——FlipView 本身有 ~300ms 动画，比动画更快连击并非支持的交互语义。

### Decision 5: kClusterGapMs 从 60ms 提升至 120ms

**初版**：`kClusterGapMs = 60`，RDP 转发事件间隔 20-30ms 被识别为同 cluster。

**修订**：提升至 `120ms`。

**原因**：Mac RDP → Windows 实测中仍有概率"轻拨一次翻两页"。推演发现：一次完整手势总时长 400-600ms，动画窗口 ~300ms。动画结束后手势尾巴仍在到达，且 RDP 网络抖动可让事件间隙达到 60-100ms（>60ms 旧阈值 < 120ms 新阈值），被误判为新 cluster → 触发二次翻页。

**权衡**：120ms 覆盖 RDP 抖动常见范围；鼠标滚轮两次刻意快速连击间隔通常 >120ms（物理滚轮每格惯性），极端快速连击（<120ms）的第二格被当作同 cluster 被吞——可接受。

**后续可调范围**：若极端网络下仍出现双翻，可进一步提升至 150-180ms，或引入 post-flip lock（动画结束后 150-200ms 内禁止新 cluster reset）。

## Risks / Trade-offs

- **[Risk] kClusterGapMs=120ms 在极端网络抖动下仍可能不够** → Mitigation: 可进一步提升或引入 post-flip lock
- **[Risk] 鼠标滚轮极端快速连击（<120ms 间隔）的第二格被吞** → Mitigation: 与 FlipView 动画时长匹配，非支持的交互语义
- **[Risk] NoScrollPhase 放弃 pending 后，Windows 原生触控板用户动画期间的快速二连滑动只翻一页** → Mitigation: 与 phase-based 路径的行为差异可接受；Windows 原生触控板若走 WM_POINTER 路径则仍享有 pending
- **[Trade-off] 不区分 Windows WM_POINTER 和 WM_MOUSEWHEEL 触控板** → phase-based 路径已正确处理 WM_POINTER，只需修复 NoScrollPhase 路径
- **[Trade-off] 物理鼠标滚轮与 RDP 触控板事件特征相同，无法区分** → 采用保守策略：NoScrollPhase 路径牺牲部分鼠标滚轮的 pending 响应能力，换取 RDP 触控板不链式翻页
