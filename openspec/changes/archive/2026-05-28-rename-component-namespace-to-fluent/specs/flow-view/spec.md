## ADDED Requirements

### Requirement: FlowView uses fluent namespace
The FlowView capability SHALL expose its public collection API under `fluent::collections` and reference scrolling helpers through `fluent::scrolling`.

#### Scenario: FlowView public names are migrated
- **WHEN** code includes `components/collections/FlowView.h`
- **THEN** `FlowView`, `FlowSelectionMode`, and related public names MUST be available under `fluent::collections`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::collections` or `view::scrolling` as canonical component namespaces
