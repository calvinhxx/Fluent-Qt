# Typography Resolution

## Contract

Fluent-Qt owns typography face resolution. Applications select semantic roles
such as `Body`, `BodyStrong`, or `Display`; they must not depend on a platform
font matcher to interpret Segoe UI Variable named instances differently.

`fluent::initializeResources()` registers four FluentQt-specific static faces:

| Family | Style | Source variable axes | Roles |
| --- | --- | --- | --- |
| `FluentQt Segoe UI Small` | Regular | `opsz=8`, `wght=400` | Caption |
| `FluentQt Segoe UI Text` | Regular | `opsz=10.5`, `wght=400` | Body, Body Large |
| `FluentQt Segoe UI Text` | Semibold | `opsz=10.5`, `wght=600` | Body Strong, Body Large Strong |
| `FluentQt Segoe UI Display` | Semibold | `opsz=36`, `wght=600` | Subtitle, Title, Title Large, Display |

The FluentQt prefix prevents collisions with a different Segoe UI Variable
version installed by Windows. Static instances are used on every supported Qt
version, including Qt 5.15 and Qt 6.2, so Windows DirectWrite, macOS CoreText,
and Linux Fontconfig/FreeType resolve the same optical size and real weight.

The original `SegoeUI-VF.ttf` remains bundled for compatibility and future
explicit variable-axis use. It is not the primary face selected by the built-in
typography roles.

## Startup behavior

Standalone applications call `fluent::initializeResources()` after creating
`QApplication`. Gallery and the hello-world example then set the application
default font to `Typography::Styles::Body`, ensuring raw Qt widgets inherit the
same Text Regular face as Fluent components.

If any bundled face cannot be registered, initialization returns `false`, emits
one warning, and installs substitutions through the original variable/system
family. This is a degraded fallback, not an exact typography result.

## Regeneration

The static assets are deterministically generated from `res/SegoeUI-VF.ttf` by:

```bash
python tools/fonts/generate_static_segoe.py
```

The script requires `fontTools`, pins both `opsz` and `wght`, rewrites family and
PostScript names, and produces the files under `res/fonts/`. Generated output
must remain byte-for-byte reproducible.

## Verification

`SegoeTest` asserts for each representative role that `QFontInfo` reports:

- the FluentQt-specific resolved family;
- the exact `Regular` or `Semibold` style;
- the expected weight and pixel size;
- `exactMatch() == true`.

It also renders Regular and Semibold to images and verifies that the actual
glyph masks differ. Minor antialiasing differences between DirectWrite,
CoreText, and FreeType remain expected; face selection, advance metrics, optical
size, and weight must not fall back.
