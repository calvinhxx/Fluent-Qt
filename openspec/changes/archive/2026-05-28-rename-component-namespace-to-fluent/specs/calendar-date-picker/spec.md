## ADDED Requirements

### Requirement: CalendarDatePicker uses fluent namespace
The CalendarDatePicker capability SHALL expose its public date-time API under `fluent::date_time` and reference supporting button and flyout components through `fluent::...` names.

#### Scenario: CalendarDatePicker public names are migrated
- **WHEN** code includes `components/date_time/CalendarDatePicker.h`
- **THEN** the component MUST be available as `fluent::date_time::CalendarDatePicker`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::date_time`, `view::basicinput`, or `view::dialogs_flyouts` as canonical component namespaces
