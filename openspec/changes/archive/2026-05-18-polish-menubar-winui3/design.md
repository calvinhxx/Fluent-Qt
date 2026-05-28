## Context

`FluentMenuBar` currently subclasses `QMenuBar` and draws its own top-level item backgrounds/text, but it also uses QSS to set menu bar background, item padding, item margin, and item radius so that Qt's internal `actionGeometry()` produces usable hit regions. This makes the implementation partially reasonable for a simple static menu, but it is not consistent with the project guideline that Fluent visuals should come from custom painting and design tokens instead of QSS/QPalette styling.

The attached Figma Windows UI Kit node for Menus & Toolbars includes MenuBar examples for light/dark mode, selected/rest/hover/pressed/disabled/focus states, and a `File Menu Button` around 40px tall. WinUI Gallery's MenuBar samples cover three functional tiers: a simple File/Edit/Help bar, a menu with keyboard accelerators, and a more complex menu with submenus, separators, and radio items. The Accessibility keyboard page also shows MenuBar access keys via `AccessKey` on `MenuBarItem` and `MenuFlyoutItem`.

## Goals / Non-Goals

**Goals:**

- Make MenuBar visual behavior deterministic and WinUI-aligned without QSS visual styling.
- Preserve existing Qt `QMenuBar`, `QMenu`, `QAction`, and `addMenu()` compatibility for application code.
- Support the WinUI-visible top-level states: rest, hover, pressed/open, disabled, selected/open, and keyboard focus.
- Ensure MenuBar integrates cleanly with `FluentMenu` flyouts that include separators, nested submenus, checked/radio-like items, disabled items, shortcuts, and accelerator text.
- Add automated tests for geometry, state changes, keyboard navigation, action triggering, theme updates, and menu content coverage.
- Refresh VisualCheck into a WinUI Gallery-style set of examples instead of a single visual-only smoke window.

**Non-Goals:**

- Replacing the entire Qt menu/action model with a custom non-Qt action tree.
- Implementing platform global menu bar behavior on macOS; `setNativeMenuBar(false)` remains required for in-window Fluent rendering.
- Building a full WinUI KeyTip overlay system in this change. Access key metadata and Alt-letter activation can be supported, but full floating key tip visuals can be deferred.
- Changing unrelated menu/flyout components outside the behavior needed by MenuBar and its dropdowns.

## Decisions

1. Keep inheritance from `QMenuBar`, but stop using QSS for visual item styling.

   The public API compatibility is valuable because existing callers can continue to use `addMenu`, `addAction`, `QAction::setShortcut`, and triggered signals. The visual layer should move to `paintEvent()` and token-driven metrics. If Qt's default `actionGeometry()` is not stable without QSS, `FluentMenuBar` will maintain its own top-level `QHash<QAction*, QRect>` layout cache and use that cache for paint/hit tests while still calling `QMenu::popup()` or triggering actions through Qt.

   Alternative considered: keep QSS only for padding/geometry. This is simpler but keeps layout and visuals coupled to stylesheet parsing and conflicts with the library's design-token architecture.

2. Treat top-level MenuBar items as lightweight Fluent button parts.

   The top-level item metric should use Body font, a 40px row/item height, Control corner radius, horizontal control padding, and subtle state fills. Hover and pressed/open states map to `subtleSecondary` and `subtleTertiary`; disabled text maps to `textDisabled`; focus uses the project focus stroke tokens and should appear for keyboard focus, not ordinary mouse hover.

   Alternative considered: reuse `Button` child widgets for each top-level item. That would provide shared styling but makes QMenuBar ownership, action insertion/removal, and native menu popup behavior more fragile. Custom painting against `QAction` is a better fit for MenuBar.

3. Use an explicit interaction state machine for top-level actions.

   `FluentMenuBar` should track hovered action, pressed action, focused action, and open menu action. Mouse move/press/release, leave, focus in/out, and action/menu visibility changes should update these states. A top-level item whose menu is visible remains in the pressed/open visual state even when the pointer is inside the flyout.

   Alternative considered: rely on `activeAction()` only. It is convenient but too dependent on Qt style behavior and does not distinguish keyboard focus from mouse hover cleanly.

4. Keep flyout rendering in `FluentMenu`, but expand the contract it must satisfy for MenuBar.

   `FluentMenu` already paints its popup plate, shadow, separators, item hover, checked state, and text. The change should make this more complete for MenuBar by adding nested submenu indication, shortcut/accelerator text alignment, disabled-state tests, checked/radio-style visual states, and deterministic light/dark geometry. Existing `QAction` checkable/exclusive groups can model radio behavior without introducing a new public action type immediately.

   Alternative considered: create separate MenuFlyoutItem classes for every WinUI item type now. That may become useful later, but the current Qt QAction model can cover the requested MenuBar fidelity with less API churn.

5. Tests should move from VisualCheck-only to behavior-first coverage.

   `TestMenuBar.cpp` currently creates a VisualCheck window with native Qt layouts, `QLabel`, and QSS background. The new tests should include non-visual checks for item geometry, state color-affecting transitions via deterministic public geometry/hit behavior, triggered actions, keyboard navigation, shortcuts, enabled/disabled handling, and theme changes. VisualCheck should remain guarded by `SKIP_VISUAL_TEST` and demonstrate the three WinUI Gallery MenuBar examples in a scroll-safe window if needed.

## Risks / Trade-offs

- Custom top-level rect cache can diverge from Qt's internal action handling. → Mitigate by using the cache consistently for paint and pointer events, and by adding tests around action insertion/removal, hidden actions, disabled actions, and menu opening.
- Keyboard behavior can vary by platform, especially Alt handling on macOS. → Mitigate by explicitly testing cross-platform-safe keys and documenting any platform-specific limitations; keep macOS in-window menu mode via `setNativeMenuBar(false)`.
- Full KeyTip visuals are larger than this polish pass. → Mitigate by supporting access key metadata/activation first and leaving floating key-tip badges as a future change if needed.
- Improving `FluentMenu` for MenuBar may affect direct context menu users. → Mitigate with focused tests for existing simple menu rendering, separators, disabled actions, and checked actions.
