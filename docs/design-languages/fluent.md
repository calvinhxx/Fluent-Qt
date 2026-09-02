# Fluent (Windows) — Design Reference

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Fluent design](README.md) › Design references

[Contents](../SUMMARY.md) · [Fluent design index](README.md) · [Fluent Design Kit Source →](figma-sources.md)
<!-- docs-nav:top:end -->

Source of truth: **Windows UI kit (Community)** — file `qpecbg7hOfos9DcHWeKlfw`.
Fluent is the project's only supported visual contract. The values below are quoted directly from our own
design headers (`src/design/*.h`), which seed the runtime `ThemeRegistry`; the Figma kit was
the original measurement source and is kept as visual grounding (see
[figma-sources.md](figma-sources.md)).

> Fluent is **token-based**: controls read semantic `FluentElement::Colors`, `themeRadius()`,
> and `themeFont()` at runtime. Those tokens **are** the seed —
> `ThemeRegistry::seedDefaults()` copies `ThemeColors::{Light,Dark,Contrast}` straight into `Colors`,
> with optional user overrides layered on top.

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
| `Fill::ControlDefault` | `#FFFFFF` | `BackgroundLayerOverlay` | white @ ~50 % (`255,255,255,128`) |
| `Stroke::ControlDefault` | black @ ~5 % (`0,0,0,12`) | `BackgroundSolid` | `#EEEEEE` |
| `Stroke::ControlStrong` | black @ ~44 % (`0,0,0,112`) | `Text::Primary` | black @ ~90 % (`0,0,0,230`) |
| `Stroke::DividerDefault` | black @ ~8 % (`0,0,0,20`) | `Text::Secondary` | black @ ~60 % (`0,0,0,154`) |
| `Stroke::FocusOuter` | black @ ~90 % (`0,0,0,230`) | `Text::OnAccentPrimary` | `#FFFFFF` |
| `System::Critical` | `#C42B1C` | `Text::AccentPrimary` | `#003E92` |
| `System::Caution` | `#9D5D00` | `System::Success` | `#0F7B0F` |
| `System::Informational` | `#015CDA` | | |

### Dark theme (`ThemeColors::Dark`)

| Role | Value | Role | Value |
|---|---|---|---|
| `Fill::AccentDefault` | `#60CDFF` | `BackgroundCanvas` | `#202020` |
| `Fill::AccentSecondary` | accent @ ~90 % (`96,205,255,230`) | `BackgroundLayer` | `#2C2C2C` |
| `Fill::AccentTertiary` | accent @ ~80 % (`96,205,255,204`) | `BackgroundLayerAlt` | `#3D3D3D` |
| `Fill::ControlDefault` | white @ ~6 % (`255,255,255,15`) | `BackgroundLayerOverlay` | `#3A3A3A` @ ~30 % (`58,58,58,76`) |
| `Stroke::ControlDefault` | white @ ~7 % (`255,255,255,17`) | `BackgroundSolid` | `#1C1C1C` |
| `Stroke::ControlStrong` | white @ ~54 % (`255,255,255,138`) | `Text::Primary` | `#FFFFFF` |
| `Stroke::DividerDefault` | white @ ~8 % (`255,255,255,20`) | `Text::Secondary` | white @ ~78 % (`255,255,255,199`) |
| `Stroke::FocusOuter` | white @ ~90 % (`255,255,255,230`) | `Text::OnAccentPrimary` | `#000000` |
| `System::Critical` | `#FF99A4` | `Text::AccentPrimary` | `#99EBFF` |
| `System::Caution` | `#FCE100` | `System::Success` | `#6CCB5F` |
| `System::Informational` | `#60CDFF` | | |

> **Headline accents:** Light `#005FB8` (a deep WinUI blue), Dark `#60CDFF` (a bright cyan).
> Note the polarity flip in `OnAccentPrimary` — **white** text on the light accent, **black** on
> the dark accent — because the dark accent is light enough to need dark text.

The neutral `Grey10…Grey200` ramp (e.g. `Grey10 #FAF9F8`, `Grey130 #605E5C`, `Grey160 #323130`,
`Grey190 #201F1E`) and the 12-swatch `Charts` list also live in this header.

### High contrast theme (`ThemeColors::Contrast`)

`FluentElement::HighContrast` is a third runtime theme, not a Dark alias. It resolves a complete
semantic palette with a black canvas, white foreground and strokes, cyan focus/accent
(`#1AEBFF`), yellow accent text, a neutral mid-grey disabled state, and opaque semantic status
colors. Disabled tokens stay distinct from the green success role. Code choosing only between
light- and dark-backed chrome uses `FluentElement::themeUsesDarkAppearance()`; token consumers
continue to read `themeColors()` and do not branch on the theme enum.

This built-in palette is deterministic across native platforms. Windows, macOS, and Linux builds
do not currently auto-detect an operating-system high-contrast preference or import the user's
system contrast colors; applications select `HighContrast` explicitly or provide `contrast`
overrides through `fluent.json`. The WebAssembly host separately maps browser
`(forced-colors: active)` into this explicit runtime theme.

