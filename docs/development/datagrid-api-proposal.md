# DataGrid API Contract (1.7-C)

Status: **C4 public delivery complete**. C++, PySide6, the native Gallery, and
the WebAssembly Gallery share the model/view and ownership contract below.

`fluent::collections::DataGrid` is a Fluent table surface built on
`QTableView`. It presents large two-dimensional models, column interaction,
selection, and delegate-based editing without becoming a data source, form
engine, or spreadsheet.

## Core boundary

DataGrid owns presentation and interaction state. The application keeps
ownership and authority over:

- `QAbstractItemModel` data, header data, flags, sorting, and persistence
- `QItemSelectionModel` selection state when supplied by the caller
- `QAbstractItemDelegate` painting, editors, validation, and commit behavior
- domain column schemas, filtering, paging, loading, and serialization

The view must not copy the whole model into an internal store. It must not
create a persistent `QWidget`, `Field`, or editor for every row or cell. Normal
cells are delegate-painted; editors exist only while active.

## Public MVP surface

The public class reuses Qt APIs wherever they already carry the contract:

- Base: `QTableView`, `FluentElement`, and `QMLPlus`
- Shared `fluent::collections::SelectionMode` for none, single, multiple, and
  extended selection
- Inherited `QAbstractItemView::selectionBehavior` for item, row, or column
  selection; no duplicate `SelectionUnit` enum in the MVP
- Inherited `QTableView` grid, sorting, word-wrap, row/column sizing, and header
  APIs
- Model `headerData()` plus `QHeaderView` for labels, visibility, resize,
  reorder, and sort indicators; no parallel column descriptor in the MVP
- Fluent appearance only where Qt does not provide it: border/background,
  empty-model placeholder, selection/current visuals, fluent scrollbars,
  scroll chaining, overscroll, theme, and design-language tokens

Every new property follows the normal getter/setter/changed-signal trio and is
silent on no-op writes. C2-C3 proved where inherited Qt APIs were sufficient;
C4 freezes the remaining public names.

## Delivery slices

### C0 — Contract and private prototype

- Use a deterministic counting model with at least 100,000 rows and 20 columns.
- Prove that initial show, resize, and scroll query data proportional to visible
  cells plus a bounded prefetch margin, not all model cells.
- Prove that retained delegates, editors, indexes, and auxiliary objects do not
  scale with total row count.
- Keep the prototype out of installed headers, `FluentQt.h`, PySide6 exports,
  Gallery catalog entries, and generated AI guidance.

Exit: this proposal and the counting-model evidence are reviewed. Avoid brittle
wall-clock CI thresholds; use structural counters and keep optional local
benchmarks for trend data.

C0 began as a non-installed prototype. Its evidence now lives in
`src/components/collections/DataGrid.*` and
`tests/components/collections/TestDataGrid.cpp`: the same 100,000 × 20 counting
model verifies viewport-bounded initial access, scroll/resize access, constant
auxiliary-object/editor counts, and caller ownership of models and delegates.
The public promotion happened only after C1-C3 passed.

### C1 — Read-only core

- Model replacement, reset, inserted/removed rows and columns, and empty model
- Horizontal and vertical headers driven by the model
- Current cell, row/cell selection, hover, disabled, focus, and alternate-row
  states
- Keyboard navigation and type-safe selection-mode translation
- Fluent scrollbars, boundary chaining, placeholder, border, and background
- Light, Dark, RTL, narrow-width, long-header, and high-DPI visual evidence

Exit: focused `Contract_*` tests pass and the large-model counter stays bounded.

During C1 the implementation was compiled privately and omitted from installed
headers, bindings, Gallery, and generated catalogs. Eight automated
`Contract_*` tests cover the four C0 structural invariants plus live model/header/empty-state
changes, shared selection-mode translation and keyboard current movement,
token palettes/read-only defaults/disabled rendering, and Fluent scrollbar
mirroring with boundary chaining. Caller-owned `QItemSelectionModel` lifetime
is covered alongside model and delegate ownership.

The full-profile `VisualCheck_ReadOnlyCore` fixture uses a host-owned window, a
24-row deterministic table, 28 px rows and headers, long English/CJK content,
an empty state, a disabled row, row selection, keyboard focus, RTL, Light/Dark,
minimum 640 × 420 content size, and a terminal-row action. The final local v2
visual-evidence manifest passes with 14 states, 7 regions, and 5 measurements.
The review fixed three concrete issues: native selection color leaking through
a translucent delegate fill, an incomplete one-stroke focus cue, and a header
that was shorter than the row cadence.

### C1 composition decision

DataGrid is a **full-profile embedded collection**, not an application shell:

