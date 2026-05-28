## Context

`ListView::wheelEvent` 当前承担三种职责：处理触控板手势 / 鼠标滚轮的滚动委托、在边界触发 overscroll 弹性形变、以及通过 `m_bounceAnim` + `m_bounceTimer` 完成 bounce-back 动画。

实测分平台行为差异：

| 平台 / 输入 | `phase()` | `pixelDelta()` | `angleDelta()` | 触发频率 | 当前问题 |
|---|---|---|---|---|---|
| macOS 触控板（本地） | `ScrollBegin/Update/Momentum/End` | 非空（按像素累积） | 不可靠 | 60Hz 平滑 | ✓ 正常 |
| macOS 鼠标滚轮（本地） | `NoScrollPhase` | 空 | ±120 倍数 | 离散 | 基本正常 |
| Windows 精密触控板（本地，Qt6） | `NoScrollPhase` | 部分驱动有，部分无 | ±120 累积 | 高频（10-20ms） | overscroll 反复触发、方向混乱 |
| Mac RDP → Windows | `NoScrollPhase` | **始终为空** | ±120 离散 | 高频（20-100ms 抖动） | 单次轻拨即触发 bounce、卡边界 |
| Windows Qt5 路径 | `NoScrollPhase` | 行为与 Qt6 不一致 | ±120 | — | **crash**（疑似动画/timer 生命周期或虚函数调用差异） |

刚归档的 `flipview-cross-platform-input` 已沉淀的关键经验：
- 物理鼠标滚轮与 RDP 转发触控板事件在 wheelEvent 层面**不可区分**，必须用统一 NoScrollPhase 路径处理。
- `kClusterGapMs = 120ms` 是覆盖 RDP 抖动（60-100ms）且不影响正常鼠标滚轮的安全阈值。
- 动画运行期间消费高频事件，避免链式触发。

ListView 与 FlipView 的差异：
- FlipView 是**离散翻页**（每次手势翻一页），ListView 是**连续滚动 + 边界 bounce**。
- ListView 的"误触发"代价是 overscroll 形变 + bounce 动画，而非翻页跳跃。
- ListView 已存在 phase-based 路径下的 bounce 触发逻辑，**不能回归 macOS 本地体验**。

### 关键约束

- **必须保持 Qt5 编译通过**：`fluent_qt_lib` 当前可在 Qt5/Qt6 双版本构建（已通过 `QtCompat.h` 抽象 `FluentEnterEvent`）。本次修复不能引入 Qt6-only 的 wheel API 调用。
- **必须保持 macOS 本地手势体验**：phase-based 路径（`ScrollBegin/Update/Momentum/End`）的 overscroll/bounce 行为不允许变化。
- **ListView 是公共组件**：被 ComboBox 弹层、TreeView 内部等多处复用，wheelEvent 副作用需谨慎。

## Goals / Non-Goals

**Goals:**
- 修复 Mac RDP → Windows 场景下 ListView 滚动的"卡边界 / 反复 bounce / 方向错乱"。
- 修复 Windows 本地 Qt6 精密触控板下的 overscroll 反复触发。
- 修复 Windows 本地 Qt5 的 crash，保持 Qt5/Qt6 双版本可用。
- 保持 macOS 本地触控板 / 鼠标滚轮体验完全不变。
- 通过模拟测试覆盖 RDP / Windows 触控板 / 鼠标滚轮 / Qt5 路径，避免后续回归。

**Non-Goals:**
- 不引入硬件级触控板检测（无可靠 API）。
- 不重写 overscroll bounce 视觉曲线（保留现有阻尼系数 `(1-ratio)²` 与回弹动画时长）。
- 不改 `ListView` 公共 API、signal、Q_PROPERTY。
- 不修改 `QListView` 基类行为；所有改动局限在 `ListView::wheelEvent` 与构造/初始化。
- 不改 `ScrollBar` 实现。
- **不引入 QML Flickable 风格的"鼠标按住拖拽 = 平移内容"**：以 WinUI 桌面端语义为准（左键按下 = 选中 / 框选 / canReorderItems 拖拽换位）。详见 Decision 6。

## Decisions

### Decision 1: 在 wheelEvent 入口将事件分类为三种路径

**选择**：

