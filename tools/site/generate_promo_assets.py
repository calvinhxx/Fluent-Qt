#!/usr/bin/env python3
"""Render the README frame and sharing artwork from the current Gallery image."""

from __future__ import annotations

import argparse
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
        from playwright.sync_api import sync_playwright
    except ImportError:
        parser.error("Install Playwright and its Chromium browser; see site-workflow.md.")

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True)
        try:
            page = browser.new_page(
                viewport={"width": 1731, "height": 1000}, device_scale_factor=1
            )
            page.goto((root / "tools/site/promo-assets.html").as_uri())
            page.evaluate("document.fonts.ready")
            page.evaluate("Promise.all([...document.images].map(image => image.decode()))")
            for selector, paths in (
                ("#readme", ("docs/assets/readme/hero.png", "site/assets/gallery/gallery-hero-real.png")),
                ("#social", ("site/assets/og.png",)),
            ):
                png = page.locator(selector).screenshot(animations="disabled")
                for relative in paths:
                    output = output_root / relative
                    output.parent.mkdir(parents=True, exist_ok=True)
                    output.write_bytes(png)
                    print(output)
        finally:
            browser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
