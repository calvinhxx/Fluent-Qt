## Why

`src/view/collections/` already contains data presentation controls, but the library does not yet provide a Fluent splitter container for resizable pane layouts. Applications that need master/detail, navigation/content, inspector panels, or editor sidebars currently have to fall back to raw `QSplitter`, losing WinUI/Fluent visual states, QML-style sizing semantics, and consistent VisualCheck coverage.

## What Changes

- Add a custom `SplitView` component under `src/view/collections/`, implemented as a Qt Widgets Fluent control with draggable splitter handles.
- Model the core behavior after Qt Quick Controls `SplitView`: horizontal/vertical orientation, per-pane minimum/preferred/maximum size, one fill pane that receives remaining space, handle hover/pressed/resizing states, and state save/restore.
- Reflect WinUI SplitView concepts where they fit Qt Widgets: pane/content examples, left/right pane placement in VisualCheck, Fluent pane/background styling, and open/closed pane scenarios suitable for navigation or list/details layouts.
- Provide a compact public API for adding/removing panes, configuring pane constraints, setting the fill pane, querying handle geometry, and responding to resize changes.
- Add automated tests and a VisualCheck demonstrating QML-style resizable panes and a WinUI Gallery-inspired navigation/content split view.

## Capabilities

### New Capabilities

- `split-view`: Covers the Fluent Qt Widgets SplitView container, including pane management, size constraints, draggable handles, fill-pane layout, orientation, state persistence, visual states, and tests.

### Modified Capabilities

无。

## Impact

- New source files: `src/view/collections/SplitView.h` and `src/view/collections/SplitView.cpp`.
- New tests: `tests/views/collections/TestSplitView.cpp`.
- Test registration update in `tests/views/collections/CMakeLists.txt`.
- Uses existing `FluentElement`, `view::QMLPlus`, design tokens, Qt painting/events, and Qt child-widget layout mechanics; no new third-party dependencies.
- Does not change existing `ListView`, `GridView`, `TreeView`, or `FlipView` public APIs.