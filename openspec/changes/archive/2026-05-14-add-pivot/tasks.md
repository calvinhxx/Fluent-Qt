## 1. Component Skeleton And Public API

- [x] 1.1 Add `src/view/navigation/Pivot.h` with `QWidget + FluentElement + QMLPlus` inheritance, `Q_OBJECT`, public enums/properties, signals, item APIs, content APIs, and geometry query declarations.
- [x] 1.2 Add `src/view/navigation/Pivot.cpp` with constructor setup, focus/hover attributes, default state, theme/font refresh, size hints, child event handling, and event override scaffolding.
- [x] 1.3 Define a `PivotItem` data record with header text, optional icon glyph, content widget pointer, enabled state, user data, and accessible name fields.
- [x] 1.4 Register required Qt metatypes for `PivotItem` and any Pivot enum types used by signals or properties.
- [x] 1.5 Confirm `Pivot.h/.cpp` participate in `fluent_qt_lib` through existing recursive source discovery, or add explicit CMake wiring only if discovery does not pick them up.

## 2. Item Model And Page Host

- [x] 2.1 Implement item collection APIs: `addItem`, `insertItem`, `removeItem`, `clearItems`, `itemCount`, `items`, and `itemAt` with safe invalid-index behavior.
- [x] 2.2 Implement metadata update APIs for header text, icon glyph, enabled state, user data, accessible name, and item page widget.
- [x] 2.3 Implement selected-index state, first-enabled auto-selection, selected-index clamping after mutation, and no duplicate selection signals.
- [x] 2.4 Implement a `QStackedLayout`-based content host or internal reuse of `StackContentHost` for synchronized page insertion, removal, replacement, animated non-overlapping transitions, and current page selection.
- [x] 2.5 Implement content widget ownership semantics: reparent assigned pages into the host, hide non-selected pages, detach removed pages, and support `takePageWidget(index)`.
- [x] 2.6 Implement item font role, icon font family, overflow behavior, and item-count notification properties with duplicate-signal suppression.

## 3. Header Layout And Geometry

- [x] 3.1 Define named metric constants for header row height, item header padding, icon size, overflow buttons, selected indicator thickness, and content gap.
- [x] 3.2 Implement header layout records for item headers, icon rects, text rects, selected indicator rects, overflow controls, and content area.
- [x] 3.3 Implement deterministic size-to-content header measurement without text ellipses or overlap between item headers, overflow controls, and content.
- [x] 3.4 Implement horizontal overflow calculation that keeps the selected header reachable, exposes visible/hidden indexes, and updates on resize.
- [x] 3.5 Implement geometry query helpers for header row, item headers, selected indicator, overflow controls, visible indexes, hidden indexes, and content area.
- [x] 3.6 Implement child content host geometry updates after every layout recomputation.

## 4. Fluent Painting And Theme Integration

- [x] 4.1 Paint header row, item headers, optional icons, selected indicator, overflow affordances, and content boundary with `QPainter`.
- [x] 4.2 Paint rest, hover, pressed, selected, disabled, Light, Dark, and overflow states using label-style text/icon colors from `themeColors()` and existing Fluent design tokens.
- [x] 4.3 Render item header text with configurable member-backed font roles and icon glyphs with Segoe Fluent Icons through the configured icon font family.
- [x] 4.4 Ensure disabled items remain visible but use disabled semantic colors and never show selected or pressed visuals.
- [x] 4.5 Implement `onThemeUpdated()` to refresh fonts/colors, update the content host background, invalidate layout only when metrics change, and preserve selection/page state.
- [x] 4.6 Avoid QSS/QPalette-driven Pivot component styling and avoid direct hard-coded design token string lookups in paint/theme code.

## 5. Pointer, Keyboard, And Selection Behavior

- [x] 5.1 Implement hit testing for item headers, overflow controls, and non-interactive header gaps.
- [x] 5.2 Implement pointer hover/press/release selection for enabled headers and suppression for disabled items or disabled controls.
- [x] 5.3 Implement item activation and selection signals, including `itemActivated`, `selectedIndexChanged`, `currentChanged`, and content host current-page synchronization.
- [x] 5.4 Implement keyboard focus entry, Left/Right traversal, Home/End, Enter/Space activation, and focus clamping after item mutation.
- [x] 5.5 Implement optional Ctrl+Tab and Ctrl+Shift+Tab cycling only if it does not interfere with normal Qt focus traversal in tests.
- [x] 5.6 Implement overflow control activation to scroll visible headers or emit hidden-index activation without implicit selection.
- [x] 5.7 Ensure programmatic selection ignores invalid and disabled indexes while preserving current valid selection.

## 6. Tests And VisualCheck

- [x] 6.1 Add `tests/views/navigation/TestPivot.cpp` with GTest setup, resource/font initialization, metatype registration, and `SKIP_VISUAL_TEST` guard conventions.
- [x] 6.2 Register `test_pivot` in `tests/views/navigation/CMakeLists.txt` using `add_qt_test_module(test_pivot TestPivot.cpp)`.
- [x] 6.3 Add automated tests for defaults, inheritance, metatypes, property signals, duplicate setter suppression, and size hints.
- [x] 6.4 Add automated tests for item/page management, metadata preservation, invalid indexes, page replacement, page take semantics, clear/remove behavior, content host synchronization, and animated non-overlapping page switching.
- [x] 6.5 Add automated tests for selected-index clamping, disabled item suppression, pointer activation, keyboard traversal, current page visibility, and selection signals.
- [x] 6.6 Add automated tests for header/content rectangles, selected indicator geometry, overflow visible/hidden indexes, no-overlap page switching, and resize recomputation.
- [x] 6.7 Add automated tests for theme refresh, accessible text updates, and selection/page state preservation across Light/Dark changes.
- [x] 6.8 Add a VisualCheck case guarded by `SKIP_VISUAL_TEST` that displays a WinUI Gallery email-style Pivot with an externally anchored label, disabled items, overflow, page switching, and Light/Dark theme switching.

## 7. Validation

- [x] 7.1 Validate the OpenSpec change with `openspec validate add-pivot --strict`.
- [x] 7.2 Build incrementally with the CMake test target `test_pivot` using `cmake --build build --target test_pivot`.
- [x] 7.3 Run automated tests with `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_pivot`.
- [x] 7.4 Run `git diff --check` for Pivot source, tests, and OpenSpec files.
- [ ] 7.5 Optionally run the manual Pivot VisualCheck without `SKIP_VISUAL_TEST=1` to inspect WinUI Gallery-style Pivot states and theme switching.
