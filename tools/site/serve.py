#!/usr/bin/env python3
"""Preview the source website with a local WASM build at the production /gallery/ URL."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site"


class SiteHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, wasm_root, **kwargs):
        self.wasm_root = wasm_root
        super().__init__(*args, directory=str(SITE), **kwargs)

    def translate_path(self, path):
        translated = Path(super().translate_path(path))
        relative = translated.relative_to(SITE)
        if relative.parts and relative.parts[0] == "gallery":
            return str(self.wasm_root.joinpath(*relative.parts[1:]))
        return str(translated)


class SiteServer(ThreadingHTTPServer):
    # In particular, do not silently share an occupied listener on Windows.
    allow_reuse_address = False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--wasm-root", type=Path, default=ROOT / "build/wasm/app")
    args = parser.parse_args()
    wasm_root = args.wasm_root.resolve()
    for name in ("index.html", "fluent_qt_gallery.js", "fluent_qt_gallery.wasm"):
        if not (wasm_root / name).is_file():
            parser.error(f"Missing {wasm_root / name}; build fluent_qt_gallery with the wasm preset.")
    handler = partial(SiteHandler, wasm_root=wasm_root)
    with SiteServer(("127.0.0.1", args.port), handler) as server:
        print(f"Site: http://127.0.0.1:{args.port}/zh-CN/", flush=True)
        print(f"WASM: {wasm_root}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
