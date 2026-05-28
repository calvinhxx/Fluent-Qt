## 1. Module Setup

- [x] 1.1 Add `src/view/scrolling/PipsPager.h` with QWidget/FluentElement/QMLPlus inheritance, public enums, Qt properties, methods, and signals.
- [x] 1.2 Add `src/view/scrolling/PipsPager.cpp` with construction defaults, focus/mouse tracking setup, and theme update plumbing.
- [x] 1.3 Add geometry constants and internal hit-test structures for pip cells and previous/next caret slots.

## 2. State and Public API

- [x] 2.1 Implement `numberOfPages`, `selectedPageIndex`, and `maxVisiblePips` setters with clamping, signal emission, geometry updates, and no duplicate signals.
- [x] 2.2 Implement `orientation`, `previousButtonVisibility`, and `nextButtonVisibility` setters with layout invalidation and repaint behavior.
- [x] 2.3 Implement `selectedIndexChanged(oldIndex, newIndex)` emission and accessible text refresh for page changes.
- [x] 2.4 Implement visible pip window calculation for `numberOfPages > maxVisiblePips`, including edge clamping and selected pip visibility.

## 3. Rendering and Layout

- [x] 3.1 Implement `sizeHint()` and `minimumSizeHint()` for horizontal/vertical layouts, collapsed buttons, and reserved caret slots.
- [x] 3.2 Implement self-painted pips with 12px hit cells and Figma dot sizes for inactive/selected Rest/Hover/Pressed/Disabled states.
- [x] 3.3 Implement Light/Dark and disabled colors using existing Fluent theme tokens and repaint from `onThemeUpdated()`.
- [x] 3.4 Implement previous/next caret rendering with 24px slots, 8px Segoe Fluent Icons glyphs, edge hiding, and `VisibleOnPointerOver` behavior.
- [x] 3.5 Ensure rendering uses antialiasing and avoids QSS for Fluent visuals.

## 4. User Interaction

- [x] 4.1 Implement mouse move/leave tracking for hovered pip/caret state.
- [x] 4.2 Implement mouse press/release selection for visible pips and previous/next caret navigation.
- [x] 4.3 Implement keyboard navigation for orientation-specific arrow keys plus Home/End.
- [x] 4.4 Ensure disabled state blocks pointer and keyboard selection while keeping disabled visual rendering.
- [x] 4.5 Ensure previous/next navigation never wraps at first or last page.

## 5. Tests and VisualCheck

- [x] 5.1 Add `tests/views/scrolling/TestPipsPager.cpp` covering default API state, property signals, clamping, visible pip windowing, and no duplicate signals.
- [x] 5.2 Add tests for horizontal/vertical size hints, caret visibility policies, edge navigation, pointer hit testing, keyboard navigation, and disabled behavior.
- [x] 5.3 Add tests for theme refresh safety, accessible text, and selected index signal old/new values.
- [x] 5.4 Add VisualCheck showing Light/Dark, horizontal/vertical, carets true/false, scrolling pips, disabled state, and a simple external-content binding example guarded by `SKIP_VISUAL_TEST`.
- [x] 5.5 Update `tests/views/scrolling/CMakeLists.txt` to register `test_pips_pager`.

## 6. Verification

- [x] 6.1 Build `test_pips_pager`.
- [x] 6.2 Run `ctest --test-dir build -R PipsPager --output-on-failure`.
- [x] 6.3 Run or document manual VisualCheck review for `test_pips_pager --gtest_filter=*VisualCheck*`.
- [x] 6.4 Run `openspec validate add-pips-pager --strict`.
- [x] 6.5 Run `openspec status --change add-pips-pager` and confirm all artifacts are complete.
