## ADDED Requirements

### Requirement: Canonical component namespace
The project SHALL expose reusable Fluent Qt component and component foundation public APIs under the `fluent` C++ namespace.

#### Scenario: Component categories use fluent namespace
- **WHEN** project-owned code declares or refers to reusable component categories such as basicinput, collections, date_time, dialogs_flyouts, menus_toolbars, navigation, scrolling, status_info, textfields, or windowing
- **THEN** it MUST use `fluent::<category>` public names
- **AND** it MUST NOT declare canonical component APIs under `view::<category>`

#### Scenario: Foundation public types use fluent namespace
- **WHEN** project-owned code declares or refers to component foundation public types such as `FluentElement`, `QMLPlus`, `AnchorLayout`, property binding helpers, state helpers, or overlay helpers
- **THEN** those canonical public names MUST live under `fluent`
- **AND** `FluentElement` MUST NOT remain a global public component foundation class

### Requirement: Legacy view namespace is removed
The migration SHALL remove the old `view::...` reusable component public API surface instead of preserving compatibility aliases.

#### Scenario: No view compatibility namespace remains
- **WHEN** active source and test files are searched after the migration
- **THEN** they MUST NOT contain `namespace view`, `view::`, `::view::`, `using namespace view`, or `using view::` for reusable component APIs
- **AND** the project MUST NOT provide namespace aliases or type aliases that keep `view::...` as a supported public component spelling

#### Scenario: Archived history may keep old names
- **WHEN** archived OpenSpec changes are searched
- **THEN** historical `view::...` references MAY remain as archive records
- **AND** active docs, active specs, README, and agent guidance MUST use `fluent::...` unless explicitly describing the migration from old names

### Requirement: Qt reflection names use fluent namespace
Qt metatype declarations and runtime metatype registration names SHALL use the `fluent::...` spelling for migrated component types.

#### Scenario: Metatype declarations use canonical names
- **WHEN** headers declare Qt metatypes or flag operators for migrated component types
- **THEN** `Q_DECLARE_METATYPE` and `Q_DECLARE_OPERATORS_FOR_FLAGS` MUST use `fluent::...` types

#### Scenario: Runtime registration strings match canonical names
- **WHEN** tests or source register migrated component types with `qRegisterMetaType(...)`
- **THEN** the registered type-name string MUST use `fluent::...`
- **AND** it MUST NOT register old `view::...` component type names

### Requirement: Include paths remain components based
The namespace migration SHALL preserve the existing canonical `components/...` include root.

#### Scenario: Includes do not move with namespace
- **WHEN** project-owned code includes reusable component headers
- **THEN** it MUST continue using paths such as `components/basicinput/Button.h` and `components/foundation/QMLPlus.h`
- **AND** the migration MUST NOT reintroduce `view/...` include paths
