## ADDED Requirements

### Requirement: ProgressRing uses fluent namespace
The ProgressRing capability SHALL expose its public status API under `fluent::status_info`.

#### Scenario: ProgressRing public names are migrated
- **WHEN** code includes `components/status_info/ProgressRing.h`
- **THEN** `ProgressRing` and related public names MUST be available under `fluent::status_info`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::status_info` as the canonical component namespace
