#!/usr/bin/env python3
"""Draw Spatial's viewport and transform-handle glyphs on the Gallery canvas."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SCALE = 4


def main():
    for name in ("SpatialView", "SpatialItem"):
        image = Image.new("RGBA", (72 * SCALE, 72 * SCALE))
        draw = ImageDraw.Draw(image)

        def scaled(point):
            return tuple(round(value * SCALE) for value in point)

        def line(points, color="white", width=2):
            draw.line([scaled(p) for p in points], fill=color, width=width * SCALE,
                      joint="curve")

        draw.rounded_rectangle(scaled((3, 3, 68, 68)), radius=16 * SCALE,
                               fill="#5965BC")
        if name == "SpatialView":
            line([(16, 24), (16, 17), (24, 17)])
            line([(48, 17), (56, 17), (56, 24)])
            line([(16, 48), (16, 55), (24, 55)])
            line([(48, 55), (56, 55), (56, 48)])
            line([(23, 26), (44, 22), (49, 44), (27, 49), (23, 26)], "#B5BEF4")
            line([(29, 25), (50, 29), (45, 49), (25, 44), (29, 25)])
        else:
            line([(23, 24), (47, 20), (51, 45), (27, 51), (23, 24)])
            line([(35, 37), (35, 16)], "#C9D8FF")
            line([(35, 37), (57, 40)], "#C9D8FF")
            line([(35, 37), (19, 53)], "#C9D8FF")
            draw.ellipse(scaled((32, 34, 38, 40)), fill="white")
        output = ROOT / "app/assets/control_images/spatial" / (name + ".png")
        output.parent.mkdir(parents=True, exist_ok=True)
        image.resize((72, 72), Image.Resampling.LANCZOS).save(output, optimize=True)
        print(output.relative_to(ROOT))


if __name__ == "__main__":
    main()
