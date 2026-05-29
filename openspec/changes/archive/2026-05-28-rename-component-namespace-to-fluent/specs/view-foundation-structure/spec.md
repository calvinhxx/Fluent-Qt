## MODIFIED Requirements

### Requirement: Overlay helpers are foundation infrastructure
The project SHALL treat same-window overlay helper APIs as component foundation infrastructure rather than component category APIs.

#### Scenario: Overlay helpers have canonical foundation paths
- **WHEN** project-owned code uses shared overlay geometry, scrim, windowing, or light-dismiss helpers
- **THEN** their canonical headers SHALL live under `components/foundation/overlay/`
- **AND** the helper namespace SHALL be `fluent::overlay`

#### Scenario: Overlay behavior remains unchanged by the move
- **WHEN** Popup, Flyout, ComboBox dropdown, or DrawerView uses migrated overlay helpers
- **THEN** same-window attachment, visible-card geometry, light-dismiss behavior, scrim stacking, transparent rendering, and animation semantics SHALL remain governed by the existing overlay behavior contract

## ADDED Requirements

### Requirement: Foundation public types use fluent namespace
The component foundation SHALL expose public mixins and layout/binding helpers under `fluent`.

#### Scenario: Foundation names are canonical under fluent
- **WHEN** code uses component foundation public types from `components/foundation/`
- **THEN** `FluentElement`, `QMLPlus`, `AnchorLayout`, `PropertyBinder`, `PropertyLink`, `PropertyChange`, and `QMLState` MUST be available under `fluent`
- **AND** `FluentElement` MUST NOT remain a global public class
- **AND** `view::QMLPlus` and `view::AnchorLayout` MUST NOT remain supported public names