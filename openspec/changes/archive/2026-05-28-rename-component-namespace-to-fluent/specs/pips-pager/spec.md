## ADDED Requirements

### Requirement: PipsPager uses fluent namespace
The PipsPager capability SHALL expose its public scrolling API under `fluent::scrolling`.

#### Scenario: PipsPager public names are migrated
- **WHEN** code includes `components/scrolling/PipsPager.h`
- **THEN** `PipsPager` and related public enum types MUST be available under `fluent::scrolling`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::scrolling` as the canonical component namespace
