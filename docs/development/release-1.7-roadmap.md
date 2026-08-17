# FluentQt 1.7 Roadmap

- Branch: `release/1.7.x`
- Status date: 2026-08-17

## Purpose

1.7 stabilizes **observable overlay semantics** across same-window surfaces,
lands **Field** as a composition shell, and delivers **DataGrid** contract-first.
Parallel tracks keep accessibility, C++/Python surface decisions, visual
evidence, and public contribution intake explicit instead of treating them as
release cleanup. It also narrows the product contract to Fluent and removes the
former Material/Cupertino API, paint branches, Gallery choices, and bindings.

This is not a Qt-version or WebView release. The reusable `FluentQt` library
still depends only on Qt Widgets. Qt 5.15 remains the current sampling strategy
(Linux full, Windows API smoke).

Related contracts:

- [Overlay Behavior](../architecture/overlay-behavior.md) — state machine, aliases, orthogonal axes
- [Component Contract Baseline](component-contract-baseline.md) — Phase 0/1 history and living matrix
- [Component API Audit](component-api-audit.md) — API-005 / API-011
- [DataGrid API Contract](datagrid-api-proposal.md) — model/view boundary, slices, and acceptance matrix
- [Accessibility Contract](accessibility-contract.md) — roles, names, state, keyboard, and event rules
- [Testing Workflow](testing-workflow.md) — CTest labels, VisualCheck, snapshot PNG limits
- [Fluent Design Contract](../design-languages/README.md) — the single supported visual contract and 1.7 migration

## Status

| Phase | Track | Scope | Status |
| --- | --- | --- | --- |
| 1.7-A Overlay contract | Function | Document `isOpen`, Opening/Open/Closing/Closed, re-entrancy, close reasons, orthogonal `modal`/`dim`/`closePolicy`, NOTIFY + no-op, compatibility aliases | Complete |
| 1.7-A Overlay implementation | Function | Compatible API/behavior for Popup, Flyout, Dialog, ContentDialog, TeachingTip; coordinator stays internal | Complete |
| 1.7-A Overlay tests | Function | `Contract_*` coverage on existing `add_qt_test_module` targets | Complete |
| 1.7-A Stale layout docs | Function | Close `FND-LAYOUT-002` (AnchorLayout size hints + Tarjan cycle diagnostics already exist) | Complete |
| 1.7-A Quality ledger | Quality | Inventory the former alternate-language branches and use it to drive a complete Fluent-only cleanup | Complete — all alternate geometry, API, Gallery, binding, test-matrix, documentation, and asset paths were removed in 1.7-P |
| 1.7-B Field | Function | Composition shell: label, editor slot, helper/error, validation presentation, focus/a11y; reuse `WidgetOwnership`; not a new editor base class | Complete — C++, Gallery, PySide6, catalogs, ownership stress tests, and focused visual evidence are green. [field-api-proposal.md](field-api-proposal.md) |
| 1.7-C0 DataGrid contract and prototype | Function | Fix model/view ownership, MVP scope, performance structure, accessibility, and cross-language acceptance before a public class lands | Complete — private 100,000 × 20 prototype and 4 structural `Contract_*` tests are green; [datagrid-api-proposal.md](datagrid-api-proposal.md) |
| 1.7-C1 DataGrid read-only core | Function | Internal component implementation, headers, visible-cell painting, empty state, keyboard current/selection, no per-cell widgets | Complete — the implementation was held private while 8 `Contract_*` tests and full-profile Light/Dark/RTL/minimum/empty/dense/scroll-end visual evidence were completed |
| 1.7-C2 DataGrid interaction | Function | Column resize/reorder/sort, row/cell selection, accessibility table semantics | Complete — 9 additional contracts (17 C0-C2 total) cover column/model authority, semantic text, logical table semantics, read-only/editable state, cache invalidation/lifetime, and no-op event silence; the public-only private adapter compiles in native Qt 6 and WASM without Qt private headers |
| 1.7-C3 DataGrid editing | Function | Delegate editors, commit/cancel, validation roles, focus and ownership | Complete — 3 additional contracts (20 C0-C3 total) cover cross-platform F2 activation, Enter/Tab commit, Escape cancel, model-authoritative rejection, caller-defined validation painting, editor geometry, and cleanup across reset/replacement/destruction; native and WASM builds pass |
| 1.7-C4 DataGrid delivery | Function | Public C++ API, PySide6 decision and coverage in the same release slice, C++/Python Gallery, WASM source/build/interaction acceptance | Complete — installed C++ API, PySide6 ownership coverage, three source-aligned examples, generated catalogs, and the 90-route WASM full smoke with DataGrid scroll/keyboard/edit checks are green |
| 1.7-Q Accessibility baseline | Quality | Repository-wide contract plus a risk-ordered audit; new or changed visible components cannot rely on visual labels alone | Complete — 69/69 inventory is machine-gated; nine focused gates close every recorded high- and medium-risk gap |
| 1.7-Q Representative visual gate | Quality | Small Light/Dark/RTL snapshot gate **or** keep the ledger + a concrete next-gate proposal if a full CI pixel diff is too large | Landed (local opt-in gate; no CI pixel job) |
| 1.7-COM Public contribution intake | Community | Contributor guide plus structured bug and feature forms with platform/surface/reproduction data | Complete |
| 1.7-P Fluent-only positioning | Product | Remove Material/Cupertino design-language and style-theme APIs, rendering branches, Gallery/Skill discovery, bindings, tests, docs, and assets | Complete |
| 1.7 closeout | Delivery | Manual C++/Python/WASM Gallery review plus native, PySide6, WASM, accessibility, AI asset, and representative visual gates | Complete |

