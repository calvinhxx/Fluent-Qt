# Fluent (Windows) — Design Reference

Source of truth: **Windows UI kit (Community)** — file `qpecbg7hOfos9DcHWeKlfw`.
Unlike the Material 3 and macOS pages, Fluent is **not** a porting target — it is the
**baseline this app already implements**. The values below are quoted directly from our own
design headers (`src/design/*.h`), which seed the runtime `ThemeRegistry`; the Figma kit was
the original measurement source and is kept as visual grounding (see
[figma-sources.md](figma-sources.md)).

> Fluent is **token-based**: controls read semantic `FluentElement::Colors`, `themeRadius()`,
> and `themeFont()` at runtime. For the Fluent brand those tokens **are** the seed —
> `ThemeRegistry::seedDefaults()` copies `ThemeColors::{Light,Dark}` straight into `Colors` and
> sets `DesignLanguage = DesignFluent`, with **no** override spec on top. Material 3 and macOS
> are deltas applied *over* this seed.

---

## 1. Color roles (from `src/design/ThemeColors.h`)

These are the canonical Fluent swatches: `ThemeRegistry::seedDefaults()` assigns each one to a
`FluentElement::Colors` field 1:1 (e.g. `Fill::AccentDefault → accentDefault`,
`Stroke::ControlStrong → strokeStrong`, `Text::Primary → textPrimary`).

### Light theme (`ThemeColors::Light`)

| Role | Value | Role | Value |
|---|---|---|---|
| `Fill::AccentDefault` | `#005FB8` | `BackgroundCanvas` | `#F3F3F3` |
| `Fill::AccentSecondary` | accent @ ~90 % (`0,95,184,230`) | `BackgroundLayer` | `#FFFFFF` |
| `Fill::AccentTertiary` | accent @ ~80 % (`0,95,184,204`) | `BackgroundLayerAlt` | `#F9F9F9` |
| `Fill::ControlDefault` | `#FFFFFF` | `BackgroundSolid` | `#EEEEEE` |
| `Stroke::ControlDefault` | black @ ~5 % (`0,0,0,12`) | `Text::Primary` | black @ ~90 % (`0,0,0,230`) |
| `Stroke::ControlStrong` | black @ ~44 % (`0,0,0,112`) | `Text::Secondary` | black @ ~60 % (`0,0,0,154`) |
| `Stroke::DividerDefault` | black @ ~8 % (`0,0,0,20`) | `Text::OnAccentPrimary` | `#FFFFFF` |
| `Stroke::FocusOuter` | black @ ~90 % (`0,0,0,230`) | `Text::AccentPrimary` | `#003E92` |
| `System::Critical` | `#C42B1C` | `System::Success` | `#0F7B0F` |
| `System::Caution` | `#9D5D00` | `System::Informational` | `#015CDA` |

### Dark theme (`ThemeColors::Dark`)

| Role | Value | Role | Value |
|---|---|---|---|
| `Fill::AccentDefault` | `#60CDFF` | `BackgroundCanvas` | `#202020` |
| `Fill::AccentSecondary` | accent @ ~90 % (`96,205,255,230`) | `BackgroundLayer` | `#2C2C2C` |
| `Fill::AccentTertiary` | accent @ ~80 % (`96,205,255,204`) | `BackgroundLayerAlt` | `#3D3D3D` |
| `Fill::ControlDefault` | white @ ~6 % (`255,255,255,15`) | `BackgroundSolid` | `#1C1C1C` |
| `Stroke::ControlDefault` | white @ ~7 % (`255,255,255,17`) | `Text::Primary` | `#FFFFFF` |
| `Stroke::ControlStrong` | white @ ~54 % (`255,255,255,138`) | `Text::Secondary` | white @ ~78 % (`255,255,255,199`) |
| `Stroke::DividerDefault` | white @ ~8 % (`255,255,255,20`) | `Text::OnAccentPrimary` | `#000000` |
| `Stroke::FocusOuter` | white @ ~90 % (`255,255,255,230`) | `Text::AccentPrimary` | `#99EBFF` |
| `System::Critical` | `#FF99A4` | `System::Success` | `#6CCB5F` |
| `System::Caution` | `#FCE100` | `System::Informational` | `#60CDFF` |

> **Headline accents:** Light `#005FB8` (a deep WinUI blue), Dark `#60CDFF` (a bright cyan).
> Note the polarity flip in `OnAccentPrimary` — **white** text on the light accent, **black** on
> the dark accent — because the dark accent is light enough to need dark text.

The neutral `Grey10…Grey200` ramp (e.g. `Grey10 #FAF9F8`, `Grey130 #605E5C`, `Grey160 #323130`,
`Grey190 #201F1E`) and the 12-swatch `Charts` list also live in this header. A third
`ThemeColors::Contrast` namespace carries the high-contrast tokens (accent `#1AEBFF`, black
canvas, yellow hyperlinks) but is not part of the Light/Dark runtime swap.

