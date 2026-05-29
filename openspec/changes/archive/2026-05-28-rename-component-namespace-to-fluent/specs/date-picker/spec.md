## ADDED Requirements

### Requirement: DatePicker uses fluent namespace
The DatePicker capability SHALL expose its public date-time API under `fluent::date_time` and reference supporting button and flyout components through `fluent::...` names.

#### Scenario: DatePicker public names are migrated
- **WHEN** code includes `components/date_time/DatePicker.h`
- **THEN** the component and public enum types MUST be available under `fluent::date_time::DatePicker`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::date_time`, `view::basicinput`, or `view::dialogs_flyouts` as canonical component namespaces
