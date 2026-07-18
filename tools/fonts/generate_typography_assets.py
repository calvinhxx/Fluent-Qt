"""Generate FluentQt's redistributable text and icon font assets.

Run from the repository root after installing the pinned dependency:

    python -m pip install -r tools/fonts/requirements.txt
    python tools/fonts/generate_typography_assets.py

Use ``--check`` in CI or before a release to verify that the committed outputs
are byte-for-byte reproducible from the vendored upstream fonts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import tempfile
from pathlib import Path

import fontTools
from fontTools.ttLib import TTFont


ROOT = Path(__file__).resolve().parents[2]
INTER_DIR = ROOT / "third_party" / "fonts" / "inter"
FLUENT_ICONS_DIR = ROOT / "third_party" / "icons" / "fluentui-system-icons"
ICON_SOURCE = FLUENT_ICONS_DIR / "FluentSystemIcons-Regular.ttf"
ICON_CATALOG_SOURCE = FLUENT_ICONS_DIR / "FluentSystemIcons-Regular.json"
ICON_ALIASES_SOURCE = ROOT / "tools" / "fonts" / "fluent_icon_aliases.json"
TYPOGRAPHY_HEADER = ROOT / "src" / "design" / "Typography.h"

FONTTOOLS_VERSION = "4.59.1"
SOURCE_SHA256 = {
    INTER_DIR / "Inter-Regular.ttf":
        "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82",
    INTER_DIR / "Inter-SemiBold.ttf":
        "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3",
    INTER_DIR / "InterDisplay-SemiBold.ttf":
        "0310d7a325896129730c6c8cf9a6e0f81ee258bedf77b1ff059b2a7b75f74e02",
    ICON_SOURCE:
        "8ebc946d0de5ff7cd8fa653020f7c4046c71e27574ac6a9013aa727021290c32",
    ICON_CATALOG_SOURCE:
        "9c782fa7f81a5a631ef68fa618305fd946d0fd0697372f11988116ee5852ae70",
}

TEXT_FACES = (
    {
        "source": INTER_DIR / "Inter-Regular.ttf",
        "file": "FluentQtUIText-Regular.ttf",
        "family": "FluentQt UI Text",
        "subfamily": "Regular",
        "postscript": "FluentQtUIText-Regular",
        "weight": 400,
    },
    {
        "source": INTER_DIR / "Inter-SemiBold.ttf",
        "file": "FluentQtUIText-Semibold.ttf",
        "family": "FluentQt UI Text",
        "subfamily": "Semibold",
        "postscript": "FluentQtUIText-Semibold",
        "weight": 600,
    },
    {
        # Inter's text cut remains clearer at Subtitle's 20 px while still
        # retaining the same geometry at the 28 px Title role.
        "source": INTER_DIR / "Inter-SemiBold.ttf",
        "file": "FluentQtUIHeading-Semibold.ttf",
        "family": "FluentQt UI Heading",
        "subfamily": "Semibold",
        "postscript": "FluentQtUIHeading-Semibold",
        "weight": 600,
    },
    {
        "source": INTER_DIR / "InterDisplay-SemiBold.ttf",
        "file": "FluentQtUIDisplay-Semibold.ttf",
        "family": "FluentQt UI Display",
        "subfamily": "Semibold",
        "postscript": "FluentQtUIDisplay-Semibold",
        "weight": 600,
    },
)

TEXT_HINT_TABLES = {"cvt ", "fpgm", "prep", "gasp"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_inputs() -> None:
    if fontTools.__version__ != FONTTOOLS_VERSION:
        raise RuntimeError(
            f"fontTools {FONTTOOLS_VERSION} is required; found {fontTools.__version__}"
        )
    for path, expected in SOURCE_SHA256.items():
        if not path.is_file():
            raise FileNotFoundError(path)
        actual = sha256(path)
        if actual != expected:
            raise RuntimeError(
                f"Unexpected upstream asset hash for {path}: {actual} (expected {expected})"
            )


def set_english_name(font: TTFont, name_id: int, value: str) -> None:
    name_table = font["name"]
    name_table.removeNames(nameID=name_id)
    # Windows Unicode and Macintosh English records cover DirectWrite,
    # CoreText, Fontconfig, and FreeType-backed Qt builds.
    name_table.setName(value, name_id, 3, 1, 0x0409)
    name_table.setName(value, name_id, 1, 0, 0)


def rename_face(
    font: TTFont,
    *,
    family: str,
    subfamily: str,
    postscript: str,
    weight: int,
) -> None:
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

    font["name"].removeNames(nameID=25)
    os2 = font["OS/2"]
    os2.usWeightClass = weight
    os2.fsSelection &= ~((1 << 5) | (1 << 6))
    if weight == 400:
        os2.fsSelection |= 1 << 6
    font["head"].macStyle &= ~(1 << 0)
    font["head"].modified = font["head"].created
    font.recalcTimestamp = False


def require_text_hinting(font: TTFont, path: Path) -> None:
    missing = TEXT_HINT_TABLES - set(font.keys())
    if missing:
        names = ", ".join(sorted(missing))
        raise RuntimeError(f"{path} is missing required TrueType hint tables: {names}")


def generate_text_faces(output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[Path] = []
    for face in TEXT_FACES:
        source_path = Path(face["source"])
        font = TTFont(source_path, recalcTimestamp=False)
        try:
            require_text_hinting(font, source_path)
            rename_face(
                font,
                family=str(face["family"]),
                subfamily=str(face["subfamily"]),
                postscript=str(face["postscript"]),
                weight=int(face["weight"]),
            )
            output = output_dir / str(face["file"])
            font.save(output, reorderTables=False)
        finally:
            font.close()

        generated = TTFont(output, lazy=True)
        try:
            require_text_hinting(generated, output)
        finally:
            generated.close()
        outputs.append(output)
    return outputs


def typography_icon_codepoints() -> dict[str, int]:
    source = TYPOGRAPHY_HEADER.read_text(encoding="utf-8")
    matches = re.findall(
        r'const QString\s+(\w+)\s*=\s*QString::fromUtf16\(u"\\u([0-9A-Fa-f]{4})"\);',
        source,
    )
    if not matches:
        raise RuntimeError(f"No icon codepoints found in {TYPOGRAPHY_HEADER}")
    return {name: int(value, 16) for name, value in matches}


def load_json_object(path: Path) -> dict[str, int | str]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError(f"Expected a JSON object in {path}")
    return value


def unicode_cmap(font: TTFont) -> dict[int, str]:
    return {
        codepoint: glyph
        for table in font["cmap"].tables
        if table.isUnicode()
        for codepoint, glyph in table.cmap.items()
    }


def set_cmap_entry(font: TTFont, codepoint: int, glyph: str) -> None:
    for table in font["cmap"].tables:
        if not table.isUnicode():
            continue
        if codepoint <= 0xFFFF or table.format in (12, 13):
            table.cmap[codepoint] = glyph


def remove_cmap_entry(font: TTFont, codepoint: int) -> None:
    for table in font["cmap"].tables:
        if table.isUnicode():
            table.cmap.pop(codepoint, None)


def generate_icon_assets(output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    catalog_source = load_json_object(ICON_CATALOG_SOURCE)
    catalog = {name: int(value) for name, value in catalog_source.items()}
    aliases_source = load_json_object(ICON_ALIASES_SOURCE)
    aliases = {name: str(value) for name, value in aliases_source.items()}
    public_codepoints = typography_icon_codepoints()

    if set(aliases) != set(public_codepoints):
        missing = sorted(set(public_codepoints) - set(aliases))
        extra = sorted(set(aliases) - set(public_codepoints))
        raise RuntimeError(f"Icon alias manifest mismatch; missing={missing}, extra={extra}")
    unknown = sorted(set(aliases.values()) - set(catalog))
    if unknown:
        raise RuntimeError(f"Unknown Fluent UI System Icon names: {unknown}")

    font = TTFont(ICON_SOURCE, recalcTimestamp=False)
    try:
        cmap = unicode_cmap(font)
        absent = sorted(set(catalog.values()) - set(cmap))
        if absent:
            raise RuntimeError(
                "Official icon catalog references absent cmap entries: "
                + ", ".join(f"U+{value:04X}" for value in absent[:20])
            )

        # The official font uses some of the legacy WinUI PUA positions still
        # exposed by Typography::Icons. Move those official entries to unused
        # supplementary PUA positions before installing semantic aliases. The
        # generated catalog is updated, so every official icon remains present.
        occupied = set(cmap)
        next_codepoint = max(occupied) + 1
        for legacy_codepoint in sorted(set(public_codepoints.values()) & occupied):
            while next_codepoint in occupied:
                next_codepoint += 1
            if next_codepoint > 0xFFFFD:
                raise RuntimeError("No private-use codepoints remain for icon remapping")
            glyph = cmap[legacy_codepoint]
            remove_cmap_entry(font, legacy_codepoint)
            set_cmap_entry(font, next_codepoint, glyph)
            for name, codepoint in catalog.items():
                if codepoint == legacy_codepoint:
                    catalog[name] = next_codepoint
            occupied.remove(legacy_codepoint)
            occupied.add(next_codepoint)
            next_codepoint += 1

        cmap = unicode_cmap(font)
        installed_aliases: dict[int, str] = {}
        for public_name, source_name in aliases.items():
            target = public_codepoints[public_name]
            source_codepoint = catalog[source_name]
            glyph = cmap.get(source_codepoint)
            if glyph is None:
                raise RuntimeError(
                    f"Alias source {source_name} U+{source_codepoint:04X} is absent"
                )
            previous = installed_aliases.get(target)
            if previous is not None and previous != glyph:
                raise RuntimeError(
                    f"Conflicting aliases for U+{target:04X}: {previous} and {glyph}"
                )
            installed_aliases[target] = glyph
            set_cmap_entry(font, target, glyph)

        rename_face(
            font,
            family="FluentQt Icons",
            subfamily="Regular",
            postscript="FluentQtIcons-Regular",
            weight=400,
        )
        font_output = output_dir / "FluentQtIcons.ttf"
        font.save(font_output, reorderTables=False)
    finally:
        font.close()

    catalog_output = output_dir / "FluentQtIcons.json"
    catalog_output.write_text(
        json.dumps(catalog, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
        + "\n",
        encoding="utf-8",
        newline="\n",
    )

    # Runtime painters receive the stable semantic PUA glyphs from
    # Typography::Icons, but Fluent UI System Icons ships separately drawn
    # 12/16/20/24 px optical variants. Preserve the semantic-glyph-to-upstream
    # mapping so IconCatalog can select the native variant for the requested
    # control slot instead of scaling the manifest's 20 px source outline.
    runtime_aliases: dict[str, str] = {}
    for public_name, source_name in sorted(aliases.items()):
        codepoint = public_codepoints[public_name]
        runtime_aliases.setdefault(f"{codepoint:X}", source_name)

    aliases_output = output_dir / "FluentQtIconAliases.json"
    aliases_output.write_text(
        json.dumps(runtime_aliases, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
        + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return [font_output, catalog_output, aliases_output]


def generate(output_root: Path) -> list[Path]:
    validate_inputs()
    outputs = generate_text_faces(output_root / "res" / "fonts")
    outputs.extend(generate_icon_assets(output_root / "res" / "icons"))
    return outputs


def check_outputs() -> None:
    with tempfile.TemporaryDirectory(prefix="fluentqt-fonts-") as temporary:
        generated = generate(Path(temporary))
        failures: list[Path] = []
        for path in generated:
            relative = path.relative_to(temporary)
            reference = ROOT / relative
            if not reference.is_file() or path.read_bytes() != reference.read_bytes():
                failures.append(relative)
        if failures:
            names = ", ".join(str(path) for path in failures)
            raise RuntimeError(f"Generated typography assets differ: {names}")
    print("Typography assets are reproducible.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify committed outputs without changing them",
    )
    args = parser.parse_args()
    if args.check:
        check_outputs()
        return

    outputs = generate(ROOT)
    for output in outputs:
        print(f"{output.relative_to(ROOT)} {output.stat().st_size} bytes {sha256(output)}")


if __name__ == "__main__":
    main()
