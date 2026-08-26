#!/usr/bin/env python3

"""Generate the deterministic MultiSelectComboBox Gallery tile."""

from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError as error:  # pragma: no cover - maintainer dependency guard
    raise SystemExit("Pillow is required: python -m pip install Pillow") from error


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = (
    ROOT
    / "app"
    / "assets"
    / "control_images"
    / "basic-input"
    / "MultiSelectComboBox.png"
)
SCALE = 4
CANVAS = 72
TILE = (3, 3, 68, 68)
GLYPH = (255, 255, 255, 245)
SECONDARY = (255, 255, 255, 205)


def _scaled(values: tuple[int, ...]) -> tuple[int, ...]:
    return tuple(value * SCALE for value in values)


def main() -> int:
    size = CANVAS * SCALE
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    gradient = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = gradient.load()
    for y in range(size):
        for x in range(size):
            progress = min(1.0, max(0.0, (x + y - 6 * SCALE) / (124 * SCALE)))
            pixels[x, y] = (
                round(43 * (1.0 - progress)),
                round(181 * (1.0 - progress) + 82 * progress),
                round(238 * (1.0 - progress) + 150 * progress),
                255,
            )

    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        _scaled(TILE), radius=16 * SCALE, fill=255
    )
    image.alpha_composite(Image.composite(gradient, image, mask))

    draw = ImageDraw.Draw(image)
    for top in (17, 31, 45):
        draw.rounded_rectangle(
            _scaled((16, top, 25, top + 9)),
            radius=2 * SCALE,
            outline=GLYPH,
            width=2 * SCALE,
        )
        draw.line(
            _scaled((32, top + 4, 54, top + 4)),
            fill=SECONDARY,
            width=2 * SCALE,
        )

    for top in (17, 45):
        draw.line(
            _scaled((18, top + 4, 20, top + 6, 24, top + 2)),
            fill=GLYPH,
            width=2 * SCALE,
            joint="curve",
        )

    image = image.resize((CANVAS, CANVAS), Image.Resampling.LANCZOS)
    image.putalpha(
        image.getchannel("A").point(lambda alpha: 0 if alpha <= 1 else alpha)
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT, format="PNG", optimize=True)
    print(f"generated {OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