### Why this is the seed

`ThemeRegistry::defaultSnapshot()` returns the compatible Light/Dark plus shared-token state;
`ThemeRegistry::defaultExtendedSnapshot()` adds the High Contrast palette. Calling
`UserTheme::apply()` starts from the extended snapshot and then layers the optional user-editable
`fluent.json` overrides.

---

## 2. Typography — **FluentQt UI static instances** (`src/design/Typography.h`)

FluentQt registers project-specific Text/Heading/Display faces generated from
the open-source, hinted static Inter fonts. This prevents platform font matchers
or a same-named system font from selecting a different face, while retaining the
TrueType hint programs needed for crisp small text. Heading roles use
**SemiBold (600)**, not Bold. Sizes and line heights are absolute pixels measured
from the kit's typography styles. See
[Typography Resolution](../architecture/typography-resolution.md).

| Role | Optical family | Size / Line (px) | Weight |
|---|---|---|---|
| Caption | FluentQt UI Text | 12 / 16 | Regular (400) |
| Body | FluentQt UI Text | **14 / 20** | Regular (400) |
| Body Strong | FluentQt UI Text | 14 / 20 | **SemiBold (600)** |
| Body Large | FluentQt UI Text | 18 / 24 | Regular (400) |
| Body Large Strong | FluentQt UI Text | 18 / 24 | SemiBold (600) |
| Subtitle | FluentQt UI Heading | 20 / 28 | SemiBold (600) |
| Title | FluentQt UI Heading | 28 / 36 | SemiBold (600) |
| Title Large | FluentQt UI Display | 40 / 52 | SemiBold (600) |
| Display | FluentQt UI Display | 68 / 92 | SemiBold (600) |

Default control text is **Body (14 px Regular)** — `Button`, `CheckBox`, `RadioButton`,
`ToggleSwitch` all construct with `themeFont(Typography::FontRole::Body)`. Icon glyphs come from the bundled
**FluentQt Icons** face (the `Typography::Icons::*` table — chevrons, CheckMark, Hyphen, etc.).

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
`Button::cornerRadii()` returns `control` (4) on all four corners by default.

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
- **Pressed motion:** Fluent nudges button content **down 0.5 px** while pressed.
- **Focus:** a two-ring focus rect — `Stroke::FocusOuter` (a near-opaque ring) over
  `Stroke::FocusInner` (the opposite polarity), drawn inset.

Application-wide motion is resolved through `MotionPolicy`. `Full` preserves component timing,
`Reduced` caps transitions at 50 ms and stops continuous motion, and `Disabled` resolves all
motion directly to its final state. A component's local `animationEnabled=false` remains the
stricter choice. `Shimmer` and indeterminate progress indicators retain their busy semantics when
their visual motion is suppressed. Changing a finite-transition component's local animation switch
from enabled to disabled while it is moving settles that transition immediately through the
component's normal completion and cleanup path; this applies to Popup/Dialog/Toast presentation,
Expander and DrawerView state changes, and StackView/StackContentHost page transitions.

The brand's **acrylic / mica** materials are window-level backdrops (Win11 DWM Mica/Acrylic,
macOS vibrancy, or a solid fallback elsewhere; Windows 10 uses the solid fallback) rather than
per-control fills, so control specs here treat the surface as opaque; the translucency lives
behind the page.

---

## 5. Component specs

These describe the resting Fluent shape and interaction implemented by each
control's paint path.

### Primitives

The kit's Primitives sheet (List Item, Surfaces, Caret, Focus Rect, Text Box Button) defines the
shared atoms: the rounded **surface** (4 px control / 8 px overlay), the two-ring **focus rect**,
the **caret**, and the inline **text-box button**.

### Text fields

TextBox is a 4 px-rounded surface with a **bottom accent underline** that thickens to the accent
color on focus — the signature Fluent input affordance.

### Basic input family

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
- **~4 px box** (`radius.control`); the inner glyph is FluentQt Icons — CheckMark when
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

## 6. Fluent is the runtime contract

Everything above is the **default** the app boots into. Concretely:

- `ThemeRegistry::seedDefaults()` copies `ThemeColors::{Light,Dark,Contrast}` into `Colors` and installs
  radius **4 / 8** plus the FluentQt UI type scale.
- `ThemeRegistry::resetToDefaults()` re-runs exactly that seed.
- `UserTheme::apply()` resets to the seed and then loads optional
  `fluent.json` semantic-token overrides.
- `ThemeRegistry::applySnapshot()` keeps its original five-member source contract, commits the
  Light/Dark and shared-token portion atomically, and preserves the current High Contrast palette.
- `ThemeRegistry::applyExtendedSnapshot()` atomically commits a complete branded
  Light/Dark/High Contrast Fluent token set.

<!-- docs-nav:bottom:start -->
---
[Contents](../SUMMARY.md) · [Fluent design index](README.md) · [Fluent Design Kit Source →](figma-sources.md)
<!-- docs-nav:bottom:end -->
