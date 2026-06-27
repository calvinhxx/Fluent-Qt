# Material 3 (Google) — Design Reference

Source of truth: **Material 3 Design Kit (Community)** — file `sfn7GB1zXX6Lu8hfhYqhbA`.
Token values below were pulled live from that file via the Figma MCP `get_variable_defs`
(see [figma-sources.md](figma-sources.md)). Everything here is the spec we port the
`DesignMaterial` branch of our controls onto.

> M3 is a **role-based** system: components never reference raw palette swatches, only
> semantic roles (`primary`, `on-primary`, `surface-container-highest`, `outline`, …).
> Our `ThemeCatalog` maps a handful of these roles onto `FluentElement::Colors`.

---

## 1. Color roles (extracted from Figma)

### Light scheme (`Schemes/*`)

| Role | Hex | Role | Hex |
|---|---|---|---|
| Primary | `#6750A4` | Surface | `#FEF7FF` |
| On Primary | `#FFFFFF` | Surface Dim | `#DED8E1` |
| Primary Container | `#EADDFF` | Surface Bright | `#FEF7FF` |
| On Primary Container | `#4F378A` | Surface Container Lowest | `#FFFFFF` |
| Secondary | `#625B71` | Surface Container Low | `#F7F2FA` |
| Secondary Container | `#E8DEF8` | Surface Container | `#F3EDF7` |
| On Secondary Container | `#4A4459` | Surface Container High | `#ECE6F0` |
| Tertiary | `#7D5260` | Surface Container Highest | `#E6E0E9` |
| Tertiary Container | `#FFD8E4` | On Surface | `#1D1B20` |
| Error | `#B3261E` | On Surface Variant | `#49454F` |
| Error Container | `#F9DEDC` | Outline | `#79747E` |
| On Error | `#FFFFFF` | Outline Variant | `#CAC4D0` |
| Inverse Surface | `#322F35` | Inverse Primary | `#D0BCFF` |
| Scrim / Shadow | `#000000` | Inverse On Surface | `#F5EFF7` |

### Dark scheme (`M3/sys/dark/*`)

| Role | Hex | Role | Hex |
|---|---|---|---|
| Primary | `#D0BCFF` | Surface | `#141218` |
| On Primary | `#381E72` | Surface Bright | `#3B383E` |
| Primary Container | `#4F378B` | Surface Container Lowest | `#0F0D13` |
| On Primary Container | `#EADDFF` | Surface Container Low | `#1D1B20` |
| Secondary | `#CCC2DC` | Surface Container | `#211F26` |
| Secondary Container | `#4A4458` | Surface Container High | `#2B2930` |
| Tertiary | `#EFB8C8` | Surface Container Highest | `#36343B` |
| Tertiary Container | `#633B48` | On Surface | `#E6E0E9` |
| Error | `#F2B8B5` | On Surface Variant | `#CAC4D0` |
| Error Container | `#8C1D18` | Outline | `#938F99` |
| On Error | `#601410` | Outline Variant | `#49454F` |
| Inverse Primary | `#6750A4` | Inverse Surface | `#E6E0E9` |

Reference tonal palette anchors (`M3/ref/*`): Primary40 `#6750A4`, Primary80 `#D0BCFF`,
Primary90 `#EADDFF`; Neutral10 `#1D1B20`, Neutral90 `#E6E0E9`; Neutral-Variant30 `#49454F`,
Neutral-Variant80 `#CAC4D0`.

### Mapping to `FluentElement::Colors` (what `ThemeCatalog` installs)

| Our token | Light | Dark |
|---|---|---|
| `accentDefault` | Primary `#6750A4` | Primary `#D0BCFF` |
| `textOnAccent` | On Primary `#FFFFFF` | On Primary `#381E72` |
| `accentSecondary`/`accentTertiary` | derived from accent (hover `.lighter`, press `.darker`) | same |
| `strokeStrong` / outline | Outline `#79747E` | Outline `#938F99` |
| `strokeSecondary` / outline-variant | `#CAC4D0` | `#49454F` |
| neutral surfaces | Surface Container family | Surface Container family |

> On Primary Container is `#4F378A` in this kit (the 2024 refresh), **not** the older `#21005D`.

---

## 2. Typography — **Roboto**

Confirmed from Figma variables; full scale is the standard M3 type scale.

