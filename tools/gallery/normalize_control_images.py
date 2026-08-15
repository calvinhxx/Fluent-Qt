#!/usr/bin/env python3

"""Check and normalize Gallery control-image canvases without enlarging artwork."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

try:
    from PIL import Image
except ImportError as error:  # pragma: no cover - maintainer dependency guard
    raise SystemExit(
        "Pillow is required: python -m pip install Pillow"
    ) from error


ROOT = Path(__file__).resolve().parents[2]
IMAGE_ROOT = ROOT / "app" / "assets" / "control_images"
QRC_PATH = ROOT / "app" / "gallery_resources.qrc"
TARGET_SIZE = (72, 72)


def _qrc_control_images() -> set[Path]:
    tree = ET.parse(QRC_PATH)
    return {
        ROOT / "app" / node.text
        for node in tree.findall(".//file")
        if node.text and node.text.startswith("assets/control_images/")
    }


def _normalized_canvas(image: Image.Image) -> Image.Image:
    source = image.convert("RGBA")
    if source.size == TARGET_SIZE:
        return source

    target_width, target_height = TARGET_SIZE
    source_width, source_height = source.size
    if source_width <= target_width and source_height <= target_height:
        normalized = Image.new("RGBA", TARGET_SIZE, (0, 0, 0, 0))
        normalized.alpha_composite(
            source,
            (
                (target_width - source_width) // 2,
                (target_height - source_height) // 2,
            ),
        )
        return normalized

    scale = min(target_width / source_width, target_height / source_height)
    scaled_size = (
        max(1, round(source_width * scale)),
        max(1, round(source_height * scale)),
    )
    resized = source.resize(scaled_size, Image.Resampling.LANCZOS)
    normalized = Image.new("RGBA", TARGET_SIZE, (0, 0, 0, 0))
    normalized.alpha_composite(
        resized,
        (
            (target_width - scaled_size[0]) // 2,
            (target_height - scaled_size[1]) // 2,
        ),
    )
    return normalized


def _corner_alpha(image: Image.Image) -> tuple[int, int, int, int]:
    alpha = image.getchannel("A")
    width, height = image.size
    return tuple(
        alpha.getpixel(point)
        for point in (
            (0, 0),
            (width - 1, 0),
            (0, height - 1),
            (width - 1, height - 1),
        )
    )


def audit(fix: bool) -> int:
    images = set(IMAGE_ROOT.rglob("*.png"))
    registered = _qrc_control_images()
    failures: list[str] = []
    normalized_paths: list[Path] = []

    for path in sorted(images):
        with Image.open(path) as opened:
            has_alpha = "A" in opened.getbands()
            image = opened.convert("RGBA")

        if fix and image.size != TARGET_SIZE:
            image = _normalized_canvas(image)
            image.save(path, format="PNG", optimize=True)
            normalized_paths.append(path)

        relative = path.relative_to(ROOT)
        if not has_alpha:
            failures.append(f"{relative}: PNG must contain an alpha channel")
        if image.size != TARGET_SIZE:
            failures.append(
                f"{relative}: expected 72x72, found {image.width}x{image.height}"
            )
        if image.getchannel("A").getbbox() is None:
            failures.append(f"{relative}: image is fully transparent")
        corners = _corner_alpha(image)
        if any(corners):
            failures.append(
                f"{relative}: canvas corners must be transparent, found {corners}"
            )

    for path in sorted(images - registered):
        failures.append(f"{path.relative_to(ROOT)}: missing from app/gallery_resources.qrc")
    for path in sorted(registered - images):
        failures.append(f"{path.relative_to(ROOT)}: qrc entry has no matching file")

    if normalized_paths:
        print(f"normalized {len(normalized_paths)} image canvas(es):")
        for path in normalized_paths:
            print(f"  {path.relative_to(ROOT)}")

    if failures:
        print("control-image audit failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print(
        f"control-image audit passed: {len(images)} registered 72x72 RGBA PNGs"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fix",
        action="store_true",
        help="center smaller canvases and proportionally shrink oversized canvases",
    )
    args = parser.parse_args()
    return audit(args.fix)


if __name__ == "__main__":
    raise SystemExit(main())
