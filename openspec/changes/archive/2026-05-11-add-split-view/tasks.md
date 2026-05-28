## 1. Component Skeleton

- [x] 1.1 Add `src/view/collections/SplitView.h` with `QWidget + FluentElement + view::QMLPlus` inheritance, enums/properties, signals, and public pane APIs.
- [x] 1.2 Add `src/view/collections/SplitView.cpp` with constructor setup, theme updates, size hints, and basic event overrides.
- [x] 1.3 Register SplitView sources in the library build configuration.

## 2. Pane Model And Layout

- [x] 2.1 Implement internal pane records and `SplitViewPaneOptions` for minimum, preferred, maximum, and fill settings.
- [x] 2.2 Implement add/insert/remove/query pane management with correct parenting and hidden-pane exclusion.
- [x] 2.3 Implement horizontal and vertical layout calculation with handle spacing, cross-axis fill, and default last-visible fill pane.
- [x] 2.4 Implement explicit fill pane resolution, multiple-fill conflict handling, and min/preferred/max clamping.
- [x] 2.5 Add pane and handle geometry query helpers for automated tests.

## 3. Handle Interaction And Rendering

- [x] 3.1 Implement handle hit testing, hover state, cursor updates, and disabled-state input suppression.
- [x] 3.2 Implement mouse press/move/release dragging that updates adjacent pane preferred sizes and emits resize signals.
- [x] 3.3 Implement `resizing` state and `resizingChanged` emission.
- [x] 3.4 Implement Fluent handle painting for rest, hover, pressed, disabled, Light, and Dark states without QSS.
- [x] 3.5 Implement configurable handle hit width and visual thickness properties.

## 4. Persistence And Theme Behavior

- [x] 4.1 Implement versioned `saveState()` containing orientation, fill pane index, pane count, and preferred sizes.
- [x] 4.2 Implement all-or-nothing `restoreState()` with malformed-data and pane-count mismatch rejection.
- [x] 4.3 Ensure `onThemeUpdated()` refreshes colors/fonts and repaints without changing pane sizes.

## 5. Tests And VisualCheck

- [x] 5.1 Add `tests/views/collections/TestSplitView.cpp` with SetUpTestSuite resource initialization following project test patterns.
- [x] 5.2 Register `test_split_view` in `tests/views/collections/CMakeLists.txt`.
- [x] 5.3 Add automated tests for defaults, pane management, orientation, constraints, fill behavior, geometry helpers, dragging, disabled input, and state persistence.
- [x] 5.4 Add VisualCheck with a QML-style multi-pane splitter and a WinUI Gallery-inspired pane/content sample using AnchorLayout where useful.
- [x] 5.5 Validate incrementally with `cmake --build build --target test_split_view` and `SKIP_VISUAL_TEST=1 ./build/tests/views/collections/test_split_view`.

## 6. Documentation And Final Validation

- [x] 6.1 Update any relevant README or component index documentation if the project maintains one for collections controls.
- [x] 6.2 Run `openspec validate add-split-view --strict` and fix any proposal/spec/task issues.
- [x] 6.3 Run `git diff --check` for changed SplitView and OpenSpec files.