| Style | Size / Line | Weight | Tracking |
|---|---|---|---|
| Display Large | 57 / 64 | 400 | -0.25 |
| Headline Medium | 28 / 36 | 400 | 0 |
| Title Medium | **16 / 24** | **500 (Medium)** | **0.15** |
| Body Medium | **14 / 20** | **400 (Regular)** | **0.25** |
| Label Large (buttons) | 14 / 20 | 500 | 0.1 |
| Label Small | **11 / 16** | **500** | **0.5** |

Button/label text uses the **Label** styles (Medium weight). Body copy uses **Body**.

> **Font:** Roboto is the M3 text face *by spec*, but the app deliberately does **not** bundle or
> override fonts — every brand renders in the bundled **Segoe UI Variable**. Treat Roboto here as
> the design reference only; a user may opt in via `typography.family` in their theme JSON if Roboto
> is installed locally.

---

## 3. Shape (corner radius scale)

`Corner/Extra-large = 28` confirmed from Figma. Full scale:

| Token | dp |
|---|---|
| None | 0 |
| Extra-small | 4 |
| Small | 8 |
| Medium | 12 |
| Large | 16 |
| Extra-large | 28 |
| Full (stadium) | half of the shorter side |

`ThemeCatalog` installs Material radius **8 (control) / 12 (overlay)**; individual controls
that are conceptually "full" (Button pill, Switch track) compute `radius = h/2` directly.

---

## 4. State layers — the defining M3 interaction

M3 does **not** swap the fill on hover/press. It paints a translucent **state layer** of the
*content* color on top of the resting fill:

| State | Opacity |
|---|---|
| Hover | **8 %** (`0x14`) |
| Focus | **10 %** (`0x1A`) |
| Pressed | **10 %** (`0x1A`) |
| Dragged | 16 % (`0x29`) |

- Filled surfaces → state layer is the **on-color** (`textOnAccent`).
- Outlined / text surfaces → state layer is the **accent** (`primary`) color.
- Selection controls (checkbox/radio/switch) → a **40 dp circular** state-layer halo behind
  the control, colored `primary` when selected / `on-surface` when not.

This adapts to light **and** dark automatically (the on-color is theme-correct), which is why
the M3 branch must use the state layer, not a `.darker()`/`.lighter()` nudge.

---

## 5. Component specs (porting targets)

### Button — ![M3 buttons](images/material3-buttons.png)
- Five types: **Elevated / Filled / Filled-tonal / Outlined / Text**.
- Standard height **40 dp**, shape **Full pill** (`radius = h/2`), label = Label Large (Medium).
- Our mapping: `Accent → Filled` (primary fill, on-primary text); `Standard → Outlined`
  (1 dp `outline` stroke, primary text, transparent fill); `Subtle → Text` (primary text,
  no fill/border). State layer per §4.

### DropDownButton — ![M3 menu](images/material3-menu.png) · ![M3 buttons](images/material3-buttons-full.png)
- A standard M3 button **surface** (Filled / Outlined / Text per `Style`, exactly like Button)
  carrying a **trailing dropdown arrow** in `on-surface-variant`.
- Click opens an **M3 menu**: a `surface-container` rounded panel at **overlay radius 12 dp**
  with elevation; items take a **state layer** on hover/selected (per §4), never a fill swap.
- The arrow sits after the label with a small gap; the resting button keeps Button's height
  (40 dp) and pill shape.
- Our mapping: DropDownButton reuses the `DesignMaterial` Button surface per `Style` and appends
  the `on-surface-variant` chevron; the flyout menu uses the overlay radius + M3 state-layer
  item highlight.

### ToggleButton
- **Unchecked** reads as an **Outlined** button (1 dp `outline` stroke, primary text, transparent
  fill). **Checked** flips to **Filled-tonal**: `secondary-container` fill + `on-secondary-container`
  text.
- Hover/press always use the **state layer** (per §4) clipped to the pill — the checked/unchecked
  distinction is the *resting* fill, never a hover fill swap.
- Same 40 dp height + Full pill as Button; label = Label Large (Medium).
- Our mapping: `DesignMaterial` ToggleButton paints the Outlined surface when off and the
  Filled-tonal (`secondary-container` / `on-secondary-container`) surface when on, with the
  state-layer veil over the top in both states.

### SplitButton (+ ToggleSplitButton)
- The M3 **split-button** idea: a single button **surface** (Filled / Outlined / Text per `Style`)
  divided into an **action segment** + a **trailing dropdown-arrow segment** by a **hairline
  divider** (`outline-variant`).
