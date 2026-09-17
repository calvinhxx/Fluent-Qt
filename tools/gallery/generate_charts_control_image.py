#!/usr/bin/env python3
"""Draw chart-specific Gallery glyphs on the shared 72px canvas."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
NAMES = ("ChartView", "LineChart", "AreaChart", "BarChart", "HorizontalBarChart",
         "PieChart", "DonutChart", "ScatterChart", "Sparkline")


def main():
    scale = 4
    for name in NAMES:
        image = Image.new("RGBA", (72 * scale, 72 * scale))
        draw = ImageDraw.Draw(image)
        def box(coords):
            return tuple(round(n * scale) for n in coords)
        draw.rounded_rectangle(box((3, 3, 68, 68)), radius=16 * scale, fill="#005FB8")
        if name in ("PieChart", "DonutChart"):
            draw.pieslice(box((17, 17, 55, 55)), -90, 80, fill="white")
            draw.pieslice(box((17, 17, 55, 55)), 85, 200, fill="#A4DFDF")
            draw.pieslice(box((17, 17, 55, 55)), 205, 265, fill="#C7A6EE")
            if name == "DonutChart":
                draw.ellipse(box((27, 27, 45, 45)), fill="#005FB8")
        elif name == "BarChart":
            for x, height in [(20, 18), (30, 29), (40, 23), (50, 38)]:
                draw.rounded_rectangle(box((x, 54 - height, x + 6, 54)), radius=2 * scale, fill="white")
        elif name == "HorizontalBarChart":
            for y, width in [(20, 36), (31, 24), (42, 17), (53, 9)]:
                draw.rounded_rectangle(box((18, y, 18 + width, y + 5)), radius=2 * scale, fill="white")
        elif name == "ScatterChart":
            for x, y in [(20, 49), (25, 37), (32, 42), (36, 30), (41, 35), (47, 20), (53, 27)]:
                draw.ellipse(box((x - 2, y - 2, x + 2, y + 2)), fill="white")
        else:
            points = [(19, 47), (28, 36), (36, 41), (44, 28), (54, 22)]
            if name == "AreaChart":
                draw.polygon([box(p) for p in points + [(54, 54), (19, 54)]], fill="#75B8DA")
            draw.line([box(p) for p in points], fill="white", width=3 * scale, joint="curve")
            if name == "Sparkline":
                draw.ellipse(box((52, 20, 56, 24)), fill="white")
        output = ROOT / "app/assets/control_images/charts" / (name + ".png")
        output.parent.mkdir(parents=True, exist_ok=True)
        image.resize((72, 72), Image.Resampling.LANCZOS).save(output, optimize=True)
        print(output.relative_to(ROOT))


if __name__ == "__main__":
    main()
