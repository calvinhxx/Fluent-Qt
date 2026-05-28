## ADDED Requirements

### Requirement: Suggestion popup preserves clear button interaction

`AutoSuggestBox` SHALL keep its built-in clear/close button interactive while the suggestion popup is open. The popup MUST NOT block the button from receiving hover, press, release, or click input.

#### Scenario: Clear button hovers while popup is open
- **WHEN** an enabled `AutoSuggestBox` contains text and opens its suggestion popup
- **AND** the pointer moves over the built-in clear/close button
- **THEN** the clear/close button MUST enter its hover visual state
- **AND** the suggestion popup MUST remain open until a dismissing action occurs

#### Scenario: Clear button click clears text while popup is open
- **WHEN** an enabled `AutoSuggestBox` contains text and opens its suggestion popup
- **AND** the user clicks the built-in clear/close button
- **THEN** the control text MUST become empty
- **AND** the clear/close button MUST become hidden
- **AND** the suggestion popup MUST close

#### Scenario: Suggestion interactions still work after the fix
- **WHEN** an enabled `AutoSuggestBox` opens its suggestion popup
- **AND** the user clicks a valid suggestion item
- **THEN** the control MUST choose the suggestion
- **AND** it MUST emit the existing suggestion and query submission signals
- **AND** the suggestion popup MUST close

#### Scenario: Outside press still light-dismisses suggestions
- **WHEN** an enabled `AutoSuggestBox` opens its suggestion popup
- **AND** the user presses outside both the suggestion popup and the owning input control
- **THEN** the suggestion popup MUST close
