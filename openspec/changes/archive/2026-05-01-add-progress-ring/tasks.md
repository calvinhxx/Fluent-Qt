## 1. 组件 API 与结构

- [x] 1.1 新增 `src/view/status_info/ProgressRing.h`，定义 `ProgressRing : public QWidget, public FluentElement, public view::QMLPlus`
- [x] 1.2 定义 `ProgressRingSize { Small, Medium, Large }` 与 `ProgressRingStatus { Running, Paused, Error }`，使用 `Q_ENUM` 暴露到 Qt 元对象系统
- [x] 1.3 添加 `isActive`、`isIndeterminate`、`minimum`、`maximum`、`value`、`ringSize`、`strokeWidth`、`status`、`backgroundVisible` 的 `Q_PROPERTY`、getter/setter 和 changed 信号
- [x] 1.4 实现默认值：inactive、indeterminate、0-100 range、value 0、Medium、3px stroke、Running、transparent background

## 2. 进度模型与尺寸

- [x] 2.1 实现 range/value clamp 逻辑，保持 `maximum > minimum` 且 `value` 始终处于有效范围
- [x] 2.2 实现 determinate 进度比例计算，并在属性变化时触发更新
- [x] 2.3 实现 `sizeHint()` / `minimumSizeHint()`，将 Small/Medium/Large 映射为 16x16、32x32、64x64
- [x] 2.4 实现 stroke width 校验，忽略 0 或负数并避免绘制裁剪

## 3. 自绘与主题

- [x] 3.1 新增 `src/view/status_info/ProgressRing.cpp`，在 `paintEvent()` 中启用抗锯齿并使用圆帽 `QPen` 绘制轨道与指示弧
- [x] 3.2 实现 backgroundVisible 轨道绘制，透明背景时不绘制完整圆形轨道
- [x] 3.3 实现 Running/Paused/Error/Disabled 颜色解析，分别使用 accent、systemCaution、systemCritical 和 disabled 语义色
- [x] 3.4 实现 `onThemeUpdated()`，刷新缓存颜色并请求重绘

## 4. Indeterminate 动画

- [x] 4.1 添加轻量 timer 和动画相位字段，active indeterminate running enabled 时启动动画
- [x] 4.2 在 timer tick 中推进相位并请求重绘，绘制固定长度旋转弧段
- [x] 4.3 在 inactive、determinate、Paused、Error、disabled、hide/destroy 场景停止 timer
- [x] 4.4 确认重复设置相同属性不会重复启动 timer 或触发冗余信号

## 5. 测试与可视化

- [x] 5.1 新增 `tests/views/status_info/TestProgressRing.cpp`，按项目 GTest 规范初始化 `QApplication`、资源和 Segoe 字体
- [x] 5.2 覆盖默认属性、属性信号、range/value clamp、尺寸映射、stroke width、backgroundVisible 行为
- [x] 5.3 覆盖 active 动画启停、determinate/indeterminate 切换、Paused/Error/disabled 停止动画场景
- [x] 5.4 新增 VisualCheck，展示 Light/Dark、Small/Medium/Large、Determinate/Indeterminate、Running/Paused/Error、透明/可见背景轨道和 `NumberBox` 调节 determinate ring 的 WinUI Gallery 风格示例，并加入 `SKIP_VISUAL_TEST` 守卫
- [x] 5.5 在 `tests/views/status_info/CMakeLists.txt` 注册 `test_progress_ring`

## 6. 验证

- [x] 6.1 构建受影响目标：`cmake --build build --target test_progress_ring`
- [x] 6.2 运行自动化测试：`SKIP_VISUAL_TEST=1 ./build/tests/views/status_info/test_progress_ring`
- [ ] 6.3 按需运行 VisualCheck：`./build/tests/views/status_info/test_progress_ring --gtest_filter="*VisualCheck*"`
- [x] 6.4 运行 `openspec validate add-progress-ring` 确认规格通过
