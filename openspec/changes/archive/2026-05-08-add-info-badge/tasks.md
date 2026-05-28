## 1. 组件 API 与结构

- [x] 1.1 新增 `src/view/status_info/InfoBadge.h`，定义 `InfoBadge : public QWidget, public FluentElement, public view::QMLPlus`
- [x] 1.2 定义 `InfoBadgeDisplayMode { Auto, Dot, Icon, Value }` 与 `InfoBadgeStatus { Informational, Attention, Caution, Success, Critical }`，使用 `Q_ENUM` 暴露到 Qt 元对象系统
- [x] 1.3 添加 `value`、`iconGlyph`、`displayMode`、`status`、`badgeOpacity`、`customBackgroundColor`、`customTextColor` 的 `Q_PROPERTY`、getter/setter 和 changed 信号
- [x] 1.4 实现默认值：`value=-1`、`displayMode=Auto`、`status=Attention`、空 icon、`badgeOpacity=1.0`、invalid custom colors
- [x] 1.5 添加必要的测试辅助查询函数（如 effectiveDisplayMode / effectiveBadgeSize），仅用于验证几何和模式选择

## 2. 显示模式、Value 与尺寸

- [x] 2.1 实现 Auto 模式选择逻辑：`value >= 0` 显示 Value，否则 icon 非空显示 Icon，否则显示 Dot
- [x] 2.2 实现显式 Dot/Icon/Value 模式，并定义 `displayMode=Value` 且 `value < 0` 的最小 value badge 尺寸行为
- [x] 2.3 实现 value clamp，传入小于 -1 的值时归一化到 -1，并在 value 变化时更新几何
- [x] 2.4 实现 `sizeHint()` / `minimumSizeHint()`：Dot=4x4、Icon=16x16、Value 高度 16 且宽度按文本内容扩展
- [x] 2.5 实现 `badgeOpacity` clamp 到 `[0.0, 1.0]`，opacity 改变只触发重绘，不改变 size hint

## 3. 自绘与主题色

- [x] 3.1 新增 `src/view/status_info/InfoBadge.cpp`，在 `paintEvent()` 中启用抗锯齿并绘制圆形/胶囊背景
- [x] 3.2 实现 value 文本绘制，使用成员化 font role 访问字体 token，避免硬编码 `themeFont("...")` 字符串
- [x] 3.3 实现 icon glyph 绘制，使用 Segoe Fluent Icons 字体并居中在 16x16 badge 内
- [x] 3.4 实现 Informational/Attention/Caution/Success/Critical 状态色解析，分别映射到 `systemInfo`、accent、`systemCaution`、`systemSuccess`、`systemCritical`
- [x] 3.5 实现 `customBackgroundColor` / `customTextColor` 覆盖逻辑，并保持默认文本/icon 使用 `textOnAccent`
- [x] 3.6 实现 disabled/opacity 绘制处理，并在 `onThemeUpdated()` 中刷新颜色、字体和重绘状态

## 4. 嵌入场景与可视化示例

- [x] 4.1 确保 InfoBadge 默认透明背景、固定 size policy，不创建额外窗口或依赖父控件类型
- [x] 4.2 在 VisualCheck 中实现独立 badge 样式矩阵：Dot/Icon/Value/Wide Value × 五种 status × Light/Dark
- [x] 4.3 在 VisualCheck 中增加按钮右上角 badge 示例，覆盖自定义红色背景和 icon glyph
- [x] 4.4 在 VisualCheck 中增加导航行右侧 value badge 示例，并展示 `badgeOpacity` 为 0 和 1 的切换状态
- [x] 4.5 在 VisualCheck 中增加动态 value 示例，覆盖 -1、0、5、10、100 等数值对尺寸和显示模式的影响

## 5. 测试与 CMake 注册

- [x] 5.1 新增 `tests/views/status_info/TestInfoBadge.cpp`，按项目 GTest 规范初始化 `QApplication`、资源和 Segoe 字体
- [x] 5.2 覆盖默认属性、属性变更信号、重复设置无信号、displayMode 选择逻辑
- [x] 5.3 覆盖 value clamp、sentinel 回退、single value / wide value 尺寸映射和 opacity 不改变尺寸
- [x] 5.4 覆盖状态切换、自定义颜色、主题切换、disabled 绘制状态不崩溃
- [x] 5.5 新增 VisualCheck 测试并加入 `SKIP_VISUAL_TEST` 和 offscreen 守卫，使用 `qApp->exec()` 阻塞
- [x] 5.6 在 `tests/views/status_info/CMakeLists.txt` 注册 `add_qt_test_module(test_info_badge TestInfoBadge.cpp)`

## 6. 验证

- [x] 6.1 如新增测试目标未被现有 build 识别，运行 `cmake -B build -DBUILD_TESTING=ON`
- [x] 6.2 构建受影响目标：`cmake --build build --target test_info_badge`
- [x] 6.3 运行自动化测试：`SKIP_VISUAL_TEST=1 ./build/tests/views/status_info/test_info_badge`
- [ ] 6.4 按需运行 VisualCheck：`./build/tests/views/status_info/test_info_badge --gtest_filter="*VisualCheck*"`
- [x] 6.5 运行 `openspec validate add-info-badge` 确认规格通过