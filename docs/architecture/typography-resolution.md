# Typography Resolution

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Overlay Behavior Contract](overlay-behavior.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Inspector Report Contract →](inspector-report.md)
<!-- docs-nav:top:end -->

## Contract

FluentQt owns typography face resolution. Applications select semantic roles
such as `Body`, `BodyStrong`, or `Display`; they do not select a platform UI
font by name.

`fluent::initializeResources()` registers five FluentQt-specific static faces:

| Family | Style | Hinted source | Roles |
| --- | --- | --- | --- |
| `FluentQt UI Text` | Regular | `Inter-Regular.ttf` | Caption, Body, Body Large |
| `FluentQt UI Text` | Semibold | `Inter-SemiBold.ttf` | Body Strong, Body Large Strong |
| `FluentQt UI Heading` | Semibold | `Inter-SemiBold.ttf` | Subtitle, Title |
| `FluentQt UI Display` | Semibold | `InterDisplay-SemiBold.ttf` | Title Large, Display |
| `FluentQt Icons` | Regular | Fluent UI System Icons Regular | `Typography::Icons` |

The text faces are renamed static Inter 4.1 fonts. Renaming avoids the upstream
Reserved Font Name and prevents a same-named system installation from winning
before the application font. The static sources retain their TrueType `cvt `,
`fpgm`, `prep`, and `gasp` tables, which gives DirectWrite and FreeType real
instructions when the selected platform policy uses them, instead of shipping
an unhinted variable instance.

FluentQt requests high-quality grayscale antialiasing for semantic text fonts.
On Windows it disables DirectWrite grid fitting because vertical hinting makes
the bundled face visibly heavier at 12-14 px; Linux retains vertical hinting
for crisp FreeType rendering, while CoreText may apply its own platform policy.
Small rasterization differences are still expected; family, weight, line
height, and covered-script metrics are the portable contract.

Inter covers the Latin, Greek, and Cyrillic UI text used by the Gallery. CJK,
emoji, and other scripts not present in the bundled face continue through Qt's
platform fallback. Supported WebAssembly builds are the exception because the
browser sandbox exposes no host CJK font: they embed the optional
`FluentQt UI Simplified Chinese` GB 2312 subset and register it as a Han-script
application fallback. CMake selects that resource without adding WebAssembly
branches to components or typography roles. Native binaries do not carry the
additional face, avoiding a large pan-CJK payload in every package.

`FluentQt Icons` retains the complete Regular catalog from Microsoft Fluent UI
System Icons 1.1.328. `Typography::Icons::catalog()` exposes every upstream
name, codepoint, optical design size, and glyph; `Typography::Icons::glyph()`
performs lookup by the stable upstream name. The semantic constants used by
controls are generated as aliases in the derivative font, without dropping an
upstream icon whose private-use codepoint collides.

Full upstream versions, hashes, licenses, and provenance are recorded in
[`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md) and `third_party/`.

## Startup behavior

Standalone applications call `fluent::initializeResources()` after creating
`QApplication`. Gallery and the hello-world example then set the application
default font to `Typography::Styles::Body`, ensuring raw Qt widgets inherit the
same Text Regular face as Fluent components.

The compiled Qt resource collection is registered on demand independently of
font loading. Resource-backed catalogs such as `Typography::Icons::catalog()`
are therefore safe to inspect during static-library startup, before
`QApplication` exists. Calling `initializeResources()` that early registers the
resource collection but returns `false` without attempting to cache a font-load
result; a later call after `QApplication` exists retries normal font
initialization.

If a bundled text face cannot be registered, initialization returns `false`,
emits a warning in the `fluentqt.typography` category, and falls back to Qt's
current system UI family. An icon registration failure also returns `false`,
but no unrelated system symbol font is substituted.

## Component font precedence

`Button` and `ToggleSwitch` start in theme-managed mode with `fontRole` set to
`Body`. In this mode, the resolved font follows `ThemeRegistry`, including
family overrides and font scaling. `setFontRole(...)` selects another semantic
role while preserving that automatic theme refresh behavior.

Calling `setFont(...)` switches that individual control to explicit-font mode.
Later theme refreshes do not replace the caller-provided font. The `fontRole`
property still records the semantic role that will be used if theme management
is restored; it does not claim that the explicit font currently matches that
role. Calling `setFontRole(...)` restores theme-managed mode even when the
requested role equals the stored role. `fontRoleChanged` is emitted only when
the stored property value changes.

`DatePicker` and `TimePicker` inherit this precedence contract from `Button`.
Their entry text and an open picker flyout follow the resolved button font, so
theme-managed updates and explicit per-control overrides remain synchronized.
The PySide6 bindings expose the inherited `fontRole` property and
`setFontRole(...)` method with the same behavior.

## Regeneration

The committed runtime assets are deterministically generated from pinned
sources in `third_party/`:

```bash
python -m pip install -r tools/fonts/requirements.txt
python tools/fonts/generate_typography_assets.py
python tools/fonts/generate_typography_assets.py --check
python tools/fonts/generate_typography_assets.py --web-fallback
python tools/fonts/generate_typography_assets.py --check-web-fallback
```

The generator verifies every upstream SHA-256 hash and its exact fontTools
version. It rewrites family and PostScript names, preserves text hint programs,
builds the complete icon catalog plus semantic aliases, and produces the files
under `res/fonts/` and `res/icons/`. `--check` regenerates into a temporary
directory and compares every output byte for byte without changing the working
tree.

Semantic icon codepoints remain stable for source compatibility. Painters call
`Typography::Icons::glyphForSize()` with the visual slot size so the runtime can
select the upstream 12, 16, 20, or 24 px optical drawing recorded in
`FluentQtIconAliases.json`. This avoids shrinking a 20 px outline into WinUI's
12 px chevron or 16 px control slot.

## Verification

`TypographyTest` asserts that representative roles resolve to the expected
family, exact `Regular` or `Semibold` face, weight, pixel size, grayscale
strategy, and hinting preference. It verifies the TrueType hint tables through
`QRawFont`, checks the complete icon catalog against representative semantic
shortcuts, and renders Regular and Semibold to confirm that the real glyph
masks differ.

<!-- docs-nav:bottom:start -->
---
[← Overlay Behavior Contract](overlay-behavior.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Inspector Report Contract →](inspector-report.md)
<!-- docs-nav:bottom:end -->
