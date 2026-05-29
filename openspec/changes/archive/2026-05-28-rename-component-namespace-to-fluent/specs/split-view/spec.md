## ADDED Requirements

### Requirement: SplitView uses fluent namespace
The SplitView capability SHALL expose its public collection API under `fluent::collections`.

#### Scenario: SplitView public names are migrated
- **WHEN** code includes `components/collections/SplitView.h`
- **THEN** `SplitView` and related public names MUST be available under `fluent::collections`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::collections` as the canonical component namespace
