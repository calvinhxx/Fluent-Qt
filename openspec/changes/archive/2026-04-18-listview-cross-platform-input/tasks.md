## 1. wheelEvent 三路分类

- [x] 1.1 在 `ListView::wheelEvent` 入口实现 `WheelKind classify(QWheelEvent*)` 内联函数（PhaseBased / NoPhasePixel / NoPhaseDiscrete）
- [x] 1.2 将现有逻辑按分类拆分为三个清晰分支，PhaseBased 与 NoPhasePixel 分支保留现有行为
- [x] 1.3 NoPhaseDiscrete 分支独立实现（不复用 NoPhasePixel 的 `phase == NoScrollPhase` 检查）

## 2. NoPhaseDiscrete cluster 节流

- [x] 2.1 在 `ListView` 添加成员 `qint64 m_lastNoPhaseTs = 0;` 与 `qreal m_clusterAccum = 0.0;`
- [x] 2.2 添加 namespace-scope 常量 `kClusterGapMs = 120` 与 `kClusterMinPx = 8.0`
- [x] 2.3 NoPhaseDiscrete 路径：基于 `QDateTime::currentMSecsSinceEpoch()` 判断是否新 cluster，更新累积量
- [x] 2.4 边界 overscroll 触发改用 `m_clusterAccum` 与 `kClusterMinPx` 判据，而非单事件 `scrollPx`

## 3. bounce 期间事件消费

- [x] 3.1 扩展 "bounce running" 分支：NoPhasePixel 与 NoPhaseDiscrete 都 `event->accept()` 并 return
- [x] 3.2 PhaseBased 路径保留 `m_bounceAnim->stop()` + 继续后续逻辑的能力

## 4. Qt5/Qt6 兼容与 crash 修复

- [x] 4.1 在所有 `m_bounceAnim` / `m_bounceTimer` 访问点加 null 守卫（含 wheelEvent / startBounceBack / showEvent / 析构等）
- [x] 4.2 构造函数中确认 bounce 动画/timer 初始化在 `setModel`、信号连接前完成
- [x] 4.3 `~ListView()` 显式 stop 动画与 timer
- [x] 4.4 移除 wheelEvent 中对 `event->timestamp()` 的潜在依赖（改用 `QDateTime::currentMSecsSinceEpoch()`）
- [ ] 4.5 复现 Qt5 crash 并定位栈（如本地无 Qt5 环境，先依赖加固守卫与代码评审推断）
- [ ] 4.6 如 4.5 发现额外 crash 根因，回到 design 迭代并补 task

## 5. 测试

- [x] 5.1 在 `tests/views/collections/TestListView.cpp` 新增 `WheelEventClassification` 测试组：注入三类事件验证分类正确
- [x] 5.2 新增 `RdpHighFreqNoBounceFlap` 测试：模拟 Mac RDP 5 个 NoPhaseDiscrete 事件（30ms 间隔，angleDelta=±60），验证不进入 overscroll
- [x] 5.3 新增 `WindowsTouchpadClusterScroll` 测试：模拟连续 NoPhaseDiscrete + ±120 angleDelta 高频序列，验证滚动平滑
- [x] 5.4 新增 `MouseWheelDiscreteScroll` 测试：模拟单次 ±120 angleDelta 事件，验证按普通滚动处理
- [x] 5.5 新增 `BounceConsumesNoPhaseEvents` 测试：bounce 运行期间注入 NoPhase 事件，验证被吞且 bounce 不被打断
- [x] 5.6 新增 `BounceInterruptedByPhaseBased` 测试：bounce 运行期间注入 PhaseBased 事件，验证 bounce 被停止并进入新流程
- [x] 5.7 新增 `MacOsTrackpadNoRegression` 测试：模拟 ScrollBegin/Update/End 边界手势，验证 overscroll/bounce 行为不变
- [x] 5.8 运行 `cmake --build build --target test_list_view && SKIP_VISUAL_TEST=1 ./build/tests/views/collections/test_list_view`，所有用例通过
- [x] 5.9 运行公共头依赖相关的回归（如 ComboBox 弹层使用 ListView）：`cmake --build build --target test_combo_box && SKIP_VISUAL_TEST=1 ./build/tests/views/basicinput/test_combo_box`

## 6. 文档与归档准备

- [x] 6.1 在 `src/view/collections/ListView.cpp` 顶部 wheelEvent 上方添加注释，说明三路分类与 cluster 节流策略（指向本 change 的 design.md）
- [x] 6.2 验证 `openspec validate listview-cross-platform-input` 通过
- [x] 6.3 运行 `openspec status --change listview-cross-platform-input` 确认 4/4 artifacts done
