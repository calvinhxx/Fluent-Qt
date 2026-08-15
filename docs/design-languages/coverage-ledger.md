# Design-language coverage ledger

Inventory of **public visible widgets** versus `FluentElement::themeDesignLanguage()`
(`DesignFluent` / `DesignMaterial` / `DesignCupertino`). This is a **1.7 quality-track
ledger**, not a paint plan. Do not restyle a control just because it appears here.

Style-theme switching still recolors everyone through `themeColors()` / radius / type.
The question is whether a control **changes shape, chrome, or interaction** when the
design-language enum changes.

Universe: widget headers in [`cmake/FluentQtInstallHeaders.cmake`](../../cmake/FluentQtInstallHeaders.cmake).
Umbrella headers, design tokens, compatibility helpers, and non-widget contracts
(`SelectionMode`, `EditingCommandRouter`, `FluentElement`, overlay geometry/shadow/window
helpers, `WindowBackdropMaterial`) are out of the count.

Evidence date: 2026-08-13. Method: `themeDesignLanguage()` / `DesignMaterial` /
`DesignCupertino` under `src/components/**`, then a paint-path read of every remaining
public widget.

Related: [README](README.md), [1.7 roadmap](../development/release-1.7-roadmap.md).

## Counts

| Bucket | Controls |
| --- | ---: |
| Geometry / interaction branch | 42 |
| Color / token only | 28 |
| Fluent-only (suppress, no replacement) | 1 |
| Not audited | 0 |
| **Public visible widgets** | **71** |

## How to read the buckets

| Kind | Meaning |
| --- | --- |
| **Geometry / interaction** | Paint (or an inherited/composed paint) reads `themeDesignLanguage()` and changes shape, chrome, or interaction — not just which token fills an unchanged outline. |
| **Color / token only** | Uses `themeColors()`, radius, type, or elevation. Switching Material/macOS recolors it. No shape/interaction branch. A few of these *do* call `themeDesignLanguage()` and only remap colors; they stay in this bucket. |
| **Fluent-only** | A language check whose non-Fluent path **hides** a Fluent affordance and does not paint a Material/macOS replacement in the library. |
| **Not audited** | Public visible widget whose paint path was not inspected. Empty this round. |

Inherited paint still counts as coverage for the public subclass (`RepeatButton` via
`Button`, `Flyout` via `Popup`, and so on).

## Highest-leverage gaps

Button family, text fields, and the Popup/Dialog overlay set already branch. The
first things users still see as Fluent-shaped under Material/macOS:

1. **NavigationView** — app shell. `paintEvent` uses `themeColorsRef()` only
   (`src/components/navigation/NavigationView.cpp`).
2. **TitleBar / Window** — every window. Token/backdrop fills
   (`TitleBar.cpp`, `Window.cpp`); no language enum.
3. **Card** — primary layout surface; `Expander` inherits it, `Accordion` composes
   `Expander` (`Card.cpp`). Header `Button`s on Expander still follow Button.
4. **Toast / ToolTip** — overlay-adjacent chrome. Token surfaces
   (`Toast.cpp`, `ToolTip.cpp`); Popup/Dialog already branch.
5. **Breadcrumb** — navigation chrome; `paintEvent` has no language enum
   (`Breadcrumb.cpp`).

Not in the top five, but in the same “users see it” set: `DrawerView`,
`ColorPicker` (spectrum is Fluent-radius; composed `LineEdit`/`Slider` already
branch), `GridView` / `FlowView`.

Do **not** start those paint paths from this ledger.

## Geometry / interaction branch (42)

