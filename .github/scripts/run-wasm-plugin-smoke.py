#!/usr/bin/env python3
"""Run the optional Plugins example without launching the Gallery."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading

from wasm_plugin_contract import run_plugin_extension_contract


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, _format: str, *_args: object) -> None:
        pass


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("build/wasm"))
    parser.add_argument("--output", type=Path, default=Path("build/plugins-wasm-validation"))
    parser.add_argument("--entry", default="/app/plugin-extension/index.html")
    args = parser.parse_args()
    from playwright.sync_api import sync_playwright

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(QuietHandler, directory=str(args.root.resolve()))
    )
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True)
            try:
                run_plugin_extension_contract(
                    browser, f"http://127.0.0.1:{server.server_port}", args.output, args.entry
                )
            finally:
                browser.close()
    finally:
        server.shutdown()
        server.server_close()
        worker.join()


if __name__ == "__main__":
    main()
