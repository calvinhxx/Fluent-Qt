## 1. 组件 API 与结构

- [x] 1.1 新增 `src/view/status_info/ProgressBar.h`，定义 `ProgressBar : public QWidget, public FluentElement, public view::QMLPlus`
- [x] 1.2 添加 `isIndeterminate`、`minimum`、`maximum`、`value`、`showPaused`、`showError`、`barWidth`、`trackThickness`、`railVisible` 的 `Q_PROPERTY`、getter/setter 和 changed 信号
- [x] 1.3 实现默认值：determinate、0-100 range、value 0、Running、barWidth 220、trackThickness 3px、railVisible true
- [x] 1.4 添加 `setRange()`、`progressRatio()`、`progressText()` 和 `isAnimationRunning()` 辅助接口，便于外部组合和测试

## 2. 进度模型与状态语义

- [x] 2.1 实现 range/value clamp 逻辑，保持 `maximum > minimum` 且 `value` 始终处于有效范围
- [x] 2.2 实现 determinate 进度比例计算，并在 value/range 变化时触发重绘
- [x] 2.3 实现 `showPaused` / `showError` 状态解析，确保 Error 优先于 Paused
- [x] 2.4 对非正 `barWidth`、非正 `trackThickness` 和非有限数值输入做防护，避免产生无效绘制状态

## 3. 自绘与主题

- [x] 3.1 新增 `src/view/status_info/ProgressBar.cpp`，在 `paintEvent()` 中启用抗锯齿并绘制中心 rail 与圆角 progress track
- [x] 3.2 实现 determinate 绘制：根据 `progressRatio()` 从左到右绘制 filled track，0 进度不绘制 filled track
- [x] 3.3 实现 indeterminate 绘制：使用约 44% 可用宽度的移动 segment 覆盖 Figma running bar 的静态比例
- [x] 3.4 实现 `sizeHint()` / `minimumSizeHint()`，默认返回 220x32，并支持自定义 `barWidth`
- [x] 3.5 实现 Running/Paused/Error/Disabled 和 rail 颜色缓存，使用 `themeColors()` 中的 accent、systemCaution、systemCritical、accentDisabled 和 stroke 语义色
- [x] 3.6 实现 `onThemeUpdated()`，刷新缓存颜色并请求重绘

## 4. Indeterminate 动画生命周期

- [x] 4.1 添加轻量 timer 和动画相位字段，visible enabled indeterminate running 时启动动画
- [x] 4.2 在 timer tick 中推进相位并请求重绘，使用 `themeAnimation().normal` 推导循环周期
- [x] 4.3 在 determinate、Paused、Error、disabled、hide/destroy 场景停止 timer
- [x] 4.4 确认重复设置相同属性不会重复启动 timer 或触发冗余信号

## 5. 测试与可视化

- [x] 5.1 新增 `tests/views/status_info/TestProgressBar.cpp`，按项目 GTest 规范初始化 `QApplication`、资源和 Segoe 字体
- [x] 5.2 覆盖默认属性、属性信号、range/value clamp、progress ratio、bar width、track thickness 和 railVisible 行为
- [x] 5.3 覆盖 showPaused/showError 优先级、enabled/disabled、theme update 和 indeterminate 动画启停
- [x] 5.4 新增 WinUI Gallery 风格测试：`NumberBox` 或等价数值输入驱动 determinate value，验证 `progressText()` 随值变化
- [x] 5.5 新增 VisualCheck，展示 Light/Dark、Determinate/Indeterminate、Running/Paused/Error、disabled、rail visible/hidden、130px WinUI Gallery 示例宽度和 220px Figma 默认宽度，并加入 `SKIP_VISUAL_TEST` 守卫
- [x] 5.6 在 `tests/views/status_info/CMakeLists.txt` 注册 `test_progress_bar`

## 6. 验证

- [x] 6.1 构建受影响目标：`cmake --build build --target test_progress_bar`
- [x] 6.2 运行自动化测试：`SKIP_VISUAL_TEST=1 ./build/tests/views/status_info/test_progress_bar`
- [x] 6.3 按需运行 VisualCheck：`./build/tests/views/status_info/test_progress_bar --gtest_filter="*VisualCheck*"`
- [x] 6.4 运行 `openspec validate add-progress-bar` 确认规格通过
