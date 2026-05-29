## MODIFIED Requirements

### Requirement: Component foundation has an explicit source home
The project SHALL organize reusable component-layer infrastructure under `src/components/foundation/` while keeping component families directly under `src/components/<category>/`.

#### Scenario: Foundation files are separated from component categories
- **WHEN** a developer inspects `src/components`
- **THEN** shared component infrastructure SHALL live under `src/components/foundation/`
- **AND** component families such as `basicinput`, `collections`, `date_time`, `dialogs_flyouts`, `menus_toolbars`, `navigation`, `scrolling`, `status_info`, `textfields`, and `windowing` SHALL remain direct children of `src/components`

#### Scenario: Public foundation mixins have canonical paths
- **WHEN** code uses `FluentElement` or `QMLPlus`
- **THEN** their canonical project headers SHALL be available at `components/foundation/FluentElement.h` and `components/foundation/QMLPlus.h`
- **AND** their implementations SHALL live in the same foundation area

#### Scenario: Private foundation implementation is not exposed in the component root
- **WHEN** foundation implementation details are needed by foundation source files
- **THEN** private headers such as the `FluentElement` private implementation SHALL live under `src/components/foundation/private/`
- **AND** component code SHALL NOT include private foundation headers directly

### Requirement: Overlay helpers are foundation infrastructure
The project SHALL treat same-window overlay helper APIs as component foundation infrastructure rather than component category APIs.

#### Scenario: Overlay helpers have canonical foundation paths
- **WHEN** project-owned code uses shared overlay geometry, scrim, windowing, or light-dismiss helpers
- **THEN** their canonical headers SHALL live under `components/foundation/overlay/`
- **AND** the helper namespace SHALL remain `view::overlay`

#### Scenario: Overlay behavior remains unchanged by the move
- **WHEN** Popup, Flyout, ComboBox dropdown, or DrawerView uses migrated overlay helpers
- **THEN** same-window attachment, visible-card geometry, light-dismiss behavior, scrim stacking, transparent rendering, and animation semantics SHALL remain governed by the existing overlay behavior contract

### Requirement: Migration validation covers foundation and overlay users
The project SHALL validate that the source reorganization does not change foundation behavior or overlay-consuming component behavior.

#### Scenario: Foundation tests still pass
- **WHEN** focused tests for `FluentElement`, `QMLPlus`, and anchor layout behavior run from the renamed component test tree
- **THEN** they SHALL pass without changing the tested public behavior

#### Scenario: Overlay-consuming component tests still pass
- **WHEN** focused tests for Popup, Flyout, ComboBox dropdown, and DrawerView run with visual checks skipped where applicable
- **THEN** they SHALL pass using the migrated overlay helper paths

## REMOVED Requirements

### Requirement: Legacy include paths remain compatible
**Reason**: The migration intentionally makes `components/...` the canonical include prefix and removes active `view/...` include paths to avoid keeping two public source-layout surfaces.

**Migration**: Update project-owned and downstream includes from `view/...` to `components/...`. The C++ namespace remains `view::...` until a separate namespace migration is proposed.
