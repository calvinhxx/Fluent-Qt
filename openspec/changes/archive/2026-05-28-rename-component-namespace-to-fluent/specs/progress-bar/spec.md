## ADDED Requirements

### Requirement: ProgressBar uses fluent namespace
The ProgressBar capability SHALL expose its public status API under `fluent::status_info`.

#### Scenario: ProgressBar public names are migrated
- **WHEN** code includes `components/status_info/ProgressBar.h`
- **THEN** `ProgressBar` and related public names MUST be available under `fluent::status_info`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::status_info` as the canonical component namespace
