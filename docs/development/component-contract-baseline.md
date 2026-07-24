# Component Contract Baseline

- Date: 2026-07-24
- Baseline: `release/1.4.x` at `a429e3d`

## Purpose

This document records the Phase 0 contract baseline and the Phase 1 behavior
fixes accepted against it. Phase 0 added executable evidence without changing
component behavior. Phase 1 repaired the confirmed foundation, text-field,
overlay, elevation, and resource-startup defects and activated their acceptance
tests without broadening the public API migration scope.

Future known-gap tests may still use `DISABLED_Contract_*` while their desired
behavior is under implementation. Phase 1 leaves no disabled contract test in
the current suite.

## Test Labels

- `contract`: active component acceptance tests.
- `known_contract_gap`: reserved for disabled target-behavior tests. The Phase 1
  suite currently has none.
- `local_full`: active, non-visual tests only.
- `ci_full`: the curated cross-platform set.

Build and run the active contract baseline:

```bash
cmake --build --preset vcpkg-linux --target fluent_qt_contract_tests --parallel
ctest --preset vcpkg-linux -L '^contract$' -LE '^known_contract_gap$' --output-on-failure
```

Run one acceptance contract explicitly:

```bash
./build/vcpkg-linux/tests/components/textfields/test_text_edit \
  --gtest_filter='TextEditTest.Contract_WidthReflowRecomputesVisibleLineHeight'
```

## Phase 1 Resolutions

| ID | Area | Accepted behavior | Active acceptance test | Status |
|---|---|---|---|---|
| `FND-STATE-001` | QMLPlus | Unknown state names do not become current state | `QMLPlusTest.Contract_InvalidStateDoesNotBecomeCurrentState` | Resolved |
| `FND-STATE-002` | QMLPlus | A transition restores properties absent from the next state | `QMLPlusTest.Contract_StateTransitionRestoresPropertiesAbsentFromNextState` | Resolved |
| `FND-LIFE-001` | QMLPlus | Destroyed state targets are removed from default-value storage | `QMLPlusTest.Contract_DestroyedStateTargetsAreRemovedFromDefaultStorage` | Resolved |
| `FND-LAYOUT-001` | AnchorLayout | Spacer and nested-layout items do not cause a null widget dereference | `AnchorLayoutTest.Contract_NonWidgetLayoutItemsDoNotCrashGeometryPass` | Resolved |
| `FND-LAYOUT-003` | AnchorLayout | Right and bottom edges use exclusive boundary coordinates so offsets equal the visible gap | `AnchorLayoutTest.Contract_RightAndBottomEdgesUseExactVisualBoundaries` | Resolved |
| `FND-LAYOUT-004` | AnchorLayout | Fill anchors keep direct widget geometry semantics while non-widget items receive layout-item geometry | `AnchorLayoutTest.Contract_FillOverridesWidgetSizePolicy` | Resolved |
| `FND-OVERLAY-001` | Overlay geometry | An oversized card anchors to the usable origin instead of using inverted clamp bounds | `FoundationContractsTest.Contract_OversizedOverlayCardUsesStableAvailableOrigin` | Resolved |
| `DSN-ELEV-001` | Elevation | `Elevation::None` has zero offset, blur, spread, and opacity | `FoundationContractsTest.Contract_ElevationNoneHasNoVisibleShadow` | Resolved |
| `TXT-LABEL-001` | Label | Qt meta-property writes and `Label::text()` report the same full text | `LabelTest.Contract_MetaPropertyTextWriteKeepsFullTextCoherent` | Resolved |
| `TXT-LABEL-002` | Label | Returning to the default color role removes only component-owned style | `LabelTest.Contract_DefaultTextColorRoleRemovesOwnedColorStyle` | Resolved |
| `TXT-EDIT-001` | TextEdit | Width-driven word wrapping recomputes visible-line height | `TextEditTest.Contract_WidthReflowRecomputesVisibleLineHeight` | Resolved |
| `TXT-EDIT-002` | TextEdit | Focusing through `QWidget*` forwards to the inner editor | `TextEditTest.Contract_BaseWidgetFocusForwardsToInnerEditor` | Resolved |
| `TXT-EDIT-003` | TextEdit | Reapplying current text is a no-op and preserves undo history | `TextEditTest.Contract_ReapplyingCurrentTextPreservesUndoHistory` | Resolved |
| `TXT-EDIT-004` | TextEdit | Visible-line bounds always satisfy `min <= max` | `TextEditTest.Contract_VisibleLineBoundsRemainOrdered` | Resolved |
| `RES-INIT-001` | Resources | Pre-application access does not permanently cache an empty resource result or failed font initialization | `ResourceInitializationTest.Contract_PreApplicationAccessDoesNotPoisonResourceInitialization` | Resolved |

## Deferred Foundation Decision

| ID | Area | Decision still required | Planned phase |
|---|---|---|---|
| `FND-LAYOUT-002` | AnchorLayout | Define item-derived size hints and deterministic cyclic-anchor diagnostics | Phase 2 |

## Active Guardrails Added in Phases 0 and 1

