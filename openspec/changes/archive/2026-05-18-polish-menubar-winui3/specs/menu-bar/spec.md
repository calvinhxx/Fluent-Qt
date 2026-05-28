## ADDED Requirements

### Requirement: WinUI-aligned top-level MenuBar items
`FluentMenuBar` SHALL render top-level menu actions using Fluent design tokens without QSS visual styling, including deterministic item geometry, text, rest, hover, pressed/open, disabled, selected/open, and keyboard focus states.

#### Scenario: Top-level item metrics match Fluent MenuBar
- **WHEN** a `FluentMenuBar` contains visible File, Edit, and Help menu actions
- **THEN** each visible top-level item uses a 40px control-height row, Body typography, Control corner radius, and token-based horizontal padding for its hit and paint rect

#### Scenario: Hover and pressed states use subtle fills
- **WHEN** the pointer moves over an enabled top-level menu item
- **THEN** the item is painted with the subtle hover fill and primary text
- **WHEN** the item is pressed or its menu is open
- **THEN** the item is painted with the pressed/open subtle fill until the menu closes

#### Scenario: Disabled item state is visually distinct
- **WHEN** a top-level menu action is disabled
- **THEN** the MenuBar does not activate it from pointer or keyboard input and paints its text using the disabled text token

#### Scenario: Keyboard focus is not shown for ordinary mouse hover
- **WHEN** the user moves or clicks the mouse over a top-level menu item
- **THEN** no keyboard focus rectangle is painted for that mouse-only interaction
- **WHEN** the user navigates MenuBar items with the keyboard
- **THEN** the focused item paints the Fluent focus visual

### Requirement: Qt action compatibility
`FluentMenuBar` SHALL preserve source-compatible Qt `QMenuBar` behavior for adding menus/actions, triggering actions, hiding actions, enabling/disabling actions, and opening associated `QMenu` instances.

#### Scenario: Existing addMenu usage continues to work
- **WHEN** application code creates `FluentMenu` instances and adds them with `FluentMenuBar::addMenu`
- **THEN** the menu titles appear as top-level MenuBar items and clicking a title opens the corresponding popup menu

#### Scenario: Action trigger is preserved
- **WHEN** a top-level action without a submenu is clicked or activated by keyboard
- **THEN** the action emits its normal Qt triggered signal exactly once

#### Scenario: Hidden actions are excluded from layout
- **WHEN** a top-level action is hidden
- **THEN** the MenuBar excludes it from item geometry, hit testing, painting, and keyboard navigation

### Requirement: MenuBar keyboard and access-key behavior
`FluentMenuBar` SHALL provide keyboard navigation and activation behavior consistent with WinUI MenuBar expectations while preserving Qt shortcuts and accelerators on menu actions.

#### Scenario: Arrow keys move between top-level items
- **WHEN** the MenuBar has keyboard focus and the user presses Left or Right
- **THEN** focus moves to the previous or next enabled visible top-level item without triggering it

#### Scenario: Enter, Space, or Down opens a submenu
- **WHEN** a focused enabled top-level item owns a submenu and the user presses Enter, Space, or Down
- **THEN** the associated menu opens and the top-level item remains in the open visual state

#### Scenario: Escape closes the open menu state
- **WHEN** a MenuBar submenu is open and the user presses Escape
- **THEN** the submenu closes and the top-level item returns to its non-open visual state

#### Scenario: Menu item keyboard accelerators remain discoverable and invokable
- **WHEN** menu actions have Qt shortcuts equivalent to WinUI keyboard accelerators such as Ctrl+N, Ctrl+O, Ctrl+S, Ctrl+Z, Ctrl+X, Ctrl+C, and Ctrl+V
- **THEN** the actions can be invoked through their shortcuts and the flyout can display accelerator text aligned separately from item text

#### Scenario: Access keys can activate menu titles
- **WHEN** a top-level menu action declares an access-key equivalent for its title
- **THEN** Alt plus the access key focuses or opens that top-level menu item without requiring pointer input

### Requirement: Menu flyout content used by MenuBar
`FluentMenu` SHALL support the MenuBar flyout content patterns shown by WinUI Gallery: simple items, separators, nested submenus, disabled items, shortcut text, and checked/radio-style items.

#### Scenario: Simple menu items trigger commands
- **WHEN** a `FluentMenu` contains plain `FluentMenuItem` or `QAction` entries
- **THEN** each visible enabled item is painted with token-based text and emits its triggered signal when activated

#### Scenario: Separators are rendered with Fluent spacing
- **WHEN** a menu contains a separator action between command groups
- **THEN** the menu reserves vertical spacing and paints a divider line using the divider stroke token

#### Scenario: Nested submenu items expose a submenu affordance
- **WHEN** a menu item owns a submenu
- **THEN** the menu paints a trailing submenu indicator and opens the nested submenu from pointer or keyboard activation

#### Scenario: Checked and radio-style items show selection state
- **WHEN** a menu item is checkable and checked, including actions grouped for exclusive radio-like behavior
- **THEN** the menu paints a selected/check indicator and keeps text/hover/disabled states readable in light and dark themes

### Requirement: MenuBar tests and VisualCheck coverage
The MenuBar change SHALL include automated tests and VisualCheck coverage for the WinUI Gallery and Figma-derived states needed to validate implementation quality.

#### Scenario: Automated tests cover interaction behavior
- **WHEN** the MenuBar tests run with `SKIP_VISUAL_TEST=1`
- **THEN** they validate geometry, visible/hidden actions, disabled actions, triggered signals, submenu open state, keyboard navigation, shortcut activation, and theme updates without requiring manual interaction

#### Scenario: VisualCheck mirrors reference examples
- **WHEN** VisualCheck is run manually
- **THEN** it presents separate examples for a simple MenuBar, a keyboard-accelerator MenuBar, and a complex MenuBar with submenus, separators, and radio/check items in a layout that is not cramped
