## ADDED Requirements

### Requirement: Pivot uses fluent namespace
The Pivot capability SHALL expose its public navigation API under `fluent::navigation`.

#### Scenario: Pivot public names are migrated
- **WHEN** code includes `components/navigation/Pivot.h`
- **THEN** `Pivot`, `PivotItem`, and related public enum types MUST be available under `fluent::navigation`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::navigation` as the canonical component namespace
