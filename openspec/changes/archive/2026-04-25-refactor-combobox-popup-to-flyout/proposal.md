## Why

`ComboBoxPopup` currently inherits from `Dialog` even though the dropdown behaves like an anchored light-dismiss popup rather than a dialog. This makes the component rely on dialog-specific semantics such as top-level dialog lifetime, drag/smoke concepts, and manual global positioning when the project already has a `Popup`/`Flyout` stack designed for anchored transient surfaces.

Refactoring the ComboBox dropdown to use `Flyout` aligns it with `AutoSuggestBox` suggestion popups, keeps dialog semantics reserved for actual dialogs, and gives future dropdown/list popups a cleaner foundation.

## What Changes

- Change `ComboBox::ComboBoxPopup` from a `Dialog` subclass to a `Flyout`-based anchored dropdown.
- Preserve the current visual contract: Fluent rounded popup card, shadow margin, theme-matched list surface, selected accent indicator, scrollbar gutter, and maximum six visible items.
- Preserve existing ComboBox public API and user behavior: clicking opens/closes, selecting an item updates `currentIndex`, editable mode mirrors selected text, Escape/outside press dismisses the popup.
- Move positioning from manual global screen coordinates to anchor-based Flyout positioning, with ComboBox-style left-edge alignment and automatic above/below placement when space is constrained.
- Reuse the existing `Popup`/`Flyout` light-dismiss lifecycle and keep `m_popupVisible` synchronized from Flyout close signals.
- Add focused tests for the new dropdown behavior and keep VisualCheck coverage for non-editable and editable ComboBox states.

## Capabilities

### New Capabilities
- `combobox-dropdown-flyout`: Defines the ComboBox dropdown as an anchored Flyout-backed list popup with ComboBox-specific sizing, alignment, dismissal, and selection behavior.

### Modified Capabilities

## Impact

- Affected code:
  - `src/view/basicinput/ComboBox.h`
  - `src/view/basicinput/ComboBox.cpp`
  - `tests/views/basicinput/TestComboBox.cpp`
- Existing public APIs should remain source-compatible.
- No new third-party dependencies are expected.
- The change intentionally does not broaden `Flyout` public API unless implementation reveals an unavoidable shared abstraction need.
