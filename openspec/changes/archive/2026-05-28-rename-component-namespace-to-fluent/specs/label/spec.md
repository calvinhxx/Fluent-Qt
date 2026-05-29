## ADDED Requirements

### Requirement: Label uses fluent namespace
The Label capability SHALL expose textfield components under `fluent::textfields` and tooltip support under `fluent::status_info`.

#### Scenario: Label public names are migrated
- **WHEN** code includes `components/textfields/Label.h` or `components/textfields/TextEdit.h`
- **THEN** `Label`, `TextEdit`, and related public names MUST be available under `fluent::textfields`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::textfields` or `view::status_info` as canonical component namespaces
