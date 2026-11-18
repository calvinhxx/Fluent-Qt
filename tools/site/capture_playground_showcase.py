#!/usr/bin/env python3
"""Capture the homepage artwork from the running CheckBox property preview."""

import argparse
from io import BytesIO
from pathlib import Path
from urllib.parse import urljoin

from PIL import Image, ImageChops, ImageDraw
from playwright.sync_api import expect, sync_playwright

SCALE = 2
CORNER_RADIUS_CSS = 16


def clip_rounded_corners(image: Image.Image, radius_px: int) -> Image.Image:
    """Keep outer rounded corners crisp and strictly transparent using 4x supersampling."""
    rgba = image.convert("RGBA")
    supersample = 4
    w, h = rgba.size
    mask = Image.new("L", (w * supersample, h * supersample), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle(
        (0, 0, w * supersample - 1, h * supersample - 1),
        radius=radius_px * supersample,
        fill=255,
    )
    smooth_mask = mask.resize((w, h), Image.Resampling.LANCZOS)
    existing_alpha = rgba.getchannel("A")
    rgba.putalpha(ImageChops.darker(existing_alpha, smooth_mask))
    return rgba


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8765/", help="Local website with a working Gallery runtime")
    args = parser.parse_args()
    output = Path(__file__).resolve().parents[2] / "site/assets/playground"
    output.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch()
        for language, prefix in (("en", ""), ("zh", "zh-CN/")):
            url = urljoin(args.url.rstrip("/") + "/", prefix + "api/checkbox/?playground=1")
            for theme in ("light", "dark", "high-contrast"):
                page = browser.new_page(
                    viewport={"width": 1280, "height": 524},
                    device_scale_factor=SCALE,
                    reduced_motion="reduce",
                )
                page.add_init_script(f'localStorage.setItem("fluent-qt-theme", "{theme}")')
                page.goto(url, wait_until="domcontentloaded")
                panel = page.locator(".playground-popover")
                expect(panel).to_have_attribute("data-preview-state", "ready", timeout=90000)
                expect(panel).to_have_attribute("data-theme", theme)
                panel.locator('[data-property="checkState"] .playground-dropdown-trigger').click()
                page.locator('.playground-dropdown-menu:visible [data-value="Checked"]').click()
                expect(panel.locator(".playground-code-text")).to_contain_text("setCheckState(Qt::CheckState::Checked)")
                page.mouse.move(0, 0)
                page.add_style_tag(content="""
                  html, body { background: transparent !important; }
                  .playground-backdrop, .documentation-header, .document-layout, .playground-resize-handle {
                    display: none !important;
                  }
                  .playground-popover {
                    box-shadow: none !important;
                    animation: none !important;
                    transition: none !important;
                  }
                """)
                page.evaluate("document.fonts.ready")
                path = output / f"checkbox-{language}-{theme}.webp"
                with Image.open(BytesIO(panel.screenshot(animations="disabled", omit_background=True))) as image:
                    cleaned = clip_rounded_corners(image, CORNER_RADIUS_CSS * SCALE)
                    cleaned.save(path, lossless=True)
                print(path)
                page.close()
        browser.close()


if __name__ == "__main__":
    main()
