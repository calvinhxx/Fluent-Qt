## Purpose

Define the canonical include-path contract for component foundation mixins and same-window overlay helpers after removing legacy forwarding headers.

## Requirements

### Requirement: Component foundation includes are canonical only
The project SHALL use canonical `components/foundation/...` include paths for component foundation mixins and same-window overlay helpers.

#### Scenario: Component source includes foundation mixins directly
- **WHEN** project-owned source or test code uses `fluent::FluentElement` or `fluent::QMLPlus`
- **THEN** it SHALL include `components/foundation/FluentElement.h` or `components/foundation/QMLPlus.h`
- **AND** it SHALL NOT include the removed legacy paths `components/FluentElement.h` or `components/QMLPlus.h`

#### Scenario: Overlay helper users include foundation overlay paths
- **WHEN** project-owned source or test code uses overlay geometry, scrim, windowing, or light-dismiss helpers
- **THEN** it SHALL include those helpers from `components/foundation/overlay/...`
- **AND** it SHALL NOT include overlay helpers from `components/internal/overlay/...`

### Requirement: Legacy forwarding headers are removed
The project SHALL remove legacy forwarding headers that previously exposed component foundation APIs from non-canonical paths.

#### Scenario: View root contains no foundation forwarding files
- **WHEN** a developer inspects `src/components`
- **THEN** `FluentElement.h` and `QMLPlus.h` SHALL NOT exist as root-level forwarding headers
- **AND** the canonical headers SHALL remain under `src/components/foundation/`

#### Scenario: Internal overlay compatibility directory is absent
- **WHEN** a developer inspects `src/components/internal/overlay/`
- **THEN** the legacy overlay forwarding headers SHALL NOT exist
- **AND** canonical overlay helper headers SHALL remain under `src/components/foundation/overlay/`

### Requirement: Include cleanup preserves behavior
The include-path cleanup SHALL NOT change runtime behavior or public component semantics beyond removing legacy source include compatibility.

#### Scenario: Foundation behavior still validates
- **WHEN** focused tests for `fluent::FluentElement`, `fluent::QMLPlus`, and fluent::AnchorLayout run
- **THEN** they SHALL pass using canonical foundation includes

#### Scenario: Overlay-consuming components still validate
- **WHEN** focused tests for Popup, Flyout, ComboBox dropdown, and DrawerView run with VisualCheck skipped where applicable
- **THEN** they SHALL pass using canonical foundation overlay includes
