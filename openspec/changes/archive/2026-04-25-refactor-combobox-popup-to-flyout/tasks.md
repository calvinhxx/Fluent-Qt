## 1. Popup Architecture Migration

- [x] 1.1 Replace `ComboBoxPopup` inheritance from `Dialog` to `Flyout` and update includes/forward references accordingly.
- [x] 1.2 Configure `ComboBoxPopup` as a non-modal, non-dimming, light-dismiss Flyout with animation disabled to preserve current immediate dropdown behavior.
- [x] 1.3 Remove Dialog-only setup from `ComboBoxPopup`, including window flags, drag configuration, and direct reliance on `Dialog::shadowSize()` / `Dialog::drawShadow()`.
- [x] 1.4 Preserve the embedded `ListView` setup, delegate assignment, theme-matched list background, selected item synchronization, and click-to-select behavior.

## 2. Anchored Dropdown Positioning

- [x] 2.1 Update `showForComboBox()` to size the Flyout card from item count, row height, compact card inset, ComboBox width, and standard Popup shadow margin.
- [x] 2.2 Implement a ComboBox-specific `computePosition()` override that left-aligns the visible card to the ComboBox, prefers below placement, flips above when space below is insufficient, and clamps to the owning top-level widget.
- [x] 2.3 Ensure reopening or resizing while already visible recomputes geometry with `move(computePosition())` rather than recreating the popup.
- [x] 2.4 Keep popup offset behavior consistent with `ComboBox::popupOffset()` through `Flyout::anchorOffset()` or equivalent internal calculation.

## 3. Lifecycle And State Synchronization

- [x] 3.1 Replace owner-driven popup hiding paths with `close()` where Popup/Flyout lifecycle state must be updated.
- [x] 3.2 Connect Flyout close lifecycle signals so Escape, outside press, item selection, and explicit `hidePopup()` all clear `m_popupVisible` and reset pressed state correctly.
- [x] 3.3 Verify editable ComboBox selection still updates the embedded `LineEdit` text and preserves validation behavior on focus changes.
- [x] 3.4 Ensure theme updates repaint the Flyout card, ListView viewport, and item delegate with current Fluent tokens.

## 4. Tests And Verification

- [x] 4.1 Add or update unit tests in `tests/views/basicinput/TestComboBox.cpp` for Flyout-backed open/close lifecycle and selection-close behavior.
- [x] 4.2 Add geometry tests for left-edge alignment, width following the ComboBox, downward placement, upward placement near the bottom edge, clamp near the right edge, list inset, and scrollbar right gutter.
- [x] 4.3 Add or update tests for Escape/outside-dismiss state synchronization if feasible with the existing Qt test harness.
- [x] 4.4 Update ComboBox VisualCheck if needed to exercise non-editable and editable dropdowns after the Flyout migration.
- [x] 4.5 Build and run the affected target only: `cmake --build build --target test_combo_box && SKIP_VISUAL_TEST=1 ./build/tests/views/basicinput/test_combo_box`.
