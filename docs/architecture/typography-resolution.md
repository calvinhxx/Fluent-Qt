# Typography Resolution

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
platform fallback. This avoids adding a very large pan-CJK font to every
binary.

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

If a bundled text face cannot be registered, initialization returns `false`,
emits a warning in the `fluentqt.typography` category, and falls back to Qt's
current system UI family. An icon registration failure also returns `false`,
but no unrelated system symbol font is substituted.

## Regeneration

The committed runtime assets are deterministically generated from pinned
sources in `third_party/`:

```bash
python -m pip install -r tools/fonts/requirements.txt
python tools/fonts/generate_typography_assets.py
python tools/fonts/generate_typography_assets.py --check
```

The generator verifies every upstream SHA-256 hash and its exact fontTools
version. It rewrites family and PostScript names, preserves text hint programs,
builds the complete icon catalog plus semantic aliases, and produces the files
under `res/fonts/` and `res/icons/`. `--check` regenerates into a temporary
directory and compares every output byte for byte without changing the working
tree.

## Verification

`TypographyTest` asserts that representative roles resolve to the expected
family, exact `Regular` or `Semibold` face, weight, pixel size, grayscale
strategy, and hinting preference. It verifies the TrueType hint tables through
`QRawFont`, checks the complete icon catalog against representative semantic
shortcuts, and renders Regular and Semibold to confirm that the real glyph
masks differ.
