#!/usr/bin/env python3

"""Generate crisp, deterministic Collection control images maintained in code."""

from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError as error:  # pragma: no cover - maintainer dependency guard
    raise SystemExit("Pillow is required: python -m pip install Pillow") from error


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "app" / "assets" / "control_images" / "collections"
SCALE = 4
CANVAS = 72


def _scaled(values: tuple[int, ...]) -> tuple[int, ...]:
    return tuple(value * SCALE for value in values)


def _data_grid() -> Image.Image:
    size = CANVAS * SCALE
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    tile_mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(tile_mask).rounded_rectangle(
        _scaled((3, 3, 68, 68)),
        radius=16 * SCALE,
        fill=255,
    )

    gradient = Image.new("RGBA", (size, size))
    pixels = gradient.load()
    top = (196, 145, 241)
    bottom = (105, 69, 167)
    for y in range(size):
        amount = y / max(1, size - 1)
        color = tuple(
            round(start * (1.0 - amount) + end * amount)
            for start, end in zip(top, bottom)
        )
        for x in range(size):
            pixels[x, y] = (*color, 255)
    canvas.alpha_composite(Image.composite(gradient, canvas, tile_mask))

    draw = ImageDraw.Draw(canvas)
    white = (255, 255, 255, 245)
    secondary = (255, 255, 255, 205)
    draw.rounded_rectangle(
        _scaled((15, 16, 57, 56)),
        radius=5 * SCALE,
        outline=white,
        width=2 * SCALE,
    )
    draw.rounded_rectangle(
        _scaled((16, 17, 56, 28)),
        radius=4 * SCALE,
        fill=(255, 255, 255, 38),
    )
    draw.line(_scaled((16, 29, 56, 29)), fill=white, width=2 * SCALE)
    for x in (29, 43):
        draw.line(_scaled((x, 17, x, 55)), fill=secondary, width=SCALE)
    for y in (38, 47):
        draw.line(_scaled((16, y, 56, y)), fill=secondary, width=SCALE)

    image = canvas.resize((CANVAS, CANVAS), Image.Resampling.LANCZOS)
    image.putalpha(
        image.getchannel("A").point(lambda alpha: 0 if alpha <= 1 else alpha)
    )
    return image


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    _data_grid().save(OUTPUT / "DataGrid.png", format="PNG", optimize=True)
    print("generated DataGrid Collection control image")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
