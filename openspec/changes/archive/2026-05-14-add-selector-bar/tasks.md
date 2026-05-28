## 1. Component Skeleton And Public API

- [x] 1.1 Add `src/view/navigation/SelectorBar.h` with `QWidget + FluentElement + QMLPlus` inheritance, `Q_OBJECT`, public properties/enums, item APIs, selection APIs, geometry query declarations, and signals.
- [x] 1.2 Add `src/view/navigation/SelectorBar.cpp` with constructor setup, focus/hover attributes, default state, theme/font refresh, size hints, accessibility setup, and event override scaffolding.
- [x] 1.3 Define a `SelectorBarItem` data record with text, optional icon glyph, enabled state, visible state, user data, and accessible name fields.
- [x] 1.4 Register required Qt metatypes for `SelectorBarItem` and any SelectorBar enum types used by signals or properties.
- [x] 1.5 Confirm `SelectorBar.h/.cpp` participate in `fluent_qt_lib` through existing recursive source discovery, or add explicit CMake wiring only if discovery does not pick them up.

## 2. Item Model And Selection Semantics

- [x] 2.1 Implement item collection APIs: `addItem`, `insertItem`, `removeItem`, `clearItems`, `itemCount`, `items`, and `itemAt` with safe invalid-index behavior.
- [x] 2.2 Implement metadata update APIs for text, icon glyph, enabled state, visible state, user data, accessible name, and item selected flag.
- [x] 2.3 Implement `selectedIndex`, `selectedItem`, first-visible-enabled auto-selection, selected-index clamping after mutation, and no duplicate selection signals.
- [x] 2.4 Ensure `setItemSelected(index, true)` selects that item and `setItemSelected(index, false)` clears current selection only when that item is selected.
- [x] 2.5 Ensure disabled and hidden items cannot be selected through pointer, keyboard, or programmatic selection.
- [x] 2.6 Implement `selectedIndexChanged`, `currentChanged`, `selectionChanged`, `itemActivated`, `itemsChanged`, and property notification signals with duplicate-signal suppression.

## 3. Lightweight Composition Boundary

- [x] 3.1 Keep `SelectorBar` free of page widget, route, frame, `QStackedLayout`, and `StackContentHost` ownership APIs.
- [x] 3.2 Add tests that connect `SelectorBar::currentChanged` to an external `StackContentHost::setCurrentIndex` and verify page switching stays outside SelectorBar ownership.
- [x] 3.3 Add tests that use selection changes to update external state, such as a simple model key or presenter-visible flag, without embedding content in selector items.

## 4. Layout, Overflow, And Geometry

- [x] 4.1 Define named metric constants for selector row height, item padding, icon size, icon gap, overflow controls, selected indicator thickness, and minimum item width.
- [x] 4.2 Implement layout records for item rectangles, icon rectangles, text rectangles, selected indicator rectangles, overflow controls, visible indexes, and hidden indexes.
- [x] 4.3 Implement deterministic horizontal measurement for visible items without overlap between text, icons, neighboring items, overflow controls, or parent bounds.
- [x] 4.4 Implement overflow calculation that keeps the selected item reachable when possible and exposes visible/hidden index helpers.
- [x] 4.5 Implement geometry query helpers for selector row, item rectangles, selected indicator, overflow controls, visible indexes, and hidden indexes.
- [x] 4.6 Recompute layout on resize, item mutation, selection change, visibility change, font metric change, and theme update.

## 5. Fluent Painting And Theme Integration

- [x] 5.1 Paint selector items, optional icons, selected indicator, overflow affordances, and focus/hover/pressed state treatments with `QPainter`.
- [x] 5.2 Paint rest, hover, pressed, selected, disabled, hidden, overflow, Light, and Dark states using semantic colors from `themeColors()` and existing Fluent tokens.
- [x] 5.3 Render item text with configurable member-backed font role properties and icon glyphs with a configurable icon font family property.
- [x] 5.4 Ensure hidden items are not painted and disabled items remain visible with disabled semantic colors.
- [x] 5.5 Implement `onThemeUpdated()` to refresh fonts/colors, invalidate layout only when metrics change, repaint, and preserve item order/selection/metadata.
- [x] 5.6 Avoid QSS/QPalette-driven SelectorBar component styling and avoid direct hard-coded design token string lookups in paint/theme code.

## 6. Pointer, Keyboard, And Accessibility

- [x] 6.1 Implement hit testing for item records, overflow controls, and non-interactive gaps.
- [x] 6.2 Implement pointer hover/press/release selection for enabled visible items and suppression for disabled, hidden, or disabled-control states.
- [x] 6.3 Implement keyboard focus entry, Left/Right traversal, Home/End, Enter/Space activation, and focus clamping after item mutation or overflow changes.
- [x] 6.4 Implement overflow activation to scroll the visible item window or emit hidden-index activation without implicit selection.
- [x] 6.5 Update accessible text when items, item visibility, enabled state, accessible names, or selection changes.
- [x] 6.6 Ensure hidden items are unreachable by keyboard focus and hit testing while disabled items remain described as disabled.

## 7. Tests And VisualCheck

- [x] 7.1 Add `tests/views/navigation/TestSelectorBar.cpp` with GTest setup, resource/font initialization, metatype registration, and `SKIP_VISUAL_TEST` guard conventions.
- [x] 7.2 Register `test_selector_bar` in `tests/views/navigation/CMakeLists.txt` using `add_qt_test_module(test_selector_bar TestSelectorBar.cpp)`.
- [x] 7.3 Add automated tests for defaults, inheritance, metatypes, property signals, duplicate setter suppression, and size hints.
- [x] 7.4 Add automated tests for item management, metadata preservation, invalid indexes, hidden/disabled behavior, clear/remove behavior, selected-state semantics, and signal order/counts.
- [x] 7.5 Add automated tests for item rectangles, selected indicator geometry, overflow visible/hidden indexes, no-overlap layout, resize recomputation, and selected item reachability.
- [x] 7.6 Add automated tests for pointer activation, keyboard traversal, overflow activation, theme refresh, accessibility text updates, and external `StackContentHost` composition.
- [x] 7.7 Add a VisualCheck case guarded by `SKIP_VISUAL_TEST` that displays WinUI Gallery-inspired page switching, hidden sample-code-like items, disabled items, overflow, and Light/Dark theme switching.

## 8. Validation

- [x] 8.1 Validate the OpenSpec change with `openspec validate add-selector-bar --strict`.
- [x] 8.2 Build incrementally with `cmake --build build --target test_selector_bar`.
- [x] 8.3 Run automated tests with `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_selector_bar`.
- [x] 8.4 Run `git diff --check` for SelectorBar source, tests, and OpenSpec files.
- [ ] 8.5 Optionally run manual SelectorBar VisualCheck without `SKIP_VISUAL_TEST=1` to inspect WinUI Gallery-style states and theme switching.