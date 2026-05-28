## Why

`src/view/scrolling/` 目前只有自绘 `ScrollBar`，业务侧如果需要可滚动容器仍要直接使用 `QScrollArea` 并手动替换滚动条，样式、可见性、主题同步和测试方式都容易分散。Figma Windows UI kit 的 Scrolling 页面和 WinUI Gallery 的 ScrollView 示例都把滚动容器作为独立控件暴露，支持水平/垂直/双向滚动、滚动条显示策略、程序化滚动和内容裁剪。

本变更新增自定义 `ScrollView`，以 `QScrollArea` 为基础能力，正式接入项目已有 Fluent `ScrollBar`，为滚动内容提供统一的 Fluent Design 容器 API。

## What Changes

- 在 `src/view/scrolling/` 新增 `ScrollView` 组件，继承 `QScrollArea`、`FluentElement` 和 `view::QMLPlus`。
- `ScrollView` 内部使用项目自定义 `view::scrolling::ScrollBar` 替换 Qt 原生水平/垂直滚动条。
- 提供水平/垂直滚动模式、滚动条可见性策略、内容 widget 设置、程序化滚动和滚动状态查询能力。
- 对齐 Figma Scrolling 页面中的 Light/Dark、vertical/horizontal/bidirectional 与 expanded/collapsed 滚动条视觉关系，并复用现有 `ScrollBar` 的绘制与自动隐藏行为。
- 新增 `tests/views/scrolling/TestScrollView.cpp`，覆盖自动化行为测试和 VisualCheck；VisualCheck 提供主题切换、垂直/水平/双向滚动示例。
- 在 `tests/views/scrolling/CMakeLists.txt` 注册 `test_scroll_view`。

## Capabilities

### New Capabilities
- `scroll-view`: 覆盖 Fluent Design Qt 组件库中的自定义滚动容器 API、ScrollBar 集成、滚动模式、可见性策略、主题响应和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/scrolling/ScrollView.h`、`src/view/scrolling/ScrollView.cpp`。
- 新增测试：`tests/views/scrolling/TestScrollView.cpp`，并更新 `tests/views/scrolling/CMakeLists.txt`。
- 复用现有 `src/view/scrolling/ScrollBar.*`，不引入第三方依赖。
- 不改变现有 `ScrollBar` 公共 API；如实现中发现滚动容器集成需要扩展 `ScrollBar`，应保持兼容并通过测试覆盖。