## Why

Qt Widgets currently lacks a reusable Fluent drawer component for edge-attached navigation, settings, and contextual panels. A custom `DrawerView` modeled after QML `Drawer` gives this library a predictable QWidget-native overlay panel without forcing applications to compose ad hoc popups, split panes, or platform windows.

## What Changes

- Add a new `view::collections::DrawerView` Qt Widgets component that slides in from a window edge and hosts arbitrary QWidget content.
- Provide QML Drawer-inspired state and control APIs: edge selection, open/close lifecycle, `position` progress, modal/modeless behavior, dim scrim, close policies, and optional interactive drag gestures.
- Use Fluent design tokens for drawer surface, stroke, shadow, smoke/dim overlay, and animation timing; no QSS/QPalette for the drawer visuals.
- Keep `Popup`, `Flyout`, `SplitView`, and existing collection widgets API-compatible; DrawerView is a new capability rather than a behavior change to existing components.
- Add automated tests and a VisualCheck demo that exercise edge placement, animation, modal dismissal, drag interaction, theme switching, and content hosting.

## Capabilities

### New Capabilities
- `drawer-view`: Defines the Fluent Qt Widgets DrawerView component, including edge-attached overlay semantics, QML Drawer-like progress/open state, content hosting, modal and light-dismiss behavior, gestures, theming, and tests.

### Modified Capabilities
无。

## Impact

- New source files: `src/view/collections/DrawerView.h` and `src/view/collections/DrawerView.cpp`.
- New tests: `tests/views/collections/TestDrawerView.cpp`.
- Test wiring update: `tests/views/collections/CMakeLists.txt` registers `test_drawer_view`.
- Root library source glob will include the new component automatically.
- Uses existing `FluentElement`, `view::QMLPlus`, design tokens, and Qt animation/event APIs.
- Does not change existing `Popup`, `Flyout`, `SplitView`, `StackView`, `FlipView`, `ListView`, `GridView`, or `TreeView` public APIs.
