#!/usr/bin/env python3
"""Render the README frame and sharing artwork from the current Gallery image."""

from __future__ import annotations

import argparse
from io import BytesIO
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-root",
        type=Path,
        help="Write review assets under this directory instead of the project root.",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output_root = args.output_root.resolve() if args.output_root else root

    try:
        from PIL import Image
        from playwright.sync_api import sync_playwright
    except ImportError:
        parser.error("Install Pillow, Playwright and its Chromium browser; see site-workflow.md.")

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True)
        try:
            page = browser.new_page(
                viewport={"width": 1731, "height": 1000}, device_scale_factor=1
            )
            page.goto((root / "tools/site/promo-assets.html").as_uri())
            page.evaluate("document.fonts.ready")
            page.evaluate("Promise.all([...document.images].map(image => image.decode()))")
            # Choose high-quality downsampling at the framed image's final
            # pixel size; keep the native PNG and output dimensions unchanged.
            page.locator("#readme img").evaluate("""image => {
                const canvas = document.createElement('canvas');
                canvas.width = image.clientWidth;
                canvas.height = image.clientHeight;
                canvas.setAttribute('role', 'img');
                canvas.setAttribute('aria-label', image.alt);
                const context = canvas.getContext('2d');
                context.imageSmoothingEnabled = true;
                context.imageSmoothingQuality = 'high';
                context.drawImage(image, 0, 0, canvas.width, canvas.height);
                image.replaceWith(canvas);
            }""")
            for selector, relative in (
                ("#readme", "docs/assets/readme/hero.png"),
                ("#social", "site/assets/og.png"),
            ):
                png = page.locator(selector).screenshot(animations="disabled")
                output = output_root / relative
                output.parent.mkdir(parents=True, exist_ok=True)
                with Image.open(BytesIO(png)) as screenshot:
                    if selector == "#social":
                        # Round RGB channels to even levels, preserving pure white.
                        # This changes each channel by at most one level and improves PNG compression.
                        levels = [value if value == 255 else value & ~1 for value in range(256)]
                        screenshot = screenshot.convert("RGB").point(levels * 3)
                    screenshot.save(output, optimize=True)
                print(output)
        finally:
            browser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
