#!/usr/bin/env python3
"""Generate SplashScreen's deterministic teal Gallery tile."""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SCALE = 4


def main() -> None:
    image = Image.new("RGBA", (72 * SCALE, 72 * SCALE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    def box(*values: int) -> tuple[int, ...]:
        return tuple(value * SCALE for value in values)

    white = (255, 255, 255, 245)
    draw.rounded_rectangle(box(3, 3, 68, 68), radius=16 * SCALE,
                           fill=(15, 166, 172, 255))
    draw.rounded_rectangle(box(15, 15, 57, 59), radius=5 * SCALE,
                           outline=white, width=2 * SCALE)
    draw.line(box(16, 24, 56, 24), fill=(255, 255, 255, 170), width=SCALE)
    draw.ellipse(box(20, 19, 22, 21), fill=white)
    draw.rounded_rectangle(box(30, 30, 42, 42), radius=3 * SCALE, fill=white)
    draw.arc(box(32, 47, 40, 55), start=-90, end=170, fill=white, width=2 * SCALE)
    output = ROOT / "app/assets/control_images/status-info/SplashScreen.png"
    image.resize((72, 72), Image.Resampling.LANCZOS).save(output, optimize=True)
    print(output.relative_to(ROOT))


if __name__ == "__main__":
    main()
