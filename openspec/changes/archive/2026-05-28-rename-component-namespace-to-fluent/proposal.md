## Why

The reusable component source tree now lives under `src/components/`, but its public C++ API still uses `view::...`, which preserves the old architectural ambiguity between reusable components and Gallery application views. Because the project is still early, this change can establish a clean component foundation now instead of carrying a long-lived compatibility surface.

## What Changes

- **BREAKING**: Rename the canonical public C++ namespace for reusable Fluent Qt components from `view::...` to `fluent::...`, for example `view::basicinput::Button` becomes `fluent::basicinput::Button`.
- **BREAKING**: Move component foundation public types into the `fluent` namespace, including `QMLPlus`, `AnchorLayout`, property binding helpers, and same-window overlay helpers such as `fluent::overlay::*`.
- **BREAKING**: Move `FluentElement` from the global namespace to `fluent::FluentElement` so component foundation types share one explicit public namespace.
- Remove the old `view::...` public component surface rather than adding namespace aliases, compatibility typedefs, or forwarding APIs.
- Keep canonical include paths as `components/...`; this change is a namespace/API migration, not another filesystem move.
- Preserve public class names, properties, signals, enum names, behavior, CTest targets, VisualCheck behavior, Qt version support, and vcpkg/CMake dependencies except where type qualification changes are required.
- Update project-owned source, tests, docs, agent guidance, and live OpenSpec specs so active guidance uses `fluent::...` consistently.
- Update Qt reflection touchpoints such as `Q_DECLARE_METATYPE`, `Q_DECLARE_OPERATORS_FOR_FLAGS`, `qRegisterMetaType(...)` names, signal spy expectations, and inheritance tests to the new canonical names.

## Capabilities

### New Capabilities

- `component-namespace`: Defines `fluent::...` as the canonical public namespace for reusable component and component foundation APIs, including the no-compatibility migration contract.

### Modified Capabilities

- `annotated-scrollbar`: Public component contract moves from `view::scrolling::AnnotatedScrollBar` and `view::QMLPlus` to `fluent::scrolling::AnnotatedScrollBar` and `fluent::QMLPlus`.
- `breadcrumb`: Public navigation item and control names move from `view::navigation` to `fluent::navigation`.
- `calendar-date-picker`: Public date-time picker names move from `view::date_time` and old button/flyout references to `fluent::date_time`, `fluent::basicinput`, and `fluent::dialogs_flyouts`.
- `component-api-consistency`: API convention requirements and audit expectations use `fluent::...` for project bases, inheritance, and namespace checks.
- `date-picker`: Public date picker names and related picker flyout/button references move to `fluent::date_time`, `fluent::basicinput`, and `fluent::dialogs_flyouts`.
- `drawer-view`: Public drawer namespace, foundation mixins, and overlay helper references move from `view::...` to `fluent::...`.
- `flow-view`: Public collection and scrolling namespace references move from `view::...` to `fluent::...`.
- `fluent-window`: Public windowing and titlebar namespace references move from `view::windowing` to `fluent::windowing`.
- `flyout`: Public flyout/popup namespace references move from `view::dialogs_flyouts` to `fluent::dialogs_flyouts`.
- `info-badge`: Public status namespace and mixin requirements move to `fluent::status_info` and `fluent::QMLPlus`.
- `info-bar`: Public status, basic input, and textfield namespace references move to `fluent::...`.
- `label`: Public textfield and tooltip namespace references move to `fluent::textfields` and `fluent::status_info`.
- `navigation-view`: Public navigation namespace, stack host references, and mixin requirements move to `fluent::navigation` and `fluent::QMLPlus`.
- `pips-pager`: Public scrolling namespace references move from `view::scrolling` to `fluent::scrolling`.
- `pivot`: Public navigation item and control references move from `view::navigation` to `fluent::navigation`.
- `progress-bar`: Public status namespace references move from `view::status_info` to `fluent::status_info`.
- `progress-ring`: Public status namespace references move from `view::status_info` to `fluent::status_info`.
- `qt-version-compatibility`: Any requirement that names migrated component classes, such as Label, uses the new `fluent::...` spelling while keeping Qt compatibility helpers unchanged.
- `scroll-view`: Public scrolling namespace references move from `view::scrolling` to `fluent::scrolling`.
- `selector-bar`: Public navigation item/control references move from `view::navigation` to `fluent::navigation`.
- `split-view`: Public collection namespace references move from `view::collections` to `fluent::collections`.
- `stack-view`: Public collection namespace and metatype references move from `view::collections` to `fluent::collections`.
- `tab-view`: Public navigation tab names and supporting component references move from `view::...` to `fluent::...`.
- `time-picker`: Public time picker names and related picker flyout/button references move to `fluent::date_time`, `fluent::basicinput`, and `fluent::dialogs_flyouts`.
- `view-foundation-structure`: Foundation and overlay helper namespace requirements move from `view` to `fluent` while preserving `src/components/foundation/` as the canonical source home.

## Impact

- Public C++ API namespace for all reusable components and component foundation helpers.
- Source and test declarations, forward declarations, using declarations, inheritance references, fully qualified types, metatype declarations, and registered metatype names.
- Live docs and specs under `README.md`, `AGENTS.md`, `docs/development/`, `docs/architecture/`, and `openspec/specs/`.
- Downstream code using `view::...` must migrate to `fluent::...`; no compatibility alias is planned for this early-version migration.
- No dependency, CMake preset, Qt support, include-root, or runtime behavior changes are intended.