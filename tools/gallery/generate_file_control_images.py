#!/usr/bin/env python3
"""Render file-entry/list Gallery tiles from the bundled Fluent icon font."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SCALE = 4


def main():
    font_root = ROOT / "third_party/icons/fluentui-system-icons"
    glyphs = json.loads((font_root / "FluentSystemIcons-Regular.json").read_text())
    font = ImageFont.truetype(str(font_root / "FluentSystemIcons-Regular.ttf"), 40 * SCALE)
    for category, name, glyph, color in (
        ("basic-input", "FileDropZone", "ic_fluent_arrow_upload_24_regular", "#1683DB"),
        ("collections", "FileListView", "ic_fluent_document_24_regular", "#9D78D7"),
    ):
        canvas = Image.new("RGBA", (72 * SCALE, 72 * SCALE))
        draw = ImageDraw.Draw(canvas)
        draw.rounded_rectangle((3*SCALE, 3*SCALE, 68*SCALE, 68*SCALE),
                               radius=16*SCALE, fill=color)
        text = chr(glyphs[glyph])
        left, top, right, bottom = font.getbbox(text)
        draw.text(((72*SCALE-right-left)/2, (72*SCALE-bottom-top)/2),
                  text, font=font, fill="white")
        output = ROOT / "app/assets/control_images" / category / (name + ".png")
        canvas.resize((72, 72), Image.Resampling.LANCZOS).save(output, optimize=True)
        print(output.relative_to(ROOT))


if __name__ == "__main__":
    main()
