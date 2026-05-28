## Why

The library does not yet provide a Fluent breadcrumb navigation control, so applications that need hierarchical path navigation must compose raw labels, buttons, and icons by hand. Adding a dedicated Breadcrumb component brings the WinUI BreadcrumbBar interaction model into the Qt Widgets Fluent component set and gives the planned navigation category its first reusable control.

## What Changes

- Add a custom `Breadcrumb` component under `src/view/navigation/`, implemented as a self-painted Qt Widgets Fluent control using `FluentElement` design tokens and `view::QMLPlus` conventions.
- Support string-based path items with per-item metadata, current-item emphasis, separator chevrons, item activation, and WinUI Gallery-style trailing path truncation after an item click.
- Implement responsive overflow modes inspired by the Windows UI Kit: no overflow, beginning overflow, and middle overflow using an ellipsis item that can expose hidden items.
- Provide standard and large sizes matching the Figma Breadcrumb variants: a compact 20px text row and a 40px title-path row.
- Add keyboard/focus, hover/pressed/disabled visual states, theme updates, accessibility names, automated tests, and a VisualCheck sample.

## Capabilities

### New Capabilities

- `breadcrumb`: Covers the Fluent Qt Widgets Breadcrumb navigation control, including path item management, item activation, overflow behavior, sizing, visual states, keyboard/focus behavior, accessibility, and tests.

### Modified Capabilities

无。

## Impact

- New source files: `src/view/navigation/Breadcrumb.h` and `src/view/navigation/Breadcrumb.cpp`.
- New tests: `tests/views/navigation/TestBreadcrumb.cpp`.
- New test category wiring: `tests/views/navigation/CMakeLists.txt` and `add_subdirectory(navigation)` in `tests/views/CMakeLists.txt`.
- Uses existing `FluentElement`, `view::QMLPlus`, design tokens, Qt painting/events, and Segoe Fluent Icons; no new third-party dependencies.
- Does not change existing collection, text field, scrolling, or basic input component APIs.
