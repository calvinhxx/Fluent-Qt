# Timeline and Stepper

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Spatial: compose existing widgets in depth](spatial-view.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:top:end -->

Timeline presents an ordered collection. Stepper presents a small set of workflow
steps. Both are available through `<FluentQt/FluentQt.h>` and the corresponding
`fluentqt` Python category modules. Their content and application rules remain
caller-owned.

## Timeline

`fluent::collections::Timeline` derives from ListView. It borrows a
`QAbstractItemModel`, uses the existing Fluent scroll bars and pointer/keyboard
path, and allocates no persistent widget per row. Large lists use Qt's batched
layout. Node transitions retain only visible-row presentation state.
The built-in status, icon and accessibility roles repaint without invalidating
row heights. Content changes and application-defined delegates retain Qt's
normal layout invalidation. Resizing reuses Qt's layout path and preserves
transitions for nodes that remain visible.

| Model input | Meaning |
| --- | --- |
| `Qt::DisplayRole` | Title |
| `Qt::DecorationRole` | Optional `QIcon` |
| `Timeline::DescriptionRole` | Supporting text |
| `Timeline::TimestampRole` | Caller-formatted time label |
| `Timeline::StatusRole` | Neutral, Active, Success, Warning or Error; defaults to Neutral |
| `Qt::AccessibleTextRole` / `Qt::AccessibleDescriptionRole` | Optional semantic text overrides |

Text wraps at the available width. Leading, Trailing and Alternate place the
rail in logical coordinates; RTL mirrors the layout. The view does not sort
rows, format dates, write model data or perform application actions. Standard
`pressed`, `clicked`, and `itemClicked` signals retain their existing meanings.
Applications may replace the item delegate to supply their own row composition;
that delegate owns its row painting, including the rail.

```cpp
auto* timeline = new fluent::collections::Timeline(this);
auto* model = new QStandardItemModel(2, 1, this);
model->setData(model->index(0, 0), "Node title");
model->setData(model->index(0, 0), "Supporting text",
               fluent::collections::Timeline::DescriptionRole);
timeline->setModel(model);
```

## Stepper

`fluent::navigation::Stepper` composes Button, Label and ScrollView. Each
`StepperItem` supplies text, optional description and icon glyph, Pending,
Completed or Error state, enabled state, application data and optional
accessible name. Captions elide at narrow widths; Label's tooltip exposes the
full text. Overflow remains scrollable rather than compressing hit targets.

The initial `currentIndex` is `-1`. Pointer, Enter, Space and accessibility
activation emit `stepRequested(index)` without changing it. The application
accepts a request by calling `setCurrentIndex`; it may validate or reject the
request first. Completed/Error state is independent of currentIndex. A disabled
step cannot request navigation, but the application can mark it current to show
a blocked position. No page host, validation, completion inference or business
action is built into the component.

```cpp
auto* steps = new fluent::navigation::Stepper(this);
steps->addItem(fluent::navigation::StepperItem("Step one", "Optional detail"));
steps->addItem("Step two");
steps->setCurrentIndex(0);
connect(steps, &fluent::navigation::Stepper::stepRequested,
        steps, &fluent::navigation::Stepper::setCurrentIndex);
```

Arrow keys move focus in the orientation's logical direction and skip disabled
steps. Home/End focus the first/last eligible step. Focus movement does not
advance the workflow. Insertion/removal before the current step preserves its
identity; removing the current step clears currentIndex. Metadata updates and
no-op setters do not emit duplicate state changes.

## Motion and accessibility

Node fills, emphasis and completion rails use shared Fluent duration/easing
tokens and MotionPolicy. An interrupted transition starts from its current
presentation. Reduced motion caps transitions at 50 ms; Disabled or a local
`animationEnabled=false` settles immediately. Hidden controls stop their
transitions. Animation frames never change application data or emit semantic
selection events.

Timeline exposes a read-only list with logical children and descriptions that
include supporting text, time and status. Stepper exposes a page-tab list with
logical children, selected/disabled/focused state and guarded request actions.
The actual focus buttons expose these same page-tab interfaces. Tab enters the
current step when available, otherwise the first available step; a sequence
with no available steps is skipped.
Caller-provided root names/descriptions remain authoritative. C++ and Python
use the same native implementation and ownership contract.

Gallery routes `timeline` and `stepper` demonstrate the public API in both
runtimes. Component tests cover state, ownership, input, geometry, accessibility
and motion convergence. Actual desktop animation timing and platform assistive
technology remain native-review boundaries.

<!-- docs-nav:bottom:start -->
---
[← Spatial: compose existing widgets in depth](spatial-view.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:bottom:end -->