### Why this is the seed, not a mapping

Material 3 and macOS each ship a `builtinSpec()` that *overrides* a handful of roles onto these
fields; **Fluent ships none** (`ThemeCatalog::apply()` adds no built-in spec for the Fluent
theme). Selecting Fluent in Settings is exactly `resetToDefaults()` — i.e. the raw
`ThemeColors` values above, unmodified.

---

## 2. Typography — **Segoe UI Variable** (`src/design/Typography.h`)

The family is `Segoe UI Variable`, resolved per role to an optical-size sub-family
(`… Small` / `… Text` / `… Display`). Heading roles use **SemiBold (600)**, not Bold. Sizes and
line heights are absolute pixels measured from the kit's typography styles.

| Role | Optical family | Size / Line (px) | Weight |
|---|---|---|---|
| Caption | Segoe UI Variable Small | 12 / 16 | Regular (400) |
| Body | Segoe UI Variable Text | **14 / 20** | Regular (400) |
| Body Strong | Segoe UI Variable Text | 14 / 20 | **SemiBold (600)** |
| Body Large | Segoe UI Variable Text | 18 / 24 | Regular (400) |
| Body Large Strong | Segoe UI Variable Text | 18 / 24 | SemiBold (600) |
| Subtitle | Segoe UI Variable Display | 20 / 28 | SemiBold (600) |
| Title | Segoe UI Variable Display | 28 / 36 | SemiBold (600) |
| Title Large | Segoe UI Variable Display | 40 / 52 | SemiBold (600) |
| Display | Segoe UI Variable Display | 68 / 92 | SemiBold (600) |

Default control text is **Body (14 px Regular)** — `Button`, `CheckBox`, `RadioButton`,
`ToggleSwitch` all construct with `themeFont("Body")`. Icon glyphs come from **Segoe Fluent
Icons** (the `Typography::Icons::*` table — chevrons, CheckMark ``, Hyphen ``, etc.).

---

## 3. Shape (`src/design/CornerRadius.h`)

A deliberately tiny two-step scale — the WinUI default.

| Token | px | Used by |
|---|---|---|
| `None` | **0** | Flush/square edges |
| `Control` | **4** | In-page controls (Button, TextBox, CheckBox box) |
| `Overlay` | **8** | Overlay containers (Flyout, Dialog, ToolTip) |
| `Indicator` | 1.5 | Selection-indicator pills (TabView/SelectorBar/Pivot bar — 3 px bar, rounded at half-thickness) |

`themeRadius().control` is **4** and `themeRadius().overlay` is **8** for the Fluent seed.
`Button::cornerRadii()` returns `control` (4) on all four corners by default. This small radius
is the most visible difference from Material 3 (8/12 + full pills) and is shared in spirit with
macOS's ~6.

---

## 4. Interaction — layered fills & acrylic context

Fluent does not use a single state-layer formula. Instead each control swaps among a small set
of **pre-defined translucent fills** in `ThemeColors`, so hover/press read correctly in both
themes without a `.darker()`/`.lighter()` guess:

- **Neutral controls** step `ControlDefault → ControlSecondary (hover) → ControlTertiary
  (pressed)`. In dark theme these are white-at-rising-alpha (~6 % → ~9 % → ~4 %); in light theme
  near-white opaque fills — same role, theme-correct value.
- **Subtle / transparent controls** use the `Fill::Subtle*` set: `SubtleTransparent` at rest →
  `SubtleSecondary` on hover (light: `0,0,0,9`; dark: `255,255,255,15`) → `SubtleTertiary` on
  drag-over. This is the "subtle" command-bar/list-item treatment.
- **Accent fills** step `AccentDefault → AccentSecondary (hover, ~90 %) → AccentTertiary
  (pressed, ~80 %)` — i.e. the accent itself fades, not a neutral veil.
- **Pressed motion:** Fluent nudges button content **down 0.5 px** while pressed (a small physical
  cue); Material/macOS use color only.
- **Focus:** a two-ring focus rect — `Stroke::FocusOuter` (a near-opaque ring) over
  `Stroke::FocusInner` (the opposite polarity), drawn inset.

The brand's **acrylic / mica** materials are window-level backdrops (Win11 DWM Mica/Acrylic,
Win10 legacy Acrylic fallback, macOS vibrancy, or a solid fallback elsewhere) rather than
per-control fills, so control specs here treat the surface as opaque; the translucency lives
behind the page.

---

## 5. Component specs (the `DesignFluent` baseline)

These describe the resting Fluent shape and interaction — the `else`/default branch of each
control's `paintEvent`. Every other brand branches off these.

### Primitives — ![Fluent primitives](images/fluent-primitives.png)

