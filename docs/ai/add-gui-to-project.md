# Adding a FluentQt GUI to any project

Use this workflow for both existing and greenfield projects. An existing TUI is
one possible signal, not a prerequisite and not the architectural boundary.
The GUI should depend on reusable domain/application behavior rather than screen
scraping or emulating another interface.

## 1. Discover the target before choosing a UI

Read the target repository's local agent instructions and inspect its build,
package, test, release, and platform configuration. Record evidence for:

- languages, build systems, supported platforms, and existing entry points;
- domain modules and public APIs that already perform useful work;
- long-running work, progress, cancellation, error, and retry behavior;
- persistence, network, filesystem, process, plugin, and authentication edges;
- current interfaces: library, CLI, TUI, service, plugin, GUI, or none;
- user outcomes and workflows, not merely a list of desired widgets.

For a large or ambiguous project, write a task-local analysis conforming to
`project-analysis.schema.json`. Do not commit that record unless it is useful
project documentation.

## 2. Choose one primary integration pattern from evidence

Query an exact pattern with:

```bash
python3 tools/ai/query_ai_catalog.py --pattern direct-library
```

| Pattern | Use it when | Important boundary |
| --- | --- | --- |
| `direct-library` | Stable domain/application APIs can be called in-process | Make ownership, thread affinity, and cancellation explicit |
| `service-api` | A service is already the authoritative boundary | Keep transport DTOs out of widgets and expose connection state |
| `structured-process` | A mature executable is the only safe reusable boundary | Require structured input/output and a stable error contract; do not parse terminal decoration |
| `plugin-extension` | The host intentionally supports embedded frontends | Follow host lifecycle, ABI/API, event-loop, and unload rules |
| `extract-core` | Useful behavior is trapped inside an existing interface | Extract and test the smallest UI-independent application service first |
| `greenfield` | No reusable interaction or application layer exists | Define domain state and use cases before composing widgets |

Reject alternatives explicitly when the choice changes process isolation,
runtime dependencies, packaging, or supported platforms. If the evidence is
insufficient, implement a small adapter spike and test it before building the
full GUI.

## 3. Define the GUI contract

Describe the smallest end-to-end user slice in terms of:

1. input and validation;
2. invoked use case;
3. loading, progress, success, empty, and error states;
4. result presentation and any persistent state;
5. cancellation, retry, close, and cleanup behavior.

Add a concise visual contract before composing widgets:

- the product or neutral Fluent visual reference and its evidence;
- a named Gallery route, sample, or component `VisualCheck` for every major
  surface or control family;
- canvas, layer, panel, card, overlay, and primary-action hierarchy;
- Light/Dark theme strategy, semantic brand colors, typography, and radius;
- normal, narrow, and minimum supported layouts;
- a density/metric sheet for shell height, panel insets, group and section
  gaps, control and row heights, footer height, and icon size;
- a component decision table with intent, chosen component, rejected
  alternative, and catalog/API evidence.

Keep four responsibilities distinct even when the first slice is small:

- **domain/application layer** owns business rules and use cases;
- **adapter layer** translates library, service, process, or plugin contracts;
- **view state/controller** makes UI state transitions explicit and testable;
- **FluentQt view** renders state and emits user intent.

Do not move business logic into signal handlers merely because the first GUI is
small. Do not replace a working interface unless replacement is an explicit
requirement; multiple frontends should share the same application behavior.

## 4. Select components by behavior

Start with an application pattern, then query specific decisions instead of
loading every component:

```bash
python3 tools/ai/query_ai_catalog.py --pattern file-workbench
python3 tools/ai/query_ai_catalog.py --guide status-and-identity
python3 tools/ai/query_ai_catalog.py --search "determinate progress"
python3 tools/ai/query_ai_catalog.py --component info-bar
```

Treat catalog candidates as a shortlist. Verify the referenced public header or
Python import, inspect the linked Gallery sample, and build on existing FluentQt
components. Each catalog entry includes its focused test and sample source.
Choose by semantics, lifetime, data shape, interaction, and density. Do not use
a raw Qt widget merely because it is familiar; document and theme every raw
widget that remains because FluentQt has no suitable public contract.

For C++, link `FluentQt::FluentQt` and use installed headers such as
`<FluentQt/FluentQt.h>` or the category header reported by the query. For
Python, use the reported `fluentqt.<category>` import and keep PySide6, Qt, and
Shiboken versions and architectures aligned.

## 5. Implement a vertical slice

Build one useful workflow before expanding navigation or visual polish:

1. Add the narrow application adapter and unit-test it without a window.
2. Model deterministic view states and transitions.
3. Compose the minimum FluentQt surface for the workflow.
4. Move blocking I/O or computation off the GUI thread.
5. Marshal results back through Qt signals or queued invocations with clear
   object ownership and cancellation.
6. Preserve existing entry points and regression tests.

Use progress controls only for work that is actually observable. Disable or
guard duplicate actions while work is running. Surface recoverable errors near
the affected workflow and log diagnostic detail through the target project's
existing logging boundary.

## 6. Validate behavior and appearance

Validation is proportional to the change but must cover all of these layers:

- existing domain and interface tests still pass;
- adapter and view-state tests cover success, empty, validation, failure,
  cancellation, and teardown paths that apply;
- the target builds through its supported build/package flow;
- relevant FluentQt focused tests pass when FluentQt itself changes;
- interactive workflows remain responsive during long-running work;
- Light/Dark themes, text fit, keyboard focus, disabled/hover/pressed states,
  scaling, resize, and supported platforms receive visual review;
- deterministic representative data is inspected at normal and narrow widths,
  concrete visual issues are fixed, and the same views are rechecked;
- the full window and 100% crops of title bars, pane headers, selected rows,
  pane footers, and the composer/input edge are reviewed so scaling cannot hide
  small alignment or spacing defects;
- repeated insets, alignments, row gaps, indicator/text separation, and
  label/value spacing are measured on the 4 px grid; selected, focused,
  text-preedit, and transient-surface states contain no overlap or stale layer;
- the application is compared beside its named Gallery/component references in
  a matching theme and scale, with no unexplained downgrade in token fidelity,
  interaction states, or visual finish;
- installers or wheels include the new GUI and required assets when shipping is
  in scope.

For changes to FluentQt's own AI contract, finish with:

```bash
python3 tools/ai/validate_ai_assets.py --project-root .
```

Report the chosen integration pattern and evidence, preserved interfaces,
implemented vertical slice, commands run, visual coverage, and any unverified
platform or packaging boundary. Do not label feasibility as verified support.
