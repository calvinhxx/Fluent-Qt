## ADDED Requirements

### Requirement: StackView uses fluent namespace
The StackView capability SHALL expose its public collection API under `fluent::collections`.

#### Scenario: StackView public names are migrated
- **WHEN** code includes `components/collections/StackView.h`
- **THEN** `StackView` and related public enum types MUST be available under `fluent::collections`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::collections` as the canonical component namespace
