## Context

`ComboBox` currently implements its dropdown as `ComboBox::ComboBoxPopup : Dialog`. The popup disables dialog animation, disables dragging, sets `Qt::Popup` window flags, manually computes global screen coordinates, and reuses `Dialog::drawShadow()` / `shadowSize()` for Fluent overlay visuals.

That inheritance gives the dropdown useful drawing helpers, but the semantic model is mismatched. A ComboBox dropdown is an anchored transient surface: it is attached to the ComboBox, light-dismisses on outside press or Escape, updates selection, then closes. It should not inherit dialog result semantics, smoke overlay behavior, drag behavior, or top-level dialog lifecycle.

The project already has a `Popup` / `Flyout` stack for anchored transient surfaces. `Flyout` provides anchor + placement semantics, defaults to non-modal light-dismiss behavior, and is already used by `AutoSuggestBox::SuggestionListPopup`, which is structurally close to a ComboBox dropdown.

Relevant current constraints:
- `Popup` is a same-window overlay attached to the original parent's top-level widget, not a `QDialog` top-level window.
- `Flyout::computePosition()` centers the popup relative to its anchor by default; ComboBox dropdowns need left-edge alignment and width matching.
- `Popup` reserves a `Spacing::Standard` shadow margin around the visible card. Content widgets must be laid out inside that margin.
- Existing ComboBox public API and behavior should remain compatible.

## Goals / Non-Goals

**Goals:**
- Refactor `ComboBox::ComboBoxPopup` to inherit from `view::dialogs_flyouts::Flyout` instead of `Dialog`.
- Keep the dropdown visually equivalent: Fluent rounded overlay card, shadow, background, stroke, compact card inset, selected accent indicator, scrollbar gutter, and up to six visible rows.
- Keep existing ComboBox public API unchanged.
- Preserve interaction behavior: click toggles dropdown, selecting an item updates `currentIndex` and closes, editable mode mirrors selected text, outside press/Escape dismisses.
- Position the dropdown as a ComboBox-specific anchored surface: left edge aligned to the ComboBox, width at least the ComboBox width, above/below placement based on available space, clamped to the owning top-level window.
- Align ComboBox dropdown architecture with `AutoSuggestBox` suggestion popup so future anchored list popups can share patterns.
- Add tests that verify ownership/lifecycle, positioning, selection, dismissal, and editable synchronization.

**Non-Goals:**
- Do not introduce new public ComboBox API.
- Do not change `Flyout` public API unless implementation proves a shared abstraction is truly necessary.
- Do not change `Dialog`, `ContentDialog`, or existing dialog behavior.
- Do not make ComboBox dropdown a native OS top-level popup that can escape the application top-level window.
- Do not redesign ComboBox visuals beyond preserving current behavior under the new base class.
- Do not merge AutoSuggestBox and ComboBox popups into a shared production abstraction in this change; that can be a follow-up once both are stable.

## Decisions

### Decision 1: `ComboBoxPopup` inherits from `Flyout`

`ComboBoxPopup` will include `view/dialogs_flyouts/Flyout.h` and inherit from `view::dialogs_flyouts::Flyout`. It will configure itself similarly to `AutoSuggestBox::SuggestionListPopup`:

- `setObjectName("ComboBoxPopup")`
- `setAnimationEnabled(false)` initially, preserving current immediate-open behavior
- `setPlacement(Flyout::Auto)`
- `setAnchorOffset(m_comboBox->popupOffset())` before showing
- `setModal(false)`
- `setDim(false)`
- `setClosePolicy(CloseOnPressOutside | CloseOnEscape)`

Rationale:
- This matches the actual dropdown semantics.
- It removes dependency on `Dialog::open/exec/done` concepts.
- It aligns ComboBox with the already implemented AutoSuggestBox suggestion popup.

Alternatives considered:
- Keep `Dialog` inheritance and only rename comments: rejected because the semantic mismatch remains.
- Inherit directly from `Popup`: possible, but `Flyout` already owns anchor/placement defaults and is closer to the intended behavior.
- Create a new `DropDownPopup` base first: useful later, but premature before the ComboBox migration proves the shape.

### Decision 2: Override `computePosition()` for ComboBox dropdown alignment

The dropdown will override `computePosition()` instead of relying on default `Flyout` positioning. The algorithm will mirror the AutoSuggestBox approach while preserving ComboBox-specific offset and sizing:

1. Resolve the ComboBox geometry in the owning top-level widget.
2. Treat the visible popup card as `width() - shadow * 2` by `height() - shadow * 2`.
3. Prefer below the ComboBox unless there is insufficient space below and more space above.
4. Align the visible card's left edge with the ComboBox left edge.
5. Clamp the visible card into the top-level widget with a small margin.
6. Return the underlying widget position by subtracting the shadow margin.

