## 1. 重构 wheelEvent NoScrollPhase 分支

- [x] 1.1 在 wheelEvent 的 NoScrollPhase 分支中，增加鼠标滚轮离散事件的快速路径：检测单次 angleDelta = ±120 且无 pixelDelta，直接翻页 + 启动 cooldown，不走 cluster 累积
- [x] 1.2 重写 NoScrollPhase cluster 累积逻辑：cluster 内累积 angleDelta 达到 kGestureThreshold 后翻页，标记 m_npConsumed=true，同一 cluster 后续事件全部消费
- [x] 1.3 将 cooldown 与动画状态绑定：动画播放期间的 NoScrollPhase 事件只更新 m_pendingFlipDir，不触发新翻页

## 2. Phase-based 路径验证与桥接

- [x] 2.1 确认 phase-based 路径（ScrollBegin/Update/Momentum/End）对 macOS 触控板行为无退化
- [x] 2.2 验证 phase-based 手势结束后的 bridge cooldown（防止 Momentum → NoScrollPhase 惯性事件触发额外翻页）仍正确工作

## 3. 动画 pending 机制统一

- [x] 3.1 确保动画结束回调中检查 m_pendingFlipDir 并执行排队翻页逻辑对所有输入源一致
- [x] 3.2 验证 pending 翻页后 m_pendingFlipDir 正确清零

## 4. 测试

- [x] 4.1 新增 GTest 用例：模拟 Windows 触控板高频 NoScrollPhase 事件序列（间隔 10ms，angleDelta=30），验证单次手势仅翻一页
- [x] 4.2 新增 GTest 用例：模拟鼠标滚轮单次 angleDelta=±120 事件，验证立即翻页
- [x] 4.3 新增 GTest 用例：模拟动画期间收到新手势，验证 pending 排队正确执行
- [x] 4.4 验证已有 macOS 触控板测试用例（如有）无退化
- [x] 4.5 构建并运行全量 test_flip_view 测试通过

## 5. 迭代修复（基于 Mac RDP → Windows 实测反馈）

- [x] 5.1 移除鼠标滚轮离散快速路径（Decision 3 revised）：RDP 触控板事件特征与物理鼠标滚轮完全相同，快速路径会导致链式翻页
- [x] 5.2 新增 GTest 用例 `RdpTouchpadHighFreq120FlipsOnce`：模拟 8 个 ±120 事件间隔 10ms，验证仅翻一页
- [x] 5.3 NoScrollPhase 动画期间不设 pending（Decision 4）：避免 RDP 网络抖动造成的"新 cluster"误判引发链式翻页
- [x] 5.4 新增 GTest 用例 `NoScrollPhaseNoPendingDuringAnimation`：验证动画期间新 cluster 事件被消费但不翻页，动画结束后用户可再次操作
- [x] 5.5 kClusterGapMs 从 60ms 提升至 120ms（Decision 5）：覆盖 RDP 网络抖动范围（60-100ms），修复"一次拨动翻两页"概率问题
- [x] 5.6 同步更新 spec.md 反映 NoScrollPhase 路径不设 pending 的新行为
