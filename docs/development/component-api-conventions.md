# Component API Conventions

Use these conventions when adding, reviewing, or auditing public Fluent component
APIs under `src/view/**`.

## Scope

- Treat public widgets under `src/view/**` as component API.
- Treat `FluentElement`, `QMLPlus`, private headers, design tokens, and
  compatibility helpers as supporting infrastructure unless the task explicitly
  targets them.
- View foundation infrastructure lives under `src/view/foundation/`. Prefer
  canonical includes such as `view/foundation/FluentElement.h`,
  `view/foundation/QMLPlus.h`, and `view/foundation/overlay/...` in project
  code.
- When performing an audit, update [Component API Audit](component-api-audit.md)
  with inventory, findings, intentional deviations, applied fixes, deferred
  follow-ups, and validation notes.

## Inheritance and Ownership

- Public widgets should expose Qt child containment, Fluent theme access, and
  `QMLPlus` support directly or through an established project base such as
  `view::basicinput::Button`.
- Button-like entry surfaces should derive from `view::basicinput::Button` when
  they do not require a more specialized Qt base class.
- Views and host components should keep caller-owned content caller-owned. Do
  not move application page choice, item composition, model ownership, or
  navigation routing into a reusable component just to normalize APIs.
- Specialized Qt bases are acceptable when they carry essential behavior, such
  as item views, scroll bars, dialogs, or line edits.

## Properties, Setters, and Signals

- `Q_PROPERTY` names should match established Qt conventions and nearby project
  components.
- Getter, setter, and signal names should form an obvious trio, for example
  `value()`, `setValue(...)`, `valueChanged(...)`.
- Boolean getters should communicate state clearly with `is*`, `has*`, `are*`,
  or an established component-specific noun such as `canReorderItems()`.
- Existing noun-style boolean getters may remain for source compatibility, but
  new compatibility aliases should prefer the clearer `is*`, `has*`, or `are*`
  shape when the original name is ambiguous.
- Repeated setter calls with the currently stored value must be no-ops and must
  not emit duplicate changed signals.
- Setter normalization should be explicit in tests when values are clamped,
  snapped, or converted to an empty value.

## Nullable Values

- Invalid Qt values such as `QDate()` and `QTime()` may represent an empty
  selection when that matches WinUI-style picker semantics.
- Empty selection behavior must be visible through public getters and focused
  tests.
- Clearing APIs should be named explicitly, such as `clearDate()`,
  `clearSelectedDate()`, or `clearSelectedTime()`.
- Setting an invalid nullable value should either clear the selection or be
  documented as ignored; tests should cover the chosen behavior.

## Open State

- Components that expose popup, flyout, dropdown, calendar, or pane state should
  provide a clear boolean getter, a command or setter to open/close, and a
  changed signal.
- `isOpen()` is the preferred common alias for button-like entry open state when
  a component also keeps a more specific legacy getter such as
  `isDropDownOpen()` or `isCalendarOpen()`.
- Existing specific getters and setters should remain for compatibility unless a
  separate breaking migration explicitly removes them.
- Light-dismiss behavior should be tested separately from programmatic close
  behavior.

## Selection and Current Item Naming

- Selection APIs should distinguish selected item(s), current item, activation,
  and reorder state.
- Multi-selection must document whether selection is Qt item-view driven or
  component-specific.
- Signals named `activated`, `clicked`, `currentChanged`, or `selectionChanged`
  should match their Qt meaning where practical.
- Collection views should not own business item composition when model/delegate
  ownership is caller-provided.

## Fix or Defer

- Prefer compatible aliases, missing no-op tests, missing nullable-value tests,
  and documentation updates inside an audit change.
- Do not remove, rename, or repurpose existing public API without a dedicated
  breaking migration proposal.
- Defer overlay open-state unification when modal, light-dismiss, visibility, or
  hosted-content semantics are not yet specified.
- Defer direct menu test expansion if the current change does not touch menu
  behavior; record the follow-up in the audit report.

## Tests and VisualCheck

- Each component should have a focused `tests/views/<category>/Test<Name>.cpp`
  file when it exposes public behavior.
- Inheritance assertions are expected when the public contract depends on a Qt
  base or project base class.
- Signal no-op behavior should be tested for audited setters with changed
  signals.
- Nullable `QDate`, `QTime`, or equivalent empty value semantics should be
  tested directly.
- VisualCheck tests must keep the `SKIP_VISUAL_TEST` guard, block with
  `qApp->exec()`, and use project Fluent controls for visible demo UI when
  practical.
