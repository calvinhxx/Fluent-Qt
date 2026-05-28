## Why

The library does not yet provide a Fluent `SelectorBar` control, so applications that need WinUI-style page or view switching must reuse `Pivot`, `TabView`, or hand-wire labels/buttons. WinUI Gallery uses `SelectorBar` as a lightweight selector whose `SelectedItem` drives external frames, item sources, and sample presenters, which matches the library's newer lightweight navigation direction.

## What Changes

- Add a custom `SelectorBar` component under `src/view/navigation/`, implemented as a Qt Widgets Fluent control using `QWidget + FluentElement + QMLPlus` conventions.
- Provide a SelectorBar-owned horizontal item row with text/icon items, selected indicator, pointer/keyboard interaction, disabled and hidden item handling, overflow behavior, accessibility text, and theme-aware Fluent visuals.
- Keep the component lightweight like the current `Pivot` and `Breadcrumb`: `SelectorBar` owns selector items and selection state only; it does not own pages, routes, frames, or content widgets.
- Expose selection APIs and signals so callers can switch an external `StackContentHost`, change an `ItemsView`-style data source, or update presenter visibility in response to selection changes.
- Add automated tests and a VisualCheck sample demonstrating WinUI Gallery-inspired usage, including external `StackContentHost` composition.

## Capabilities

### New Capabilities
- `selector-bar`: Covers the Fluent Qt Widgets SelectorBar control, including item management, selected item/index state, hidden/disabled behavior, overflow, pointer and keyboard interaction, visual states, accessibility, lightweight external composition, tests, and VisualCheck.

### Modified Capabilities
无。

## Impact

- New source files under `src/view/navigation/`, expected as `SelectorBar.h` and `SelectorBar.cpp`.
- New tests under `tests/views/navigation/`, expected as `TestSelectorBar.cpp`, registered in `tests/views/navigation/CMakeLists.txt`.
- Uses existing `FluentElement`, `QMLPlus`, design tokens, Qt painting/events, and navigation component patterns; no new third-party dependencies.
- Does not change existing `Breadcrumb`, `NavigationView`, `Pivot`, `StackContentHost`, or `TabView` public APIs.