| Area | Guard |
|---|---|
| Property binding | An ordinary Qt property is synchronized initially and after its notify signal |
| State handling | Unknown states are rejected, transitions restore defaults, and destroyed targets are removed safely |
| AnchorLayout | Basic anchors resolve deterministically, right/bottom offsets equal visible gaps, fill preserves widget geometry, and non-widget layout items receive geometry safely |
| Overlay geometry | Normal cards clamp inside bounds and oversized cards use a stable usable origin |
| Elevation | `None` paints no visible shadow in either theme |
| Label | The derived setter, inherited getter, and derived meta-object writes stay coherent; caller style is preserved |
| TextEdit | Meta-properties, focus forwarding, wrapping height, line-bound ordering, and undo-preserving no-ops are guarded |
| Resource startup | Resource catalogs are safe before `QApplication`; font initialization retries after the GUI application exists |
| CI discovery | Contract and known-gap tests have separate CTest labels |
| Memory safety | Linux contract tests have an opt-in ASan/UBSan preset and scheduled lane |
| Compilation coverage | Weekly Linux x64 full validation builds every registered Qt/GTest target |

## Full UILib Review Matrix

The install-header allowlist is the public inventory source. Every category
below was reviewed against property/signal behavior, inherited Qt APIs,
ownership, focus/input, locale/RTL/accessibility, DPI/painting, and tests.

| Category | Public surface reviewed | Current disposition |
|---|---|---|
| Foundation | FluentElement, QMLPlus, AnchorLayout, ThemeRegistry, StyleThemeCatalog, overlay helpers | Phase 1 state/lifetime/non-widget layout/oversized overlay defects resolved; size hints, cycle diagnostics, theme transactions, and persistence remain Phase 2 |
| Basic input | Button, CheckBox, ColorPicker, ComboBox, DropDownButton, HyperlinkButton, RadioButton, RatingControl, RepeatButton, Slider, SplitButton, ToggleButton, ToggleSplitButton, ToggleSwitch | Base-class coherence, keyboard, RTL, and accessibility are Phase 2/3 decisions |
| Collections | DrawerView, FlipView, FlowView, GridView, ListView, SplitView, StackView, TreeView | Ownership/model/reorder contracts are Phase 2; large-model benchmarks are Phase 4 |
| Date and time | CalendarDatePicker, CalendarView, DatePicker, TimePicker | Locale ownership and atomic range updates are Phase 2/3 |
| Dialogs and flyouts | CoachMark, ContentDialog, Dialog, Flyout, Popup, TeachingTip | Overlay lifecycle/property semantics are Phase 2 |
| Menus and toolbars | FluentMenu, FluentMenuItem, MenuBar | Shared text-editing menu and direct menu contracts remain Phase 2 |
| Navigation | Breadcrumb, NavigationView, Pivot, SelectorBar, StackContentHost, TabView | Page ownership, global event filters, RTL, and accessibility are Phase 2/3 |
| Scrolling | AnnotatedScrollBar, PipsPager, ScrollBar, ScrollView | Empty-selection policy is intentionally unchanged; inherited API coherence is Phase 2 |
| Status and info | InfoBadge, InfoBar, ProgressBar, ProgressRing, Shimmer, ToolTip | Accessible values/copy, timer strategy, and facade coherence are Phase 2/3/4 |
| Text fields | AutoSuggestBox, Label, LineEdit, NumberBox, PasswordBox, TextEdit | Phase 1 Label/TextEdit coherence, focus, sizing, and no-op defects resolved; broader editing-facade and inheritance decisions remain Phase 2 |
| Windowing | TitleBar, Window, backdrop contracts | Content ownership, caption accessibility, and platform surface lifecycle are Phase 2/3 |
| Design | Animation, Breakpoints, CornerRadius, Elevation, IconCatalog, Material, Spacing, ThemeColors, Typography | Phase 1 zero-elevation semantics resolved; painter/DPI and header-cost work remains Phase 4 |

## Decisions Deliberately Deferred

These are not locked as 1.4.x behavior by Phase 0:

- Whether a zero-page `PipsPager` uses selected index `0` or `-1`.
- Whether TextEdit line-count sizing remains a fixed-height policy or becomes
  minimum/maximum size guidance.
- Whether ComboBox, ScrollView, StackView, Label, and picker classes keep their
  current Qt inheritance or move to composition.
- Common open-state semantics across Popup, Flyout, Dialog, ContentDialog,
  TeachingTip, and split buttons.
- Public ownership names and migration strategy for externally supplied widgets.

Locking these prematurely would turn an implementation accident into a public
contract. They require the Phase 2 API migration proposal.

## Exit Criteria for Phases 0 and 1

- The contract aggregate target builds on the current host.
- Active `contract` tests pass without a desktop session.
- Every Phase 1 acceptance test is active; the suite contains no
  `DISABLED_Contract_*` test.
- At least one Linux scheduled lane compiles every registered test binary.
- Windows and Linux run the active contract subset successfully.
- Linux ASan/UBSan runs the active contract subset without changing release
  builds.
- The affected Label/TextEdit/QMLPlus/AnchorLayout/resource modules pass their
  complete non-visual test sets.
- TextEdit focus, auto-height, overflow scrolling, window resizing, and
  light/dark preview behavior receive a real Gallery interaction check.
- Deferred API and AnchorLayout design decisions remain explicit rather than
  being silently locked by implementation accidents.
