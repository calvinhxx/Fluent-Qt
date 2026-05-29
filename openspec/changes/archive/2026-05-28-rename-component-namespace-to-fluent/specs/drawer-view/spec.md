## ADDED Requirements

### Requirement: DrawerView uses fluent namespace
The DrawerView capability SHALL expose its public collection API under `fluent::collections` and use `fluent` foundation and overlay helpers.

#### Scenario: DrawerView public names are migrated
- **WHEN** code includes `components/collections/DrawerView.h`
- **THEN** `DrawerView`, `DrawerEdge`, `ClosePolicy`, and related public names MUST be available under `fluent::collections`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::collections`, `view::QMLPlus`, or `view::overlay` as canonical component namespaces
