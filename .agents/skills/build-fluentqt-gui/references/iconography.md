# Iconography

Use one coherent icon system so controls read as one desktop product rather
than a collection of unrelated assets. Borrow the rigor of mature web design
systems—one family, named size tokens, normalized geometry, semantic states—
while preserving desktop hit targets, accessibility, scaling, and Qt theme
behavior.

## Contents

- Choose one source and record provenance
- Decide when to reuse, repair, or generate
- Define grid and optical sizes
- Optimize the application identity assets
- Derive a palette seed from the approved identity
- Use outline and filled states deliberately
- Color icons by semantic role
- Make icon-only actions accessible
- Implement with FluentQt and Qt
- Iconography acceptance gate

## Choose one source and record provenance

Prefer the icon source already exposed by FluentQt or the target project's
established, licensed vector set. A project-owned extension may add missing
domain symbols, but it must normalize to the same grid, stroke character,
corner treatment, and optical weight.

Record the family name, package or repository path, license/provenance, and
whether any glyph is project-owned. Do not copy an icon from a screenshot or
silently import a frontend package into a desktop binary. Do not mix icon packs
inside one navigation, toolbar, list, or action hierarchy merely because each
contains a convenient glyph.

Reject these substitutes for product icons:

- emoji, whose rendering changes by platform and font;
- arbitrary Unicode characters used as arrows, tools, status, or overflow;
- a raster image scaled into several control sizes;
- text abbreviations where a familiar semantic icon exists;
- decorative symbols with no action, status, or domain meaning.

If no existing icon communicates a domain action, keep a short text action or
create one licensed project-owned vector. A novel ambiguous glyph is not an
upgrade over clear copy.

## Decide when to reuse, repair, or generate

Inventory official marks, application icons, favicons, package artwork, vector
sources, and license terms before generating anything. Use this decision order:

1. **Reuse** an authoritative, current asset when its provenance and intended
   product relationship are clear.
2. **Repair** it when the concept is correct but padding, silhouette, contrast,
   source resolution, or platform packaging is weak.
3. **Extend** the established family for missing domain actions while matching
   its grid, stroke, corner character, and optical weight.
4. **Generate** a new identity only when no suitable asset exists or the user
   explicitly requests a new identity.

For generation, translate the product's primary object, verb, material, and
tempo into three genuinely different silhouettes. Avoid the default AI visual
vocabulary—sparkles, generic robots, chat bubbles, glowing brains, and abstract
hexagons—unless the product evidence specifically requires it. Use an available
image-generation capability for broad exploration or construct vector-native
candidates directly. Resolve the selected candidate as a clean vector or
high-resolution master; never ship pseudo-text, a screenshot crop, or the first
unreviewed generated image.

Show candidates at native launcher size, 32 px, and 16 px on light, dark, and
neutral backgrounds. A new identity or material brand change requires human
selection. A compatibility wrapper must not imply official endorsement or
silently replace a valid upstream mark.

## Define grid and optical sizes

Record one source grid plus compact glyph, standard glyph, and action-slot
sizes in the design brief. Use 4 px-aligned tokens; a useful desktop starting
point is a 20 px source grid, 16 px compact glyph, 20 px standard glyph, and
24–28 px action slot. The owning component may require a larger accessible hit
target than the visible icon.

Align the optical shape, not only the asset rectangle:

- normalize view boxes and remove accidental transparent padding;
- center asymmetric arrows, play symbols, and chevrons by perceived weight;
- keep baseline-adjacent icons visually aligned with their text cap height;
- reserve a stable icon slot so labels do not shift between rows or states;
- compare peer actions at native scale rather than judging enlarged previews;
- never fix one visibly small glyph by enlarging only that control's layout.

Use one size per hierarchy. Compact metadata actions may be smaller than the
primary toolbar, but peers in either group keep the same slot and optical
weight.

## Optimize the application identity assets

Keep one editable master and derive platform outputs from it. The optimization
pass must:

- remove accidental canvas padding and center the perceived silhouette;
- simplify detail that disappears at 16–32 px;
- preserve a stable safe area instead of scaling the mark to every edge;
- test the shape in grayscale and against light, dark, and desktop backgrounds;
- avoid baked shadows or backgrounds on the transparent in-product mark;
- produce the target platform's required multi-resolution resources;
- verify the packaged binary rather than only the source PNG or SVG.

Do not enlarge a blurry raster and call it a high-resolution icon. Regenerate
or redraw from a vector-quality source. Compare app-file, launcher/Dock/taskbar,
switcher, title-bar, and settings/about appearances because operating systems
may cache or mask them differently.

## Separate the application icon from the in-app mark

Treat these as related assets with different jobs, not one image reused at two
scales:

- The file-system, launcher, Dock, taskbar, and app-switcher icon is a complete
  application tile. Follow the target platform's silhouette, safe area, and
  multi-resolution packaging. On macOS, use an opaque rounded container inside
  the icon canvas (with transparent outer corners where the asset format
  expects them); never leave a colored brand glyph floating directly on an
  unknown desktop or Dock background.
- The title-bar or compact in-product mark is the transparent brand glyph. It
  must not carry the launcher tile, shadow, or baked background, and should be
  rendered from vector or a scale-appropriate source at its native optical
  size.

