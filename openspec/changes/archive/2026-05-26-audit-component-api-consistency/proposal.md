## Why

The component library now has broad coverage across basic input, collections, date/time, navigation, scrolling, status, text fields, and windowing. As the surface area grows, inconsistent inheritance choices, property naming, signal semantics, open/close state names, and test expectations make new components harder to review and make AI-assisted debugging less reliable.

This change creates a focused audit and cleanup pass that documents the project API conventions, identifies inconsistencies, and applies low-risk consistency fixes without redesigning component ownership boundaries.

## What Changes

- Add a component API consistency audit covering public headers under `src/view/**`, matching tests under `tests/views/**`, and relevant OpenSpec specs.
- Define durable conventions for:
  - component inheritance and mixin usage,
  - Qt property getter/setter/signal naming,
  - repeated setter no-op behavior,
  - nullable value semantics,
  - open/close state naming,
  - selection/current/activated naming,
  - ownership and application-layer composition boundaries,
  - test and VisualCheck coverage expectations.
- Produce a documented audit report listing findings, risk level, affected components, and recommended action.
- Apply only low-risk consistency fixes in this change, such as missing no-op setter tests, naming documentation, small test assertions, and documentation alignment.
- Defer breaking renames or large public API migrations into follow-up changes unless they can be bridged with compatibility aliases.

## Capabilities

### New Capabilities

- `component-api-consistency`: Defines cross-component API consistency requirements for this Qt Widgets Fluent component library.

### Modified Capabilities

- None.

## Impact

- Affected source areas:
  - `src/view/**` public component headers and selected low-risk implementation details.
  - `tests/views/**` focused component tests and VisualCheck conventions.
  - `openspec/specs/**` only where audit findings require documentation alignment or follow-up references.
  - `readme.md` or a new docs note for the audit report and API convention checklist.
- No new runtime dependency is expected.
- No breaking public API removal should happen in this change. Any breaking rename or behavior migration must be captured as a follow-up proposal.