## Non-goals

- WebView inside FluentQt UILib (library stays Qt Widgets only).
- Qt 5.15 EOL. Linux full + Windows API smoke remains the sampling strategy.
- Unifying overlay inheritance. Do not force DrawerView, ComboBox, SplitButton,
  DropDownButton, and Dialog onto one base class.
- Reimplementing AnchorLayout `sizeHint` or Tarjan cycle detection.
- Implementing Field or DataGrid in the overlay slice.
- Shipping a public DataGrid that creates a `QWidget`/`Field` for every cell or
  owns the caller's model, selection model, delegate, sorting, or persistence.
- Adding alternate design-language paint branches. Visual work targets the
  Fluent contract only.
- A CI pixel-diff job without a matching host. `VISUAL_SNAPSHOT=1` still writes
  PNGs under `build/<preset>/visual/` for migrated VisualCheck tests and only
  checks that a non-empty file was produced. The 1.7 representative gate
  compares three checked-in PNGs locally (`visual_gate`); hosted offscreen
  runners are not an approval host
  ([testing-workflow.md](testing-workflow.md)).

## Overlay slice acceptance (1.7-A)

Docs:

- [overlay-behavior.md](../architecture/overlay-behavior.md) defines public
  `isOpen` as **logical requested state**, distinct from animation-complete
  (`opened` / `closed`) and `QWidget` visibility.
- Old names stay as aliases (`aboutToShow` / `aboutToHide`, `setSmokeEnabled`,
  TeachingTip `CloseReason`). Breaking removals wait for a major version.

API / behavior:

- Popup, Flyout, Dialog, ContentDialog, TeachingTip share the observable
  state machine and NOTIFY + no-op rules.
- `modal`, `dim`, and `closePolicy` are orthogonal. Dialog's `setSmokeEnabled`
  remains the historical modal+dim bundle.
- `fluent::overlay::OverlayCoordinator` is not installed and is not app API.
- Same-window overlay model only (no native `Qt::Dialog` / `Qt::Tool` windows).
- SplitButton / DropDownButton keep menu `isOpen`; they are not overlay
  state-machine participants.

Tests (existing modules, `Contract_*` names so CTest gets the `contract` label):

- isOpen vs visibility vs animation-disabled sync settle
- Opening / Open / Closing / Closed order, including aliases
- Re-entrancy: open while opening, close while closing, open while closing
- Close reasons
- NOTIFY on real changes; silence on no-ops
- modal / dim / closePolicy orthogonality
- Compatibility aliases still work
- Theme change does not mutate open state

VisualCheck, if touched, keeps `SKIP_VISUAL_TEST` and `qApp->exec()`.

## Field slice acceptance (1.7-B)

- `Field` remains a composition shell. It presents label, required, helper,
  validation, focus, and accessibility state without taking ownership of an
  editor's value or rewriting its text.