Derive both from the same approved mark and palette, but keep separate source
files and resource names. Inspect Finder or Explorer, Dock or taskbar, app
switcher, and title bar independently; a correct title glyph does not prove the
packaged application icon is correct.

## Derive a palette seed from the approved identity

Treat the identity as evidence, not a bucket of pixels to copy blindly. Record:

- one dominant chromatic anchor from the mark, excluding transparent pixels,
  white tile backgrounds, shadows, and anti-aliased edge noise;
- one optional supporting hue only when the mark or product world justifies it;
- the neutral temperature suggested by the identity and subject materials;
- which roles must remain semantic and independent, especially critical,
  caution, information, and success.

Use the anchors to build a restrained semantic palette through
[Theme system](theme-system.md). The mark, focus, selected state, and primary
action may share a family relationship; routine chrome should remain neutral.
Do not sample every visible icon color into the interface or tint every pane,
border, and action with the brand hue.

## Use outline and filled states deliberately

Choose a default outline or filled character and record it. Filled variants
are reserved for a meaningful state such as selected, active, recording, or a
critical semantic status. They are not random decoration and must not make one
ordinary action appear primary merely because that asset happens to be filled.

Keep hover, pressed, focus, checked, selected, and disabled changes in the
owning FluentQt control whenever possible. Do not swap to an unrelated glyph
family for one state. Motion between icons is allowed only when the change
communicates state and remains readable with reduced motion.

## Color icons by semantic role

Default action and navigation icons inherit the owning surface's semantic
foreground role. Secondary icons use the same muted role as their peer text.
Use accent or status color only when the icon itself communicates the active,
success, warning, or error state.

Do not hard-code a Light-only gray, use several accent colors in one toolbar,
or paint every icon with brand color. Verify contrast in Light and Dark themes,
including disabled, selected, hover, focus, and material-backed states. A
multicolor illustration belongs to content, onboarding, or an empty state—not
to routine chrome.

## Make icon-only actions accessible

Use icon-only actions only when the symbol is familiar in context or screen
space is genuinely constrained. Every icon-only control requires:

- a programmatic accessible name describing the action, not the glyph;
- a tooltip when the meaning is not persistently visible nearby;
- a stable keyboard-focusable target and visible focus cue;
- hover, pressed, checked when applicable, and disabled states;
- a hit target appropriate to the owning FluentQt component;
- no information conveyed by color or icon shape alone when a status label is
  needed for comprehension.

Pair destructive, uncommon, permission-sensitive, or high-cost actions with
text when ambiguity would be consequential. Overflow does not excuse unnamed
actions.

## Implement with FluentQt and Qt

Prefer a FluentQt component that already owns its icon state and spacing.
Verify the public header or Python API, Gallery sample, theme behavior, and
focused test before inventing custom painting. Keep one application-level map
from semantic action ids to icon assets; do not scatter filesystem paths or
glyph code points through widgets.

For custom assets:

- keep vector sources when possible and package them through the target's
  established resource system;
- preserve high-DPI rendering through `QIcon` or an equivalent vector-aware
  path instead of caching one low-resolution pixmap;
- map foreground color from semantic theme state rather than permanently
  recoloring the source asset;
- give custom icon buttons the same layout slot, state model, accessibility,
  and lifetime as their FluentQt peers;
- test missing-resource behavior so a broken asset does not collapse layout or
  expose a raw path.

Do not add a second design-language enum or icon-style branch to FluentQt.
Product-specific domain icons stay in the application layer unless they are a
reusable, documented component capability.

## Review at actual desktop conditions

Inspect icon groups in the final built application at normal and narrow widths,
Light and Dark themes, supported display scales, and native screenshot size.
Review selected, focus, hover, pressed, and disabled states plus any state that
changes outline to filled.

Record a visual finding when:

- peer icons have visibly different stroke weight or corner character;
- a glyph is optically off-center even when its rectangle is centered;
- icon and label baselines wander across repeated rows;
- an icon becomes muddy, clipped, or too small at a supported scale;
- icon color competes with the primary action or loses semantic contrast;
- an icon-only action is ambiguous or lacks accessible state.

## Iconography acceptance gate

Before concept approval:

- one family and provenance are recorded;
- the reuse, repair, extend, or generate decision is recorded;
- generated identities have three distinct candidates and a human selection;
- grid, glyph, and action-slot sizes are explicit;
- application tile, in-product mark, small-size validation, and palette seed
  strategies are explicit;
- every concept states how the shared family supports its visual direction;
- outline/filled, color, and icon-only policies are defined;
- comps use real icons rather than emoji, arbitrary glyphs, or mixed packs.

Before final visual acceptance:

- the independent `iconography` score is at least 4/5 with final-build
  evidence;
- Light/Dark, scale, focus, disabled, selected, and narrow conditions pass;
- peer actions share optical weight, slots, and state treatment;
- operating-system application icons use a platform-appropriate complete tile,
  while compact in-app marks remain transparent and crisp;
- the packaged application icon is inspected at launcher and 16–32 px detail,
  and the approved identity visibly relates to—but does not overwhelm—the
  semantic application palette;
- no icon source, license, accessible name, or missing-resource boundary is
  unresolved.
