## ADDED Requirements

### Requirement: View foundation includes are canonical only
The project SHALL use canonical `view/foundation/...` include paths for view foundation mixins and same-window overlay helpers.

#### Scenario: Component source includes foundation mixins directly
- **WHEN** project-owned source or test code uses `FluentElement` or `QMLPlus`
- **THEN** it SHALL include `view/foundation/FluentElement.h` or `view/foundation/QMLPlus.h`
- **AND** it SHALL NOT include the removed legacy paths `view/FluentElement.h` or `view/QMLPlus.h`

#### Scenario: Overlay helper users include foundation overlay paths
- **WHEN** project-owned source or test code uses overlay geometry, scrim, windowing, or light-dismiss helpers
- **THEN** it SHALL include those helpers from `view/foundation/overlay/...`
- **AND** it SHALL NOT include overlay helpers from `view/internal/overlay/...`

### Requirement: Legacy forwarding headers are removed
The project SHALL remove legacy forwarding headers that previously exposed view foundation APIs from non-canonical paths.

#### Scenario: View root contains no foundation forwarding files
- **WHEN** a developer inspects `src/view`
- **THEN** `FluentElement.h` and `QMLPlus.h` SHALL NOT exist as root-level forwarding headers
- **AND** the canonical headers SHALL remain under `src/view/foundation/`

#### Scenario: Internal overlay compatibility directory is absent
- **WHEN** a developer inspects `src/view/internal/overlay/`
- **THEN** the legacy overlay forwarding headers SHALL NOT exist
- **AND** canonical overlay helper headers SHALL remain under `src/view/foundation/overlay/`

### Requirement: Include cleanup preserves behavior
The include-path cleanup SHALL NOT change runtime behavior or public component semantics beyond removing legacy source include compatibility.

#### Scenario: Foundation behavior still validates
- **WHEN** focused tests for `FluentElement`, `QMLPlus`, and AnchorLayout run
- **THEN** they SHALL pass using canonical foundation includes

#### Scenario: Overlay-consuming components still validate
- **WHEN** focused tests for Popup, Flyout, ComboBox dropdown, and DrawerView run with VisualCheck skipped where applicable
- **THEN** they SHALL pass using canonical foundation overlay includes