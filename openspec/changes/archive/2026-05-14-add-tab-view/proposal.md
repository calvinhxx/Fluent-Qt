## Why

The library currently has navigation primitives such as Breadcrumb, but it does not provide a Fluent/WinUI-style tab workspace control for multi-document or multi-page surfaces. A custom Qt Widgets TabView will let applications present closable, addable, selectable tabs with Fluent visual states while staying consistent with the repository's self-painted component architecture.

## What Changes

- Add a new `view::navigation::TabView` control with `QWidget + FluentElement + view::QMLPlus` inheritance.
- Add a tab item model/API that supports header text, optional icon glyph, optional content widget, closeability, enabled state, and user data.
- Add Fluent rendering for tab row placement in title bar or content area, active/inactive tabs, hover/pressed/focus states, close-button states, add-tab affordance, and overflow affordances based on the Windows UI Kit TabView reference.
- Add user interactions for selection, add requested, close requested, keyboard navigation, and optional in-row tab reordering.
- Add tests and VisualCheck coverage for API behavior, layout modes, interaction states, theme switching, overflow, and reference-like examples.

## Capabilities

### New Capabilities
- `tab-view`: Provides a Fluent WinUI-style TabView component for Qt Widgets, including tab item management, selection, close/add/reorder interactions, width modes, close button overlay modes, overflow, keyboard navigation, and theme-aware custom painting.

### Modified Capabilities

## Impact

- Adds source files under `src/view/navigation/` for the TabView component and its item model.
- Adds unit and VisualCheck tests under `tests/views/navigation/` and updates that category's CMake test registration.
- Uses existing design tokens, Fluent icon font glyphs, QMLPlus anchor/layout support, and `FluentElement` theme update patterns.
- Does not introduce new third-party dependencies or change existing component APIs.