- Each segment carries its **own state layer** on hover/press (per §4), so the two halves light
  independently; the arrow segment opens the same M3 menu (`surface-container`, radius 12).
- ToggleSplitButton inherits SplitButton; its action segment follows the **ToggleButton** rule
  above (Outlined when off → Filled-tonal when on) while the arrow segment stays the surface style.
- Our mapping: `DesignMaterial` SplitButton renders the Button surface per `Style`, inserts the
  `outline-variant` hairline between the two segments, and applies the state layer per segment;
  ToggleSplitButton layers the toggle fill onto the action half.

### HyperlinkButton
- Maps to the M3 **Text button**: **primary/accent** text, **no resting fill or border**.
- Hover/press paint an **accent state-layer pill** (the `primary` color veil at 8/10 %, per §4)
  behind the label — the only chrome the control ever shows.
- Height + label follow Button (Text type); the link reads as a tappable accent label, not a box.
- Our mapping: `DesignMaterial` HyperlinkButton renders the Text-button surface — `accentDefault`
  text, transparent rest, accent state-layer pill on hover/press.

### Switch — ![M3 switch](images/material3-switch.png)
- Track **52 × 32 dp**, fully rounded (`radius 16`).
- **OFF**: track = `surface-container-highest` fill **+ 2 dp `outline` border**; thumb = **16 dp**
  diameter, colored `outline` (mid-gray). Optional ✕ icon.
- **ON**: track = `primary` fill (no border); thumb = **24 dp** diameter, colored `on-primary`
  (white). Optional ✓ icon. The thumb **grows 16 → 24** as it travels; pressed → ~28 dp.
- 40 dp state-layer halo behind the thumb on hover/focus/press.

### Checkbox — ![M3 checkbox](images/material3-checkbox.png)
- Box **18 × 18 dp**, corner radius **2 dp**.
- **Unchecked**: 2 dp `on-surface-variant` outline, transparent fill.
- **Checked / Indeterminate**: `primary` fill + `on-primary` glyph (✓ / dash).
- 40 dp circular state-layer halo. (Error variant = same in `error` red — not needed for the gallery.)

### Radio button — ![M3 radio](images/material3-radio.png)
- Outer ring **20 dp**, **2 dp** stroke.
- **Selected**: ring + inner dot both `primary`; dot ≈ **10 dp** (50 % of inner area).
- **Unselected**: `on-surface-variant` ring, no dot.
- 40 dp state-layer halo.

### Slider — ![M3 slider](images/material3-sliders.png)
- **New M3 "expressive" slider** (the kit's current design): a **thick track** (≈16 dp at the
  medium size; scales 4 → 40 by density), active portion = `primary`, inactive = `secondary-container`.
- The handle is a **vertical bar/pill** (≈4 dp wide, taller than the track, fully rounded),
  **not** a round knob, with a small **gap** between the handle and each track segment.
- Stop-indicator dots sit at the track ends; state-layer halo on the handle when active.
- *Pragmatic port:* thicken the track and tint inactive with `secondary-container`; a vertical
  bar handle is the distinctive cue. A round `primary` handle is an acceptable interim if the
  bar handle is too costly, but the thick two-tone track is the minimum.

### Text field (LineEdit/TextBox) — ![M3 text fields](images/material3-textfields.png)
- Two variants: **Filled** and **Outlined**.
- **Filled**: `surface-container-highest` fill, **rounded TOP corners only** (square bottom), and
  a **bottom underline** — 1 dp `on-surface-variant` at rest → **2 dp `primary` on focus**.
- **Outlined**: transparent fill inside a **1 dp `outline` rounded rect** → **2 dp `primary`
  outline on focus**; the floating label **notches into the top edge** of the outline when raised.
- Both carry an optional **label**, **supporting text** below, and **leading/trailing icons**
  (`on-surface-variant`). Error state recolors outline/underline, label, supporting text and the
  trailing icon to `error` red.
- Our mapping: the port uses the **Outlined** variant — a rounded rect with a **1 dp `strokeStrong`**
  border that thickens to **2 dp `accentDefault`** on focus. Placeholder/leading-trailing affordances
  follow the same role mapping; error = the `error` token.