- C++ has 18 passing `Contract_*` tests. Owned, borrowed, and reparented PySide6
  editors also pass isolated garbage-collection stress tests.
- The C++ and Python Galleries expose 69 entries, 90 routes, and 205 samples;
  Field contributes three source-aligned live examples and a registered 72 px
  layout icon.
- The generated AI catalog contains 69 guided components and 205 samples. Its
  generation, retrieval evaluation, and asset validation pass.
- Focused Light, Dark, narrow, minimum-width, and wrapped-content evidence
  passes the `build-fluentqt-gui` lite visual contract. Field delegates focus
  and disabled editor visuals; the contract suite verifies those transitions
  do not replace or mutate the slotted editor.
- The full Python Gallery suite passes, including the Window sample source
  parity checks for `window-content-host` and `window-custom-titlebar`.

## DataGrid execution (1.7-C)

DataGrid is intentionally split so the repository does not expose an unstable
large API just to show a table in Gallery:

1. **C0 contract and prototype — complete:** the original non-installed
   [DataGrid contract](datagrid-api-proposal.md) uses a 100,000 × 20 counting
   model and proves that initial show, scroll, resize, cell widgets/editors,
   and caller-owned model/delegate lifetime stay structurally bounded. The
   prototype was not installed or bound until the C4 promotion gate.
2. **C1 read-only core — complete:** the private library implementation keeps
   models, delegates, selection models, header data, and business persistence
   caller-owned. Eight `Contract_*` tests and a full-profile component
   VisualCheck cover model changes, headers, empty state, selection/current,
   keyboard, tokens, disabled rendering, Fluent scrollbars, Light/Dark, RTL,
   minimum width, long localized content, and scroll end.
3. **C2 interaction and accessibility — complete:** nine additional
   contracts now cover resize/reorder/hide/show/sort model authority, native
   logical table/cell/header/focus/selection/action semantics, model-provided
   accessible text, empty-state description ownership, bounded large-model
   lookup, cell-cache invalidation across model mutation/replacement, and one
   reset event with same-model no-op silence. A public-Qt-only private adapter
   now adds view/model-authoritative read-only/editable state, preserves Qt's
   logical row-major hierarchy, and releases cached logical interfaces with the
   view. Matching bare `QTableView` and DataGrid probes did not produce a stable
   FluentQt-only Cocoa crash, so full OS hierarchy enumeration stays a manual
   release diagnostic. The private source compiles and links in native Qt 6 and
   the supported Qt 6.9.3 / Emscripten 3.1.70 build without Qt private headers
   or a browser route.
4. **C3 editing and validation — complete:** three additional contracts prove
   real delegate editors, cross-platform F2 activation, Enter/Tab commit,
   Escape cancel, model-authoritative rejection, caller-defined validation
   roles/painting, active-cell geometry, and editor cleanup across reset,
   replacement, and destruction. Field informs label/error presentation, but
   a Field widget is never instantiated for each inactive cell. All 155
   repository contract tests pass, and the implementation compiles in the
   supported native Qt 6.9.3 and Qt 6.9.3 / Emscripten 3.1.70 builds.
5. **C4 delivery — complete:** `DataGrid.h` is installed and exported through
   `<FluentQt/FluentQt.h>`; PySide6 ships the same model/view ownership boundary
   with runtime, manifest, garbage-collection, and Gallery coverage. C++ and
   Python expose the same three examples. The generated Gallery/AI catalogs
   report 69 components, 90 routes, and 205 samples. The Qt 6.9.3 / Emscripten
   3.1.70 Gallery builds and its 90-route full browser smoke verifies the live
   DataGrid route, large-model scrolling, keyboard selection, and delegate
   editing.

Wall-clock CI timing is not an MVP contract. Structural counters prove that
model access, editors, and retained objects scale with the viewport rather
than total row count; optional local benchmarks track regressions.

## Accessibility execution (1.7-Q)

The [Accessibility Contract](accessibility-contract.md) starts from existing
evidence in CommandBar, Field, InfoBadge, Toast, FlowView, navigation controls,
pickers, and Window. It does not claim that the rest of the library is already
covered.

1. **Complete:** inventory all 69 public visible components as native Qt
   semantics, augmented semantics, custom adapter, gap, or not applicable. The
   [machine-checked ledger](accessibility-inventory.md) currently records 22
   native, 9 augmented, 34 adapter, 0 gap, and 4 not-applicable components.
