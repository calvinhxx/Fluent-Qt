## MODIFIED Requirements

### Requirement: Component API audit report
The project SHALL maintain an audit report for component API consistency covering public components under `src/components/**`, matching tests under `tests/components/**`, and relevant OpenSpec specs.

#### Scenario: Audit report records findings
- **WHEN** the component API consistency audit is performed
- **THEN** the audit report SHALL list each finding with component path, category, severity, rationale, and recommended action

#### Scenario: Intentional deviations are recorded
- **WHEN** a component intentionally differs from a general convention because its existing spec or architecture requires it
- **THEN** the audit report SHALL mark the deviation as intentional and reference the reason instead of requiring a cosmetic rewrite

#### Scenario: Follow-up migrations are separated
- **WHEN** a finding requires a breaking rename, ownership redesign, or broad migration
- **THEN** the audit report SHALL defer that work to a follow-up proposal rather than landing it as an unannounced fix

## ADDED Requirements

### Requirement: Component source layout migration preserves API semantics
The component API consistency contract SHALL treat the `src/view` to `src/components` move as a source layout migration, not as an API behavior redesign.

#### Scenario: Namespace migration is deferred
- **WHEN** component declarations are moved to `src/components`
- **THEN** existing public class names, properties, signals, inheritance, and `view::...` namespaces MUST remain unchanged
- **AND** any future `fluent::...` namespace migration MUST be proposed separately

#### Scenario: Caller-owned composition remains unchanged
- **WHEN** shell or host components such as NavigationView, StackContentHost, TabView, or Window are moved
- **THEN** their caller-owned item, page, and content composition boundaries MUST remain unchanged
