## ADDED Requirements

### Requirement: Windowing components use fluent namespace
The Fluent windowing capability SHALL expose Window and TitleBar public APIs under `fluent::windowing`.

#### Scenario: Windowing public names are migrated
- **WHEN** code includes headers under `components/windowing/`
- **THEN** `Window`, `TitleBar`, and related public names MUST be available under `fluent::windowing`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::windowing` as the canonical component namespace
