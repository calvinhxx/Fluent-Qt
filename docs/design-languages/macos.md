# macOS (Apple) — Design Reference

Source of truth: **macOS 27 UI Kit (Community, Apple Design Resources, Beta)** — file
`W0PjLoNXuQyLACYlAE3QKi`. Color values below were pulled live via the Figma MCP
`get_variable_defs` on the *Colors* page (`0:687`); component shapes from the *Buttons*
(`207:14487`), *Toggles* (`207:14470`) and *Sliders and Dials* (`207:14494`) pages.
See [figma-sources.md](figma-sources.md). This is the spec we port the `DesignCupertino`
branch onto.

> macOS controls are **accent-driven and physically restrained**: a tiny corner radius, a
> faint bezel/border, a subtle shadow, and the system **accent color (blue by default)** for
> the "on"/selected/prominent state. The texture comes from *understatement*, not from big
> fills or halos.

---

## 1. System colors

### Accent (system tint) colors — from the kit

The kit resolves a single value per variable (the captured mode is the **dark** appearance —
labels resolve to white). Use these as the macOS-27 refreshed tints:

| Tint | Opaque | Vibrant | Tint | Opaque | Vibrant |
|---|---|---|---|---|---|
| **Blue** (default accent) | `#0091FF` | `#0A99FF` | Mint | `#00DAC3` | `#2DE0CD` |
| **Green** | `#30D158` | `#3BDB63` | Teal | `#00D2E0` | `#2DD7E0` |
| **Red** | `#FF4245` | `#FF4747` | Cyan | `#3CD3FE` | `#47D8FC` |
| Orange | `#FF9230` | `#FF9E33` | Indigo | `#6D7CFF` | `#7163FF` |
| Yellow | `#FFD600` | `#FFE014` | Purple | `#DB34F2` | `#E647FC` |
| Pink | `#FF375F` | `#FF4169` | Brown | `#B78A66` | `#C29672` |
| Gray | `#98989F` | — | | | |

### Canonical HIG values (what `ThemeCatalog` currently installs)

The long-standing AppKit system colors, which our preset uses for the accent:

| Color | Light | Dark |
|---|---|---|
| **systemBlue** (default `controlAccentColor`) | `#007AFF` | `#0A84FF` |
| systemGreen | `#34C759` | `#30D158` |
| systemRed | `#FF3B30` | `#FF453A` |
| systemGray | `#8E8E93` | `#98989F` |

> Keep `accentDefault = #007AFF / #0A84FF` for the preset (the familiar macOS blue); the kit's
> `#0091FF` is the 27-beta refresh and is within the same hue if we ever want to track it.

### Labels & fills (dark appearance, from the kit)

Apple expresses neutral text/fills as **white at decreasing alpha** (and black on light):

| Label | Alpha | Fill (opaque) | Alpha |
|---|---|---|---|
| Primary | `#FFFFFF` @ 85 % | Primary | `#FFFFFF` @ 10 % |
| Secondary | @ 55 % | Secondary | @ 8 % |
| Tertiary | @ 25 % | Tertiary | @ 5 % |
| Quaternary | @ 10 % | Quaternary | @ 3 % |

This is exactly the **theme-aware veil** pattern our controls already use
(`darkTheme ? white@a : black@a`).

---

## 2. Typography — **SF Pro**

System font `SF Pro` (Text/Display optical sizes). Confirmed `Footnote = 10 / 13 / Regular`.
Standard macOS control text is **13 pt** (`.AppleSystemUIFont`); large title 26, title-1 22,
headline 13 semibold, body 13, caption 10. Numerals and control labels use Regular/Medium.

> **Font:** SF Pro is the macOS system face *by spec*, but the app deliberately does **not** bundle
> or override fonts — every brand renders in the bundled **Segoe UI Variable** (SF Pro can't be
> redistributed). On real macOS a user may set `typography.family` to the system font via their theme JSON.

---

## 3. Shape & elevation

