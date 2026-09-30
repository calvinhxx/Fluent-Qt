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

- Master: `tools/gallery/artwork/<category-id>/<Title>.svg`
- Export: `app/assets/control_images/<category-id>/<Title>.png`
- Every PNG has one matching SVG master with the same category and title.
  `Placeholder.svg` and `Placeholder.png` sit directly in their respective roots.
- `<category-id>` matches `GalleryComponentCategory.id`
  (for example `layout`, `status-info`, `foundation`)
- `<Title>` matches the Gallery card title
  (for example `Card.png`, `Toast.png`, `FontIcon.png`)
- Register the SVG master in `app/gallery_resources.qrc`, using an
  `assets/control_images/<category-id>/<Title>.svg` alias and the relative
  `../tools/gallery/artwork/<category-id>/<Title>.svg` source path.
- Resolve images through `galleryControlImageResource()`; do not hard-code
  fallbacks that reuse unrelated category artwork
- Edit the SVG master and regenerate its PNG. C++, Python, and WebAssembly
  display and package the SVG masters; PNGs remain checked-in exports.
- Masters contain self-contained vector geometry on a `0 0 72 72` view box.
  Outline text before saving; embedded bitmaps, external references, and live
  font-dependent text are rejected by the exporter.
- Use basic paths, shapes, groups, and linear/radial gradients supported by
  Qt 5.15 and Qt 6. The exporter rejects unsupported clipping and newer SVG
  masks, filters, patterns, symbols, and markers. Bake such effects into
  ordinary vector geometry when authoring artwork.

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

Author rounded shapes and transparency in the SVG. The shared exporter renders
at 4x resolution with Qt SVG and downsamples with Qt's smooth image scaler.
Preserve partial-alpha edge pixels rather than thresholding curves to opaque
or transparent pixels.

Gallery cards reserve a 40 × 40 logical-pixel icon slot and center SVG artwork
in a 36 × 36 rectangle. `GalleryArtwork` and its Python counterpart render the
SVG at the paint device's pixel ratio: 45 × 45 at 125%, 72 × 72 at 2x, and
108 × 108 at 3x. They cache parsed documents with a 128-entry limit and reuse
Qt's memory-bounded pixmap cache, keyed by source, physical size, and pixel
ratio. Warm scrolling and repainting reuse those pixels; a scale change gets
a fresh SVG render. FontIcon glyph tiles use the complete 40 × 40 slot.

Qt SVG is a Gallery dependency. The base `FluentQt` library remains independent
of it. The Python Gallery uses the Qt SVG module supplied by PySide6.

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
| `charts` | steel blue |
| `spatial` | indigo |
| `text-fields` | blue |

When adding an icon to an existing category, sample neighboring icons in that
folder and match their hue family. Do not invent a new accent color per
control.

## Generation Checklist

1. Match the category color family above.
2. Keep the motif simple enough to read at 72 × 72.
3. Save an editable SVG under the matching `artwork/<category-id>/` folder.
4. Export the PNG with the shared tool below. Canvas corners remain alpha 0
   and curved edges retain partial coverage.
5. Keep the export under the matching `control_images/<category-id>/` folder.
6. Register the SVG master and its runtime alias in `app/gallery_resources.qrc`.
7. Rebuild Gallery and confirm the card image on light and dark chrome.
8. Run the source/export freshness check and the canvas/resource audit below.

## Export All Categories

Use the matched PySide6 development environment described in the
[binding build guide](../../bindings/pyside6/README.md#build-from-source).
The exporter uses PySide6's Qt SVG implementation and creates no desktop window:

```bash
python3 tools/gallery/export_control_images.py
python3 tools/gallery/export_control_images.py --check
python3 tools/gallery/normalize_control_images.py
```

Pass category/title keys to update or check a focused set:

```bash
python3 tools/gallery/export_control_images.py navigation/Stepper collections/Timeline
python3 tools/gallery/export_control_images.py --check navigation/Stepper
```

`--check` compares decoded pixels without rewriting files. It reports stale or
missing PNGs and PNGs without a corresponding master. The canvas audit also
checks source coverage and registration in `app/gallery_resources.qrc`.
The shared CI and release preflight run `validate_control_artwork.py` without
Qt or Pillow to check SVG compatibility, paired export coverage, resource
aliases, and duplicate registrations. Pixel freshness and alpha checks use
the tools above in the matched development environment.
All categories use this exporter; category-specific drawing scripts are not
alternative sources of truth.

The project owns the SVG illustrations. Outlined text uses the bundled Inter
fonts, while FileDropZone and FileListView retain glyph outlines from the
bundled Fluent UI System Icons font. See the font and icon attribution in
[third-party notices](../../THIRD_PARTY_NOTICES.md).

## Verification

Quick alpha sanity check for a candidate icon:

- pixel `(0, 0)` alpha is `0`
- transparent pixel ratio is roughly in the same band as neighboring icons in
  that category (often about 15–25% for full rounded tiles)
- opaque content stays inside the rounded tile, not flush to the bitmap edge

The audit also detects sustained hard steps along transparent silhouettes.
It ignores pixel-aligned straight edges and isolated opaque pixels at curve
tangents. This catches hard rounded masks even when the artwork contains
semi-transparent pixels elsewhere; it does not replace visual review of the
interior artwork or scaled rendering. `--fix` only normalizes canvas size and
does not blur or repair an aliased outline.

Check the detector's positive and negative cases with:

```bash
python3 tools/gallery/test_normalize_control_images.py
python3 tools/gallery/test_export_control_images.py
```

<!-- docs-nav:bottom:start -->
---
[← AI-assisted GUI verification](gui-verification-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Tooltip Usage →](tooltip-usage.md)
<!-- docs-nav:bottom:end -->
