## Why

The library does not yet provide a Fluent `Pivot` control, so applications that need a WinUI-style category switcher must choose between overusing `TabView` or manually wiring labels/buttons with a stacked content area. Adding Pivot fills the lightweight navigation gap represented by WinUI Gallery's email Pivot sample with `PivotItem Header` pages; section labels such as `EMAIL` remain ordinary external `Label` widgets anchored by the caller.

## What Changes

- Add a custom `Pivot` component under `src/view/navigation/`, implemented as a Qt Widgets Fluent control using `QWidget + FluentElement + QMLPlus` conventions.
- Provide a Pivot-owned header strip for `PivotItem` headers, selection indicator, overflow, pointer/keyboard focus, disabled states, and theme-aware Fluent visuals.
- Provide page/content management through a stacked content host based on `QStackLayout` behavior so each pivot item can own an optional QWidget page while the selected page is shown.
- Support public APIs for adding, inserting, updating, removing, clearing, selecting, and querying pivot items with header text, optional icon glyph, content widget, enabled state, metadata, and accessibility text.
- Adapt WinUI Gallery Pivot behavior for Qt Widgets: Pivot-owned item headers, externally anchored section labels, `PivotItem Header` pages, selected-index changes, item activation, keyboard navigation, stacked content switching, automated tests, and VisualCheck coverage.

## Capabilities

### New Capabilities
- `pivot`: Covers the Fluent Qt Widgets Pivot control, including owned headers, pivot item/page management, selected index, stacked content host behavior, overflow, pointer and keyboard interaction, visual states, accessibility, tests, and VisualCheck.

### Modified Capabilities
无。

## Impact

- New source files under `src/view/navigation/`, expected to include `Pivot.h` and `Pivot.cpp`, with optional private helper types if the implementation needs to separate header layout from page hosting.
- New tests under `tests/views/navigation/`, expected as `TestPivot.cpp`, registered in `tests/views/navigation/CMakeLists.txt`.
- Uses existing `FluentElement`, `QMLPlus`, design tokens, Qt painting/events, `QStackLayout`, and existing navigation component patterns; no new third-party dependencies.
- Does not change existing Breadcrumb, NavigationView, TabView, TabStrip, or StackContentHost public APIs unless reuse reveals a small shared helper that can be introduced without behavior changes.
