## ADDED Requirements

### Requirement: SelectorBar uses fluent namespace
The SelectorBar capability SHALL expose its public navigation API under `fluent::navigation`.

#### Scenario: SelectorBar public names are migrated
- **WHEN** code includes `components/navigation/SelectorBar.h`
- **THEN** `SelectorBar`, `SelectorBarItem`, and related public enum types MUST be available under `fluent::navigation`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::navigation` as the canonical component namespace
