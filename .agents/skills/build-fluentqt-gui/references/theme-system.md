# Theme System

Use semantic theme tokens to express a product's visual language. Do not treat
the accent color as the whole theme.

## Establish evidence

Use this evidence order:

1. existing project design tokens, brand guide, and shipped UI;
2. official product pages or current application UI;
3. repository assets and screenshots;
4. a neutral built-in Fluent baseline when no brand evidence exists.

Record the source and whether it is authoritative or inferred. Extract roles,
not isolated pixels: canvas, layer, alternate layer, primary/secondary text,
control fills, strokes, accent, focus, critical, caution, information, success,
control radius, overlay radius, typography, and density.

## Choose the supported strategy

- Use the built-in Fluent theme when branding is not part of the task.
- Fluent is the only supported component geometry and interaction language.
  Use `fluent::UserTheme::apply()` to load the optional user-editable
  Fluent token override before constructing the main window.
- Use `fluent::UserTheme::applyAccentOverride()` when only the accent
  changes and the remaining semantic palette stays valid.
- For a C++ application-specific brand, start from
  `fluent::ThemeRegistry::defaultSnapshot()`, override complete Light and Dark
  semantic roles, then commit once with `applySnapshot()`.
- In PySide6, use the public `fluentqt.foundation` functions such as
  `apply_user_theme`, `set_accent_color`, `set_theme`, and `set_font_scale`.
  Do not invent a private full-palette API. If a full branded snapshot is
  required but not publicly bound, either scope the design to supported tokens
  or add and test the missing public binding in FluentQt.

Install tokens before constructing the main window. Persist the user's explicit
Light/Dark choice when the application has settings; a `system` choice may be
resolved at startup when cross-version dynamic system tracking is unavailable.

## Install window material with the theme

Color tokens are not the whole surface. An application-owned Fluent window
requests Mica (default) or Acrylic and lets unused canvas, pane gaps, and
chrome rest areas reveal that material. An embedded GUI inherits its
host-owned window instead. Follow [Premium shell](premium-shell.md) for both
paths.

- Call `Window::setBackdropEffect(BackdropEffect::Mica)` (or Acrylic) before
  show. Do not switch to `Solid` to make screenshots look simpler.
- Use `setAutoFillBackground(false)` on hosts and gaps intended to reveal the
  owning material. Set a collection view's `backgroundVisible` to false only
  when it belongs to that continuous canvas; retain one deliberate background
  when the view is a bounded surface. If it sits on a painted host, follow the
  complete parent-surface contract in [Premium shell](premium-shell.md)
  instead of clearing through it or letting Qt restore a default base fill.
- Never apply `Qt::WA_TranslucentBackground` to descendant content widgets;
  that punches a hole through painted-Mica fallback instead of revealing it.
- A helper that paints `bgCanvas` / `bgLayer` onto every `QWidget` is a theme
  defect, not a layer strategy. Use `Card` only for an independent object.
  Follow [Signature surface](signature-surface.md) for composers and pane
  chrome on material.

Solid is allowed only when a host, capture harness, or accessibility surface
requires an opaque backing store. Record `window_backdrop_reason` in the visual
evidence manifest. Opaque host fills require `surface_fill_reason`.

## Map the whole palette

Define both modes together. For each mode, verify:

- accent default/secondary/tertiary/disabled and text on accent;
- canvas, layer, alternate layer, and solid backgrounds;
- primary, secondary, tertiary, and disabled text;
- default, secondary, strong, card, divider, surface, and focus strokes;
- default, secondary, tertiary, alternate, subtle, and disabled controls;
- critical, caution, information, and success foreground/background pairs;
- neutral greys and charts when the application uses them;
- control and overlay radius.

Maintain contrast through semantic roles. Do not derive Dark mode by merely
inverting Light mode, and do not use the brand accent for every status.

## Keep the hierarchy restrained

- Canvas is the quietest large surface.
- Sidebars and inspectors normally use an alternate layer.
- Cards introduce hierarchy only when spacing alone is insufficient.
- Use one accent primary action per local decision region.
- Standard actions remain neutral; subtle actions belong in chrome or compact
  tool areas.
- Preserve small semantic status colors even in an otherwise monochrome theme.

## Audit raw Qt widgets

FluentQt controls receive registry updates automatically. Raw Qt widgets do
not. For every visible raw widget:

1. decide whether a FluentQt component can replace it;
2. otherwise apply semantic `QPalette` roles rather than global hard-coded QSS;
3. refresh the palette when the theme changes, using a `FluentElement`/
   `FluentWidget` bridge or an application theme notification;
4. check viewports, documents, selections, links, placeholders, disabled text,
   and popup/overlay surfaces in both modes.

Keep QSS local to behavior or geometry that tokens cannot express. Avoid an
application-wide stylesheet that bypasses component states.

## Theme acceptance gate

- Switching modes repaints the open window without restart.
- No visible control retains a Light background or dark text in Dark mode.
- Focus and disabled states remain legible.
- Brand literals are centralized in one application theme module.
- Theme token tests cover representative Light/Dark values and radii.
- An application-owned window requests Mica or Acrylic unless a Solid reason
  is recorded; an embedded surface preserves and records the host window.
- Pane gaps reveal the owning material rather than a stamped opaque palette,
  unless the host contract requires an opaque surface.