- **Corner radius is small** and scales with control size: mini ≈ 4, small ≈ 5, regular ≈ **6**,
  large ≈ 7–8. (`ThemeCatalog` installs macOS radius **6 / 10**.)
- Standard controls carry a **1 px hairline border** (`separator`/control-stroke) plus a very
  **soft 1 px drop shadow** — the "bezel". Avoid heavy borders or large radii.

---

## 4. Component specs (porting targets)

### Button — ![macOS buttons](images/macos-buttons.png)
- **Push (standard)**: light **bezel** — near-white fill in light / `white @ 10%` in dark, a
  hairline border, faint shadow, radius ≈ 6, label 13 pt. Pressed → slight darken.
- **Prominent (default action)**: **accent (blue) fill** + white text — used for the default button.
- **Destructive**: red (`systemRed`) fill or red text.
- **Tinted**: accent @ ~12–15 % fill + accent text.
- **Borderless / text**: accent text only, no fill/border.
- Our mapping: `Standard → bezel`, `Accent → prominent blue fill`, `Subtle → borderless`,
  critical → destructive red.

### DropDownButton — ![macOS pop-up & pull-down buttons](images/macos-popup-buttons.png)
- macOS maps this to the **pull-down button**: the same restrained **bezel** as the push button —
  `bgLayerAlt` fill + **1 px hairline** + **radius 6** + a faint shadow — with a **single trailing
  chevron `⌄`** (one glyph, unlike the pop-up button's `⌃⌄` pair).
- Click opens a **rounded menu** panel; pressed/open → slight darken of the bezel.
- Our mapping: `DesignCupertino` DropDownButton reuses the push-button bezel (hairline + radius 6
  + soft shadow) and appends the single `⌄` chevron, opening the menu surface.

### ToggleButton
- **Unchecked** is the standard **bezel** push-button (subtle fill + hairline + faint shadow).
- **Checked** reads as **selected/recessed**: **accent (blue) fill + white text** — the same
  prominent treatment as a default button, signalling the pressed-in state.
- Knob/label aside, only the fill swaps between states; radius/shadow stay the bezel idiom.
- Our mapping: `DesignCupertino` ToggleButton draws the bezel when off and the `accentDefault`
  fill + white text when on (selected).

### SplitButton (+ ToggleSplitButton)
- A **bezel** push-button split into an **action segment** + a **trailing chevron segment** by a
  **1 px hairline divider** — both halves share the bezel fill, hairline border and radius 6.
- The chevron segment opens the rounded menu; pressed segment darkens slightly.
- ToggleSplitButton inherits SplitButton; its action segment follows the **ToggleButton** rule
  (accent fill + white text when checked) while the chevron segment stays the bezel.
- Our mapping: `DesignCupertino` SplitButton renders the push-button bezel with a hairline divider
  between the action and chevron segments; ToggleSplitButton tints the action half accent when on.

### HyperlinkButton
- macOS treats this as a **link**: **accent (blue) text**, understated — no fill, border or shadow.
- Optional **underline on hover**; otherwise the accent color alone marks it as tappable.
- Our mapping: `DesignCupertino` HyperlinkButton renders accent-blue link text with no bezel,
  optionally underlining on hover — the quietest of the button family.

### Switch (NSSwitch) — ![macOS toggles](images/macos-toggles.png)
- Pill track, white circular **knob** with a hairline ring + soft shadow.
- **OFF**: track = neutral/tertiary gray fill.
- **ON**: track = **accent BLUE** (`controlAccentColor`) — **not green**. (iOS `UISwitch` is
  green; macOS `NSSwitch` uses the accent. This is the headline correction for our `DesignCupertino`
  toggle, which previously used systemSuccess green.)
- Knob stays white in both states; only the track changes.

### Checkbox — (top group of the Toggles image)
- Small rounded square, corner radius ≈ **3.5**, size ≈ 14.
- **Checked**: accent (blue) fill + white ✓. **Mixed**: accent fill + white dash.
- **Unchecked**: control fill (near-white / `white@10%`) + hairline border.

### Radio button — (middle group of the Toggles image)
- Circle ≈ 16. **Selected**: accent fill + white center dot. **Unselected**: control fill + hairline border.

### Slider — ![macOS sliders](images/macos-sliders.png)
- **Thin track** (≈ 3–4 px), active portion = **accent blue**, inactive = neutral gray/tertiary.
- Thumb = **white circle** (≈ 16–20) with a hairline border + soft drop shadow (knob is large
  relative to the thin track — that contrast is the macOS cue).
- Tick-mark sliders and circular dials exist but are out of scope.

### Text field (LineEdit/TextBox) — ![macOS text fields](images/macos-textfields.png)
- A **thin rounded rect** (radius ≈ **5–6**) with a **hairline border** and a near-white /
  `white @ 10%` fill — the same restrained bezel as the push button.
- **Focus**: a bright **accent (blue) focus RING** (~2–3 px) wraps the field — the headline macOS
  cue; the border itself stays subtle, the ring does the talking.
- **Disabled**: gray fill, dimmed text/border.
- Our mapping: an opaque **`bgLayerAlt` bezel fill** + rounded rect + hairline at rest (the same
  raised bezel as ComboBox, so the field is visible on any surface — not just on a card); on focus draw
  the `accentDefault` ring rather than thickening the border. Placeholder/value text follow the
  `Labels/*` alpha scale. (M3, by contrast, is the **Outlined** variant — transparent fill by spec.)

### Multi-line text field (TextEdit)
- A multi-line text box gets the **identical** treatment to the single-line Text field above: a
  **thin rounded rect** (radius ≈ 5–6) with a **hairline border** + near-white / `white @ 10%`
  fill, and on **focus** the bright **accent (blue) ring** — only taller, with text over several rows.
- Our mapping: TextEdit reuses the Text-field frame/focus spec verbatim; no separate visual rules.

### Search Fields (AutoSuggestBox) — ![macOS search fields](images/macos-search-fields.png)
- A macOS **search field** is a **rounded field** (the same restrained bezel — hairline border,
  small radius, near-white / dark fill) with a **leading magnifier** glyph and, once typed into, a
  **trailing clear (✕)** button. Focus shows the **accent blue ring** like any text field.
- Our mapping: `AutoSuggestBox`, `NumberBox` and `PasswordBox` all **subclass LineEdit**, so they
  **inherit** the macOS text-field frame (rounded rect + hairline at rest, accent ring on focus)
  rather than redefining it. Only their **trailing affordances** are control-specific — the
  search/clear glyph (AutoSuggestBox), the spin buttons (NumberBox), or the reveal toggle
  (PasswordBox) — laid over the shared bezel.

### ComboBox (Pop-up / Pull-down button) — ![macOS pop-up & pull-down buttons](images/macos-popup-buttons.png)
- **Closed field** is a **bezel push-button**: subtle fill + **1 px hairline** + small radius **6**
  + a faint shadow, with a **trailing chevron** — a **pop-up** button shows the up/down pair
  (`⌃⌄`), a **pull-down** button shows a single `⌄`. Pressed/open → slight darken.
- **Popup**: a **rounded menu** panel with **checkmarks** marking the selected item (and optional
  leading icons / trailing shortcut glyphs).
- Our mapping: closed field reuses the push-button bezel (hairline + radius 6 + soft shadow) plus
  the trailing chevron; the popup uses the menu surface with a checkmark on the current value.

### Tabs (Pivot) — ![macOS segmented control](images/macos-segmented.png)
- The macOS analog of a Pivot is a **segmented control**, **not** underline tabs: a **rounded
  container** holding equal segments separated by **thin dividers**.
- **Selected segment**: **accent (blue) fill + white text**. Other segments are neutral (label text
  on the container fill); dividers hide next to the selected segment.
- Our mapping: the `DesignCupertino` Pivot renders as a segmented control — a rounded track with
  hairline dividers, the active segment filled `accentDefault` with white text, the rest neutral.
  (So Cupertino reads as a segmented control rather than an underlined tab strip.)

### SelectorBar (segmented selector) — ![macOS segmented control](images/macos-segmented.png)
- macOS maps a SelectorBar directly onto the **segmented control**: a **rounded container** holding
  equal segments separated by **thin dividers**.
- **Selected segment**: **accent (blue) fill + white text**; other segments neutral (label text on
  the container fill); dividers hide next to the selected segment.
- Our mapping: the `DesignCupertino` SelectorBar renders as a segmented control — rounded track,
  hairline dividers, active segment filled `accentDefault` with white text, the rest neutral
  (the same treatment as the Pivot above).

### TabView (document tabs)
- Document-style tabs are **not** a native macOS pattern, so the port is a tasteful, restrained
  adaptation rather than a literal control.
- **Selected tab**: a **restrained accent-tinted active segment** (subtle accent fill / tint, not a
  loud blue block); inactive tabs read neutral. Keep it understated per the macOS bezel idiom.
- Our mapping: TabView leans on the segmented-control language — the active tab is the accent-tinted
  segment, the rest neutral — kept deliberately quiet rather than a full prominent fill.

### ProgressBar (linear)
- macOS determinate bar: a **thin (~6 pt) fully-rounded** bar. **Track** = a quaternary neutral fill (light
  gray; map to `controlSecondary` / `bgSolid`); **active** = the system **accent** (`accentDefault`). No stop
  dot, no gap (those are M3-only). Indeterminate uses an animated barber-pole; we approximate with a
  travelling rounded segment.
- Our mapping: `DesignCupertino` ProgressBar paints a thin rounded neutral track + a rounded `accentDefault`
  fill; `showPaused → systemCaution`, `showError → systemCritical`. Deliberately quiet — no M3 stop dot.

### ProgressRing (circular)
- macOS indeterminate is the signature **spoke spinner**: 8–12 short **tapered lines** arranged radially,
  their **opacity fading** around the circle (the leading spoke opaque, trailing spokes faint), rotating.
  Determinate is a **thin (~3 pt) accent ring** on a faint neutral track.
- Our mapping: `DesignCupertino` ProgressRing draws the **spoke spinner** when indeterminate (tapered
  `textSecondary`/accent spokes with a fading alpha ramp) and a thin `accentDefault` ring when determinate;
  paused/error recolor the active stroke.

### InfoBar
- macOS has no "InfoBar"; status surfaces follow the **quiet bezel idiom** — a **rounded (~10) container**
  with a restrained tonal fill (subtle `systemXxxBg`, or `bgLayerAlt`), a **hairline `strokeStrong`** border,
  the icon tinted `systemXxx`, and `textPrimary` body. No loud full-width colored band.
- Our mapping: `DesignCupertino` InfoBar drops the Fluent left accent bar for a rounded-10 hairline-bordered
  container with a subtle `systemXxxBg` fill and a `systemXxx`-tinted icon.

### InfoBadge
- macOS notification/dock badge: a **fully-rounded** red capsule/circle (system **red**, `systemCritical`)
  with a **white** number; the attention variant is a small **red dot**. The canonical badge is red, but a
  badge may carry another semantic color.
- Our mapping: `DesignCupertino` InfoBadge renders **fully rounded** (radius = height / 2) filled with the
  badge's semantic color (defaulting to the system red look), white value text; the dot variant is a bare
  filled circle.

### CalendarView / CalendarDatePicker
- macOS graphical date picker (NSDatePicker, graphical style): a compact month grid; the **selected day**
  is a filled **accent** circle/rounded square with **white** text; **today** is marked subtly (bold or an
  accent ring). Quiet hairline framing, small radius — never a loud block.
- Our mapping: `DesignCupertino` CalendarView fills the selected day with `accentDefault` + a white glyph
  and marks today with an `accentDefault` ring; day hover uses the theme-aware veil. `CalendarDatePicker`
  is a `Button` subclass → its closed field inherits the macOS bezel automatically; only the popup
  (CalendarView) needs the branch.

### DatePicker / TimePicker (spinner pickers)
- macOS uses textual steppers (NSDatePicker textfield-and-stepper) or the graphical clock — NOT looping
  spinner columns — so the spinner port is an adaptation. The selection cue is the **accent** highlight on
  the selected value (like a focused list row); the closed field is the push-button **bezel** (hairline +
  radius 6 + faint shadow).
- Our mapping: `DesignCupertino` DatePicker/TimePicker highlight each column's selected row with an
  `accentDefault` fill (white text), and the closed field reuses the bezel. Hover via the theme-aware veil
  (white-on-dark / black-on-light), visible in both appearances.

### Menu (FluentMenu)
- macOS **menu**: a rounded panel (subtle hairline + soft shadow) on a light material; the **highlighted
  item is a solid accent-blue bar** with **white** text (and white checkmark/submenu chevron). Checked items
  carry a leading checkmark; unhighlighted items are plain `textPrimary` over the panel fill.
- Our mapping: `DesignCupertino` FluentMenu fills the **active item with `accentDefault`** and flips its
  label, shortcut, checkmark and chevron to **`textOnAccent` (white)**; the panel keeps the `bgLayer` fill +
  hairline border. Non-active items stay transparent with normal text.

### MenuBar (FluentMenuBar)
- macOS **menu bar**: a title is highlighted with the **accent-blue bar + white text** while its menu is
  **open** (or pressed); plain hover is a faint darken.
- Our mapping: `DesignCupertino` FluentMenuBar fills an **open/pressed** title with `accentDefault` + white
  text, and uses the theme-aware **veil** for plain hover — never a permanent fill.

### ContentDialog
- macOS **alert / sheet**: a rounded card with a **hairline border** and soft shadow; the action buttons sit
  at the bottom, the default button accent-filled. Quiet framing, no loud chrome.
- Our mapping: `DesignCupertino` ContentDialog keeps the two-region content/button layout but swaps the outer
  border to a **hairline `strokeStrong`** (divider stays `strokeDivider`); the default button is already the
  accent push-button via the shared Button bezel.

### Flyout / Popup (shared card) · ComboBox dropdown
- macOS **popover**: a rounded panel on a light material with a **crisp 1 px hairline** edge and a soft
  shadow. The same surface backs the **ComboBox dropdown** (inherits the shared `Popup` card) and the
  gallery **Flyout**.
- Our mapping: `DesignCupertino` `Popup::paintEvent` keeps the `bgLayer` fill + radius + shadow and draws a
  **hairline `strokeStrong`** border (vs. Fluent's `strokeDefault`), giving every base-`Popup` consumer the
  crisp macOS popover edge.

### TeachingTip
- macOS **popover with a beak**: a rounded bubble + tail on a light material, **hairline** edge + soft
  shadow.
- Our mapping: `DesignCupertino` TeachingTip keeps the `bgLayer` bubble + tail + shadow and strokes the
  united outline with a **hairline `strokeStrong`** pen.

### Dialog (base modal)
- `Dialog` is the base of `ContentDialog` and gets the same macOS **alert/sheet** treatment: rounded card,
  **hairline border**, soft shadow.
- Our mapping: `DesignCupertino` Dialog swaps the card's outer border to a hairline `strokeStrong` (mirroring
  the ContentDialog branch); the two-region content/button layout is kept.

### CoachMark
- A callout-with-beak; macOS renders it as a **popover** — rounded bubble + tail, **hairline** edge, soft
  shadow.
- Our mapping: `DesignCupertino` CoachMark strokes the bubble outline with a hairline `strokeStrong` pen.

### ScrollBar / ScrollView / AnnotatedScrollBar
- The macOS **overlay scrollbar** is a thin **translucent thumb** (`label` color at low alpha), rounded,
  inset, with no rail at rest — it fades in on scroll.
- Our mapping: `DesignCupertino` ScrollBar paints the thumb as a neutral translucent **veil** (theme-aware),
  rail suppressed at rest; AnnotatedScrollBar keeps its `accentDefault` indicator.

### PipsPager / FlipView
- macOS **page control**: small dots, the **current page filled** (`label`/accent), others dim
  `tertiaryLabel`; carousel nav is a quiet translucent affordance.
- Our mapping: `DesignCupertino` PipsPager/FlipView fill the active pip `accentDefault`, inactive pips a dim
  neutral, and the nav buttons use the theme-aware veil for hover/press. (FlipView dark-mode fills come only
  from valid seeded tokens — guarding the past white-nav-button-in-dark regression.)

### RatingControl
- macOS ratings are filled accent/`systemYellow` stars over a dim neutral outline.
- Our mapping: `DesignCupertino` RatingControl keeps filled stars `accentDefault`, tones unfilled stars to a
  dim neutral, hover via the theme-aware veil.

### Collections — ListView / TreeView / GridView / FlowView
- macOS **source-list / table selection** is the signature **solid accent-blue row fill with white text**
  (`NSTableView`/`NSOutlineView` selected row); hover is a faint darken. No left pill.
- Collections are **two-layered** here (gallery item *delegate* paints the row/tile, the *view* adds a Fluent
  accent pill/overlay). Our mapping: under `DesignCupertino` the **delegate** fills the selected row with
  solid `accentDefault` and flips its text/glyphs to `textOnAccent` (white), hover via the theme-aware veil,
  while the **view suppresses its Fluent accent pill/overlay** so the accent row is the sole cue. Image tiles
  keep the accent border + check; drag-insertion indicators stay accent.

### Window chrome / TitleBar caption controls
- macOS window controls are the **traffic lights**: three small (~12 px) circles at the **leading**
  edge — **close `#FF5F57` (red), minimize `#FFBD2E` (yellow), zoom `#28C840` (green)**, in that
  order. The **×/−/+ glyphs appear only on hover** over the cluster; when the window is **inactive**
  the three dots desaturate to a uniform **gray**. This is the headline contrast with Windows, whose
  caption buttons are a **trailing** minimize/maximize/close glyph row (close goes critical-red on hover).
- This is a **real platform difference**, so on the actual targets it is the OS that draws it: on macOS
  the app keeps the native traffic lights (`prefersNativeMacControls`, leading inset reserved); on Windows
  the app self-draws the Fluent caption-button row (`usesCustomWindowChrome`, trailing reserved). The
  `Window`/`TitleBar` chrome therefore stays **platform-correct** and is intentionally *not* forced to
  follow a cosmetic style-theme choice.
- The **gallery TitleBar sample** is where the difference is *showcased by design language*: it renders
  leading traffic lights under `DesignCupertino` and the trailing Windows caption row under
  `DesignFluent`/`DesignMaterial`, swapping live on a Style-theme change (the sample's `MacTrafficLights`
  widget custom-paints the dots, hover glyphs and inactive gray). Mapping lives in
  `captionStyleForDesignLanguage()` (`app/view/widgets/samples/WindowingSamples.{h,cpp}`).

---

## 5. Quick checklist for the `DesignCupertino` branch

- [ ] **Switch ON = accent blue, not green.**
- [ ] Small radii (≈ 6 controls), hairline border + faint shadow ("bezel"), never heavy outlines.
- [ ] Selected checkbox/radio = accent fill + **white** glyph/dot; unselected = control fill + hairline.
- [ ] Slider: thin track, accent active fill, **white** round knob with hairline + shadow.
- [ ] Text field / ComboBox: hairline bezel + small radius 6; focus = **accent blue ring**, not a heavier border.
- [ ] ComboBox closed field carries a trailing chevron (pop-up `⌃⌄` / pull-down `⌄`); popup uses checkmarks.
- [ ] Pivot = **segmented control** (rounded track, hairline dividers); selected segment = **accent fill + white text**.
- [ ] Hover/press feedback via the **theme-aware veil** (white-on-dark / black-on-light at low
      alpha) and accent gradient modulation — verify it is visible in **both** light and dark.