2. **Complete:** audit and gate the high-risk composite surfaces across
   navigation, collections, values, date/time pickers, overlays, scrolling,
   color input, and autocomplete.
3. Add focused `Contract_Accessibility*` tests when a component is changed;
   preserve caller-provided names and descriptions and emit events only after
   real state changes. **First gate complete:** CalendarView exposes logical
   Day/Month/Year tables, range/selection/focus state, actions, and no-op-safe
   events through a private adapter. **Second gate complete:** Breadcrumb,
   Pivot, SelectorBar, TabView, and PipsPager share private logical-item
   infrastructure for ordered children, selection, focus, actions, and
   effective-change-only events. TabView also exposes add, close, and reorder;
   PipsPager keeps every logical page available beyond the painted pip window.
   **Third gate complete:** ToggleSwitch, RatingControl, NumberBox, ProgressBar,
   and ProgressRing share a private value boundary for checkable, bounded,
   editable-text-preserving, determinate, and indeterminate semantics. Five
   focused contracts cover roles, bounds, state, actions, caller text ownership,
   and effective-change-only events. **Fourth gate complete:** SplitButton and
   ToggleSplitButton expose one `ButtonMenu` object with distinct primary and
   menu actions, checked and popup state, real keyboard paths, and no fake child
   widgets. Four focused contracts cover action separation, menu lifetime,
   caller text ownership, and changed-state-specific no-op silence. **Fifth gate
   complete:** Popup, Flyout, TeachingTip, and CoachMark expose stable
   pane/help-balloon roles, logical open/modal state, dismiss actions, target
   relations, announcements, and focus behavior through a shared private
   adapter. Five focused contracts cover caller content, Escape reasons,
   lifecycle events, focus return, and no-op silence; the Gallery intro tour
   adds step text and a trapped action focus order. **Sixth gate complete:**
   ColorPicker, DatePicker, TimePicker, AnnotatedScrollBar, and AutoSuggestBox
   expose color/value editors, pending picker columns, logical annotation links,
   and autocomplete popup relations through private adapters. Four focused
   contracts preserve caller text and editable-text behavior while covering
   keyboard actions and effective-change-only events. **Seventh gate complete:**
   DropDownButton, DrawerView, and ToolTip expose menu-button, pane, tooltip,
   expanded/modal, target-relation, dismissal, focus-return, and logical
   lifecycle semantics through three focused contracts without changing their
   public APIs.
   **Eighth gate complete:** FlipView exposes authored pages, a bounded current
   page, and orientation-aware navigation while filtering its paint-only
   overlay. SplitView exposes native pane subtrees plus focusable, bounded,
   keyboard-resizable grip children. Four focused contracts cover structure,
   value, action, focus, keyboard, and no-op event behavior without adding
   public API.
   **Ninth gate complete:** HyperlinkButton exposes link target, activation,
   and traversed state; InfoBar exposes notification text, severity,
   announcements, hosted actions, and dismissal; Shimmer exposes busy state
   independently of animation. Five focused contracts close the final three
   medium-risk gaps without adding public API or frame-level events.
4. Integrate the checklist into DataGrid C1-C3 instead of running a late
   accessibility rewrite.

## Delivery order

