"""Generate deterministic static typography faces from the bundled Segoe UI VF.

Requires fontTools. Run from the repository root:

    python tools/fonts/generate_static_segoe.py

The generated families intentionally use a FluentQt-specific prefix so Qt never
matches a same-named system Segoe installation before the application font.
"""

from __future__ import annotations

from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "res" / "SegoeUI-VF.ttf"
OUTPUT_DIR = ROOT / "res" / "fonts"


FACES = (
    {
        "file": "FluentQtSegoeUISmall-Regular.ttf",
        "family": "FluentQt Segoe UI Small",
        "subfamily": "Regular",
        "postscript": "FluentQtSegoeUISmall-Regular",
        "weight": 400,
        "axes": {"wght": 400.0, "opsz": 8.0},
    },
    {
        "file": "FluentQtSegoeUIText-Regular.ttf",
        "family": "FluentQt Segoe UI Text",
        "subfamily": "Regular",
        "postscript": "FluentQtSegoeUIText-Regular",
        "weight": 400,
        "axes": {"wght": 400.0, "opsz": 10.5},
    },
    {
        "file": "FluentQtSegoeUIText-Semibold.ttf",
        "family": "FluentQt Segoe UI Text",
        "subfamily": "Semibold",
        "postscript": "FluentQtSegoeUIText-Semibold",
        "weight": 600,
        "axes": {"wght": 600.0, "opsz": 10.5},
    },
    {
        "file": "FluentQtSegoeUIDisplay-Semibold.ttf",
        "family": "FluentQt Segoe UI Display",
        "subfamily": "Semibold",
        "postscript": "FluentQtSegoeUIDisplay-Semibold",
        "weight": 600,
        "axes": {"wght": 600.0, "opsz": 36.0},
    },
)


def set_english_name(font: TTFont, name_id: int, value: str) -> None:
    name_table = font["name"]
    name_table.removeNames(nameID=name_id)
    # Unicode Windows and Macintosh English records cover DirectWrite,
    # CoreText, Fontconfig, and FreeType-backed Qt builds.
    name_table.setName(value, name_id, 3, 1, 0x0409)
    name_table.setName(value, name_id, 1, 0, 0)


def rename_face(font: TTFont, face: dict[str, object]) -> None:
    family = str(face["family"])
    subfamily = str(face["subfamily"])
    postscript = str(face["postscript"])
    full_name = f"{family} {subfamily}"

    for name_id, value in (
        (1, family),
        (2, subfamily),
        (3, f"FluentQt:{postscript}"),
        (4, full_name),
        (6, postscript),
        (16, family),
        (17, subfamily),
        (21, family),
        (22, subfamily),
    ):
        set_english_name(font, name_id, value)

    # A static instance must not retain the variable PostScript prefix.
    font["name"].removeNames(nameID=25)

    weight = int(face["weight"])
    os2 = font["OS/2"]
    os2.usWeightClass = weight
    os2.fsSelection &= ~((1 << 5) | (1 << 6))
    if weight == 400:
        os2.fsSelection |= 1 << 6

    # Neither of the generated 600-weight faces is classified as Bold.
    font["head"].macStyle &= ~(1 << 0)


def generate() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    source = TTFont(SOURCE, recalcTimestamp=False)
    try:
        for face in FACES:
            static_font = instantiateVariableFont(
                source,
                face["axes"],
                inplace=False,
                optimize=True,
                updateFontNames=True,
            )
            static_font.recalcTimestamp = False
            rename_face(static_font, face)
            static_font.save(OUTPUT_DIR / str(face["file"]), reorderTables=False)
            static_font.close()
    finally:
        source.close()


if __name__ == "__main__":
    generate()
