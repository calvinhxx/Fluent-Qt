## Context

`src/view/navigation/` now contains lightweight navigation controls such as `Breadcrumb` and the refactored `Pivot`, plus `StackContentHost` for external page composition. `SelectorBar` should follow that direction rather than returning to a control-owned page host model.

WinUI Gallery uses `SelectorBar` in three useful patterns: a selected item drives an external frame navigation, a selected item swaps an external item source, and hidden/visible sample-code selector items pick the first visible item. Those examples rely on `SelectorBar.SelectedItem`, `SelectorBar.Items.IndexOf(selectedItem)`, `SelectorBarItem.IsSelected`, and `SelectionChanged`, but the content being switched lives outside the selector.

## Goals / Non-Goals

**Goals:**

- Add `view::navigation::SelectorBar` as a Qt Widgets Fluent component using `QWidget + FluentElement + QMLPlus` inheritance.
- Provide a horizontal selector row with text, optional icon glyphs, selected indicator, overflow, hidden/disabled item handling, pointer/keyboard support, theme updates, and accessibility text.
- Keep selection state explicit through `selectedIndex`, `selectedItem`, `setSelectedIndex()`, and a WinUI-inspired item selected flag API.
- Let callers use `currentChanged` or `selectionChanged` to drive external content, including `StackContentHost`, data source swaps, or presenter visibility.
- Cover the component with automated tests and a VisualCheck sample that uses `StackContentHost` externally.

**Non-Goals:**

- Do not make `SelectorBar` own pages, routes, frames, `QStackedLayout`, or `StackContentHost` instances.
- Do not implement TabView behaviors such as close buttons, add-tab affordances, document reordering, or title-bar integration.
- Do not implement a XAML template/delegate system or arbitrary QWidget content inside selector headers in v1.
- Do not change existing `Breadcrumb`, `NavigationView`, `Pivot`, `StackContentHost`, or `TabView` public APIs.
- Do not style the control with QSS/QPalette; component visuals remain custom-painted from Fluent tokens.

## Decisions

### 1. Model SelectorBar as a lightweight selector, not a page container

`SelectorBar` will own only ordered selector items, current selection, interaction state, layout records, and painting. It will emit signals when selection changes, and callers decide whether that means `StackContentHost::setCurrentIndex()`, `ItemsView` source changes, or a presenter visibility update.

Rationale: this mirrors WinUI Gallery usage where `SelectorBar` selects and external code navigates a frame or changes data. It also matches the current lightweight `Pivot`/`Breadcrumb` boundary and avoids duplicating `StackContentHost` ownership in every navigation selector.

Alternative considered: give `SelectorBarItem` an optional QWidget page like the old Pivot design. That would make simple samples easier, but it would blur responsibilities and recreate the page ownership coupling that was just removed from Pivot and TabView.

### 2. Use a small value record for items

The public item record should include `text`, optional `iconGlyph`, `enabled`, `visible`, `data`, and `accessibleName`. Selection remains a single control-level index, but an item-selected setter can mirror WinUI's `SelectorBarItem.IsSelected` by selecting or clearing the matching index.

Rationale: Qt callers get simple value semantics like `PivotItem` and `BreadcrumbItem`, while still supporting WinUI-style item selection and first-visible-item initialization.

### 3. Keep layout deterministic and testable

The row will be custom-painted from layout records. Geometry helpers should expose the row, item rectangles, selected indicator, overflow controls, visible indexes, hidden indexes, and focused/hovered item where useful. The selected indicator must not change item geometry.

Rationale: navigation controls in this repo already rely on geometry helpers for reliable tests. Deterministic records prevent VisualCheck-only regressions and make overflow behavior testable.

### 4. Implement hidden and disabled behavior separately

Hidden items do not participate in layout, hit testing, keyboard traversal, or visible index lists. Disabled items remain visible but cannot be selected by pointer, keyboard, or programmatic selection. When selection is invalidated by hiding, disabling, or removing an item, the control should clamp to the nearest visible enabled item or `-1`.

Rationale: WinUI Gallery hides sample code selector items with no content and selects the first visible item. Disabled state is a separate accessibility and visual concern.

### 5. Use Pivot-like visuals with SelectorBar-specific metrics

The control can reuse the visual language of the light Pivot header: Body text, optional Segoe Fluent icon glyph, accent selected indicator, text/icon color changes for hover/pressed/selected/disabled, and no large button chrome around each header. Metrics should be fixed and responsive enough for horizontal tool surfaces.

Rationale: `SelectorBar` is visually adjacent to Pivot but should remain slightly more general. Using the same semantic tokens keeps the navigation family coherent while avoiding a dependency on Pivot internals.

### 6. Overflow exposes indexes before menus

For v1, overflow should be deterministic and testable through either scroll buttons or a more-button event that emits hidden indexes. A full popup menu for hidden items can be composed later using existing flyout/menu controls without blocking the base selector.

Rationale: the immediate requirement is a custom selector bar and testable hidden item behavior. Exposing hidden indexes keeps the public API useful without adding a new popup interaction surface.

## Risks / Trade-offs

- [Risk] `SelectorBar` and `Pivot` may duplicate header layout and painting logic. -> Mitigation: keep metrics and helper methods local for now, and only extract shared helpers after a real third use case emerges.
- [Risk] Users may expect `SelectorBar` to switch pages automatically. -> Mitigation: document and test the external composition pattern with `StackContentHost` so the boundary is obvious.
- [Risk] Overflow behavior could feel incomplete without a popup menu. -> Mitigation: provide visible/hidden index helpers and an overflow signal; a later change can compose a menu without changing item state APIs.
- [Risk] Hidden item selection can surprise callers if indexes shift visually but not logically. -> Mitigation: keep APIs index-based over the full item list and expose `visibleItemIndexes()` / `hiddenItemIndexes()` for clarity.
- [Risk] Exact WinUI brush names may not map one-to-one to local tokens. -> Mitigation: use existing semantic text, accent, disabled, background, and stroke tokens and validate through VisualCheck.

## Migration Plan

No migration is required. This change adds a new component and test target without modifying existing component APIs.

## Open Questions

- Should the first implementation expose both scroll-button overflow and more-button overflow, or start with one behavior and leave the enum extensible?
- Should `setItemSelected(index, false)` clear selection only when the item is currently selected, or ignore false writes to keep selection stable? The spec chooses clear-on-current-false for WinUI parity, but implementation can revisit before apply if tests reveal awkwardness.