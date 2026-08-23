# Inspector Report Contract

The FluentQt Inspector is an opt-in, read-only diagnostic pass over a built
`QWidget` tree. It reports evidence for people and coding agents; it never
rewrites layout, text, accessibility metadata, focus order, or scroll policy.

The v1 report is available to C++ through `<FluentQt/Diagnostics.h>` and to
PySide6 through `fluentqt.inspect_widget(...)`. Both entry points execute the
same native rules and return the same report shape. Internal finding objects
remain private so rules can improve without expanding the compatibility
surface. Diagnostics are intentionally excluded from the umbrella
`<FluentQt/FluentQt.h>` header so applications that do not inspect built UI
trees do not inherit the Qt JSON header cost.

## Use

Run the Inspector after the target widget is visible and pending layout events
have been processed:

```cpp
#include <FluentQt/Diagnostics.h>

const QJsonObject report =
    fluent::diagnostics::Inspector::report(window.contentWidget());
```

```python
report = fluentqt.inspect_widget(window.contentWidget())
```

The C++ `InspectorOptions` and equivalent Python keyword arguments can change
the minimum hit area, spacing grid, and enabled checks. Layout-grid checking
is opt-in. Generated C++ and PySide6 Workbench projects also expose the default
report through `--quality-report`.

## Report shape

`Inspector::report(root)` returns one JSON object. The normative shape is also
available as [`inspector-report.schema.json`](inspector-report.schema.json).

```json
{
  "schema_version": 1,
  "tool": "FluentQt Inspector",
  "root": {
    "class": "QWidget",
    "object_name": "mainWindow",
    "width": 1200,
    "height": 800
  },
  "summary": {
    "findings": 2,
    "by_severity": { "info": 0, "warning": 2, "error": 0 },
    "by_category": { "accessibility": 1, "input": 1 }
  },
  "findings": []
}
```

Each finding has a stable `code`, `category`, `severity`, deterministic widget
`path`, root-relative `rect`, concise `message`, and rule-specific `details`.
Reports must not contain pointer values, timestamps, host paths, or translated
messages. The stable fields are intended for comparison; `message` is for
display only.

## v1 rules

| Code | Evidence | Default |
|---|---|---|
| `text.clipped-without-full-value` | Single-line plain text exceeds its content rect and no accessible name, description, value, help text, or tooltip preserves the full value | On |
| `accessibility.missing-name` | A visible, enabled accessible control role has no authored name | On |
| `input.small-hit-area` | A visible, enabled direct pointer/input target is smaller than the configured desktop hit area | On, 24 x 24 px |
| `focus.unreachable` | A focusable widget is absent from the root focus chain | On |
| `action.duplicate-entry` | Multiple visible widgets declare the same `fluentSemanticAction`, or multiple `QToolButton`s use the same `QAction` | On |
| `scroll.nested-boundary` | A scroll area without an explicit `scrollChainingEnabled` contract and an ancestor can both scroll on the same axis | On |
| `layout.off-grid` | Explicit non-negative layout margins or spacing do not follow the configured grid | Off, opt-in |

Built-in Qt implementation children such as internal scroll-bar containers are
excluded where they would only duplicate their owning control. Structural
scrollers and selectable static text are not treated as unnamed controls or
undersized pointer targets. A focus proxy may inherit the accessible name
authored on its public container. A finding is a review prompt, not proof of a
defect.

## Deferred rules

Baseline and optical-alignment checks need component semantics to avoid noisy
geometry guesses. Wheel-event ownership also needs a runtime probe before the
Inspector can distinguish deliberate containment from broken boundary
handoff. Those rules must land with focused false-positive cases rather than
being inferred from screenshots alone.

## Compatibility

The Inspector uses Qt Widgets and Qt Core only and performs no network or file
I/O. The same scan code compiles on desktop and WebAssembly. Public C++ and
PySide6 calls share the native implementation and versioned JSON contract.
Repository acceptance is driven by the versioned
[`application-scenes.json`](../ai/evals/application-scenes.json) manifest,
which currently exercises both Gallery pages and the generated C++ Workbench.
