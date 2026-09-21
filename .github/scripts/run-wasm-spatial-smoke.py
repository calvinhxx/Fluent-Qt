#!/usr/bin/env python3
"""Exercise Gallery's optional WebGL compositor and save reproducible metrics."""

import argparse
import base64
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading
from urllib.parse import urlencode

from playwright.sync_api import sync_playwright


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("build/wasm"))
    parser.add_argument("--output", type=Path, default=Path("build/spatial-validation/web"))
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--channel", default=None, help="For example chrome; default is bundled Chromium")
    parser.add_argument("--software-gl", action="store_true", help="Force SwiftShader and require 2D fallback")
    parser.add_argument("--quality", action="store_true", help="Capture the same page in 2D, flat GPU, 3D and 2D return")
    parser.add_argument("--render-scale", default="default", help="default (native), native, adaptive, or an explicit pixel ratio")
    parser.add_argument("--device-scale-factor", type=float, default=2, help="Browser display density; default is Retina 2x")
    parser.add_argument("--viewport-width", type=int, default=1280)
    parser.add_argument("--viewport-height", type=int, default=800)
    parser.add_argument("--window-mode", choices=("default", "windowed", "maximized"), default="default")
    args = parser.parse_args()
    if args.viewport_width <= 0 or args.viewport_height <= 0:
        parser.error("viewport dimensions must be positive")
    if args.quality and args.software_gl:
        parser.error("--quality requires a working hardware renderer; use --software-gl separately")
    args.output.mkdir(parents=True, exist_ok=True)
    handler = partial(QuietHandler, directory=str(args.root.resolve()))
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    logs = []
    try:
        with sync_playwright() as playwright:
            browser_args = ["--use-angle=swiftshader", "--enable-unsafe-swiftshader"] if args.software_gl else []
            browser = playwright.chromium.launch(headless=not args.headed, channel=args.channel, args=browser_args)
            info = browser.new_browser_cdp_session().send("SystemInfo.getInfo")
            page = browser.new_page(viewport={"width": args.viewport_width, "height": args.viewport_height}, device_scale_factor=args.device_scale_factor, color_scheme="light")
            page.on("console", lambda message: logs.append(message.text))
            page.on("pageerror", lambda error: logs.append(str(error)))
            mode = "spatial-quality" if args.quality else "spatial-fallback" if args.software_gl else "spatial"
            query = {"wasm-smoke": mode}
            if args.window_mode != "default":
                query["window-mode"] = args.window_mode
            if args.render_scale != "default":
                query["render-scale"] = args.render_scale
            url = f"http://127.0.0.1:{server.server_port}/app/?{urlencode(query)}"
            page.goto(url)
            frames = []
            try:
                if args.quality:
                    for stage, name in enumerate(("2d", "gpu-flat", "3d", "2d-return")):
                        page.wait_for_function("stage => JSON.parse(document.documentElement.dataset.fluentQtQualityFrame || '{}').stage === stage", arg=stage, timeout=45_000)
                        frame = page.evaluate("JSON.parse(document.documentElement.dataset.fluentQtQualityFrame)")
                        frame["canvas"] = page.locator("canvas").evaluate("canvas => ({width: canvas.width, height: canvas.height, css: {width: canvas.clientWidth, height: canvas.clientHeight}})")
                        frames.append(frame)
                        page.screenshot(path=str(args.output / f"quality-{name}.png"))
                        xs, ys = zip(*frame["button"])
                        detail = page.screenshot(path=str(args.output / f"button-{name}.png"), clip={"x": min(xs)-2, "y": min(ys)-2, "width": max(xs)-min(xs)+5, "height": max(ys)-min(ys)+5})
                        # Compare real glyph-edge contrast, not only whether a frame exists.
                        # Normalization by ink mass tolerates the small perspective shrink.
                        frame["text_edge_contrast"] = page.evaluate("""async encoded => {
                            const bytes = Uint8Array.from(atob(encoded), c => c.charCodeAt(0));
                            const bitmap = await createImageBitmap(new Blob([bytes], {type: 'image/png'}));
                            const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
                            const context = canvas.getContext('2d');
                            context.drawImage(bitmap, 0, 0);
                            const data = context.getImageData(0, 0, canvas.width, canvas.height).data;
                            const gray = i => (data[i] + data[i+1] + data[i+2]) / 3;
                            let ink = 0, energy = 0;
                            for (let y = 0; y < canvas.height; ++y)
                                for (let x = 0; x < canvas.width; ++x) {
                                    const i = (y * canvas.width + x) * 4;
                                    const value = gray(i);
                                    ink += 255 - value;
                                    if (x) energy += (value - gray(i-4)) ** 2;
                                    if (y) energy += (value - gray(i-canvas.width*4)) ** 2;
                                }
                            return energy / Math.max(1, ink);
                        }""", base64.b64encode(detail).decode("ascii"))
                        page.evaluate("stage => document.documentElement.dataset.fluentQtQualityAcknowledged = String(stage)", stage)
                    reference = frames[0]["text_edge_contrast"]
                    # Report contrast retention to 0.1%. Fractional cache grids can
                    # vary by a few hundredths of a score unit at the cutoff.
                    if reference < 20 or any(round(frame["text_edge_contrast"] / reference, 3) < .9 for frame in frames[1:]):
                        raise RuntimeError(f"Projected text lost edge contrast: 2D={reference:.2f}, 3D={frames[2]['text_edge_contrast']:.2f}")
                    for frame in frames[1:3]:
                        cache = frame["cache"]
                        if cache["cacheDpr"] < frame["dpr"]:
                            raise RuntimeError("Panel cache fell below native pixel density")
                        if cache["cacheEstimatedBytes"] > cache["cacheBudgetBytes"]:
                            raise RuntimeError("Panel caches exceeded their aggregate budget")
                    profile = page.evaluate("window.fluentQtRenderProfile")
                    if args.render_scale in ("default", "native") and profile["effectiveDpr"] != profile["nativeDpr"]:
                        raise RuntimeError(f"Default rendering reduced native resolution: {profile}")
                    if any(frame["dpr"] != frames[0]["dpr"] for frame in frames[1:]):
                        raise RuntimeError("Enabling 3D changed the canvas pixel ratio")
                page.wait_for_function("['pass','fail'].includes(document.documentElement.dataset.fluentQtSmoke)", timeout=120_000)
                result = page.evaluate("({state: document.documentElement.dataset.fluentQtSmoke, detail: document.documentElement.dataset.fluentQtSmokeDetail, metrics: JSON.parse(document.documentElement.dataset.fluentQtSpatialMetrics || '{}')})")
            except Exception as error:
                result = {"state": "fail", "detail": str(error)}
            result["browser"] = browser.version
            result["gpu"] = info["gpu"]
            result["headless"] = not args.headed
            result["forced_software_gl"] = args.software_gl
            if frames:
                result["frames"] = frames
            page.screenshot(path=str(args.output / "web-spatial.png"))
            (args.output / "web-spatial.json").write_text(json.dumps(result, indent=2) + "\n")
            (args.output / "browser.log").write_text("\n".join(logs))
            print(json.dumps({key: value for key, value in result.items() if key != "gpu"}, indent=2))
            print("GPU devices:", info["gpu"].get("devices"))
            browser.close()
            return 0 if result["state"] == "pass" else 1
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    raise SystemExit(main())