### Multi-line text field (TextEdit)
- A multi-line text box gets the **identical** treatment to the single-line Text field above:
  the **Outlined** frame — a rounded rect with a **1 dp `strokeStrong`** border that thickens to
  **2 dp `accentDefault`** on focus — only taller, with the text wrapping over several rows.
- Our mapping: TextEdit reuses the Text-field frame/focus spec verbatim; no separate visual rules.

### Search / AutoSuggestBox — ![M3 search](images/material3-search.png)
- M3 **Search** is a **rounded "search bar"**: a filled `surface-container-*` container with a
  generous (often fully rounded) radius, a **leading search/menu icon** (`on-surface-variant`),
  and **trailing affordances** (clear ✕, mic, or an avatar). The docked/full-screen variants drop
  a suggestion list below the bar.
- Our mapping: `AutoSuggestBox`, `NumberBox` and `PasswordBox` all **subclass LineEdit**, so they
  **inherit** the M3 Outlined text-field frame (rounded rect, `strokeStrong` → `accentDefault` on
  focus) rather than redefining it. Only their **trailing affordances** are control-specific — the
  search/clear glyph (AutoSuggestBox), the spin buttons (NumberBox), or the reveal toggle
  (PasswordBox) — laid over the shared frame.

### ComboBox — ![M3 menu](images/material3-menu.png)
- **Closed field** reads as an **M3 Outlined text field**: rounded rect, **1 dp `outline`** →
  **2 dp `primary` on focus/open**, with a **trailing dropdown arrow** in `on-surface-variant`.
- **Popup menu**: a `surface-container` rounded panel (overlay radius **12 dp**) with **elevation**;
  list items get a **state layer** on hover/selected per §4 (selected/hover veil over the item row).
- Our mapping: closed field reuses the Outlined text-field treatment (`strokeStrong` → `accentDefault`
  on open) plus the trailing chevron; the popup uses the overlay radius and the M3 state layer for
  item highlight rather than a fill swap.

### Tabs (Pivot) — ![M3 tabs](images/material3-tabs.png)
- **Active tab**: `primary` text **+ a `primary` bottom indicator bar** — rounded, ~2–3 dp tall,
  spanning the label width.
- **Inactive tab**: `on-surface-variant` text, no indicator.
- **Hover**: a subtle `primary` **state layer** over the tab (per §4). An optional **icon** sits
  above or beside the label.
- Our mapping: the `DesignMaterial` Pivot draws the moving `primary` underline indicator under the
  selected label, recolors active text to `accentDefault`, and uses the state-layer veil on hover —
  not a pill or fill behind the tab.

### SelectorBar (segmented selector) — ![M3 tabs](images/material3-tabs.png)
- The M3 analog of a SelectorBar is the **segmented button** group: a **rounded container** of
  equal **rounded segments**.
- **Selected segment**: `secondary-container` / tinted-accent fill + **accent text** (often with a
  leading ✓). **Unselected segment**: **transparent** fill + `textSecondary` (`on-surface-variant`).
- **Hover/press**: a subtle **state layer** (per §4) over the segment, not a fill swap.
- Our mapping: the `DesignMaterial` SelectorBar renders as segmented buttons — rounded container/
  segments, the active segment filled `secondary-container` with `accentDefault` text, the rest
  transparent with `textSecondary`, hover veiled via the state layer.

### TabView (document tabs)
- Document-style tabs are **not** a native M3 pattern, so the port is a tasteful adaptation of the
  M3 **Tabs/Pivot** language above.
- **Selected tab**: `primary` (`accentDefault`) text **+ a rounded `primary` bottom indicator**;
  **inactive tabs**: `on-surface-variant` (`textSecondary`) text, no indicator. Hover uses the
  state-layer veil.
- Our mapping: TabView reuses the Tabs spec — active text `accentDefault` over the rounded `primary`
  underline — rather than inventing a new chrome for the document tab strip.

### ProgressBar (linear) — ![M3 progress](images/material3-progress.png)
- M3 "Linear progress indicator" (expressive update): a **~4 dp track**, **fully rounded** ends. The
  **active indicator** is `primary` (`accentDefault`); the **track** is `secondary-container` (we tint
  `accentDefault` at low alpha, falling back to `controlSecondary`). Determinate leaves a **gap (~4 dp)**
  between the active end and the remaining track, and pins a **stop dot** — a small `primary` circle — at
  the far (trailing) end of the track. Indeterminate animates a travelling rounded segment.
