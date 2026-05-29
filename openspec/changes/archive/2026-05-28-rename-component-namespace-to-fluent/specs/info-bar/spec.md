## ADDED Requirements

### Requirement: InfoBar uses fluent namespace
The InfoBar capability SHALL expose its public status API under `fluent::status_info` and reference supporting components through `fluent::...` names.

#### Scenario: InfoBar public names are migrated
- **WHEN** code includes `components/status_info/InfoBar.h`
- **THEN** `InfoBar` and related public names MUST be available under `fluent::status_info`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::status_info`, `view::basicinput`, or `view::textfields` as canonical component namespaces
