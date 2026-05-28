## 1. Skeleton 与公共 API

- [x] 1.1 新增 Popup.h / Popup.cpp，建立 QWidget + FluentElement + view::QMLPlus 三重继承结构
- [x] 1.2 定义基础属性与信号槽：isOpen、closePolicy、modal、dim、animationEnabled、popupProgress、open/close/setIsOpen、opened/closed/aboutToShow/aboutToHide
- [x] 1.3 提供唯一定位入口 setPosition(QWidget* relativeTo, const QPoint& localPos)
- [x] 1.4 在 tests/views/dialogs_flyouts/CMakeLists.txt 中注册 test_popup

## 2. 视觉绘制与主题

- [x] 2.1 使用 paintEvent 自绘 Popup 背景、描边和阴影，不使用 QSS / QPalette
- [x] 2.2 采用 Fluent token：themeRadius().overlay、themeShadow(Elevation::High)、themeColors().bgLayer、themeColors().strokeDefault
- [x] 2.3 用内部 shadow margin 扩大 widget rect，并让定位语义始终对应可见卡片左上角
- [x] 2.4 onThemeUpdated() 触发 Popup 与 Scrim 的重绘

## 3. Open / Close 生命周期

- [x] 3.1 open() 时将 Popup reparent 到原始 parent 的 top-level widget，并在未设置位置时默认居中
- [x] 3.2 open() 前激活 layout，并在 totalSizeHint() 有效时按布局驱动尺寸
- [x] 3.3 close()、setIsOpen()、aboutToShow/aboutToHide/opened/closed/isOpenChanged 行为完成闭环
- [x] 3.4 Popup 默认保持隐藏，只有显式 open() 才会显示，避免父窗口 show() 时意外显现

## 4. 定位与交互

- [x] 4.1 setPosition(QWidget*, QPoint) 将局部坐标映射到 top-level 坐标系
- [x] 4.2 支持以 top-level widget 作为 relativeTo 实现固定位置显示
- [x] 4.3 未设置位置时默认在 top-level widget 内居中
- [x] 4.4 当调用侧需要基于最终内容高度反推位置时，由调用侧先激活 layout 再计算 localPos

## 5. ClosePolicy、Modal 与 Scrim

- [x] 5.1 实现 CloseOnPressOutside / CloseOnEscape 两种关闭策略，默认同时启用
- [x] 5.2 通过 qApp eventFilter 实现 light-dismiss，并保证外部事件不被消费
- [x] 5.3 通过 keyPressEvent 实现 Escape 关闭
- [x] 5.4 modal 模式下创建 PopupScrim 拦截背景交互
- [x] 5.5 dim 模式下由 PopupScrim 绘制 smoke 遮罩

## 6. 动画

- [x] 6.1 用单个 QPropertyAnimation 驱动 popupProgress
- [x] 6.2 Enter / Exit 动画使用 themeAnimation().normal、entrance、exit
- [x] 6.3 动画采用 opacity-only 方案，由 QGraphicsOpacityEffect 统一作用到背景和子控件
- [x] 6.4 animationEnabled = false 时同步完成 opened / closed 流程

## 7. 测试与手工验证

- [x] 7.1 补齐行为测试：默认属性、信号顺序、top-level reparent、相对坐标定位、closePolicy、modal scrim、动画
- [x] 7.2 添加统一 VisualCheck 窗口，覆盖固定位置、默认居中、modal+dim、sticky、relative position 等展示场景
- [x] 7.3 手工运行 VisualCheck，检查点击交互、主题切换和相对定位示例

## 8. 文档与验证

- [x] 8.1 在 readme.md 中补充 Popup 介绍
- [x] 8.2 在 .github/copilot-instructions.md 中补充 Popup 目录说明
- [x] 8.3 增量构建与非 visual 测试通过
- [x] 8.4 openspec validate implement-popup --strict 通过
