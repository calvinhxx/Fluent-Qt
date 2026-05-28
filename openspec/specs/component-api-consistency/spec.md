## Requirements

### Requirement: Component API audit report
The project SHALL maintain an audit report for component API consistency covering public components under `src/view/**`, matching tests under `tests/views/**`, and relevant OpenSpec specs.

#### Scenario: Audit report records findings
- **WHEN** the component API consistency audit is performed
- **THEN** the audit report SHALL list each finding with component path, category, severity, rationale, and recommended action

#### Scenario: Intentional deviations are recorded
- **WHEN** a component intentionally differs from a general convention because its existing spec or architecture requires it
- **THEN** the audit report SHALL mark the deviation as intentional and reference the reason instead of requiring a cosmetic rewrite

#### Scenario: Follow-up migrations are separated
- **WHEN** a finding requires a breaking rename, ownership redesign, or broad migration
- **THEN** the audit report SHALL defer that work to a follow-up proposal rather than landing it as an unannounced fix

### Requirement: Public API naming conventions
The project SHALL define and apply consistent naming conventions for public component properties, methods, and signals.

#### Scenario: Repeated setters are no-ops
- **WHEN** a public property setter receives the value already stored by the component
- **THEN** the component SHALL NOT emit duplicate changed signals

#### Scenario: Boolean properties use clear getter names
- **WHEN** a component exposes a boolean public property
- **THEN** its getter naming SHALL clearly communicate state semantics, using forms such as `isOpen`, `isEnabled`, `hasSelection`, or established component-specific equivalents

#### Scenario: Open state APIs are documented consistently
- **WHEN** a component exposes popup, flyout, dropdown, calendar, or pane open state
- **THEN** its public API SHALL document the open-state getter, mutator or command methods, and changed signal consistently with nearby components

#### Scenario: Nullable value semantics are explicit
- **WHEN** a component uses an invalid Qt value such as `QDate()` or `QTime()` to represent an empty selected value
- **THEN** the public API and tests SHALL make that empty-state behavior explicit

### Requirement: Component inheritance and ownership conventions
The project SHALL document and test component inheritance and ownership conventions without forcing unrelated components into one base class.

#### Scenario: Widget-host components preserve Fluent mixins
- **WHEN** a public component is implemented as a QWidget-hosted Fluent control
- **THEN** it SHALL provide QWidget child containment, Fluent theme access, and QMLPlus binding support either directly or through an established project base component

#### Scenario: Button-like controls use Button when appropriate
- **WHEN** a public component behaves as a clickable button-like entry surface and does not need a more specialized Qt item-view base
- **THEN** the audit SHALL evaluate whether it should derive from `view::basicinput::Button` or document why it intentionally does not

#### Scenario: Caller-owned content remains caller-owned
- **WHEN** a component is documented as a shell, container, view, or host whose content is supplied by callers
- **THEN** the component SHALL NOT absorb application-owned item composition, selection routing, or page choice as part of this consistency pass

### Requirement: Test coverage for consistency contracts
The project SHALL add or update focused tests for API consistency findings that can be verified automatically.

#### Scenario: Inheritance expectations are tested
- **WHEN** a component's public contract depends on inheriting a specific Qt base or project base component
- **THEN** its focused test SHALL verify that inheritance contract where practical

#### Scenario: Signal no-op behavior is tested
- **WHEN** a public setter is audited or changed
- **THEN** the focused test SHALL verify that repeated values do not emit duplicate change signals where the property has a changed signal

#### Scenario: VisualCheck conventions are preserved
- **WHEN** an existing VisualCheck is touched during the consistency pass
- **THEN** it SHALL keep the `SKIP_VISUAL_TEST` guard, use shared Qt test setup, and prefer project Fluent components for visible demo UI where practical

### Requirement: Documentation of durable conventions
The project SHALL document component API conventions in a durable location that future component proposals and implementations can reference.

#### Scenario: Convention checklist is published
- **WHEN** the audit change is completed
- **THEN** the project SHALL include a concise checklist covering inheritance, ownership, properties, signals, nullable values, open state, selection naming, tests, and VisualCheck expectations

#### Scenario: README links to detailed guidance
- **WHEN** the convention checklist is stored outside the README
- **THEN** `readme.md` SHALL link to the detailed guidance so new contributors can find it from the project entry point
