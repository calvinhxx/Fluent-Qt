# FluentQt 1.7 Roadmap

- Branch: `release/1.7.x`
- Status date: 2026-08-14

## Purpose

1.7 stabilizes **observable overlay semantics** across same-window surfaces, then
lands **Field** as a composition shell, then **DataGrid**. A parallel quality
track adds a small representative Light/Dark/RTL visual gate and a
design-language coverage ledger — not a half-broken CI pixel job.

This is not a Qt-version or WebView release. The reusable `FluentQt` library
still depends only on Qt Widgets. Qt 5.15 remains the current sampling strategy
(Linux full, Windows API smoke).

Related contracts:

- [Overlay Behavior](../architecture/overlay-behavior.md) — state machine, aliases, orthogonal axes
- [Component Contract Baseline](component-contract-baseline.md) — Phase 0/1 history and living matrix
- [Component API Audit](component-api-audit.md) — API-005 / API-011
- [Testing Workflow](testing-workflow.md) — CTest labels, VisualCheck, snapshot PNG limits
- [Design Language Coverage Ledger](../design-languages/coverage-ledger.md) — who branches on `themeDesignLanguage()`

## Status

| Phase | Track | Scope | Status |
| --- | --- | --- | --- |
| 1.7-A Overlay contract | Function | Document `isOpen`, Opening/Open/Closing/Closed, re-entrancy, close reasons, orthogonal `modal`/`dim`/`closePolicy`, NOTIFY + no-op, compatibility aliases | Complete |
| 1.7-A Overlay implementation | Function | Compatible API/behavior for Popup, Flyout, Dialog, ContentDialog, TeachingTip; coordinator stays internal | Complete |
| 1.7-A Overlay tests | Function | `Contract_*` coverage on existing `add_qt_test_module` targets | Complete |
| 1.7-A Stale layout docs | Function | Close `FND-LAYOUT-002` (AnchorLayout size hints + Tarjan cycle diagnostics already exist) | Complete |
| 1.7-A Quality ledger | Quality | Inventory which controls branch on design language vs color-only vs Fluent-only; propose the next visual gate | Complete — [coverage-ledger.md](../design-languages/coverage-ledger.md) (42 geometry / 28 color-only / 1 Fluent-only) |
| 1.7-B Field | Function | Composition shell: label, editor slot, helper/error, validation presentation, focus/a11y; reuse `WidgetOwnership`; not a new editor base class | Complete — C++, Gallery, PySide6, catalogs, ownership stress tests, and focused visual evidence are green. [field-api-proposal.md](field-api-proposal.md) |
| 1.7-C DataGrid | Function | After Field; large collection surface. Spec only until Field lands | Not started |
| 1.7-Q Representative visual gate | Quality | Small Light/Dark/RTL snapshot gate **or** keep the ledger + a concrete next-gate proposal if a full CI pixel diff is too large | Landed (local opt-in gate; no CI pixel job) |

## Non-goals

- WebView inside FluentQt UILib (library stays Qt Widgets only).
- Qt 5.15 EOL. Linux full + Windows API smoke remains the sampling strategy.
- Unifying overlay inheritance. Do not force DrawerView, ComboBox, SplitButton,
  DropDownButton, and Dialog onto one base class.
- Reimplementing AnchorLayout `sizeHint` or Tarjan cycle detection.
- Implementing Field or DataGrid in the overlay slice.
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
- The C++ and Python Galleries expose 68 entries, 89 routes, and 202 samples;
  Field contributes three source-aligned live examples and a registered 72 px
  layout icon.
- The generated AI catalog contains 68 guided components and 202 samples. Its
  generation, retrieval evaluation, and asset validation pass.
- Focused Light, Dark, narrow, minimum-width, and wrapped-content evidence
  passes the `build-fluentqt-gui` lite visual contract. Field delegates focus
  and disabled editor visuals; the contract suite verifies those transitions
  do not replace or mutate the slotted editor.
- The full Python Gallery suite passes, including the Window sample source
  parity checks for `window-content-host` and `window-custom-titlebar`.

## Subsequent proposal

### DataGrid (1.7-C)

Larger than Field. Track columns, virtualization, selection, and editing after
Field's ownership and validation presentation exist. Field now clears the
sequencing prerequisite; DataGrid design and implementation were not started
in this slice.

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

Linux sampling (full) and Windows API smoke stay on Qt 5.15 as today; see
[linux-workflow.md](linux-workflow.md).
