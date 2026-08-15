#!/usr/bin/env python3

"""Generate the crisp, deterministic Layout control-image family."""

from __future__ import annotations

from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError as error:  # pragma: no cover - maintainer dependency guard
    raise SystemExit(
        "Pillow is required: python -m pip install Pillow"
    ) from error


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "app" / "assets" / "control_images" / "layout"
SCALE = 4
CANVAS = 72
TILE = (3, 3, 68, 68)
CORAL = (242, 92, 61, 255)
GLYPH = (255, 255, 255, 245)
SECONDARY = (255, 255, 255, 205)


def _scaled(values: tuple[int, ...]) -> tuple[int, ...]:
    return tuple(value * SCALE for value in values)


def _canvas() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new(
        "RGBA",
        (CANVAS * SCALE, CANVAS * SCALE),
        (0, 0, 0, 0),
    )
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        _scaled(TILE),
        radius=16 * SCALE,
        fill=CORAL,
    )
    return image, draw


def _rounded_rect(
    draw: ImageDraw.ImageDraw,
    rect: tuple[int, int, int, int],
    *,
    radius: int = 4,
    width: int = 2,
) -> None:
    draw.rounded_rectangle(
        _scaled(rect),
        radius=radius * SCALE,
        outline=GLYPH,
        width=width * SCALE,
    )


def _line(
    draw: ImageDraw.ImageDraw,
    points: tuple[int, ...],
    *,
    fill: tuple[int, int, int, int] = GLYPH,
    width: int = 2,
) -> None:
    draw.line(
        _scaled(points),
        fill=fill,
        width=width * SCALE,
        joint="curve",
    )


def _accordion(draw: ImageDraw.ImageDraw) -> None:
    _rounded_rect(draw, (19, 15, 53, 57), radius=5)
    for y in (29, 43):
        _line(draw, (20, y, 52, y), fill=SECONDARY, width=1)
    for y in (22, 36, 50):
        _line(draw, (24, y, 40, y), width=2)
        _line(draw, (45, y - 1, 48, y + 2), width=1)
        _line(draw, (48, y + 2, 51, y - 1), width=1)


def _card(draw: ImageDraw.ImageDraw) -> None:
    _rounded_rect(draw, (18, 20, 54, 52), radius=5)
    _line(draw, (24, 31, 48, 31), width=2)
    _line(draw, (24, 38, 43, 38), fill=SECONDARY, width=2)


def _divider(draw: ImageDraw.ImageDraw) -> None:
    _line(draw, (19, 25, 53, 25), fill=SECONDARY, width=2)
    _line(draw, (25, 36, 47, 36), width=2)
    _line(draw, (19, 47, 53, 47), fill=SECONDARY, width=2)


def _expander(draw: ImageDraw.ImageDraw) -> None:
    _rounded_rect(draw, (18, 16, 54, 56), radius=5)
    _line(draw, (19, 30, 53, 30), fill=SECONDARY, width=1)
    _line(draw, (24, 23, 40, 23), width=2)
    _line(draw, (45, 22, 48, 25), width=1)
    _line(draw, (48, 25, 51, 22), width=1)
    _line(draw, (24, 38, 48, 38), width=2)
    _line(draw, (24, 45, 43, 45), fill=SECONDARY, width=2)


def _field(draw: ImageDraw.ImageDraw) -> None:
    _line(draw, (20, 18, 34, 18), width=2)
    _line(draw, (39, 16, 39, 20), width=1)
    _line(draw, (37, 18, 41, 18), width=1)
    _rounded_rect(draw, (19, 25, 53, 42), radius=4)
    _line(draw, (24, 33, 43, 33), fill=SECONDARY, width=2)
    _line(draw, (20, 49, 41, 49), fill=SECONDARY, width=2)
    draw.ellipse(_scaled((46, 46, 54, 54)), outline=GLYPH, width=SCALE)
    _line(draw, (48, 50, 50, 52), width=1)
    _line(draw, (50, 52, 53, 48), width=1)


def _write(name: str, painter) -> None:
    image, draw = _canvas()
    painter(draw)
    image = image.resize(
        (CANVAS, CANVAS),
        Image.Resampling.LANCZOS,
    )
    # Lanczos can leave alpha=1 ringing three pixels outside the authored tile.
    # Remove only that invisible residue so transparent canvas bounds remain
    # deterministic without hardening the visible antialiased edge.
    image.putalpha(
        image.getchannel("A").point(
            lambda alpha: 0 if alpha <= 1 else alpha
        )
    )
    image.save(OUTPUT / f"{name}.png", format="PNG", optimize=True)


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, painter in (
        ("Accordion", _accordion),
        ("Card", _card),
        ("Divider", _divider),
        ("Expander", _expander),
        ("Field", _field),
    ):
        _write(name, painter)
    print("generated 5 Layout control images")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
