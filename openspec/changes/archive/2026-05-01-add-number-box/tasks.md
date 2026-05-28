## 1. 组件 API 与文件结构

- [x] 1.1 新增 `src/view/textfields/NumberBox.h`，定义 `NumberBox : public LineEdit`
- [x] 1.2 定义 `SpinButtonPlacementMode { Hidden, Compact, Inline }` 并使用 `Q_ENUM` 暴露到 Qt 元对象系统
- [x] 1.3 添加 `value`、`minimum`、`maximum`、`smallChange`、`largeChange`、`header`、`acceptsExpression`、`spinButtonPlacementMode`、spinner metrics、`displayPrecision`、`formatStep` 的 `Q_PROPERTY`、getter/setter 和 changed 信号
- [x] 1.4 实现默认值：value NaN、unbounded range、smallChange 1、largeChange 10、Hidden spinner、expressions disabled、displayPrecision -1、formatStep 0、empty header
- [x] 1.5 实现 `sizeHint()` / `minimumSizeHint()`，保证无 header 高度 32px、有 header 高度 60px、推荐最小宽度不小于 124px

## 2. 数值模型、提交与表达式解析

- [x] 2.1 新增 `src/view/textfields/NumberBox.cpp`，连接 `editingFinished` / Enter key 路径到统一 commit 逻辑
- [x] 2.2 实现普通数字解析，空文本和无效文本提交后将 `value` 设为 NaN 并保留用户文本
- [x] 2.3 实现受限递归下降表达式 parser，支持数字、空白、括号、一元正负号、`+`、`-`、`*`、`/`、`^`
- [x] 2.4 实现 `acceptsExpression` 开关：关闭时拒绝表达式，开启时提交表达式结果
- [x] 2.5 实现 NaN 比较、重复 setter 去重和 `valueChanged` 信号触发规则

## 3. Range、step 与格式化

- [x] 3.1 实现 `minimum` / `maximum` setter 的 range 归一化和有效 value clamp
- [x] 3.2 实现 `smallChange` / `largeChange` 正数校验，忽略 0 或负数
- [x] 3.3 实现 spinner 和键盘 Up/Down/PageUp/PageDown 的 step 行为，结果 clamp 到 range
- [x] 3.4 实现 NaN 状态下 step 起点：优先 0，否则使用最接近 0 的 range 边界
- [x] 3.5 实现 `displayPrecision` 格式化，`-1` 使用紧凑十进制，非负值固定小数位
- [x] 3.6 实现 `formatStep` round half up，并覆盖 0.25 金额格式化示例

## 4. SpinButton 布局与交互状态

- [x] 4.1 使用两个 `view::basicinput::RepeatButton` 创建上/下 spinner 按钮，设置 Standard、IconOnly、Small 和 ChevronUp/ChevronDown glyph
- [x] 4.2 实现 Hidden 模式：隐藏 spinner，允许 `LineEdit` clear button 行为
- [x] 4.3 实现 Inline 模式：右侧常驻水平排列 down/up spinner，使用独立且更高的 inline 按钮尺寸，两个按钮之间留出可配置间隔，永久预留两个按钮和间隔的完整文本右边距并禁用 clear button
- [x] 4.4 实现 Compact 模式：右侧默认显示上下叠放 spinner，保持独立竖排按钮尺寸，与输入框上下边框保留可见间隙，永久预留完整文本右边距，hover 仅使用按钮自身状态色反馈
- [x] 4.5 实现 spinner hover/pressed/focus/disabled 状态同步，确保按钮不抢走文本输入焦点
- [x] 4.6 实现 Compact spinner 按钮尺寸、Inline spinner 按钮尺寸、右边距、Compact 预留宽度、按钮间距、文本间距和 icon 尺寸可配置，并让布局/边距随属性变化重算

## 5. Fluent 绘制与主题响应

- [x] 5.1 复用或扩展 `LineEdit` frame 绘制，保证 Rest/Hover/Pressed/Disabled/Focus 与 Figma Number Box 变体一致
- [x] 5.2 实现 header 绘制，参考 `PasswordBox` 的 20px header、8px gap、elide 和 disabled 颜色
- [x] 5.3 更新 spinner、clear button、text margins 和 child geometry，确保不遮挡文本、placeholder、selection 或 focus bottom border
- [x] 5.4 实现 `onThemeUpdated()`，刷新输入框、header、spinner、clear button 的字体、颜色、selection 和 icon 状态
- [x] 5.5 确认 Light/Dark、Equation、Inline Spinner、Compact Spinner 的关键状态可通过 VisualCheck 展示

## 6. 自动化测试与 VisualCheck

- [x] 6.1 新增 `tests/views/textfields/TestNumberBox.cpp`，按项目 GTest 规范初始化 `QApplication`、资源和 Segoe 字体
- [x] 6.2 覆盖默认属性、属性信号、NaN 语义、重复 setter、range clamp、step 校验和 sizeHint
- [x] 6.3 覆盖普通数字解析、表达式开关、表达式优先级、无效表达式和除零
- [x] 6.4 覆盖 spinner visibility、Inline 水平排布和按钮间距、Inline/Compact 独立按钮尺寸、Compact 默认可见纵向排布和上下边框间隙、Inline/Compact/Hidden 文本边距、button step、keyboard step 和 disabled 状态
- [x] 6.5 覆盖 spinner metrics 配置、`displayPrecision`、`formatStep`、0.25 rounder、编辑中不抢写文本
- [x] 6.6 新增 VisualCheck，展示表达式 NumberBox、Inline/Compact SpinButton placement、0.25 格式化金额 NumberBox、Header、Disabled、Light/Dark 主题，并加入 `SKIP_VISUAL_TEST` 守卫和 `qApp->exec()`
- [x] 6.7 在 `tests/views/textfields/CMakeLists.txt` 注册 `add_qt_test_module(test_number_box TestNumberBox.cpp)`

## 7. 验证

- [x] 7.1 构建受影响目标：`cmake --build build --target test_number_box`
- [x] 7.2 运行自动化测试：`SKIP_VISUAL_TEST=1 ./build/tests/views/textfields/test_number_box`
- [ ] 7.3 按需运行 VisualCheck：`./build/tests/views/textfields/test_number_box --gtest_filter="*VisualCheck*"`
- [x] 7.4 运行 `openspec validate add-number-box` 确认规格通过