| Decision | Selected contract | Rejected alternative |
| --- | --- | --- |
| Primary object | Caller-owned two-dimensional `QAbstractItemModel` snapshot plus incremental notifications | A FluentQt-owned record store that duplicates domain state |
| Signature surface | Compact `QTableView` viewport with delegate-painted cells, model headers, selection/current visuals, and overlay Fluent scrollbars | `ScrollView` plus one child widget per cell, which cannot stay bounded |
| Native bridge | Private theme-aware `QTableView` adapter; Qt retains keyboard, selection, header, and model semantics | A custom canvas table that would reimplement mature Qt behavior and accessibility |
| Density | 28 px header/row cadence, 10 px leading cell inset, 1 px dividers, 24 px capture-host inset | Roomy card rows or persistent `Field`/editor widgets |
| Sparse state | Wrapped caller text centered inside the bounded table surface | An application-specific illustration or action owned by the reusable view |

The selected concept aligns with a compact document/database grid: the table
remains dominant and headers/scrollbars stay supporting chrome. A live-console
layout was used as a contrast and rejected because C1 has no monitoring panels
or always-visible commands. A card collection was also rejected because it
destroys column comparison. No product marks, colors, copy, or screenshot
geometry are transferred.

One C2 risk is now explicit: a macOS 15.7 / Qt 6.9 Cocoa accessibility
hierarchy query reproduced a nondeterministic `libqcocoa` `EXC_BAD_ACCESS` in
AppKit's accessibility array lookup. Identical disabled 100,000 × 20 probes for
bare `QTableView` and DataGrid both subsequently returned the same table root;
there is no stable FluentQt-only reproduction. The first full hierarchy query
was slow for both because Qt reports the complete logical matrix plus headers
as children. OS-level hierarchy enumeration therefore stays a manual platform
diagnostic, not an automated contract.

### C2 — Column interaction and accessibility

- Resize, reorder, hide/show, and sort through `QHeaderView` and model APIs
- Application-owned persistence of column layout; DataGrid exposes no hidden
  settings store
- `QAccessible::Table` semantics with logical rows/cells, header names,
  current/selected state, row/column counts, and keyboard actions
- Caller-supplied accessible names/descriptions are preserved

Exit: mouse and keyboard interaction agree, accessibility tests query the
logical table instead of child cell widgets, and no-op state changes stay
silent.

Current C2 evidence adds nine automated contracts (17 total for C0-C2):

- header resize/reorder/hide/show and sort requests stay in `QHeaderView` and
  the caller model; DataGrid adds no column store
- the native `QAccessible::Table` interface exposes logical cells, row/column
  headers, focus, selection, and toggle actions while preserving
  caller-provided name and description text
- the painted empty-state text becomes the automatic accessible description
  only while the application has not supplied an override
- a direct logical lookup of the last cell in a 100,000 × 20 model creates no
  per-cell widgets and performs bounded model work
- cell and header interfaces prefer `Qt::AccessibleTextRole` and
  `Qt::AccessibleDescriptionRole` without changing `Qt::DisplayRole`; Qt's
  table-level `columnDescription()` remains the visible header by native
  contract
- row removal invalidates cached logical cells, and replacing the complete
  model clears the old cell cache before the new model is queried
- replacing a model emits one accessible `ModelReset`; assigning the active
  model again is a silent no-op
- a public-Qt-only logical table adapter reports root and per-cell `readOnly`
  and `editable` state from view edit triggers plus model flags, while keeping
  the same row-major cell/header hierarchy as Qt's native item-view adapter
- logical cell/header cache entries are released with the view and invalidated
  across structural model changes without retaining per-cell widgets

The model-replacement contract found and fixed a concrete native-adapter gap:
without an explicit accessible reset, Qt retained a valid-looking cached cell
whose persistent index belonged to the previous model, so the replacement
table could expose an empty cell name. DataGrid now invalidates that logical
cache with Qt's public `QAccessibleTableInterface::modelChange()` contract and
emits the matching event; it does not import Qt private accessibility code.

The private adapter uses only public `QAccessibleWidget`, table, table-cell,
action, model, header, and selection APIs. Qt 6-only selection-interface code
is version-guarded; no Qt private item-view accessibility header is imported.
The same source compiles and links in the supported Qt 6.9.3 / Emscripten
3.1.70 WASM build. This remains a compatibility check only; C2 does not add a
browser Gallery route or claim a public WebAssembly surface.

The nondeterministic Cocoa full-hierarchy probe remains a manual platform
diagnostic because the same behavior was observed with a bare `QTableView`.
It is release-acceptance evidence, not a deterministic C2 implementation
blocker. C2 is complete; the C3 editing contract is described below.

### C3 — Editing and validation

- Editing is enabled by model flags and performed by the active delegate
- Enter/F2 starts editing where supported; Enter/Tab commits according to Qt
  item-view behavior; Escape cancels without changing model data
- `setData()` success/failure remains model authority
- Validation state/message travel through reviewed model/delegate roles or a
  custom delegate; DataGrid does not mutate display/edit values to show errors
- Editor creation, focus, geometry, destruction, and ownership are covered by
  tests, including model reset while editing

Field's validation presentation is a semantic reference, not a per-cell widget
implementation.

The C3 contract fixed these decisions before public promotion:

