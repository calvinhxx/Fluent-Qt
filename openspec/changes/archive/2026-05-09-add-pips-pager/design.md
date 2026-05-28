## Context

`src/view/scrolling` currently contains `ScrollBar` and `ScrollView`; `tests/views/scrolling` already registers `test_scrollbar` and `test_scroll_view`. The root build gathers `src/**/*.h` and `src/**/*.cpp`, so adding `PipsPager.h/.cpp` under `src/view/scrolling` is enough for library compilation, while tests still need explicit CMake registration.

Figma node `30180:110389` documents the Windows UI Kit PipsPager page. The main component set includes Light/Dark, Horizontal/Vertical, and Carets true/false variants. The pip part uses a 12px hit container; inner dot size changes by selected/inactive and Rest/Hover/Pressed/Disabled state: inactive rest/disabled 4px, inactive hover 5px, inactive pressed 3px, selected rest/disabled 6px, selected hover 7px, selected pressed 5px. Caret buttons use 24px slots and Segoe Fluent Icons glyphs `EDD9`, `EDDA`, `EDDB`, and `EDDC`.

WinUI Gallery shows two core usage patterns: binding `SelectedPageIndex` to a `FlipView.SelectedIndex`, and changing `Orientation`, `PreviousButtonVisibility`, and `NextButtonVisibility` at runtime. Microsoft Learn adds `MaxVisiblePips`, default five visible pips, scrolling pips when page count exceeds the visible count, non-wrapping previous/next behavior, and `VisibleOnPointerOver` navigation buttons.

## Goals / Non-Goals

**Goals:**

- Add `view::scrolling::PipsPager` as a Fluent Qt Widgets control.
- Match WinUI/Figma core API: page count, selected page index, max visible pips, orientation, previous/next button visibility, and selected-index change notification.
- Match Figma visual metrics for pip/caret sizing, Light/Dark token-driven colors, and selected/inactive state changes.
- Support pointer, keyboard, and programmatic navigation without wrapping from first to last or last to first.
- Add automated tests and a VisualCheck in `tests/views/scrolling`.

**Non-Goals:**

- Do not implement a full `FlipView` or carousel in this change.
- Do not add custom pip templates/styles in v1; expose stable metrics and state behavior first.
- Do not modify `ScrollBar` or `ScrollView` requirements.
- Do not support non-linear page graphs or explicit page numbering inside the control.

## Decisions

1. **Implement as one self-painted QWidget.**
   - `PipsPager` inherits `QWidget`, `FluentElement`, and `view::QMLPlus`.
   - Pips and carets are painted in `paintEvent()` and represented internally by hit rectangles.
   - Rationale: the control is visually compact and state-heavy; avoiding child widgets keeps hover/pressed animation, scrolling pip windows, and layout deterministic.
   - Alternative considered: compose multiple `Button` children. That would simplify clicks but complicate pip window scrolling, disabled edge behavior, and VisualCheck pixel stability.

2. **Expose WinUI-aligned properties.**
   - Public properties:
     - `numberOfPages` default `5`
     - `selectedPageIndex` default `0`
     - `maxVisiblePips` default `5`
     - `orientation` default `Qt::Horizontal`
     - `previousButtonVisibility` default `Collapsed`
     - `nextButtonVisibility` default `Collapsed`
   - Use enum `PipsPagerButtonVisibility { Collapsed, Visible, VisibleOnPointerOver }` to mirror WinUI.
   - Clamp `selectedPageIndex` to `[0, numberOfPages - 1]` when pages exist; use `0` when there are no pages.

3. **Compute a visible pip window around the selected page.**
   - `visiblePipCount = min(numberOfPages, maxVisiblePips)`; if `maxVisiblePips == 0` or `numberOfPages == 0`, draw no pips.
   - When `numberOfPages > maxVisiblePips`, calculate `firstVisiblePage` so the selected page is centered when possible and clamped at the start/end.
   - This matches the WinUI scrolling-pips behavior without requiring a separate scroll animation in v1.

4. **Treat caret slots separately from caret visibility.**
   - `Collapsed` does not reserve the 24px caret slot.
   - `Visible` reserves the slot; edge buttons are hidden/disabled at first/last page but the slot remains stable.
   - `VisibleOnPointerOver` reserves the slot and only paints the button while hovered or keyboard-focused, with the same edge disabling rules.
   - Rationale: this follows WinUI's difference between collapsed and hidden but space-preserving button behavior.

5. **Use token-driven colors and Figma sizes.**
   - Dot and caret color comes from `FluentElement::themeColors()` semantic colors, using control-strong/text-secondary style tokens for active/inactive and disabled alpha.
   - The geometry uses stable constants: pip cell 12px, caret slot 24px, caret icon 8px, horizontal/vertical gap from the cell grid rather than ad hoc layout.
   - Rendering uses antialiasing and no QSS.

6. **Input model mirrors page navigation.**
   - Clicking a visible pip selects that page.
   - Clicking previous/next moves by one page only when possible.
   - Keyboard navigation supports Left/Right for horizontal, Up/Down for vertical, and Home/End for first/last.
   - Disabled widget state blocks user input while preserving rendering in Disabled state.

7. **Accessibility uses current page context.**
   - When selected index changes, update accessible name/description to include `Page x of n selected`.
   - Emit `selectedIndexChanged(oldIndex, newIndex)` in addition to the Qt property signal for business logic and tests.

## Risks / Trade-offs

- [Risk] Exact WinUI theme brush names do not map one-to-one to this project's tokens. -> Mitigation: use existing Fluent semantic colors and assert geometry/state behavior in tests; VisualCheck covers final look.
- [Risk] Dot hit areas are small. -> Mitigation: keep the 12px Figma hit container even when the visible dot is 3-7px.
- [Risk] Large page counts can cause expensive per-page work. -> Mitigation: only compute and paint the visible pip window, not every page.
- [Risk] Edge caret behavior can shift layout if implemented as hide/show. -> Mitigation: reserve slots for `Visible` and `VisibleOnPointerOver`, and vary paint/enabled state only.
- [Risk] VisualCheck could block automation. -> Mitigation: follow existing `SKIP_VISUAL_TEST` guard used by scrolling tests.
