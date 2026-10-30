#!/usr/bin/env python3
"""Check static website themes and responsive homepages without a WASM build."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import sys
import threading
from urllib.parse import urlsplit

from serve import SiteHandler

PROJECT_ROOT = Path(__file__).resolve().parents[2]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


class QuietSiteHandler(SiteHandler):
    log_message = QuietHandler.log_message


def prepare_site_theme_contract(project_root: Path) -> None:
    # Homepages are generated artifacts. A fresh checkout has neither locale;
    # refresh both even if an earlier local run left stale files behind.
    subprocess.run(
        [sys.executable, str(project_root / "tools/site/generate_localized_site.py")],
        cwd=project_root,
        check=True,
    )


def run_site_theme_contract(browser: object, output: Path, *, site_root: Path | None = None) -> dict:
    if site_root is None:
        prepare_site_theme_contract(PROJECT_ROOT)
        # The static contract must never load Gallery; no WASM build is needed.
        handler = partial(QuietSiteHandler, wasm_root=output / "no-wasm-runtime")
    else:
        site_root = site_root.resolve()
        for name in ("index.html", "zh-CN/index.html"):
            if not (site_root / name).is_file():
                raise RuntimeError(f"Website artifact is missing {name}: {site_root}")
        handler = partial(QuietHandler, directory=str(site_root))
    output.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    results = []
    try:
        for language, locale in (("en", ""), ("zh-CN", "zh-CN/")):
            page = browser.new_page(viewport={"width": 1280, "height": 800})
            errors, runtime_requests = [], []
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.on("request", lambda request: runtime_requests.append(request.url)
                    if urlsplit(request.url).path.startswith("/gallery/")
                    or urlsplit(request.url).path.endswith(".wasm") else None)
            try:
                page.route("https://api.github.com/**", lambda route: route.abort())
                page.emulate_media(color_scheme="light", forced_colors="none")
                site_url = f"http://127.0.0.1:{server.server_port}/{locale}"
                response = page.goto(site_url, wait_until="domcontentloaded")
                if response is None or not response.ok:
                    raise RuntimeError("Could not load the website theme contract")
                page.wait_for_selector("html[data-i18n-ready]", timeout=10_000)

                def active_theme() -> str:
                    return page.evaluate("document.documentElement.dataset.theme")

                def stored_theme() -> str | None:
                    return page.evaluate("localStorage.getItem('fluent-qt-theme')")

                toggle = page.locator("[data-theme-toggle]")
                if active_theme() != "light":
                    raise RuntimeError("Website did not start in the emulated light theme")
                toggle.click()
                if active_theme() != "dark" or stored_theme() != "dark":
                    raise RuntimeError("Website light-to-dark theme transition failed")
                toggle.click()
                if active_theme() != "high-contrast" or stored_theme() != "high-contrast":
                    raise RuntimeError("Website dark-to-high-contrast transition failed")
                gallery_theme = page.evaluate(
                    "new URL(galleryUrl()).searchParams.get('host-theme')"
                )
                if gallery_theme != "high-contrast":
                    raise RuntimeError("Website Gallery URL omitted the high-contrast host theme")

                page.reload(wait_until="domcontentloaded")
                page.wait_for_selector("html[data-i18n-ready]", timeout=10_000)
                if active_theme() != "high-contrast":
                    raise RuntimeError("Website did not restore the high-contrast preference")
                page.locator("[data-theme-toggle]").click()
                if active_theme() != "light" or stored_theme() != "light":
                    raise RuntimeError("Website high-contrast-to-light transition failed")

                page.emulate_media(forced_colors="active")
                page.wait_for_function(
                    "document.documentElement.dataset.theme === 'high-contrast'"
                )
                if not page.locator("[data-theme-toggle]").is_disabled():
                    raise RuntimeError("Website theme control stayed active during forced-colors")
                if stored_theme() != "light":
                    raise RuntimeError("forced-colors overwrote the stored website preference")
                page.emulate_media(forced_colors="none")
                page.wait_for_function("document.documentElement.dataset.theme === 'light'")
                if page.locator("[data-theme-toggle]").is_disabled():
                    raise RuntimeError("Website theme control did not recover after forced-colors")
                for width in (1280, 390):
                    page.set_viewport_size({"width": width, "height": 800})
                    page.goto(site_url + "#top")
                    page.wait_for_selector("html[data-i18n-ready]")
                    if page.evaluate("document.documentElement.scrollWidth > innerWidth"):
                        raise RuntimeError(f"Website overflows horizontally at {width}px")
                    page.wait_for_function("""width =>
                        (getComputedStyle(document.querySelector('.hero-window')).transform === 'none') === (width < 900)
                    """, arg=width)
                    page.wait_for_function("getComputedStyle(document.querySelector('.hero-message')).opacity === '1'")
                    if page.locator('[data-showcase-frame]').get_attribute('src'):
                        raise RuntimeError("Website eagerly loaded the opt-in Spatial runtime")
                    page.screenshot(path=str(output / f"{language}-{width}.png"))
                if errors:
                    raise RuntimeError(f"Website page errors ({language}): {errors}")
                if runtime_requests:
                    raise RuntimeError(f"Static website eagerly requested WASM ({language}): {runtime_requests}")
                results.append({"language": language, "themes": ["light", "dark", "high-contrast"],
                                "forced_colors": "passed", "widths": [1280, 390], "runtime_requests": []})
            finally:
                page.close()
    finally:
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)
    return {"status": "passed", "source": str(site_root or PROJECT_ROOT / "site"), "pages": results}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site-root", type=Path, help="Validate an already generated website artifact")
    parser.add_argument("--output", type=Path, default=PROJECT_ROOT / "build/site-theme-browser")
    args = parser.parse_args()
    from playwright.sync_api import sync_playwright

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True)
        try:
            result = run_site_theme_contract(browser, args.output, site_root=args.site_root)
        finally:
            browser.close()
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("Website browser theme contract passed: both languages, three themes, forced colors, wide/narrow, no WASM")


if __name__ == "__main__":
    main()
