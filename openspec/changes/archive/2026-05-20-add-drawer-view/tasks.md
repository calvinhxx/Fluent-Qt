## 1. Component Skeleton And API

- [x] 1.1 Add `src/view/collections/DrawerView.h` with `QWidget + FluentElement + view::QMLPlus` inheritance, `DrawerEdge` enum, close policy flags, Q_PROPERTY declarations, public API, signals, and geometry query helpers.
- [x] 1.2 Add `src/view/collections/DrawerView.cpp` with constructor defaults, accessibility/focus setup, size hints, duplicate-signal suppression, and `onThemeUpdated()` repaint behavior.
- [x] 1.3 Implement content widget accessors and ownership semantics: `setContentWidget()`, `contentWidget()`, replacement detach without delete, and content geometry updates.
- [x] 1.4 Add private state for original parent/top-level tracking, panel/content/scrim geometry, animation, drag state, and event filter connections using safe `QPointer` storage.

## 2. Open Close Lifecycle And Geometry

- [x] 2.1 Implement same-window `open()`, `close()`, `setIsOpen(bool)`, and lifecycle signals without creating `Qt::Window`, `Qt::Dialog`, or `QDialog` surfaces.
- [x] 2.2 Implement `position` clamping, position change notification, animation-disabled synchronous transitions, and position-based `QPropertyAnimation` for open/close.
- [x] 2.3 Implement left, right, top, and bottom panel geometry from top-level rect, drawer length, effective length clamping, and normalized position.
- [x] 2.4 Recompute panel, content, and scrim geometry on top-level resize, drawer length changes, edge changes, and position changes.
- [x] 2.5 Ensure close completion hides the drawer, clears transient filters/scrim state, and remains safe if parent/top-level widgets are destroyed.

## 3. Modal Scrim And Light Dismiss

- [x] 3.1 Implement an internal scrim widget or helper that covers the top-level behind the drawer when modal or dim behavior is needed.
- [x] 3.2 Paint the dim scrim with Fluent smoke/material tokens and keep non-dim modal scrim visually transparent but input-blocking.
- [x] 3.3 Implement close policy handling for outside pointer press, Escape, and `NoAutoClose` without duplicate close transitions.
- [x] 3.4 Preserve non-modal background interaction by allowing outside press events to continue after light-dismiss handling.

## 4. Interactive Drag And Fluent Painting

- [x] 4.1 Implement top-level event filtering for closed edge drag detection using normalized `dragMargin`.
- [x] 4.2 Implement drawer-panel drag open/close updates with axis-aware distance mapping and halfway-threshold settle behavior.
- [x] 4.3 Respect `interactive=false`, disabled state, and active animation state when processing pointer gestures.
- [x] 4.4 Paint the drawer panel background, edge stroke, and shadow/elevation with Fluent design tokens and no QSS/QPalette surface styling.
- [x] 4.5 Keep hosted content clipped and correctly raised above the scrim while the drawer slides.

## 5. Automated Tests And VisualCheck

- [x] 5.1 Add `tests/views/collections/TestDrawerView.cpp` covering default properties, inheritance pattern, size hints, duplicate-signal suppression, and animation-disabled open/close lifecycle.
- [x] 5.2 Add geometry tests for all four edges, partial position, drawer length normalization, content geometry, and top-level resize recomputation.
- [x] 5.3 Add content hosting tests for set, replace, clear, parent changes, visibility, and non-deletion semantics.
- [x] 5.4 Add modal, dim, close policy, outside press, Escape, non-modal propagation, disabled behavior, and parent destruction safety tests.
- [x] 5.5 Add interactive drag tests for edge drag open, panel drag close, drag threshold settle, `interactive=false`, and negative drag margin normalization.
- [x] 5.6 Add a VisualCheck test guarded by `SKIP_VISUAL_TEST` that shows left/right/top/bottom drawers, modal/dim/interactive toggles, hosted content, and theme switching.
- [x] 5.7 Register `test_drawer_view` in `tests/views/collections/CMakeLists.txt`.

## 6. Validation

- [x] 6.1 Run `openspec validate add-drawer-view --strict`.
- [x] 6.2 Build the focused target with `cmake --build --preset vcpkg-osx --target test_drawer_view`.
- [x] 6.3 Run `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/collections/test_drawer_view`.
- [x] 6.4 Run `git diff --check` for DrawerView source, tests, collection CMake, and OpenSpec files.
- [ ] 6.5 Optionally run the manual DrawerView VisualCheck without `SKIP_VISUAL_TEST=1` to inspect edge placement, dimming, and gestures.