| Control | Evidence |
| --- | --- |
| Button | `Button.cpp` — M3 pill + state layer; macOS bezel gradient |
| RepeatButton | via `Button` |
| ToggleButton | `ToggleButton.cpp` — outlined/tonal pill vs macOS bezel |
| CompoundButton | via `Button`; extra Fluent-only 0.5px press offset (`CompoundButton.cpp`) |
| DropDownButton | via `Button`; chevron tint per language (`DropDownButton.cpp`) |
| SplitButton | `SplitButton.cpp` — split surface / chevron |
| ToggleSplitButton | via `SplitButton` |
| HyperlinkButton | `HyperlinkButton.cpp` — M3 pill state layer; macOS hover underline |
| CheckBox | `CheckBox.cpp` — box shape + Fluent-only row hover fill |
| RadioButton | `RadioButton.cpp` |
| ToggleSwitch | `ToggleSwitch.cpp` |
| Slider | `Slider.cpp` — track thickness / thumb |
| ComboBox | `ComboBox.cpp` — field + chevron |
| LineEdit, TextEdit | `LineEdit.cpp`, `TextEdit.cpp` — underline vs outlined vs hairline+ring |
| PasswordBox, NumberBox, AutoSuggestBox | same field chrome; Fluent falls through (`PasswordBox.cpp`, `NumberBox.cpp`, `AutoSuggestBox.cpp`) |
| DatePicker, TimePicker | closed field + flyout (`DatePicker.cpp`, `TimePicker.cpp`) |
| CalendarView | day indicator (`CalendarView.cpp`) |
| CalendarDatePicker | via `Button`; popup is `Flyout` hosting `CalendarView` |
| Popup | overlay card stroke (`Popup.cpp`: M3 borderless, macOS `strokeStrong`, Fluent `strokeDefault`) |
| Flyout, CommandBarFlyout | via `Popup` (no local `paintEvent`) |
| Dialog, ContentDialog | overlay / dialog chrome (`Dialog.cpp`, `ContentDialog.cpp`) |
| TeachingTip, CoachMark | outline pen (`TeachingTip.cpp`, `CoachMark.cpp`) |
| ProgressBar, ProgressRing | dispatched painters (`ProgressBar.cpp`, `ProgressRing.cpp`) |
| InfoBar | container + badge vs glyph (`InfoBar.cpp`) |
| ScrollBar, PipsPager | thumb / pips (`ScrollBar.cpp`, `PipsPager.cpp`) |
| FluentMenu, FluentMenuBar | item / bar chrome (`Menu.cpp`, `MenuBar.cpp`) |
| CommandBar | bar fill + macOS bottom hairline (`CommandBar.cpp`) |
| Pivot, TabView | selection chrome (`Pivot.cpp`, `TabView.cpp`) |
| SelectorBar | M3/macOS segment fill; Fluent underline suppressed (`SelectorBar.cpp`) |
| ListView | delegate row fill per language (`ListView.cpp` `defaultRowSelectionFill`); Fluent accent pill suppressed |
| FlipView | surface + pips (`FlipView.cpp`) |

## Fluent-only (1)

| Control | Evidence |
| --- | --- |
| TreeView | `TreeView.cpp` hides the Fluent accent pill when `themeDesignLanguage() != DesignFluent`. Library has no M3/macOS replacement; Gallery samples do that in `app/view/widgets/samples/CollectionSampleDelegates.cpp`. |

`ListView` and `SelectorBar` also suppress a Fluent indicator off-Fluent, but they
**do** paint a Material/macOS replacement, so they stay in the geometry bucket.

## Color / token only (28)

Still Light/Dark correct via tokens. Switching style theme recolors them; it does
not change control shape.

| Area | Controls | Notes |
| --- | --- | --- |
| Input | ColorPicker, RatingControl | ColorPicker spectrum uses `themeRadius().control` (`ColorPicker.cpp`). RatingControl **calls** `themeDesignLanguage()` only to retint empty stars (`RatingControl.cpp`). |
| Layout | Card, Divider, Expander, Accordion, Field | `Card.cpp`; Expander is a `Card`; Accordion composes Expander; Field is color/token only (status colors from `themeColors()`, no `themeDesignLanguage()` branch) |
| Collections | GridView, FlowView, DrawerView, SplitView, StackView | `themeColorsRef()` fills; StackView has no paint of its own |
| Navigation | NavigationView, Breadcrumb, StackContentHost | `themeColorsRef()` / `bgLayer` |
| Windowing | Window, TitleBar, WindowBackdrop | backdrop / frame tokens |
| Status | Toast, ToolTip, Avatar, Shimmer, InfoBadge | InfoBadge **calls** `themeDesignLanguage()` to remap Attention→red on macOS and force white on-color (`InfoBadge.cpp`); geometry is unchanged |
| Scrolling | ScrollView, AnnotatedScrollBar | tokens only |
| Text | Label | `themeColorsRef()` |
| Foundation | FontIcon, OverlayScrim | `FontIcon.cpp`; OverlayScrim uses `themeSmoke()` |

## Intentionally out of scope for 1.7 painting

- Do not paint the color-only list because it appears here.
- Overlay 1.7-A already has a Popup/Dialog/TeachingTip language × theme stroke
  branch in those painters; that is evidence, not a new visual gate.
- DataGrid remains a function-track follow-up, not design-language work.

## Next-gate proposal (1.7-Q)

When a representative pixel gate is worth the baseline cost:

1. Keep Button pointer/focus/disabled Light/Dark (`ComponentStateMatrixVisualCheck`).
2. Add Popup (or ContentDialog) Light/Dark × LTR/RTL.
3. Add LineEdit rest/focus Light/Dark × LTR/RTL.
4. Check baselines into the repo; fail CI only against those names.
5. Do **not** invent a full-inventory diff job. `VISUAL_SNAPSHOT=1` today only
   checks that a non-empty PNG was written
   ([testing-workflow.md](../development/testing-workflow.md)).