```cpp
enum class WheelKind { PhaseBased, NoPhasePixel, NoPhaseDiscrete };
WheelKind classify(QWheelEvent* e) {
    if (e->phase() != Qt::NoScrollPhase) return WheelKind::PhaseBased;
    if (!e->pixelDelta().isNull())      return WheelKind::NoPhasePixel;
    return WheelKind::NoPhaseDiscrete;  // 鼠标滚轮 / Mac RDP / Qt5 默认
}
```

- **PhaseBased**：保持当前实现（macOS 本地触控板）。
- **NoPhasePixel**：保持当前实现，scrollPx 直接来自 pixelDelta（Windows 部分精密触控板驱动）。
- **NoPhaseDiscrete**：**新逻辑** — 引入 cluster 节流。

**理由**：
- 把 RDP / 鼠标滚轮 / Qt5 路径三者合并为一类（它们在事件层面不可区分），用同一套保守策略。
- 显式分类避免后续调整时遗漏分支。

**备选（已淘汰）**：
- 用 `QWheelEvent::source()` 区分 `MouseEventSynthesizedBySystem` → 在 RDP 场景不可靠。
- 检测 angleDelta 是否为 ±120 倍数 → Qt5 部分驱动会上报非 120 值。

### Decision 2: NoPhaseDiscrete 路径引入 cluster 节流（120ms gap）

**选择**：

```cpp
// 成员
qint64 m_lastNoPhaseTs = 0;       // 上次 NoPhaseDiscrete 事件时间（ms）
qreal  m_clusterAccum = 0.0;      // 当前 cluster 内累积像素
constexpr int kClusterGapMs = 120;
constexpr qreal kClusterMinPx = 8.0;  // 单 cluster 至少累积 8px 才进入 overscroll

// NoPhaseDiscrete 处理
const qint64 now = QDateTime::currentMSecsSinceEpoch();
if (now - m_lastNoPhaseTs > kClusterGapMs) m_clusterAccum = 0.0;
m_lastNoPhaseTs = now;
m_clusterAccum += scrollPx;

// 边界触发 overscroll 改用 m_clusterAccum 而非单次 scrollPx
if ((atStart && m_clusterAccum > kClusterMinPx) || (atEnd && m_clusterAccum < -kClusterMinPx)) {
    overscroll = m_clusterAccum * 0.5;
    ...
}
```

**理由**：
- RDP 高频事件（60-100ms 抖动）在 120ms 内归并为一个 cluster，不会因为单次小 delta 反复进入 overscroll。
- 鼠标滚轮"快速滚动"事件间隔 < 120ms 也归并为一个 cluster，符合用户感知的"连续滚动"。
- `kClusterMinPx = 8` 抑制 RDP 微小抖动事件。

**备选（已淘汰）**：
- 仅靠 bounceTimer 节流 → 已有但不足以抑制方向反转事件。
- 累积 angleDelta 而非 scrollPx → angleDelta 在 RDP 下方向不稳，累积像素更可靠。

### Decision 3: bounce 运行期间消费 NoPhaseDiscrete + 同步消费"高频抖动"

**选择**：扩展现有 "bounce running → consume NoScrollPhase" 分支：

```cpp
if (m_bounceAnim && m_bounceAnim->state() == QAbstractAnimation::Running) {
    if (kind == WheelKind::PhaseBased) {
        m_bounceAnim->stop();   // macOS 触控板可打断
    } else {
        event->accept();        // NoPhasePixel + NoPhaseDiscrete 一律吞掉
        return;
    }
}
```

**理由**：
- 当前实现已经吞掉 NoScrollPhase；扩展到 NoPhasePixel 是为 Windows 精密触控板某些驱动打开的 pixel 通道做对称处理。
- macOS 触控板（PhaseBased）保留打断能力 — 对应"用户手指主动按压"的真实意图。

### Decision 4: 加固 m_bounceAnim / m_bounceTimer 生命周期，修复 Qt5 crash

**选择**：

