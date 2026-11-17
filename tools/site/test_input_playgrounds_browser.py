#!/usr/bin/env python3
"""Exercise every registered component playground editor against the real WASM bridge."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
from urllib.parse import urljoin

ROOT = Path(__file__).resolve().parents[2]


def load_browser_contract():
    path = Path(__file__).with_name('test_component_playground_browser.py')
    spec = importlib.util.spec_from_file_location('component_playground_browser', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(browser, base_url, output, screenshots=False, routes=None):
    audit = load_browser_contract().load_audit().run
    if routes is None:
        routes = [item['component'] for item in json.loads((ROOT / 'app/assets/playgrounds/components.json').read_text())]
    return audit(browser, base_url, output, screenshots, routes=routes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wasm-root', type=Path, default=ROOT / 'build/wasm')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/input-playgrounds/browser')
    parser.add_argument('--screenshots', action='store_true')
    parser.add_argument('--component', action='append')
    args = parser.parse_args()
    from playwright.sync_api import sync_playwright
    with load_browser_contract().website_server(args.wasm_root.resolve()) as url, sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True, ignore_default_args=['--hide-scrollbars'])
        try:
            run(browser, url, args.output, args.screenshots, args.component)
        finally:
            browser.close()


if __name__ == '__main__':
    main()