| Order | Work | Exit condition |
| --- | --- | --- |
| 0 | Close current 1.7 worktree | Complete — manual C++/Python/WASM Gallery review is accepted; Skill assets and normal release gates are green |
| 1 | Public contribution intake | `CONTRIBUTING.md` and issue forms collect reproducible platform and surface data |
| 2 | DataGrid C0 | Contract reviewed; private large-model prototype satisfies structural counters |
| 3 | DataGrid C1-C2 + accessibility | Read-only and interaction contracts pass without per-cell widgets |
| 4 | DataGrid C3-C4 | Editing, validation, public API, binding decision, Gallery, and WASM acceptance pass |
| 5 | Accessibility inventory + first focused gate | 69/69 visible components are classified and machine-gated; CalendarView logical table/action/event contracts pass |
| 6 | Shared navigation-selection accessibility | Breadcrumb, Pivot, SelectorBar, TabView, and PipsPager expose stable logical items, state, actions, selection, and no-op-safe events through five focused contracts |
| 7 | Composite-value accessibility | ToggleSwitch, RatingControl, NumberBox, ProgressBar, and ProgressRing expose stable value/state/action contracts through five focused tests |
| 8 | Split-action accessibility | SplitButton and ToggleSplitButton expose primary/menu actions, checked/popup state, and keyboard access through four focused tests |
| 9 | Transient-surface accessibility | Popup, Flyout, TeachingTip, and CoachMark expose open/modal state, dismissal, target relations, announcements, and focus behavior through five focused tests |
| 10 | Remaining high-risk accessibility gates | Complete — ColorPicker, DatePicker, TimePicker, AnnotatedScrollBar, and AutoSuggestBox expose stable private-adapter contracts; no high-risk inventory gaps remain |
| 11 | Menu-button and auxiliary-surface accessibility | Complete — DropDownButton, DrawerView, and ToolTip expose keyboard, state, relation, dismissal, lifecycle, and focus-return contracts |
| 12 | Collection paging and splitter accessibility | Complete — FlipView exposes page value/actions and authored content; SplitView exposes native panes plus keyboard-resizable grip values |
| 13 | Semantic-presentation accessibility | Complete — HyperlinkButton, InfoBar, and Shimmer expose link, notification, announcement, dismissal, and busy-state contracts; no inventory gaps remain |
| 14 | Fluent-only product decision | Deprecation contract, Gallery migration, documentation, Python warning, and cross-Agent Skill all name Fluent as the only supported visual language |

Keep repository-wide visual repaints and shell geometry work separate from the
now-frozen DataGrid model/view delivery so each change remains reviewable.

## Quality track

Snapshot PNG generation for migrated VisualCheck tests still has **no**
baseline/diff. 1.7-Q landed a **small representative** Light/Dark/RTL gate
instead of a CI screenshot farm:

- Three checked-in PNGs in [tests/visual-baselines/](../../tests/visual-baselines/README.md)
  (Button Light, Button Dark, TreeView RTL)
- Opt-in CTest label `visual_gate`; default CTest keeps `SKIP_VISUAL_TEST=1`
- Exact pixel compare on the macOS arm64 approval host; headless/offscreen skips
- No default-red GitHub Actions pixel job (font engine and `offscreen` diverge)

Commands and CI limits: [testing-workflow.md](testing-workflow.md)
Representative visual gate.

Do not start painting every control. Overlay or LineEdit variants can join this
allowlist later; they are not required to close 1.7-Q.

## How to validate

Final macOS closeout evidence (2026-08-17):

- C++ `local_full`: 1471/1471 tests passed. The Fluent-only cleanup retains 25
  focused behavior, no-op signal, state, and Light/Dark rendering contracts;
  platform-specific and interactive VisualCheck cases remained skipped by
  contract.
- PySide6: the binding rebuilt and all 95 registered tests passed.
- WASM: the 90-route full browser smoke passed, including DataGrid scrolling,
  keyboard selection, and delegate editing.
- Accessibility inventory: 69 components, zero gaps. AI assets validate at
  69 components and 205 samples. The representative visual baseline comparison
  passed on the local approval host.
- Manual C++, Python, and WASM Gallery acceptance was completed before the
  release worktree was split into reviewable commits.

Configure (needs `VCPKG_ROOT`):

```bash
cmake --preset vcpkg-osx
```

Build the overlay-focused test targets:

```bash
cmake --build --preset vcpkg-osx --parallel --target \
  test_popup test_flyout test_dialog test_content_dialog test_teaching_tip
```

Run with anchored CTest labels:

```bash
ctest --preset vcpkg-osx -L '^test_popup$' --output-on-failure
ctest --preset vcpkg-osx -L '^test_flyout$' --output-on-failure
ctest --preset vcpkg-osx -L '^test_dialog$' --output-on-failure
ctest --preset vcpkg-osx -L '^test_content_dialog$' --output-on-failure
ctest --preset vcpkg-osx -L '^test_teaching_tip$' --output-on-failure
```

Build and run the public DataGrid contracts:

```bash
cmake --build --preset vcpkg-osx --target test_data_grid --parallel
ctest --preset vcpkg-osx -L '^test_data_grid$' --output-on-failure
```

Linux sampling (full) and Windows API smoke stay on Qt 5.15 as today; see
[linux-workflow.md](linux-workflow.md).