Rationale:
- A standard Flyout centers on the anchor; a ComboBox dropdown should line up with the control's left edge.
- Positioning inside `computePosition()` lets `Popup::open()` resize/layout first, then position using final dimensions.
- It keeps the shadow margin compensation localized and consistent with Popup/Flyout expectations.

Alternatives considered:
- Continue manual `mapToGlobal()` and screen clamp: rejected because it bypasses the Popup/Flyout overlay coordinate system.
- Extend `Flyout::Placement` with a new left-aligned bottom placement: rejected for now because only ComboBox needs this precise behavior today.

### Decision 3: Use `Flyout::closed` to synchronize ComboBox state

`ComboBoxPopup` will connect the inherited `closed` signal to the owner's `onPopupHidden()` path. `ComboBox::hidePopup()` should call `m_popup->close()` rather than `hide()` so the Popup/Flyout lifecycle remains coherent.

Rationale:
- Direct `hide()` bypasses `Popup::closed`, `isOpenChanged(false)`, event-filter cleanup, and internal state transitions.
- Synchronizing through `closed` handles Escape and outside-click dismissal as well as explicit owner-driven closure.

Alternatives considered:
- Keep a `hideEvent()` override and call `onPopupHidden()`: possible but less aligned with Popup's lifecycle API and easier to desynchronize.

### Decision 4: Preserve Fluent content layout inside the popup card

The `ListView` remains the popup content widget and uses the existing `ComboBoxItemDelegate`. The popup will continue to use the standard shadow margin (`Spacing::Standard`) and place the list view inside the visible card with a compact inset so the `QAbstractScrollArea` viewport never touches the rounded card edge. The embedded list paints the same `bgLayer` surface as the Flyout card, and selected/hovered item backgrounds reserve space for the Fluent scrollbar gutter when scrolling is needed.

Rationale:
- The change is architectural, not a visual redesign.
- Keeping the delegate localized to ComboBox reduces behavioral and visual risk while allowing the scrollbar gutter to avoid covering item highlights.
- The repo memory notes that Popup/Flyout content must respect the shadow margin to avoid card/layout mismatch.

Alternatives considered:
- Attach a layout directly to the Popup root: rejected because Popup reserves shadow margins and direct root layouts can desynchronize content size from the painted card.
- Rewrite dropdown content using `QComboBox`'s native view: rejected because the project self-paints Fluent visuals.
- Let the ListView viewport fill the entire rounded card: rejected because transparent-window composition can expose square viewport corners and make the scrollbar overlap selected item backgrounds.

### Decision 5: Keep same-window overlay semantics

The refactored ComboBox dropdown will be constrained to the owning top-level widget, consistent with `Popup` and `Flyout`. It will not use `Qt::Popup` native top-level window flags.

Rationale:
- Same-window overlay behavior is the current project contract for Popup/Flyout.
- It gives consistent event filtering and rendering with other Fluent floating surfaces.
- It avoids mixing native top-level popup behavior with custom QWidget overlay behavior.

Alternatives considered:
- Extend Popup/Flyout to support native popup mode: out of scope and would affect all popup users.

## Risks / Trade-offs

- **[Risk] Same-window Flyout may not escape the application window like native `Qt::Popup`.** → Mitigation: This is aligned with existing Popup/Flyout specs; tests should assert clamp/up-flip behavior inside the top-level widget.
- **[Risk] Calling `close()` instead of `hide()` may change timing when popup animations are later enabled.** → Mitigation: Keep animation disabled for this migration, and verify close signals update `m_popupVisible` promptly.
- **[Risk] Popup event filtering may allow the original outside mouse event to propagate, which could trigger the ComboBox click handler again in edge cases.** → Mitigation: Add a regression test or manual VisualCheck for outside-click dismissal and ComboBox re-open behavior.
- **[Risk] Shadow margin sizing may cause off-by-one geometry differences from the Dialog version.** → Mitigation: Centralize `shadow = Spacing::Standard`, assert list geometry/card width in tests, and visually verify the dropdown.
- **[Trade-off] Not extracting a shared anchored-list popup means AutoSuggestBox and ComboBox keep some duplicated positioning logic.** → This keeps this change small; extraction can follow after both components settle.

## Migration Plan

1. Replace `Dialog` include/inheritance in `ComboBox.h` with `Flyout`.
2. Update `ComboBoxPopup` construction to configure Flyout light-dismiss behavior and connect `closed` to ComboBox state synchronization.
3. Replace `showForComboBox()` positioning with anchor-based sizing and `showAt(m_comboBox)` / `move(computePosition())` when already open.
4. Implement ComboBox-specific `computePosition()` for left-aligned above/below placement.
5. Replace `hide()` calls in ComboBox popup flow with `close()` where lifecycle state matters.
6. Update tests and VisualCheck coverage.
7. Run the affected ComboBox test target only, per project testing policy.

Rollback is straightforward: restore the previous `Dialog` inheritance and manual global positioning if a platform-specific Popup/Flyout overlay issue appears.