The kit's Primitives sheet (List Item, Surfaces, Caret, Focus Rect, Text Box Button) defines the
shared atoms: the rounded **surface** (4 px control / 8 px overlay), the two-ring **focus rect**,
the **caret**, and the inline **text-box button**.

### Text fields — ![Fluent text fields](images/fluent-text-fields.png)

TextBox is a 4 px-rounded surface with a **bottom accent underline** that thickens to the accent
color on focus — the signature Fluent input affordance.

### Basic input family — ![Fluent basic input](images/fluent-basic-input.png)

Overview of the control family below.

### Button (`Button.cpp`, default branch)
- Flat **4 px rounded-rect** (`themeRadius().control`); no pill, no gradient.
- **Accent** (or checked Standard) → `accentDefault` fill + `textOnAccent` text + `strokeStrong`
  border; hover → `accentSecondary`, pressed → `accentTertiary` with the border flattened to
  transparent.
- **Standard** → `controlDefault` fill + `textPrimary` + `strokeDefault` border; hover →
  `controlSecondary`, pressed → `controlTertiary` (border fades to `strokeDivider`, text dims to
  secondary).
- **Subtle** → transparent fill + `textPrimary`; hover → `subtleSecondary`, pressed →
  `subtleTertiary`; a checked Subtle keeps a faint `subtleSecondary` rest fill.
- Pressed nudges content down **0.5 px**. Critical-on-hover swaps to `systemCritical`.

### ToggleSwitch (`ToggleSwitch.cpp`, default branch)
- **Pill track 40 × 20** (`kTrackRadius = 10`) + a **circular knob** (rounded-rect at half-size).
- Knob size grows by state: **12** rest → **14** hover → **17 × 14** pressed; it animates across
  the track via `knobPosition` (fast / decelerate easing).
- **ON**: track fill + stroke = `accentDefault` (hover `accentSecondary`, pressed `accentTertiary`);
  knob = `textOnAccent`.
- **OFF**: track fill = `controlAltSecondary` with a `strokeStrong` outline (hover
  `controlAltTertiary`, pressed `controlTertiary`); knob = `textSecondary`.

### CheckBox (`CheckBox.cpp`, default branch)
- **~4 px box** (`radius.control`); the inner glyph is Segoe Fluent Icons — CheckMark when
  checked, Hyphen when indeterminate — with an animated scale-in (`checkProgress`).
- **Unchecked**: `controlDefault` fill + `strokeDefault` border (hover → `controlSecondary` +
  `strokeStrong`, pressed → `controlTertiary`).
- **Checked / Indeterminate**: `accentDefault` fill (hover `accentSecondary`, pressed
  `accentTertiary`), no border, `textOnAccent` glyph.
- Optional whole-row `subtleSecondary` hover background (4 px rounded).

### RadioButton (`RadioButton.cpp`, default branch)
- **Ring + dot**: outer circle at the control's `circleSize`; inner dot ≈ **50 %** of the ring.
- **Selected**: ring fills with `accentDefault` (hover `accentSecondary`, pressed
  `accentTertiary`), no outline; inner dot = `textOnAccent`. The dot grows **20 %** on hover
  (`dotScale → 1.2`).
- **Unselected**: `controlDefault` fill + outline (`strokeDefault`, `strokeStrong` on hover), no
  dot.

### Slider (`Slider.cpp`, default branch)
- **Thin track** — `m_trackHeight = 4 px`, fully rounded; inactive = `controlAltSecondary`,
  active/filled = `accentDefault` (`accentDisabled` when disabled).
- **Circular knob**, `m_handleSize = 20` (base radius 10): a `bgSolid`-filled **white outer
  ring** with a `strokeStrong` 1 px border (which masks the track behind it, reading as a border)
  plus an **accent inner dot**.
- The inner dot **grows with hover**: `innerScale = 0.45 + 0.25 × hoverRatio` (≈ 0.45 rest → 0.70
  hover); pressed → `accentTertiary`, hover → `accentSecondary`.

---

## 6. `DesignFluent` is the baseline

Everything above is the **default** the app boots into. Concretely:

- `ThemeRegistry::seedDefaults()` copies `ThemeColors::{Light,Dark}` into `Colors`, installs
  radius **4 / 8** and the Segoe UI Variable type scale, and sets
  `DesignLanguage = DesignFluent`.
- `ThemeRegistry::resetToDefaults()` re-runs exactly that seed.
- `ThemeCatalog::apply(Fluent)` is just `resetToDefaults()` + `DesignFluent` with **no
  `builtinSpec`** — there is nothing to override because the seed already *is* Fluent.
- Selecting **Material 3** or **macOS** in Settings layers a `builtinSpec()` (a few role swaps +
  radius + design language) **on top of** this same seed.

So the [Material 3](material-3.md) and [macOS](macos.md) references describe *deltas*; this page
describes the **floor** they are measured against. To restore it from any brand, pick **Fluent**
(or call `resetToDefaults()`).
