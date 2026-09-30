#!/usr/bin/env python3

"""Export the Gallery's editable SVG masters to registered 72 x 72 PNGs."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "tools" / "gallery" / "artwork"
IMAGE_ROOT = ROOT / "app" / "assets" / "control_images"
CANVAS = 72
SCALE = 4
SVG_NAMESPACE = "http://www.w3.org/2000/svg"
SUPPORTED_ELEMENTS = {
    "svg", "title", "desc", "defs", "g", "path", "rect", "circle", "ellipse",
    "line", "polyline", "polygon", "linearGradient", "radialGradient", "stop", "use",
}


def validate_source(path: Path) -> None:
    """Require self-contained vector geometry on the Gallery source canvas."""
    root = ET.parse(path).getroot()
    if root.tag != f"{{{SVG_NAMESPACE}}}svg":
        raise ValueError(f"{path}: expected an SVG document")
    dimensions = tuple(float(root.get(key, "0").removesuffix("px"))
                       for key in ("width", "height"))
    view_box = tuple(float(value) for value in root.get("viewBox", "").split())
    if dimensions != (CANVAS, CANVAS) or view_box != (0, 0, CANVAS, CANVAS):
        raise ValueError(f"{path}: expected width/height 72 and viewBox 0 0 72 72")
    for element in root.iter():
        name = element.tag.rsplit("}", 1)[-1]
        if name in {"image", "text", "script", "foreignObject"}:
            raise ValueError(f"{path}: {name} is not outlined vector artwork")
        if name not in SUPPORTED_ELEMENTS:
            raise ValueError(f"{path}: {name} is outside the Qt 5.15-compatible artwork subset")
        for attribute, value in element.attrib.items():
            if attribute.rsplit("}", 1)[-1] in {
                "clip-path", "mask", "filter", "marker-start", "marker-mid", "marker-end",
            }:
                raise ValueError(f"{path}: {attribute} is outside the Qt 5.15-compatible artwork subset")
            if attribute.rsplit("}", 1)[-1] == "href" and not value.startswith("#"):
                raise ValueError(f"{path}: external artwork references are unsupported")


def source_pairs(source_root: Path, image_root: Path) -> list[tuple[Path, Path]]:
    """Pair each master with its identically named category PNG."""
    sources = sorted(source_root.rglob("*.svg"))
    expected = {source.relative_to(source_root).with_suffix(".png") for source in sources}
    orphaned = sorted(path.relative_to(image_root) for path in image_root.rglob("*.png")
                      if path.relative_to(image_root) not in expected)
    if not sources:
        raise ValueError(f"{source_root}: no SVG masters found")
    if orphaned:
        raise ValueError("PNG files without SVG masters: " + ", ".join(map(str, orphaned)))
    return [(source, image_root / source.relative_to(source_root).with_suffix(".png"))
            for source in sources]


def render_source(path: Path):
    """Supersample vector strokes and retain antialiased alpha when exporting."""
    from PySide6.QtCore import QRectF, Qt
    from PySide6.QtGui import QImage, QPainter
    from PySide6.QtSvg import QSvgRenderer

    validate_source(path)
    renderer = QSvgRenderer(str(path))
    if not renderer.isValid():
        raise ValueError(f"{path}: SVG renderer rejected the master")
    image = QImage(CANVAS * SCALE, CANVAS * SCALE, QImage.Format_ARGB32_Premultiplied)
    image.fill(Qt.transparent)
    painter = QPainter(image)
    painter.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
    renderer.render(painter, QRectF(0, 0, image.width(), image.height()))
    painter.end()
    return image.scaled(CANVAS, CANVAS, Qt.IgnoreAspectRatio, Qt.SmoothTransformation)


def matches_export(path: Path, image) -> bool:
    from PySide6.QtGui import QImage

    if not path.is_file():
        return False
    existing = QImage(str(path))
    return (not existing.isNull() and existing.size() == image.size()
            and existing.hasAlphaChannel()
            and existing.convertToFormat(QImage.Format_RGBA8888)
            == image.convertToFormat(QImage.Format_RGBA8888))


def export_images(source_root: Path, image_root: Path, *, check: bool = False,
                  assets: tuple[str, ...] = ()) -> int:
    pairs = source_pairs(source_root, image_root)
    selected = set(assets)
    known = {source.relative_to(source_root).with_suffix("").as_posix() for source, _ in pairs}
    if selected - known:
        raise ValueError("Unknown artwork: " + ", ".join(sorted(selected - known)))
    stale = []
    count = 0
    for source, output in pairs:
        key = source.relative_to(source_root).with_suffix("").as_posix()
        if selected and key not in selected:
            continue
        count += 1
        rendered = render_source(source)
        if matches_export(output, rendered):
            continue
        if check:
            stale.append(output)
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            if not rendered.save(str(output), "PNG"):
                raise ValueError(f"{output}: unable to save PNG export")
    if stale:
        for output in stale:
            print(f"stale control image: {output}", file=sys.stderr)
        return 1
    print(f"{'verified' if check else 'exported'} {count} SVG/PNG control-image pair(s)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assets", nargs="*", help="optional category/title keys, such as navigation/Stepper")
    parser.add_argument("--check", action="store_true", help="report stale or missing PNG exports without writing")
    args = parser.parse_args()
    # Exporting is a headless maintainer operation; no desktop window is created.
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtGui import QGuiApplication
    app = QGuiApplication.instance() or QGuiApplication([])
    try:
        return export_images(SOURCE_ROOT, IMAGE_ROOT, check=args.check, assets=tuple(args.assets))
    except (ValueError, ET.ParseError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
