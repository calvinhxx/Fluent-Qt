## ADDED Requirements

### Requirement: TabView uses fluent namespace
The TabView capability SHALL expose its public navigation API under `fluent::navigation` and reference supporting components through `fluent::...` names.

#### Scenario: TabView public names are migrated
- **WHEN** code includes `components/navigation/TabView.h`
- **THEN** `TabView`, `TabViewItem`, and related public enum types MUST be available under `fluent::navigation`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::navigation`, `view::basicinput`, or `view::textfields` as canonical component namespaces