- Our mapping: `DesignMaterial` ProgressBar paints a rounded track, a rounded `accentDefault` active
  segment with a ~4 px gap before the remaining track, and a `accentDefault` stop dot at the trailing end;
  `showPaused → systemCaution`, `showError → systemCritical`. (The wavy active track is out of scope — the
  **gap + stop dot** are the signature M3 cue.)

### ProgressRing (circular) — ![M3 progress](images/material3-progress.png)
- M3 "Circular progress indicator": an **arc** in `primary` with **rounded caps**, ~4 dp stroke, on a light
  `secondary-container` track ring. Determinate adds the same **gap + stop dot** (a small `primary` dot at
  the arc's leading end); indeterminate spins the arc.
- Our mapping: `DesignMaterial` ProgressRing strokes a light track ring plus a rounded-cap `accentDefault`
  arc with a small leading stop dot; paused/error recolor the arc, matching the Fluent value semantics.

### InfoBar (M3 ≈ Snackbar / Banner) — ![M3 snackbar](images/material3-snackbar.png)
- M3 has no "InfoBar"; the nearest filled status container is the **Snackbar/Banner** — a rounded
  (**Corner Medium ≈ 12**) tonal container with a leading icon + supporting text and **no heavy left accent
  strip**.
- Our mapping: `DesignMaterial` InfoBar drops the Fluent left accent bar and renders a **rounded-12 tonal
  container** filled with the severity's `systemXxxBg`, the icon (and title) tinted `systemXxx`, body text
  `textPrimary`, border suppressed (the tonal fill carries the severity).

### InfoBadge — ![M3 badges](images/material3-badge.png)
- M3 "Badge": a **small badge** = a 6 dp **dot**; a **large badge** = a **fully rounded** pill (~16 dp tall)
  holding a white number. M3's default badge color is **error red** (`#B3261E` / on-error white), but a
  badge may carry any semantic color.
- Our mapping: `DesignMaterial` InfoBadge renders **fully rounded** (radius = height / 2), filled with the
  badge's semantic color (attention → `accentDefault`, otherwise the matching `systemXxx`), value text in
  white (`textOnAccent`); the dot variant is a bare filled circle.

### CalendarView / CalendarDatePicker — ![M3 date picker](images/material3-datepicker.png)
- M3 date picker calendar: each day is a **circular** (~40 dp) target. **Selected day** = a filled
  `primary` (`accentDefault`) circle with `on-primary` (`textOnAccent`) text; **today** = a 1 dp `primary`
  (`accentDefault`) **outline ring** with normal text; other days plain `on-surface`. Hover/press use a
  **circular state layer** over the day cell. The surrounding container is rounded (Corner Large/XL).
- Our mapping: `DesignMaterial` CalendarView draws the selected-day indicator as a filled `accentDefault`
  circle + `textOnAccent` glyph, today as an `accentDefault` outline ring, and day hover as the M3 circular
  state layer **inscribed in the cell** (an oversized halo clips into a hard band — see the batch-1 lesson).
  `CalendarDatePicker` is a `Button` subclass, so its closed field inherits the M3 Outlined/Filled surface
  automatically; only the popup (CalendarView) needs the branch.

### DatePicker / TimePicker (spinner pickers) — ![M3 time picker](images/material3-datepicker.png)
- M3's native pickers are a **modal calendar** (date) and a **clock dial / numeric input** (time) — NOT
  WinUI's looping spinner columns — so the spinner port is a tasteful adaptation in the M3 idiom rather
  than a literal clock. M3's selection cue is a `secondary-container` / `primary` rounded highlight behind
  the selected value with accent text; the closed field is the Outlined text-field treatment.
- Our mapping: `DesignMaterial` DatePicker/TimePicker paint each loop column's **selected-row highlight** as
  a rounded `accentDefault`-tinted pill with accent text, and the closed field reuses the Outlined field
  surface (1 dp outline → `accentDefault` on focus/open). Row hover uses the state layer.

### Menu (FluentMenu) — ![M3 menu](images/material3-menu.png)
- M3 **menu**: a `surface-container` rounded panel at **overlay radius 12 dp** carried by **elevation**
  (shadow), **not** a 1 dp border. Items take a **state layer** (on-surface veil) on hover/active —
  never a fill swap; a checked item shows a **leading checkmark**, a submenu a trailing chevron.
- Our mapping: `DesignMaterial` FluentMenu drops the card border (`Qt::NoPen`, elevation via the existing
  layered shadow), keeps the `bgLayer` panel, and renders the active/hover/checked item highlight as a
  **neutral on-surface state layer** (`veil` ≈ 8 %) inscribed in the item's rounded rect, leaving text
  `textPrimary`/`textSecondary`. Checkmark + chevron unchanged.

### MenuBar (FluentMenuBar) — ![M3 toolbar](images/material3-toolbar.png)
- M3 has no classic desktop menu bar; the nearest idiom is the **docked/floating toolbar** — a
  `surface-container` strip whose items react with a **state layer**, never an accent fill.
- Our mapping: `DesignMaterial` FluentMenuBar paints each title's hover as a neutral on-surface state layer
  (`veil` ≈ 8 %) and pressed/open as a deeper veil (≈ 12 %), text stays `on-surface` — no accent fill bar.

### ContentDialog — ![M3 dialog](images/material3-dialog.png)
- M3 **basic dialog**: a single **`surface-container-high`** rounded card (**Corner Extra Large ≈ 28 dp**)
  with a headline, supporting text, and **text buttons at the bottom-right** — **no divider** splitting a
  separate button bar, and elevation rather than a stroke. A scrim dims the content behind.
- Our mapping: `DesignMaterial` ContentDialog fills the **whole card with one `bgLayer` surface** (no
  `bgCanvas` button region, no `strokeDivider` line) and draws **no outer border** (`Qt::NoPen`), relying on
  the existing shadow for elevation. (Button widgets/layout are unchanged; only the painted background is.)

### Flyout / Popup (shared card) — ![M3 menu](images/material3-menu.png)
- M3 flyout/menu surfaces are **elevated `surface-container`** panels: rounded (overlay radius 12 dp),
  shadow-borne, **borderless**. The same panel backs the **ComboBox dropdown** (which inherits the shared
  `Popup` card) and the gallery **Flyout**.
- Our mapping: `DesignMaterial` `Popup::paintEvent` keeps the `bgLayer` fill + overlay radius + layered
  shadow but drops the visible border (`Qt::NoPen`), so every base-`Popup` consumer reads as an M3 elevated
  surface.

### TeachingTip — ![M3 tooltip](images/material3-tooltip.png)
- M3 **rich tooltip**: a `surface-container` rounded panel with a Title (`on-surface`) + supporting text
  (`on-surface-variant`) + optional text actions, carried by **elevation** with **no border**. (M3's *plain*
  tooltip is the small dark `inverse-surface` pill — that maps to a Tooltip, not the TeachingTip callout.)
- Our mapping: `DesignMaterial` TeachingTip keeps the `bgLayer` bubble (+ tail + shadow) but draws **no
  outline stroke** (`Qt::NoPen`), matching the M3 rich-tooltip elevation-only edge.

### Dialog (base modal) — ![M3 dialog](images/material3-dialog.png)
- `Dialog` is the base class of `ContentDialog`; it gets the same M3 **basic-dialog** treatment — a single
  `surface-container-high` tonal card, **no divider**, **borderless** (elevation via shadow), scrim behind.
- Our mapping: `DesignMaterial` Dialog fills the card with one `bgLayer` surface and draws no outer stroke
  (`Qt::NoPen`), mirroring the ContentDialog branch.

### CoachMark — ![M3 tooltip](images/material3-tooltip.png)
- CoachMark is a callout-with-beak, the sibling of TeachingTip; M3 carries it as an **elevated
  `surface-container`** with **no border** (the rich-tooltip idiom).
- Our mapping: `DesignMaterial` CoachMark keeps the `bgLayer` bubble + beak + shadow and drops the outline
  stroke (`Qt::NoPen`).

### ScrollBar / ScrollView / AnnotatedScrollBar
- M3's kit has **no standalone scrollbar component** (scrolling is gesture-first); the platform scrollbar is
  a **thin rounded thumb** in an `on-surface` tone with **no rail at rest**, widening on hover.
- Our mapping: `DesignMaterial` ScrollBar paints the thumb as a neutral on-surface **veil** (theme-aware:
  white-on-dark / black-on-light) with the rail suppressed at rest; AnnotatedScrollBar keeps its
  `accentDefault` position indicator. Geometry/animation unchanged.

### PipsPager
- M3 page indicators: a row of dots, the **active dot filled `primary`** (`accentDefault`), inactive dots a
  smaller neutral `on-surface-variant`; the prev/next affordances use a **state layer** on hover/press.
- Our mapping: `DesignMaterial` PipsPager fills the selected pip `accentDefault`, inactive pips neutral, and
  the nav buttons take an inscribed neutral on-surface state layer (`veil` 8 %/12 %), never an accent fill.

### RatingControl
- M3 has no first-party star rating; star ratings render as filled `primary` glyphs over a neutral
  `on-surface-variant` outline.
- Our mapping: `DesignMaterial` RatingControl keeps filled stars `accentDefault` and tones the unfilled stars
  to a neutral on-surface color; hover preview uses the theme-aware veil.

### FlipView
- A carousel: bottom **pips** + prev/next nav buttons. M3 → active pip `primary`, inactive neutral; nav
  buttons take a neutral **state layer** (no accent fill).
- Our mapping: `DesignMaterial` FlipView fills the active pip `accentDefault`, inactive pips neutral, and the
  circular nav buttons take an inscribed neutral state layer. (Dark-mode fills come only from valid seeded
  tokens / veils — guarding the historical white-nav-button-in-dark regression.)

### Collections — ListView / TreeView / GridView / FlowView — ![M3 lists](images/material3-lists.png)
- M3 **list item**: leading icon/avatar + headline + supporting text; the **selected** item is a rounded
  **`secondary-container`** tonal fill with `on-secondary-container` text (NOT a left accent bar); hover/press
  add an on-surface **state layer**. M3 has no Fluent-style animated left **pill** indicator.
- Collections in this app are **two-layered**: the gallery item *delegate*
  (`app/view/widgets/samples/CollectionSampleDelegates.cpp`) paints the row/tile fill + text, and the *view*
  (`src/components/collections/*`) paints an extra Fluent accent **pill/overlay** indicator on top.
- Our mapping: under `DesignMaterial` the **delegate** renders the selected row as a tonal `accentDefault`
  wash (≈16 % light / 28 % dark) with `textPrimary` and hover as a neutral on-surface `veil`, while the
  **view suppresses its Fluent accent pill/overlay** (ListView `paintSelectedIndicator`, TreeView overlay
  indicator) so the tonal row is the sole selection cue. GridView/FlowView keep the accent
  drag-insertion indicator (accent is correct everywhere) and drop any redundant Fluent-only selection
  overlay. Image tiles keep the accent border + check affordance.

### Window chrome / TitleBar caption controls
- M3 is a **mobile/web-first** system and does not specify desktop window caption controls; a Material
  desktop shell (e.g. GTK/Material You on Linux) uses a conventional **trailing minimize/maximize/close**
  row. So under `DesignMaterial` the gallery TitleBar sample reuses the same **trailing caption-button
  row** as Fluent — each button is a `Subtle` `Button`, so it automatically paints in the **M3 idiom**
  (state-layer hover) via the Button branch. The leading **macOS traffic lights** are `DesignCupertino`-only.
- See [macos.md → Window chrome](macos.md#window-chrome--titlebar-caption-controls) for the full
  platform contrast and the `captionStyleForDesignLanguage()` mapping.

---

## 6. Quick checklist for the `DesignMaterial` branch

- [ ] Pills/stadiums where M3 is round (Button, Switch track).
- [ ] State **layer** (on-color veil at 8/10/10 %), never a fill swap.
- [ ] Switch: outlined OFF track + small gray thumb; filled ON track + bigger white thumb; thumb grows.
- [ ] Checkbox 2 dp radius; Radio 2 dp ring + concentric dot.
- [ ] Selection halos are circular and theme-correct (`primary` vs `on-surface`).
- [ ] Slider: thick two-tone track (`primary` / `secondary-container`).
- [ ] Text field / ComboBox: outlined rounded rect, 1 dp `outline` → **2 dp `primary` on focus**.
- [ ] ComboBox popup + Tabs hover use the **state layer**, not a fill swap.
- [ ] Tabs: `primary` text + rounded `primary` bottom indicator on the active tab.
- [ ] Menus / flyouts / dialogs / teaching tips are **borderless** elevated `surface-container` panels (drop the stroke; elevation = shadow). Dialog is one tonal surface, no button-bar divider.
- [ ] Menu + MenuBar item highlight = neutral **on-surface state layer**, never an accent fill bar.
