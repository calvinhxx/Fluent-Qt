# Custom themes and component overrides

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Fluent design](README.md) › Design references

[← Fluent (Windows): design reference](fluent.md) · [Contents](../SUMMARY.md) · [Fluent design index](README.md) · [Fluent Design Kit Source →](figma-sources.md)
<!-- docs-nav:top:end -->

FluentQt supports custom colors, corner radii, font families, and font scaling
within the Fluent visual system. Use global configuration for the whole
application and local overrides for individual components. Light, Dark, and
HighContrast remain the theme modes; customization needs no new theme enum.

## Global customization

`UserTheme::applyOverrides()` merges configuration into the current global
theme without reading or writing files. Unspecified fields keep their current
values. Components refresh automatically when the configuration changes.
The method returns `true` when state changes, or `false` when the input is
invalid or unchanged.

```cpp
#include <FluentQt/FluentQt.h>
#include <QJsonObject>

fluent::UserTheme::applyOverrides({
    {"light", QJsonObject{{"accentDefault", "#006B5E"}}},
    {"dark", QJsonObject{{"accentDefault", "#60DCC8"}}},
    {"radius", QJsonObject{{"control", 6}, {"overlay", 10}}},
    {"font", QJsonObject{{"scale", 1.15}}}
});
```

```python
import fluentqt

fluentqt.apply_theme_overrides({
    "light": {"accentDefault": "#006B5E"},
    "dark": {"accentDefault": "#60DCC8"},
    "radius": {"control": 6, "overlay": 10},
    "font": {"scale": 1.15},
})
```

When only `accentDefault` is supplied, the secondary accent colors and
on-accent text colors are derived from it. Set them explicitly in the same
configuration when exact values are needed. Omitting `contrast` preserves
the current high-contrast palette.

C++ callers can also use `ThemeRegistry::extendedSnapshot()` and
`applyExtendedSnapshot()` to submit a complete, typed configuration.
`ThemeRegistry::resetToDefaults()`, or `reset_theme_tokens()` in Python,
restores global built-in values without clearing component overrides.

## Per-component overrides

Components that inherit `FluentElement` support `setThemeOverrides()`. It
replaces the component's entire previous override configuration. Unspecified
fields follow the latest global theme rather than a snapshot taken when the
overrides were set.

```cpp
auto* button = new fluent::basicinput::Button("Local action", parent);
button->setThemeOverrides({
    {"light", QJsonObject{{"textPrimary", "#7A2454"}}},
    {"dark", QJsonObject{{"textPrimary", "#FFABD8"}}},
    {"radius", QJsonObject{{"control", 12}}},
    {"font", QJsonObject{{"scale", 1.25}}}
});

// Clear local token overrides and follow the current global theme again.
button->clearThemeOverrides();
```

```python
button = fluentqt.Button("Local action")
fluentqt.set_widget_theme_overrides(button, {
    "light": {"textPrimary": "#7A2454"},
    "dark": {"textPrimary": "#FFABD8"},
    "radius": {"control": 12},
    "font": {"scale": 1.25},
})
saved = fluentqt.widget_theme_overrides(button)
fluentqt.set_widget_theme_overrides(button, {})  # Restore global tokens.
```

Local overrides apply only to the current `FluentElement`. They do not
propagate to children, siblings, or separately composed popups. A custom
delegate that reads its host's tokens uses the host's overrides; independent
components keep their own configuration. Use global configuration for an
application-wide change, or the component's public API to adjust part of a
composite control. Plain `QWidget` instances do not support these token
overrides; the Python setter returns `False` for them.

`themeOverrides()` / `widget_theme_overrides()` returns a copy of the stored
local configuration. Writing the same configuration again does not trigger a
refresh. To change one field, read the configuration, edit it, and write the
whole configuration back. Colors, radii, and fonts are overridden independently;
setting a font size does not remove colors from theme management.

## Font precedence

For components that support the explicit-font contract, such as Button,
ToggleSwitch, ComboBox, DatePicker, and TimePicker, the order is:

1. The caller's explicit `setFont()`.
2. The component's local `font` token overrides.
3. The global font configuration and current `fontRole`.

`setFontRole(component->fontRole())` clears the explicit font and restores
role-based resolution. Any local `font` overrides still apply.
`clearThemeOverrides()` clears token overrides without clearing a separate
`setFont()` choice. See [Typography resolution](../architecture/typography-resolution.md)
for the full contract.

Local `font.scale` is an absolute multiplier of the built-in role size; it is
not multiplied by the global scale. Use `QFont::setPixelSize()` or
`setPointSizeF()` for an exact size. A token affects a component only if the
component uses it. This API does not expose every QWidget property or
compile-time dimension as a configurable field.

For example, Body's built-in size is 14 px. A global scale of 1.5 makes it
21 px; a local scale of 2.0 makes it 28 px. Overriding only `font.family`
retains the global scale and the weights of roles such as BodyStrong and Title.
A custom family does not inherit the built-in font's `styleName`. Qt matches
available faces by weight, so the result depends on which weights the family
provides.

## Configuration fields and files

| Field | Value and constraints |
| --- | --- |
| `light` / `dark` / `contrast` | Color objects, such as `textPrimary`, `controlDefault`, `accentDefault`, `bgCanvas`, and `strokeDefault`; see the exported template for all fields |
| `radius` | `none`, `control`, and `overlay`; integers from 0 to 64 |
| `font.family` | Font-family string; an empty string restores the built-in family |
| `font.scale` | Number from 0.5 to 4.0 |

Colors are strings in `#RRGGBB`, `#RRGGBBAA`, or QColor named-color format.
Alpha is the last pair in the eight-digit hexadecimal format. Runtime APIs
take the configuration object described above, without the file's
`schemaVersion` / `theme` / `overrides` wrapper. Unknown fields, incorrect types,
or out-of-range values reject the entire runtime update and leave existing
state unchanged.

For file-based configuration, C++ callers can explicitly export an editable
template with `UserTheme::exportTemplate()` and find its path with
`UserTheme::filePath()`. After editing `fluent.json`, call `UserTheme::apply()`
or Python's `apply_user_theme()`. This restores the built-in theme before
loading the file, replacing the previous global in-memory configuration.
Calling `apply()` does not create or rewrite the file. File loading retains
its existing error-tolerance rules, which differ from strict runtime validation.

## Validation scope

`TestThemeOverrides.cpp` covers atomic global updates, local isolation, theme
switching, global configuration changes, default restoration, invalid input,
and font precedence. The Python bindings have corresponding regression tests.
Local overrides do not guarantee adequate contrast for custom colors. Check
text, focus, and disabled states after changing interaction colors.

<!-- docs-nav:bottom:start -->
---
[← Fluent (Windows): design reference](fluent.md) · [Contents](../SUMMARY.md) · [Fluent design index](README.md) · [Fluent Design Kit Source →](figma-sources.md)
<!-- docs-nav:bottom:end -->
