## 1. Baseline And Inventory

- [x] 1.1 Record active `view::`, `::view::`, `namespace view`, `using namespace view`, `using view::`, and `Q_DECLARE_.*view::` references in `src/`, `tests/`, `docs/`, `README.md`, `AGENTS.md`, and `openspec/specs/`, excluding archived OpenSpec history.
- [x] 1.2 Record global `FluentElement` declarations/usages that must become `fluent::FluentElement` or resolve through `namespace fluent` parent lookup.
- [x] 1.3 Inventory Qt reflection touchpoints: `Q_DECLARE_METATYPE`, `Q_DECLARE_OPERATORS_FOR_FLAGS`, `qRegisterMetaType(...)` strings, `QMetaEnum::fromType`, and inheritance/static type assertions.
- [x] 1.4 Confirm `components/...` include paths and `src/components/` / `tests/components/` directories remain unchanged by this migration.

## 2. Foundation Namespace Migration

- [x] 2.1 Move `FluentElement` and its private implementation declarations/definitions into `namespace fluent` while preserving theme, token, and lifecycle behavior.
- [x] 2.2 Move `QMLPlus`, `AnchorLayout`, `PropertyBinder`, `PropertyLink`, `QMLState`, and `PropertyChange` declarations/definitions from `namespace view` to `namespace fluent`.
- [x] 2.3 Move overlay helper declarations/definitions from `view::overlay` to `fluent::overlay` without changing same-window overlay geometry, light-dismiss, scrim, stacking, or transparent rendering behavior.
- [x] 2.4 Update foundation tests under `tests/components/` to use `fluent::FluentElement`, `fluent::QMLPlus`, and `fluent::AnchorLayout`.
- [x] 2.5 Verify no `view` namespace aliases, compatibility typedefs, or forwarding namespace declarations are introduced for foundation APIs.

## 3. Component Source Namespace Migration

- [x] 3.1 Rename `src/components/basicinput` component namespaces and cross-component references from `view::basicinput` to `fluent::basicinput`.
- [x] 3.2 Rename `src/components/textfields` component namespaces and references from `view::textfields` to `fluent::textfields`.
- [x] 3.3 Rename `src/components/dialogs_flyouts` component namespaces and overlay helper references from `view::dialogs_flyouts` / `view::overlay` to `fluent::dialogs_flyouts` / `fluent::overlay`.
- [x] 3.4 Rename `src/components/collections` component namespaces and scrolling dependencies from `view::collections` / `view::scrolling` to `fluent::collections` / `fluent::scrolling`.
- [x] 3.5 Rename `src/components/scrolling` component namespaces and status-info dependencies from `view::scrolling` / `view::status_info` to `fluent::scrolling` / `fluent::status_info`.
- [x] 3.6 Rename `src/components/navigation` component namespaces and supporting component references from `view::navigation` and related `view::...` names to `fluent::navigation` and related `fluent::...` names.
- [x] 3.7 Rename `src/components/date_time` component namespaces and picker button/flyout references from `view::date_time`, `view::basicinput`, and `view::dialogs_flyouts` to `fluent::date_time`, `fluent::basicinput`, and `fluent::dialogs_flyouts`.
- [x] 3.8 Rename `src/components/menus_toolbars`, `src/components/status_info`, and `src/components/windowing` namespaces and component references to their `fluent::...` equivalents.
- [x] 3.9 Update all component forward declarations, fully qualified member types, base class references, signal/slot type references, and namespace closing comments to `fluent::...`.

## 4. Qt Reflection And Test Migration

- [x] 4.1 Update all `Q_DECLARE_METATYPE` and `Q_DECLARE_OPERATORS_FOR_FLAGS` declarations for migrated component types to `fluent::...`.
- [x] 4.2 Update all `qRegisterMetaType(...)` template arguments and string names for migrated component types to `fluent::...`.
- [x] 4.3 Update tests under `tests/components/**` from `using namespace view`, `using view::...`, and `view::...` references to `fluent::...`.
- [x] 4.4 Update VisualCheck code to use `fluent::AnchorLayout` and project Fluent components without changing `SKIP_VISUAL_TEST`, `VISUAL_SNAPSHOT`, or `qApp->exec()` behavior.
- [x] 4.5 Preserve test target names, CTest labels, direct binary paths, and `add_qt_test_module(...)` registrations.

## 5. Documentation And OpenSpec Synchronization

- [x] 5.1 Update README, AGENTS.md, and `docs/development/` guidance so active component namespace examples use `fluent::...`.
- [x] 5.2 Update `docs/architecture/overlay-behavior.md` and overlay-related guidance to refer to `fluent::overlay` where helper namespaces are named.
- [x] 5.3 Update live OpenSpec specs under `openspec/specs/` so active public component names, mixin names, metatype names, and helper namespaces use `fluent::...`.
- [x] 5.4 Leave archived OpenSpec changes as historical records unless an archived reference is copied into active guidance.
- [x] 5.5 Update component API audit/convention docs to record this as a deliberate breaking namespace migration with no `view` compatibility aliases.

## 6. Validation

- [x] 6.1 Run namespace searches to confirm no active reusable component API references remain for `namespace view`, `view::`, `::view::`, `using namespace view`, `using view::`, or `Q_DECLARE_.*view::` outside archived history and explicit migration notes.
- [x] 6.2 Run `openspec validate rename-component-namespace-to-fluent --strict`.
- [x] 6.3 Run `openspec validate --all --strict` after live spec synchronization.
- [x] 6.4 Configure with `cmake --preset vcpkg-osx` if CMake discovery, moc, or test discovery changes require regeneration.
- [x] 6.5 Build `fluent_qt_lib` with `cmake --build --preset vcpkg-osx --target fluent_qt_lib`.
- [x] 6.6 Build focused representative test targets from foundation, basicinput, dialogs_flyouts, collections, scrolling, navigation, date_time, status_info, and windowing.
- [x] 6.7 Run focused CTest label validation for representative component targets with VisualCheck skipped.
- [x] 6.8 Document any unrelated pre-existing build or test failures separately from namespace migration regressions.