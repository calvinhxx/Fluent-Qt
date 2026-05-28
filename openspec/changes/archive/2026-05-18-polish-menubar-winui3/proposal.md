## Why

The current `FluentMenuBar` is visually close for a simple top-level menu, but it still relies on QSS for geometry and styling and only covers a narrow subset of WinUI MenuBar behavior. Figma and WinUI Gallery both show MenuBar as a keyboard-accessible command surface with distinct rest, hover, pressed/open, disabled, selected, and focus states, plus flyout integration for separators, submenus, radio/check items, keyboard accelerators, and access keys.

## What Changes

- Rework `FluentMenuBar` so top-level item geometry, hit testing, painting, and interaction states are driven by Fluent tokens and widget code instead of QSS visual styling.
- Align top-level MenuBar item sizing and states with the Windows UI Kit MenuBar references, including 40px item height, control corner radius, subtle hover fill, pressed/open fill, disabled text, and keyboard focus visuals.
- Preserve Qt `QMenuBar`/`QMenu` compatibility for existing `addMenu`, `QAction`, shortcut, and trigger behavior while making the visual layer deterministic.
- Improve keyboard support for horizontal navigation, activation, Escape/Alt behavior where Qt does not already provide it consistently, and expose access key expectations for menu titles and menu items.
- Extend `FluentMenu` coverage used by MenuBar flyouts to include nested submenus, separators, checked/radio states, disabled states, accelerator text, and predictable light/dark rendering.
- Replace the current VisualCheck-only coverage with focused automated tests plus a richer VisualCheck that mirrors WinUI Gallery's simple, accelerator, and complex MenuBar examples.

## Capabilities

### New Capabilities
- `menu-bar`: Defines WinUI-aligned MenuBar behavior, top-level item states, keyboard/access key expectations, flyout integration, and test/demo coverage for Fluent Qt widgets.

### Modified Capabilities

## Impact

- Affected source: `src/view/menus_toolbars/MenuBar.h`, `src/view/menus_toolbars/MenuBar.cpp`, `src/view/menus_toolbars/Menu.h`, and `src/view/menus_toolbars/Menu.cpp`.
- Affected tests: `tests/views/menus_toolbars/TestMenuBar.cpp` and the menus/toolbars CMake test registration if new test cases or helpers are added.
- Public API impact should stay source-compatible with existing `QMenuBar` and `QMenu` usage; optional Fluent-specific properties may be added for font role, access key display, or accelerator rendering if needed.
- No new third-party dependencies are expected.
