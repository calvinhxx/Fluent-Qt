## ADDED Requirements

### Requirement: ScrollView uses fluent namespace
The ScrollView capability SHALL expose its public scrolling API under `fluent::scrolling`.

#### Scenario: ScrollView public names are migrated
- **WHEN** code includes `components/scrolling/ScrollView.h` or `components/scrolling/ScrollBar.h`
- **THEN** `ScrollView`, `ScrollBar`, and related public names MUST be available under `fluent::scrolling`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::scrolling` as the canonical component namespace
