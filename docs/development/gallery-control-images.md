# Gallery Control Images

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Gallery and site

[← AI-assisted GUI verification](gui-verification-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Tooltip Usage →](tooltip-usage.md)
<!-- docs-nav:top:end -->

Rules for Gallery component-card artwork under
`app/assets/control_images/`.

## When to Use

- Adding a new Gallery component or foundation topic card image
- Regenerating or replacing an existing control icon
- Reviewing whether new artwork matches an existing category family

## File Layout

- Path: `app/assets/control_images/<category-id>/<Title>.png`
- `<category-id>` matches `GalleryComponentCategory.id`
  (for example `layout`, `status-info`, `foundation`)
- `<Title>` matches the Gallery card title
  (for example `Card.png`, `Toast.png`, `FontIcon.png`)
- Register every new file in `app/gallery_resources.qrc`
- Resolve images through `galleryControlImageResource()`; do not hard-code
  fallbacks that reuse unrelated category artwork

## Canvas and Alpha

- Size: **72 × 72** PNG with an alpha channel (`Format32bppArgb`)
- **Canvas background must be transparent**
- The painted tile is a rounded square inset inside the 72 × 72 canvas
  (typical inset ≈ 3 px, corner radius ≈ 16 px)
- Corner pixels outside the rounded tile must be fully transparent
  (`alpha = 0`), matching existing assets such as
  `foundation/Iconography.png` and `status-info/Shimmer.png`
- Do **not** ship icons with opaque white, black, or near-opaque fringe
  filling the square outside the rounded tile

When generating artwork with an image model, assume the model may emit an
opaque full-bleed square. Always post-process with a transparent rounded-rect
mask before committing.

Gallery cards reserve a 40 × 40 logical-pixel icon slot but center bitmap
artwork in a 36 × 36 rectangle. This maps a 72 × 72 source one-for-one on a
2x backing store instead of blurring it through a 72 → 80 upscale. Do not
enlarge the bitmap to fill the slot in application or binding code. FontIcon
glyph tiles may use the complete 40 × 40 slot because they render at the target
device resolution.

## Category Color Families

Icons in the same category share one background color family. Glyphs stay
high-contrast (usually white) on that family:

| Category id | Shared family |
| --- | --- |
| `foundation` | purple / indigo |
| `basic-input` | blue |
| `status-info` | teal / cyan |
| `layout` | coral / terracotta |
| `scrolling` | yellow / amber |
| `menus-toolbars` | purple |
| `collections` | purple |
| `text-fields` | blue |

When adding an icon to an existing category, sample neighboring icons in that
folder and match their hue family. Do not invent a new accent color per
control.

## Generation Checklist

1. Match the category color family above.
2. Keep the motif simple enough to read at 72 × 72.
3. Export or resize to 72 × 72 PNG.
4. Apply a transparent rounded-rect mask so canvas corners are alpha 0.
5. Add the file under the correct `control_images/<category-id>/` folder.
6. Register it in `app/gallery_resources.qrc`.
7. Rebuild Gallery and confirm the card image on light and dark chrome.
8. Run `python tools/gallery/normalize_control_images.py`. Use `--fix` only
   when importing a legacy image whose canvas is not 72 × 72; smaller images
   are centered without enlarging their artwork, while oversized canvases are
   proportionally reduced.

The five Layout-family tiles are deterministic assets. Regenerate them with
`python tools/gallery/generate_layout_control_images.py` so their coral fill,
line weight, radius, and alpha treatment stay identical.

## Verification

Quick alpha sanity check for a candidate icon:

- pixel `(0, 0)` alpha is `0`
- transparent pixel ratio is roughly in the same band as neighboring icons in
  that category (often about 15–25% for full rounded tiles)
- opaque content stays inside the rounded tile, not flush to the bitmap edge

<!-- docs-nav:bottom:start -->
---
[← AI-assisted GUI verification](gui-verification-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Tooltip Usage →](tooltip-usage.md)
<!-- docs-nav:bottom:end -->
