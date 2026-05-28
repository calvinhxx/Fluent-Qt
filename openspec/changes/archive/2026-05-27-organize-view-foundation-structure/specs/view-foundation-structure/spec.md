## ADDED Requirements

### Requirement: View foundation has an explicit source home
The project SHALL organize reusable view-layer infrastructure under `src/view/foundation/` while keeping component families directly under `src/view/<category>/`.

#### Scenario: Foundation files are separated from component categories
- **WHEN** a developer inspects `src/view`
- **THEN** shared view infrastructure SHALL live under `src/view/foundation/`
- **AND** component families such as `basicinput`, `collections`, `date_time`, `dialogs_flyouts`, `navigation`, `scrolling`, `status_info`, `textfields`, and `windowing` SHALL remain direct children of `src/view`

#### Scenario: Public foundation mixins have canonical paths
- **WHEN** code uses `FluentElement` or `QMLPlus`
- **THEN** their canonical project headers SHALL be available at `view/foundation/FluentElement.h` and `view/foundation/QMLPlus.h`
- **AND** their implementations SHALL live in the same foundation area

#### Scenario: Private foundation implementation is not exposed in the view root
- **WHEN** foundation implementation details are needed by foundation source files
- **THEN** private headers such as the `FluentElement` private implementation SHALL live under `src/view/foundation/private/`
- **AND** component code SHALL NOT include private foundation headers directly

### Requirement: Overlay helpers are foundation infrastructure
The project SHALL treat same-window overlay helper APIs as view foundation infrastructure rather than component category APIs.

#### Scenario: Overlay helpers have canonical foundation paths
- **WHEN** project-owned code uses shared overlay geometry, scrim, windowing, or light-dismiss helpers
- **THEN** their canonical headers SHALL live under `view/foundation/overlay/`
- **AND** the helper namespace SHALL remain `view::overlay`

#### Scenario: Overlay behavior remains unchanged by the move
- **WHEN** Popup, Flyout, ComboBox dropdown, or DrawerView uses migrated overlay helpers
- **THEN** same-window attachment, visible-card geometry, light-dismiss behavior, scrim stacking, transparent rendering, and animation semantics SHALL remain governed by the existing overlay behavior contract

### Requirement: Legacy include paths remain compatible
The project SHALL keep established public include paths usable during the foundation migration.

#### Scenario: Public mixin includes keep compiling
- **WHEN** existing code includes `view/FluentElement.h` or `view/QMLPlus.h`
- **THEN** those includes SHALL continue to compile by forwarding to the canonical foundation headers

#### Scenario: Existing overlay helper includes keep compiling
- **WHEN** existing code includes overlay helper headers from `view/internal/overlay/`
- **THEN** those includes SHALL continue to compile by forwarding to the canonical `view/foundation/overlay/` headers

#### Scenario: New project code prefers canonical paths
- **WHEN** source files are added or materially edited as part of the migration
- **THEN** they SHALL prefer canonical `view/foundation/...` include paths unless the file exists specifically to preserve legacy include compatibility

### Requirement: Migration validation covers foundation and overlay users
The project SHALL validate that the source reorganization does not change foundation behavior or overlay-consuming component behavior.

#### Scenario: Foundation tests still pass
- **WHEN** focused tests for `FluentElement`, `QMLPlus`, and anchor layout behavior run
- **THEN** they SHALL pass without changing the tested public behavior

#### Scenario: Overlay-consuming component tests still pass
- **WHEN** focused tests for Popup, Flyout, ComboBox dropdown, and DrawerView run with visual checks skipped where applicable
- **THEN** they SHALL pass using the migrated overlay helper paths