- DataGrid remains read-only by default. Applications opt into editing through
  inherited `QAbstractItemView::editTriggers`; model `Qt::ItemIsEditable` flags
  remain authoritative for each index.
- The default path keeps `QStyledItemDelegate` semantics. F2/Enter/Tab/Escape
  are tested through the real editor event path rather than a DataGrid-owned
  value buffer.
- A failed model `setData(..., Qt::EditRole)` never changes display/edit data.
  Whether an editor stays open after rejection is delegate policy; DataGrid
  does not report a successful commit or synthesize a replacement value.
- C3 proves application-defined validation roles through a caller-provided
  delegate. It does not reserve a public `Qt::UserRole` range until C4 shows
  that a library role is necessary and collision-safe.
- Only the active index may own a transient editor widget. Model reset,
  replacement, cancel, commit, and view destruction must leave no retained
  editor or stale focus target.

Three additional automated contracts (20 total for C0-C3) now verify the
complete editing boundary:

- DataGrid stays read-only until the caller enables `editTriggers`; when
  `EditKeyPressed` is enabled, F2 starts the active delegate editor consistently
  across desktop styles. Enter and Tab commit through the delegate/model path,
  while Escape cancels without changing model data.
- A caller model can reject `setData(..., Qt::EditRole)` and publish an
  application-defined validation role. A caller delegate can render that role
  without DataGrid changing display or edit values; DataGrid reserves no public
  validation role.
- Editors are transient and viewport-bounded. Commit, cancel, model reset,
  model replacement, and view destruction release the editor; geometry follows
  the active cell and the caller-provided delegate remains authoritative.

The focused target and the repository's 155-test contract suite pass on native
Qt 6.9.3. The same implementation compiles and links in the supported Qt 6.9.3
/ Emscripten 3.1.70 build. C3 is complete.

Exit: commit/cancel, rejected edits, validation display, focus, and lifetime
contracts pass with both the default and a caller-provided delegate.

### C4 — Public delivery

- Install the public header and export it from `<FluentQt/FluentQt.h>` only
  after C0-C3 contracts are stable
- Add three source-aligned Gallery samples: large read-only data, column and
  selection interaction, and delegate editing/validation
- Record the public Python surface decision before release. The intended 1.7
  outcome is C++ and PySide6 support in the same release slice; if binding work
  is blocked, DataGrid remains private/preview
- PySide6 coverage includes model/delegate/selection ownership, subclassing,
  garbage collection, runtime imports, stubs, and Python Gallery examples
- WASM coverage includes source compatibility, build, large-model interaction,
  scrolling, keyboard selection, and editing in the browser Gallery
- Update generated catalogs only through their generators

Exit: C++, PySide6, and WASM acceptance is recorded in the 1.7 roadmap and the
public catalog does not claim a surface that was not shipped.

C4 reached that exit with an installed aggregate C++ header, 21 automated
DataGrid contracts, a PySide6 facade and manifest entry, ownership/GC coverage,
and three matching C++/Python Gallery samples. Generated catalogs now expose
69 components, 90 routes, and 205 samples. The WebAssembly Gallery builds with
Qt 6.9.3 / Emscripten 3.1.70; full browser smoke traverses all 90 routes and
checks the live DataGrid examples for end-of-model scrolling, keyboard
selection, and delegate editor commit.

## Contract matrix

| Area | Required behavior | Evidence |
| --- | --- | --- |
| Ownership | Caller models, selection models, and delegates remain caller-owned | Replacement/destruction tests in C++ and PySide6 |
| Scale | Work and retained objects follow the viewport, not total cell count | Counting model and editor/delegate counters |
| Selection | Current, selected, activated, row, and cell semantics stay distinct | Mouse/keyboard `Contract_*` tests |
| Columns | Model headers are canonical; view state does not alter business data | Header reset/reorder/sort tests |
| Editing | Delegate and model decide editor/value/commit; Escape cancels | Commit/cancel/reject/reset tests |
| Validation | Presentation never rewrites display/edit roles | Custom model/delegate tests |
| Accessibility | Logical table/header/cell roles, names, state, and actions | `QAccessible` contract tests |
| Visual | Tokens, density, focus, selection, error, RTL, and DPI remain legible | Focused VisualCheck evidence |
| Cross-language | Public surface is supported or explicitly excluded before release | API manifest, binding tests, roadmap |
| WebAssembly | Browser build and core interaction remain usable | WASM Gallery smoke and manual matrix |

## Deferred from MVP

- Spreadsheet formulas, merged cells, pivoting, grouping, and tree tables
- Frozen panes, batch clipboard, fill handles, and Excel-style range editing
- Built-in filtering UI, pagination, remote-data adapters, CSV import/export,
  and application persistence
- A FluentQt-owned column schema duplicating model header roles
- Built-in validation roles; application-defined model roles and delegates are
  the intentional MVP contract

Deferrals require a concrete use case and a separate contract; they are not
implicit DataGrid responsibilities.