1. 所有访问点增加 null 守卫：`if (m_bounceAnim && m_bounceAnim->state() == ...)`、`if (m_bounceTimer) m_bounceTimer->start()`。
2. 构造函数中 bounce 动画/timer 初始化提前到所有可能的 `QListView` 内部信号触发之前（即在 `setModel` 之前完成）。
3. 在 `~ListView()` 显式 stop 动画/timer，避免析构时仍有 pending tick。
4. `QDateTime::currentMSecsSinceEpoch()` 替代任何 `event->timestamp()`（Qt5 在某些平台返回 0）。

**理由**：
- Qt5 crash 推测来自 wheelEvent 在 polish 完成前被触发（如父窗口在构造期间通过事件转发），导致访问空 `m_bounceAnim`。
- 即使根因不在此，加固守卫无副作用。
- `event->timestamp()` 在 Qt5 macOS/Windows 路径上不一致，统一用 system clock 更可靠。

**调试策略**：在 task 中预留"Qt5 crash 复现 + 栈定位"步骤；如果加固后仍 crash，再针对性修复。

### Decision 5: Qt5/Qt6 兼容通过 QtCompat.h 抽象，wheelEvent 不直接调用 Qt6-only API

**选择**：
- 不在 wheelEvent 使用 `event->position()`（Qt6）/ `event->pos()`（Qt5）— 当前实现未使用，保持。
- 不使用 `QSinglePointEvent` 等 Qt6-only 派生类。
- `pixelDelta()` / `angleDelta()` / `phase()` 在 Qt5/Qt6 行为一致，可直接使用。
- 如未来需要新增鼠标位置依赖，统一通过 `QtCompat.h` 新增 `fluentWheelPos(QWheelEvent*)`。

**理由**：保持单一代码路径，避免 `#if QT_VERSION` 分散。

### Decision 6: 不实现 QML Flickable 风格的"按住拖拽 = 平移内容"

**选择**：保持 WinUI ListView 桌面端语义 — 鼠标左键按下用于**选中 / 框选 / 行内拖拽换位**（`canReorderItems`），**不**叠加 drag-to-scroll / kinetic flick。滚动只通过滚轮、触控板手势、滚动条拖拽进行。

**理由**：
- WinUI 3 ListView 在桌面端（鼠标输入）本身就不带 flick；QML `ListView`/`Flickable` 的"按住拖动"是触屏原生语义，移植到桌面会与"点击选中"冲突。
- 已有 `m_canReorderItems` 占用左键 press+move，再叠加 flick 会让两种交互不可调和。
- 触控屏 / 触摸事件会自动走 Qt 的 TouchGesture 路径，已能 work，无需手动接入 `QScroller`。
- 保持组件在 ComboBox 弹层、TreeView 等复用场景下的行为可预期。

**备选（已淘汰）**：
- `QScroller::grabGesture(viewport(), LeftMouseButtonGesture)` → 会改写鼠标事件流，破坏选中 / DnD / canReorderItems 语义。
- 自实现 mousePress/Move 的阈值滚动 → 与 `m_canReorderItems` 互斥成本高，且偏离 WinUI 视觉规范。

**结论**：如未来确有触屏专用需求，再单独以 `Q_PROPERTY(bool flickEnabled)` 形式补充，且与 `canReorderItems` 互斥。

## Risks / Trade-offs

- **[风险] cluster 累积 8px 阈值可能误吞掉鼠标滚轮"轻轻一拨"** → Mitigation：8px ≈ 鼠标滚轮单次 ±120 angleDelta 转换值（120/120*20*0.5=10px），刚好在阈值之上；正常滚动不受影响。如反馈过钝，调整为 6px。
- **[风险] bounce 期间统一吞掉 NoPhasePixel 可能影响 Windows 触控板"快速反向滚动"打断 bounce 的体验** → Mitigation：bounce 动画时长 ~250ms，用户极少在 bounce 期间反向；如需打断，PhaseBased 仍可（macOS 路径）。
- **[Trade-off] cluster 节流引入了"NoPhaseDiscrete 路径下 8px 以下的微小滚动被忽略"** → 接受：RDP 抖动正是产生这类事件的主因，业务无感。
- **[风险] Qt5 crash 根因未明，加固守卫可能不足** → Mitigation：tasks 中包含"复现 + 栈定位"专项步骤；如需进一步调整需求会回到 design 迭代。

## Migration Plan

无 API 变更，无迁移成本。仅业务侧需重新编译。
