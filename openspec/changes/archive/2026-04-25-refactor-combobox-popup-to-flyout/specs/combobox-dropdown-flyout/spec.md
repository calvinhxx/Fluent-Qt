## ADDED Requirements

### Requirement: ComboBox dropdown uses Flyout semantics

`ComboBox::ComboBoxPopup` SHALL be implemented as an anchored `Flyout`-based popup rather than a `Dialog`-based popup. The dropdown SHALL use the Popup/Flyout open-close lifecycle and SHALL NOT expose or depend on dialog result semantics such as `exec()` or `done(int)`.

#### Scenario: Dropdown opens as Flyout-backed popup
- **WHEN** a ComboBox dropdown is opened
- **THEN** the dropdown SHALL use the Popup/Flyout lifecycle with `isOpen()` becoming true
- **AND** the dropdown SHALL be non-modal and non-dimming

#### Scenario: Dropdown does not use dialog semantics
- **WHEN** a ComboBox dropdown is shown or hidden
- **THEN** the dropdown SHALL NOT require dialog result handling
- **AND** the dropdown SHALL NOT enable Dialog smoke overlay or dragging behavior

### Requirement: Dropdown is anchored to the ComboBox

The ComboBox dropdown SHALL be anchored to its owning ComboBox. Its visible card width SHALL be at least the ComboBox width, and its visible card left edge SHALL align with the ComboBox left edge unless clamping is required to keep the card inside the owning top-level widget.

#### Scenario: Dropdown aligns with ComboBox left edge
- **WHEN** the ComboBox has enough horizontal space in the owning top-level widget
- **THEN** the dropdown visible card left edge SHALL align with the ComboBox left edge
- **AND** the dropdown visible card width SHALL be greater than or equal to the ComboBox width

#### Scenario: Dropdown clamps near window edge
- **WHEN** the ComboBox is near the owning top-level widget's right edge and the dropdown would overflow
- **THEN** the dropdown visible card SHALL be clamped inside the top-level widget with a small margin

### Requirement: Dropdown chooses above or below placement based on available space

The ComboBox dropdown SHALL prefer opening below the ComboBox. If the available space below is insufficient for the dropdown card and the space above is greater, it SHALL open above the ComboBox instead. Placement SHALL be computed after the popup has its final size.

#### Scenario: Enough space below opens downward
- **WHEN** the ComboBox is positioned with enough space below for the dropdown card
- **THEN** the dropdown visible card SHALL appear below the ComboBox

#### Scenario: Insufficient space below opens upward
- **WHEN** the ComboBox is positioned near the bottom of the owning top-level widget and there is more usable space above
- **THEN** the dropdown visible card SHALL appear above the ComboBox

### Requirement: Dropdown preserves selection behavior

Selecting an item in the dropdown SHALL update the owning ComboBox current index and close the dropdown. In editable mode, selecting an item SHALL also update the embedded LineEdit text to the selected item text.

#### Scenario: Selecting item updates current index
- **WHEN** a user clicks a valid dropdown item
- **THEN** the ComboBox current index SHALL become the clicked item index
- **AND** the dropdown SHALL close

#### Scenario: Editable ComboBox mirrors selected text
- **WHEN** an editable ComboBox dropdown item is selected
- **THEN** the embedded LineEdit text SHALL equal the selected item text
- **AND** the dropdown SHALL close

### Requirement: Dropdown supports light-dismiss behavior

The ComboBox dropdown SHALL close when the user presses Escape while the popup is active or presses outside the popup. The owning ComboBox SHALL synchronize its open state after both explicit and light-dismiss closure paths.

#### Scenario: Escape closes dropdown
- **WHEN** the ComboBox dropdown is open and Escape is pressed
- **THEN** the dropdown SHALL close
- **AND** the owning ComboBox SHALL no longer consider the popup visible

#### Scenario: Outside press closes dropdown
- **WHEN** the ComboBox dropdown is open and the user presses outside the dropdown card
- **THEN** the dropdown SHALL close
- **AND** the owning ComboBox SHALL no longer consider the popup visible

### Requirement: Dropdown preserves Fluent visuals

The Flyout-backed dropdown SHALL preserve the Fluent visual contract: rounded overlay card, elevation shadow, theme background and stroke, a theme-matched embedded ListView background, 40px item rows, hover/selected row backgrounds, selected accent indicator, and scroll chrome that does not cover item visuals.

#### Scenario: Theme update repaints dropdown visuals
- **WHEN** the active Fluent theme changes while the ComboBox or dropdown exists
- **THEN** the dropdown and its item delegate SHALL repaint using the current theme tokens

#### Scenario: Selected item shows accent indicator
- **WHEN** the dropdown opens with a valid current ComboBox index
- **THEN** the corresponding dropdown item SHALL be selected
- **AND** the selected item SHALL render the Fluent accent indicator

#### Scenario: Dropdown content preserves rounded card edges
- **WHEN** the dropdown is rendered inside the Flyout card
- **THEN** the embedded ListView SHALL be inset from the visible card edges enough to avoid exposing square viewport corners
- **AND** the ListView background SHALL match the dropdown card layer color

#### Scenario: Scrollbar does not cover item highlight
- **WHEN** the dropdown contains more rows than the visible item limit and vertical scrolling is needed
- **THEN** the vertical Fluent scrollbar SHALL remain close to the right card edge without touching it
- **AND** hover or selected item backgrounds SHALL reserve horizontal space for the scrollbar instead of painting underneath it
