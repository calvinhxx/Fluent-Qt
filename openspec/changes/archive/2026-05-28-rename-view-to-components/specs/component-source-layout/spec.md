## ADDED Requirements

### Requirement: Canonical component source tree
The project SHALL store reusable Fluent Qt widgets and component foundation infrastructure under `src/components/`.

#### Scenario: Component source categories are direct children
- **WHEN** a developer inspects `src/components`
- **THEN** reusable component categories such as `basicinput`, `collections`, `date_time`, `dialogs_flyouts`, `menus_toolbars`, `navigation`, `scrolling`, `status_info`, `textfields`, and `windowing` MUST be direct children of `src/components`
- **AND** shared component infrastructure MUST live under `src/components/foundation`

#### Scenario: Application views remain outside components
- **WHEN** a developer inspects the Gallery application layer
- **THEN** application business code MUST remain under root-level `app/`
- **AND** `app/model`, `app/view`, and `app/viewmodel` MUST NOT be compiled into the reusable component library target

### Requirement: Canonical component include paths
The project SHALL use `components/...` as the canonical include prefix for reusable component headers.

#### Scenario: Project-owned component includes use components prefix
- **WHEN** live source, tests, and docs include reusable component headers
- **THEN** they MUST use paths such as `components/basicinput/Button.h`, `components/foundation/FluentElement.h`, and `components/foundation/QMLPlus.h`
- **AND** they MUST NOT use `view/...` include paths except in archived history or explicit migration notes

#### Scenario: Namespace remains unchanged
- **WHEN** code includes headers from `components/...`
- **THEN** the C++ namespace for existing components MUST remain `view::...` during this change
- **AND** namespace migration to `fluent::...` MUST be handled by a separate change

### Requirement: Canonical component test tree
The project SHALL mirror reusable component categories under `tests/components/`.

#### Scenario: Component tests move with source categories
- **WHEN** component tests are inspected
- **THEN** tests for reusable component categories MUST live under `tests/components/<category>/`
- **AND** root component infrastructure tests MUST live under `tests/components/`

#### Scenario: VisualCheck paths use component test tree
- **WHEN** VisualCheck documentation or direct-run examples reference component test binaries
- **THEN** those paths MUST use `tests/components` output locations rather than `tests/views`
- **AND** the `SKIP_VISUAL_TEST` and `VISUAL_SNAPSHOT` behavior MUST remain unchanged

### Requirement: Component library target excludes app code
The reusable component library target SHALL compile component, design, compatibility, utility, and resource code without absorbing application-layer Gallery code.

#### Scenario: Component library build inputs are bounded
- **WHEN** `fluent_qt_lib` is configured
- **THEN** it MUST compile source from `src/components`, `src/design`, `src/compatibility`, `src/utils`, and repository resources
- **AND** it MUST NOT compile source from root-level `app/`

#### Scenario: Gallery can link the component library
- **WHEN** a Gallery executable target is added later
- **THEN** it MUST link against `fluent_qt_lib` rather than adding Gallery business code to the component library

### Requirement: Live guidance uses component terminology
The project SHALL synchronize live documentation, OpenSpec specs, agent guidance, and test guidance to the component source layout.

#### Scenario: Active references use new paths
- **WHEN** live README, docs, AGENTS.md, source comments, tests, CMake files, and OpenSpec specs are searched
- **THEN** active guidance MUST reference `src/components` and `tests/components`
- **AND** stale `src/view` or `tests/views` references MUST remain only in archived OpenSpec history or explicit migration notes

#### Scenario: Component terminology replaces component-layer ambiguity
- **WHEN** live guidance describes reusable Fluent Qt widgets
- **THEN** it MUST use component terminology for the library layer
- **AND** it MUST reserve `app/view` terminology for application-layer Gallery views
