## 1. 组件 API 与结构

- [x] 1.1 新增 `src/view/status_info/InfoBar.h`，定义 `InfoBar : public QWidget, public FluentElement, public view::QMLPlus`
- [x] 1.2 定义 `InfoBarSeverity { Informational, Success, Warning, Error }` 并通过 `Q_ENUM` 暴露
- [x] 1.3 添加 `isOpen`、`title`、`message`、`severity`、`isClosable`、`isIconVisible`、`singleLine`、`preferredWidth` 的 `Q_PROPERTY`、getter/setter 和 changed 信号
- [x] 1.4 添加 `setActionWidget(QWidget*)`、`actionWidget()`、`closed()` 与 `actionWidgetChanged()`，支持托管/替换/清空外部 action 控件
- [x] 1.5 实现默认值：open、title 为 `Title`、message 为空、Informational、closable、icon visible、singleLine、preferredWidth 600、无 action widget
- [x] 1.6 为 title/message typography role 使用可配置成员或子控件属性，避免在主题刷新和绘制路径中直接硬编码 Design Token 字符串

## 2. 布局与子控件

- [x] 2.1 新增 `src/view/status_info/InfoBar.cpp`，创建内部 title/message label、close icon button、action 容器和主布局
- [x] 2.2 实现 single-line 布局：icon slot、title、message、action、close button 横向排列，默认高度 50px，title/message 间距约 12px
- [x] 2.3 实现 multi-line 布局：title 独立一行、message 换行、action widget 位于文本下方，默认无 action 高度至少 110px，有 action 高度至少 158px
- [x] 2.4 实现 `sizeHint()` / `minimumSizeHint()`，根据 `isOpen`、`preferredWidth`、`singleLine`、message wrapping 和 action widget 计算尺寸
- [x] 2.5 实现 `setPreferredWidth()` 正数校验，非正输入保持旧值不变，尺寸变化时调用 `updateGeometry()`
- [x] 2.6 实现 icon visible 和 closable 切换时的布局重排，确保隐藏 icon/close 后释放对应空间且文本不重叠

## 3. 自绘与主题

- [x] 3.1 在 `paintEvent()` 中启用抗锯齿，自绘 InfoBar 半透明 card 背景、1px stroke 和约 3px 圆角
- [x] 3.2 绘制 16px severity badge，包括圆形背景和居中 Segoe Fluent Icons glyph
- [x] 3.3 映射 severity 颜色和图标：Informational/accent info、Success/systemSuccess、Warning/systemCaution、Error/systemCritical
- [x] 3.4 实现 Light/Dark 主题下 background、stroke、text、badge 和 disabled 颜色缓存
- [x] 3.5 实现 `onThemeUpdated()`，刷新缓存颜色、内部 label 字体/颜色、close/action 子控件状态并请求重绘
- [x] 3.6 实现 disabled 状态处理，禁用时文本/badge/close/action 使用 disabled 语义且 close button 不触发关闭

## 4. 行为语义

- [x] 4.1 实现 `setIsOpen()`，同步 open 状态、visibility 或空尺寸语义，并触发 `isOpenChanged`
- [x] 4.2 连接 close button 点击：`isClosable` 为 true 且 enabled 时调用 `setIsOpen(false)` 并发出 `closed()`
- [x] 4.3 实现 `setTitle()` / `setMessage()`，更新 label 内容、单行 elide 或多行 wrapping，并刷新尺寸
- [x] 4.4 实现 `setSeverity()`，更新 badge 图标/颜色并保持 title/message/open 状态不变
- [x] 4.5 实现 `setActionWidget()` 的 ownership 与布局管理，替换旧 action 时从布局移除并避免重复 delete 或悬挂父子关系
- [x] 4.6 处理重复设置相同值：不触发信号、不重复布局、不冗余重绘

## 5. 测试与可视化

