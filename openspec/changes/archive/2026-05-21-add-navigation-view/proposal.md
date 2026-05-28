## Why

Applications need a reusable Fluent shell container that can switch between side and top navigation layouts without forcing the component to own application navigation items, selection, settings, or page routing. The previous all-in-one direction made `NavigationView` too heavy for this codebase: the component should provide the chrome/content structure, while application code supplies the actual chrome widgets and pages.

## What Changes

- Add `view::navigation::NavigationView` as a lightweight Qt Widgets shell layout component using `QWidget + FluentElement + view::QMLPlus`.
- Provide side, compact, minimal, and top display modes with configurable pane/top metrics and auto mode thresholds.
- Expose three chrome slots: header, main, and footer. In side mode these are stacked vertically; in top mode they are placed horizontally as header, main, stretch, footer.
- Expose an internal `StackContentHost` as the content surface. Applications add/switch pages directly through the host.
- Keep item lists, indicators, settings/back/search controls, selection, nested expansion, and chrome content composition at the application/test layer.

## Capabilities

### New Capabilities
- `navigation-view`: Covers the Fluent Qt Widgets NavigationView shell container, including pane display modes, chrome slot layout, content host integration, geometry queries, theme refresh, and tests.

### Modified Capabilities
无。

## Impact

- New source files: `src/view/navigation/NavigationView.h` and `src/view/navigation/NavigationView.cpp`.
- New tests: `tests/views/navigation/TestNavigationView.cpp`.
- Test wiring update: `tests/views/navigation/CMakeLists.txt` registers `test_navigation_view`.
- Uses existing `StackContentHost`, `FluentElement`, `view::QMLPlus`, and design tokens.
- Does not change existing Breadcrumb, Pivot, SelectorBar, StackContentHost, collection, scrolling, text field, basic input, or shell component APIs.
