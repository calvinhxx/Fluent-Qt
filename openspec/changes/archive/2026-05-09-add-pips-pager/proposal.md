## Why

当前 `scrolling` 模块已有 Fluent `ScrollBar` 和 `ScrollView`，但缺少用于线性分页内容的 PipsPager。业务侧在图片浏览、轮播、FlipView/ScrollView 分页场景中需要一个可点击、可键盘操作、可绑定 selected index 的点状分页指示器，否则只能临时拼装按钮或标签，难以对齐 Windows UI Kit 和 WinUI Gallery。

本变更新增 `PipsPager` 自定义控件，对齐 Figma Windows UI Kit 中 PipsPager 的 Light/Dark、水平/垂直、带/不带 caret、selected/inactive pip 状态，以及 WinUI Gallery 中与 FlipView 集成、Orientation 和 Previous/Next Button Visibility 的核心行为。

## What Changes

- 在 `src/view/scrolling/` 新增 `PipsPager` 组件，遵循项目现有 `QWidget + FluentElement + view::QMLPlus` 模式并通过 `paintEvent()` 自绘 pips。
- 支持 `numberOfPages`、`selectedPageIndex`、`maxVisiblePips`、`orientation`、`previousButtonVisibility`、`nextButtonVisibility` 等 Qt 属性和变更信号。
- 支持用户点击 pip、点击 previous/next caret、键盘方向键/Home/End 导航，并发出 `selectedPageIndexChanged`/`selectedIndexChanged(old,new)` 信号。
- 支持 Figma/WinUI 视觉状态：Light/Dark、selected/inactive、Rest/Hover/Pressed/Disabled；pip 容器为 12px，内部圆点按状态在 3px/4px/5px/6px/7px 之间变化。
- 支持水平与垂直方向；带 navigation caret 时使用 Segoe Fluent Icons：Left `EDD9`、Right `EDDA`、Up `EDDB`、Down `EDDC`，图标尺寸 8px，按钮区域 24px。
- 支持 `Visible`、`VisibleOnPointerOver`、`Collapsed` 三种前后按钮可见性策略；首/末页时对应按钮不可用或隐藏且不支持首尾循环。
- 支持 `numberOfPages > maxVisiblePips` 时自动滚动可见 pip 窗口，使当前页尽量居中。
- 添加 `tests/views/scrolling/TestPipsPager.cpp` 与 VisualCheck，覆盖 API、绘制尺寸、输入交互、主题、滚动 pip 和 WinUI Gallery 关键场景。

## Capabilities

### New Capabilities

- `pips-pager`: scrolling 模块中的 WinUI 风格 PipsPager 控件，覆盖公共 API、分页状态、水平/垂直布局、导航按钮可见性、pip 自绘、输入交互、可访问性公告和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/scrolling/PipsPager.h`、`src/view/scrolling/PipsPager.cpp`。
- 更新测试配置：`tests/views/scrolling/CMakeLists.txt` 注册 `test_pips_pager`。
- 新增测试：`tests/views/scrolling/TestPipsPager.cpp`。
- 依赖现有 Design Token、`FluentElement`、`QMLPlus`、Qt painting、Qt input events 和 Segoe Fluent Icons 字体；不引入第三方依赖。
- 不修改现有 `ScrollBar` 或 `ScrollView` 公共 API；PipsPager 可单独使用，也可由业务代码绑定到 FlipView/ScrollView/轮播控件的 selected index。