- [x] 5.1 新增 `tests/views/status_info/TestInfoBar.cpp`，按项目 GTest 规范初始化 `QApplication`、资源和 Segoe 字体
- [x] 5.2 覆盖默认属性、属性信号、重复设置无信号、preferredWidth 非正防护和 size hint 默认值
- [x] 5.3 覆盖 `isOpen` 关闭/重新打开、close button 点击、`isClosable=false`、disabled close 行为和 `closed()` 信号
- [x] 5.4 覆盖 severity 切换、icon visible 切换、theme update 和渲染像素抽样，确认 severity 视觉状态变化
- [x] 5.5 覆盖 single-line 与 multi-line 布局：短消息、长消息 wrapping、message elide、不重叠、无 action 110px、有 action 158px 尺寸语义
- [x] 5.6 覆盖 action widget：None、`Button`、`HyperlinkButton`、替换 action、清空 action，并确认 action widget 可见且 InfoBar 不接管 hyperlink url 逻辑
- [x] 5.7 新增 VisualCheck，展示 Light/Dark、Single Line/Multi Line、Informational/Success/Warning/Error、action none/button/hyperlink、icon visible/hidden、closable/non-closable 和 WinUI Gallery 三组示例，并加入 `SKIP_VISUAL_TEST` 守卫
- [x] 5.8 在 `tests/views/status_info/CMakeLists.txt` 注册 `test_info_bar`

## 6. 验证

- [x] 6.1 构建受影响目标：`cmake --build build --target test_info_bar`
- [x] 6.2 运行自动化测试：`SKIP_VISUAL_TEST=1 ./build/tests/views/status_info/test_info_bar`
- [x] 6.3 按需运行 VisualCheck：`./build/tests/views/status_info/test_info_bar --gtest_filter="*VisualCheck*"`
- [x] 6.4 运行 `openspec validate add-info-bar` 确认规格通过

## 7. 视觉细节优化

- [x] 7.1 调整 close button 几何，通过 `contentMargins.right()` 控制按钮到 InfoBar 右边框的边距
- [x] 7.2 将 InfoBar 背景按 severity 切换到 `systemInfoBg`、`systemSuccessBg`、`systemCautionBg`、`systemCriticalBg`
- [x] 7.3 将 severity badge 默认 glyph 调整为圆内简洁字形，并暴露四种 severity glyph 属性用于后续替换
- [x] 7.4 暴露布局、尺寸、圆角、字体 role、severity icon 大小等配置属性，减少 `.cpp` 内部硬编码
- [x] 7.5 补充 UT 覆盖 close 边距、severity 背景变化、可配置属性信号和按钮触发打开行为

## 8. Figma iconfont 对齐修正

- [x] 8.1 读取 Figma InfoBar Badge 子层，确认 Informational/Warning/Critical/Success 使用 Segoe Fluent Icons 的 Badge12 glyph
- [x] 8.2 在 `Typography::Icons` 增加 `AsteriskBadge12`、`ImportantBadge12`、`ErrorBadge12`、`CheckmarkBadge12` 常量
- [x] 8.3 将 InfoBar severity badge 默认 glyph 改为 Figma 对应 iconfont glyph，并保持 glyph 属性可覆盖
- [x] 8.4 使用 `QPainterPath` 的实际 glyph bounds 做视觉居中，避免普通 `drawText` baseline 导致 warning/error 偏移
- [x] 8.5 补充 UT 锁定 Figma Badge12 glyph 默认值和 `severityIconBackgroundInset` 配置

## 9. API 收敛

- [x] 9.1 移除冗余 `closeButtonMargin` 属性、setter、信号和内部成员
- [x] 9.2 统一使用 `contentMargins.right()` 表达 close button 到 InfoBar 右边框的距离
- [x] 9.3 更新 UT 与规格文档，避免 close-only margin 与整体内容边距重复

## 10. 单行交互细节

- [x] 10.1 将 InfoBar 单行 message 的省略逻辑交给自定义 `Label`，保留完整原文用于 hover tooltip
- [x] 10.2 在 close button 左侧保留内容间距，避免 message 省略号贴近关闭按钮
- [x] 10.3 补充 UT 覆盖 InfoBar message elide 状态、完整文本保留和 close button 前间距