# Design Language References

Fluent-Qt ships three switchable **style themes** (Settings → *Style theme*): Fluent (Windows),
Material 3 (Google), and macOS. Beyond recoloring, each brand changes component **shape and
interaction** through `FluentElement::DesignLanguage` (`DesignFluent` / `DesignMaterial` /
`DesignCupertino`), which controls' `paintEvent`s branch on.

This folder is the **canonical spec** for all three design languages — Fluent quoted from our own
design headers (it is the reference implementation), with Material 3 and macOS measured against
the linked design kits — so the control work has a single, verifiable target instead of guesswork.

## Documents

- [Fluent (Windows)](fluent.md) — the **baseline** the other two are deltas on. Real token values
  from `src/design/*.h` (accents `#005FB8` / `#60CDFF`, radius 4 / 8, FluentQt UI), and the
  `DesignFluent` per-component specs. Selecting Fluent is just `resetToDefaults()`.
- [Material 3 (Google)](material-3.md) — role colors, Roboto type scale, shape scale, **state
  layers**, and per-component specs (buttons, selection controls, sliders, text fields, tabs, and the
  Status & info family — progress, snackbar/InfoBar, badges).
- [macOS (Apple)](macos.md) — system/accent colors, SF Pro, small-radius bezel treatment, and
  per-component specs. **Note:** the macOS switch "on" state is **accent blue, not green**.
- [Design Kit Sources and Verification](figma-sources.md) — file keys, node IDs,
  and the measurement workflow. External kit screenshots and source layers are
  intentionally not redistributed.

## How it maps to the code

| Layer | Responsibility |
|---|---|
| [`ThemeRegistry`](../../src/components/foundation/ThemeRegistry.h) | Runtime store of the active Light/Dark `Colors`, radius, font, and `DesignLanguage`. Seeded from the Fluent defaults. |
| [`ThemeCatalog`](../../app/viewmodel/ThemeCatalog.h) | Installs the brand preset (the role→`Colors` mapping + radius + design language) and any user JSON overrides. |
| Control `paintEvent`s | Read `themeDesignLanguage()` and branch shape/interaction. `Button`, `ToggleSwitch`, `CheckBox`, `RadioButton`, `Slider` are design-language-aware today. |

The Fluent baseline is documented in [fluent.md](fluent.md) (and seeds `ThemeRegistry`); the
Material 3 and macOS pages describe the deltas layered on top of it.

## Interaction rule that applies to every brand branch

Hover/press **must** be visible in light *and* dark. Use the **theme-aware veil**
(`darkTheme ? white@α : black@α`) for neutral surfaces, the **on-color state layer** for M3
fills, and **accent gradient modulation** for accent fills — never a bare `.darker()`/`.lighter()`
on a surface color, which is theme-fragile. See each per-system doc's checklist.
