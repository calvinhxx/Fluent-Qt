## 1. Baseline And Test Harness

- [x] 1.1 Review current `FluentMenuBar` and `FluentMenu` behavior against the new `menu-bar` spec and note any existing behavior that must be preserved.
- [x] 1.2 Replace the VisualCheck-only `TestMenuBar.cpp` structure with a fixture that can run automated tests under `SKIP_VISUAL_TEST=1`.
- [x] 1.3 Add test helpers for creating simple, accelerator, and complex File/Edit/View/Help MenuBar examples using `FluentMenuBar`, `FluentMenu`, and Qt actions.
- [x] 1.4 Add geometry tests for visible, hidden, disabled, and inserted top-level actions.

## 2. Top-Level MenuBar Rendering

- [x] 2.1 Remove QSS visual styling from `FluentMenuBar::onThemeUpdated()` and move metrics to token-driven C++ code.
- [x] 2.2 Add a deterministic top-level item rect cache for visible actions, including Body font measurement, 40px row height, control padding, item spacing, and invalidation on action/layout/theme changes.
- [x] 2.3 Rework `paintEvent()` to draw background, top-level item fill, text, disabled text, pressed/open state, and keyboard focus visual from Fluent tokens.
- [x] 2.4 Add public or test-visible geometry access where needed to validate top-level item rects without screenshot dependence.

## 3. Top-Level MenuBar Interaction

- [x] 3.1 Track hovered, pressed, focused, and open-menu actions explicitly instead of relying only on `activeAction()`.
- [x] 3.2 Implement pointer hit testing with the custom rect cache and ensure disabled/hidden actions are skipped.
- [x] 3.3 Open associated menus at the correct global position and keep the top-level action in pressed/open state while its menu is visible.
- [x] 3.4 Preserve normal Qt triggered behavior for top-level actions that do not own a submenu.
- [x] 3.5 Implement keyboard navigation for Left/Right/Home/End and activation via Enter/Space/Down.
- [x] 3.6 Implement Escape/open-state cleanup and cross-platform-safe access key activation for top-level menu titles.

## 4. FluentMenu Flyout Coverage

- [x] 4.1 Extend `FluentMenu` painting to reserve and render shortcut/accelerator text aligned separately from item labels.
- [x] 4.2 Add submenu affordance painting for actions that own nested menus.
- [x] 4.3 Ensure separators, disabled actions, checkable actions, and exclusive radio-style action groups render correctly in light and dark themes.
- [x] 4.4 Verify popup position and animation still account for shadow margins without clipping or offset drift.

## 5. Tests And VisualCheck

- [x] 5.1 Add automated tests for action triggering, menu opening, disabled actions, hidden actions, keyboard navigation, shortcut activation, and theme updates.
- [x] 5.2 Add automated tests for `FluentMenu` separator, submenu, checked/radio, disabled, and shortcut text behavior where it can be asserted without manual screenshots.
- [x] 5.3 Rebuild VisualCheck as separate WinUI Gallery-style examples: simple MenuBar, keyboard accelerator MenuBar, and complex submenu/separator/radio MenuBar.
- [x] 5.4 Use project Fluent components and `AnchorLayout`-style layout patterns in visible test UI where practical, avoiding native QLabel/QSS demo styling.

## 6. Validation

- [x] 6.1 Build the affected target with `cmake --build build --target test_menu_bar`.
- [x] 6.2 Run `SKIP_VISUAL_TEST=1 ./build/tests/views/menus_toolbars/test_menu_bar`.
- [x] 6.3 Run a focused whitespace check for changed MenuBar/Menu source and test files.
- [x] 6.4 Manually run the MenuBar VisualCheck and compare top-level item states and flyout patterns with the Figma and WinUI Gallery references.
