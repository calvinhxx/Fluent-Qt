## 1. Component Skeleton

- [x] 1.1 Add `src/view/navigation/TabView.h` with `QWidget + FluentElement + view::QMLPlus` inheritance, `Q_OBJECT`, public enums, properties, signals, and geometry query helper declarations.
- [x] 1.2 Add `src/view/navigation/TabView.cpp` with constructor setup, focus/hover attributes, internal content host creation, theme/font refresh, `sizeHint()`, `minimumSizeHint()`, and event override scaffolding.
- [x] 1.3 Add `TabViewItem` data record with text, icon glyph, content widget, closeability, enabled state, user data, and accessible name fields.
- [x] 1.4 Register required Qt metatypes for `TabViewItem` and TabView enum types.
- [x] 1.5 Confirm the root library build picks up `TabView.h/.cpp` through existing recursive source discovery.

## 2. Public API And Content Hosting

- [x] 2.1 Implement tab collection APIs: `addTab`, `insertTab`, `removeTab`, `clearTabs`, `tabCount`, `tabs`, and `tabAt` with safe invalid-index behavior.
- [x] 2.2 Implement metadata update APIs for tab text, icon glyph, closeability, enabled state, user data, accessible name, and content widget.
- [x] 2.3 Implement selected-index state, selected-index clamping, first-tab auto-selection, and selection changed signals without duplicate emissions.
- [x] 2.4 Implement content host parenting, selected content visibility, hidden non-selected content, content resizing, and content-area geometry updates.
- [x] 2.5 Finalize and implement documented ownership/take semantics for content widgets removed from tabs.
- [x] 2.6 Implement public properties for width mode, close overlay mode, placement, add-button visibility, reorder enabled, keyboard accelerator enabled, and configurable font/icon roles.

## 3. Layout, Width Modes, And Overflow

- [x] 3.1 Implement metric calculation for 40px row height, 32px tab visual height, tab padding, icon slots, close button rects, and 40x32 add/overflow affordances.
- [x] 3.2 Implement layout record generation for tabs, close buttons, add button, overflow buttons, drag indicator, and content area.
- [x] 3.3 Implement `Equal` tab width mode with minimum and maximum width constraints.
- [x] 3.4 Implement `SizeToContent` tab width mode using text/icon/close measurements and elision-safe clamping.
- [x] 3.5 Implement `Compact` tab width mode with inactive tab compaction and selected tab label preservation when space allows.
- [x] 3.6 Implement close button overlay reservation for `Auto`, `OnHover`, and `Always` without hover-induced geometry shifts.
- [x] 3.7 Implement horizontal overflow visible-window calculation, previous/next overflow controls, selected-tab bring-into-view behavior, and resize recomputation.
- [x] 3.8 Implement geometry query helpers for tabs, close buttons, add button, overflow controls, visible tab indexes, and content area.

## 4. Fluent Painting And Theme Updates

- [x] 4.1 Implement tab row painting for `InTitleBar` and `InContentArea` placements using existing semantic colors and strokes.
- [x] 4.2 Implement active and inactive tab painting with rounded geometry, primary/secondary text treatment, icon glyphs, and text elision.
- [x] 4.3 Implement hover, pressed, focus, disabled, add button, close button, overflow button, and drag indicator visuals without changing layout geometry.
- [x] 4.4 Implement close, add, and overflow glyph drawing through Segoe Fluent Icons and project typography helpers, adding missing icon constants only if required.
- [x] 4.5 Implement `onThemeUpdated()` to refresh fonts/colors, invalidate layout where metrics change, repaint, and preserve tab order, selection, and content state.
- [x] 4.6 Update accessible text for control, selected tab, tab records, close buttons, add button, and overflow affordances.

## 5. Pointer, Keyboard, And Reorder Interaction

- [x] 5.1 Implement hit testing for tab body, close button, add button, overflow controls, and non-interactive row gaps.
- [x] 5.2 Implement pointer hover/press/release selection for enabled tabs and ignore disabled tabs or disabled controls.
- [x] 5.3 Implement add button click to emit `addTabRequested()` without creating application tabs.
- [x] 5.4 Implement close button click and close keyboard actions to emit `tabCloseRequested(index)` only for closeable tabs.
- [x] 5.5 Implement programmatic close/remove flow that updates tab order, selection, content visibility, layout, and signals exactly once.
- [x] 5.6 Implement keyboard focus movement with Left/Right/Home/End and Enter/Space activation for the focused interactive record.
- [x] 5.7 Implement configurable accelerators for Ctrl+T, Ctrl+W, Delete, Ctrl+1..Ctrl+9, including disabled-accelerator behavior.
- [x] 5.8 Implement optional in-row drag reorder using `QApplication::startDragDistance()`, preserving tab content and selected logical tab.
- [x] 5.9 Ensure drag reorder never starts from close/add/overflow hit rectangles and emits `tabMoved(from, to)` plus `tabsChanged()` only on effective moves.

## 6. Tests And VisualCheck

- [x] 6.1 Add `tests/views/navigation/TestTabView.cpp` with GTest setup consistent with existing navigation tests.
- [x] 6.2 Register `test_tab_view` in `tests/views/navigation/CMakeLists.txt` using `add_qt_test_module(test_tab_view TestTabView.cpp)`.
- [x] 6.3 Add automated tests for defaults, inheritance, metatypes, item management, metadata preservation, invalid indexes, and duplicate-signal suppression.
- [x] 6.4 Add automated tests for selected-index clamping, content host visibility, content resizing, ownership/take semantics, and clear/remove behavior.
- [x] 6.5 Add automated tests for width modes, close overlay modes, placement modes, overflow visible indexes, resize recomputation, and geometry helpers.
- [x] 6.6 Add automated tests for pointer add/close/select behavior, keyboard focus movement, keyboard accelerators, disabled behavior, and drag reorder.
- [x] 6.7 Add automated tests for theme refresh and accessible text updates.
- [x] 6.8 Add a VisualCheck case guarded by `SKIP_VISUAL_TEST` that displays title-bar/content-area placement, Light/Dark, active/inactive tabs, close modes, width modes, overflow, disabled tabs, add interaction, and a WinUI Gallery-inspired document workspace.

## 7. Validation

- [x] 7.1 Build the new test target with `cmake --build build --target test_tab_view`.
- [x] 7.2 Run `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_tab_view` and ensure automated tests pass.
- [x] 7.3 Run the manual VisualCheck binary with `--gtest_filter="*VisualCheck*"` when visual review is needed and confirm Figma/WinUI reference states are represented.
- [x] 7.4 Run `openspec status --change add-tab-view` and confirm the change is apply-ready.