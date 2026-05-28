## Why

FlipView 的 `wheelEvent` 当前依赖 `QWheelEvent::phase()` 来区分触控板手势（phase-based）和鼠标滚轮（NoScrollPhase）。macOS 触控板和 Windows 鼠标滚轮工作正常，但 **Windows 精密触控板（Precision Touchpad）** 存在兼容性问题：大部分 Windows 触控板驱动将两指滚动映射为 `WM_MOUSEWHEEL`（全部事件为 `NoScrollPhase`），而非 `WM_POINTER`（带完整 phase 链）。当前的 NoScrollPhase cluster 检测（基于 `kClusterGapMs=60ms` 间隔阈值）在 Windows 触控板高频连续事件下容易误判 cluster 边界，导致单次手势被拆成多次翻页，或快速手势被吞掉。

## What Changes

- 统一 FlipView 的输入处理策略，使 macOS 触控板、Windows 精密触控板、鼠标滚轮三种输入源在同一套逻辑下正确工作
- 重写 NoScrollPhase 分支的 cluster 检测与去抖机制，适配 Windows 触控板的事件特征（高频、无 phase、angleDelta 为 120 的整数倍）
- 保留 phase-based 路径用于 macOS 触控板（ScrollBegin/Update/Momentum/End）和 Windows WM_POINTER 精密触控板
- 增加单元测试覆盖 Windows 触控板场景的模拟事件序列

## Capabilities

### New Capabilities
- `flipview-wheel-input`: FlipView 滚轮/触控板统一输入处理——涵盖 macOS trackpad、Windows Precision Touchpad、鼠标滚轮三种输入源的事件分发、累积、去抖、翻页触发逻辑

### Modified Capabilities

## Impact

- `src/view/collections/FlipView.h` — 可能调整 wheel 相关成员变量
- `src/view/collections/FlipView.cpp` — 重写 `wheelEvent()` 方法中 NoScrollPhase 分支
- `tests/views/collections/TestFlipView.cpp` — 新增 Windows 触控板模拟测试用例
- 不影响公开 API（`addPage`/`setCurrentIndex`/`goNext`/`goPrevious` 等）
- 无破坏性变更
