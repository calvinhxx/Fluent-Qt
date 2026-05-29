## ADDED Requirements

### Requirement: InfoBadge uses fluent namespace
The InfoBadge capability SHALL expose its public status API under `fluent::status_info`.

#### Scenario: InfoBadge public names are migrated
- **WHEN** code includes `components/status_info/InfoBadge.h`
- **THEN** `InfoBadge` and related public names MUST be available under `fluent::status_info`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::status_info` or `view::QMLPlus` as canonical component namespaces
