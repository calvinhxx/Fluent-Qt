#!/usr/bin/env python3

"""Launch the built WebAssembly examples in Chromium and verify runtime state."""

from __future__ import annotations

import argparse
import math
import sys
import threading
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlencode


DEFAULT_RENDER_PIXEL_BUDGET = 1_600_000
DEFAULT_RENDER_DPR_CAP = 1.25


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, _format: str, *_args: object) -> None:
        pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path("build/wasm"),
        help="WebAssembly build root containing app/ and examples/.",
    )
    parser.add_argument("--mode", choices=("fast", "full"), default="fast")
    parser.add_argument(
        "--device-scale-factor",
        type=float,
        default=2.0,
        help="Chromium device scale factor. Defaults to a representative Retina display.",
    )
    parser.add_argument(
        "--render-scale",
        default="default",
        help="Gallery render scale: default, native, or a positive number.",
    )
    parser.add_argument(
        "--window-mode",
        choices=("default", "windowed", "maximized"),
        default="default",
        help="Gallery top-level window presentation. Defaults to responsive auto mode.",
    )
    parser.add_argument("--viewport-width", type=int, default=1280)
    parser.add_argument("--viewport-height", type=int, default=800)
    return parser.parse_args()


def adaptive_render_dpr(
    device_scale_factor: float, viewport_width: int, viewport_height: int
) -> float:
    viewport_pixels = max(1, viewport_width * viewport_height)
    budget_dpr = math.sqrt(DEFAULT_RENDER_PIXEL_BUDGET / viewport_pixels)
    quantized = math.floor(min(DEFAULT_RENDER_DPR_CAP, budget_dpr) * 20) / 20
    return min(device_scale_factor, max(1.0, quantized))


def require_files(root: Path) -> None:
    required = (
        root / "app" / "index.html",
        root / "app" / "fluent_qt_gallery.js",
        root / "app" / "fluent_qt_gallery.wasm",
        root / "app" / "licenses.html",
        root / "app" / "hello-world" / "index.html",
        root / "app" / "hello-world" / "fluentqt_hello_world.js",
        root / "app" / "hello-world" / "fluentqt_hello_world.wasm",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError("Missing WebAssembly artifact(s): " + ", ".join(missing))


def run_smoke(
    root: Path,
    mode: str,
    device_scale_factor: float,
    render_scale: str,
    window_mode: str,
    viewport_width: int,
    viewport_height: int,
) -> None:
    try:
        from playwright.sync_api import sync_playwright
    except ImportError as error:
        raise RuntimeError(
            "Playwright is required: python -m pip install playwright"
        ) from error

    handler = partial(QuietHandler, directory=str(root))
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    base_url = f"http://127.0.0.1:{server.server_port}"

    page_errors: list[str] = []
    local_request_failures: list[str] = []
    console_messages: list[str] = []
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True)
            page = browser.new_page(
                viewport={"width": viewport_width, "height": viewport_height},
                device_scale_factor=device_scale_factor,
            )
            page.on("pageerror", lambda error: page_errors.append(str(error)))
            page.on(
                "console",
                lambda message: console_messages.append(
                    f"{message.type}: {message.text}"
                ),
            )

            def record_failed_request(request: object) -> None:
                url = str(getattr(request, "url", ""))
                if url.startswith(base_url):
                    failure = getattr(request, "failure", None)
                    local_request_failures.append(f"{url}: {failure}")

            page.on("requestfailed", record_failed_request)

            query = {"wasm-smoke": mode}
            if render_scale != "default":
                query["render-scale"] = render_scale
            if window_mode != "default":
                query["window-mode"] = window_mode
            gallery_url = f"{base_url}/app/index.html?{urlencode(query)}"
            gallery_started = time.perf_counter()
            response = page.goto(gallery_url, wait_until="domcontentloaded")
            if response is None or not response.ok:
                raise RuntimeError(f"Gallery request failed: {gallery_url}")

            # Wait for the C++ probe to expose a real LineEdit coordinate, then
            # drive physical browser keys. Setting the hidden HTML input value
            # directly would bypass the Qt WASM input-context path that this
            # smoke is intended to protect.
            page.wait_for_function(
                """() => {
                    const root = document.documentElement.dataset;
                    return root.fluentQtTextInputState === 'ready'
                        || root.fluentQtSmoke === 'fail';
                }""",
                timeout=180_000,
            )
            input_probe = page.evaluate(
                """() => ({
                    state: document.documentElement.dataset.fluentQtTextInputState,
                    x: Number(document.documentElement.dataset.fluentQtTextInputX),
                    y: Number(document.documentElement.dataset.fluentQtTextInputY),
                    expected: document.documentElement.dataset.fluentQtTextInputExpected,
                    smoke: document.documentElement.dataset.fluentQtSmoke,
                    detail: document.documentElement.dataset.fluentQtSmokeDetail
                })"""
            )
            if input_probe["smoke"] == "fail":
                raise RuntimeError(
                    "Gallery failed before browser text input: "
                    f"{input_probe['detail']}"
                )
            if (
                input_probe["state"] != "ready"
                or not input_probe["expected"]
                or not math.isfinite(input_probe["x"])
                or not math.isfinite(input_probe["y"])
            ):
                raise RuntimeError(
                    f"Gallery exposed an invalid text input probe: {input_probe}"
                )
            # Headless Chromium does not always activate a freshly created page
            # before its first canvas click. Bring it forward so this mirrors a
            # user's trusted click instead of testing an inactive tab.
            page.bring_to_front()
            canvas = page.locator("canvas")
            canvas_box = canvas.bounding_box()
            if canvas_box is None:
                raise RuntimeError("Gallery canvas has no clickable geometry")
            canvas.click(
                position={
                    "x": input_probe["x"] - canvas_box["x"],
                    "y": input_probe["y"] - canvas_box["y"],
                },
                force=True,
            )
            # Chromium's headless backend has no OS browser window to activate,
            # so a trusted canvas click may leave document.activeElement on the
            # body even though the Qt widget received focus. The C++ smoke has
            # already asserted that the real browser surface accepts focus;
            # explicitly focus Qt's hidden input here only to continue testing
            # physical key delivery in headless CI.
            if page.evaluate(
                "document.activeElement?.tagName !== 'INPUT'"
            ):
                page.locator("body > input[type='text']").focus()
            page.wait_for_function(
                "document.activeElement?.tagName === 'INPUT'",
                timeout=5_000,
            )
            for key in input_probe["expected"]:
                page.keyboard.press(key)
            page.wait_for_function(
                """() => {
                    const root = document.documentElement.dataset;
                    return root.fluentQtTextInputState === 'pass'
                        || root.fluentQtTextInputState === 'fail'
                        || root.fluentQtSmoke === 'fail';
                }""",
                timeout=15_000,
            )
            input_result = page.evaluate(
                """() => ({
                    state: document.documentElement.dataset.fluentQtTextInputState,
                    smoke: document.documentElement.dataset.fluentQtSmoke,
                    detail: document.documentElement.dataset.fluentQtSmokeDetail
                })"""
            )
            if input_result["state"] != "pass":
                raise RuntimeError(
                    f"Browser text input probe failed: {input_result}"
                )

            page.wait_for_function(
                "['pass', 'fail'].includes(document.documentElement.dataset.fluentQtSmoke)",
                timeout=180_000,
            )
            gallery_state = page.evaluate(
                """() => ({
                    loaded: document.documentElement.dataset.fluentQtLoaded,
                    smoke: document.documentElement.dataset.fluentQtSmoke,
                    detail: document.documentElement.dataset.fluentQtSmokeDetail,
                    error: document.documentElement.dataset.fluentQtError || '',
                    nativeDpr: Number(document.documentElement.dataset.fluentQtNativeDpr),
                    renderDpr: Number(document.documentElement.dataset.fluentQtRenderDpr),
                    renderMode: document.documentElement.dataset.fluentQtRenderMode,
                    renderProfile: document.documentElement.dataset.fluentQtRenderProfile,
                    pixelBudget: Number(document.documentElement.dataset.fluentQtPixelBudget),
                    windowMode: document.documentElement.dataset.fluentQtWindowMode,
                    stageSurface: (() => {
                        const host = document.querySelector('#qt-container');
                        const footer = document.querySelector('footer');
                        const rect = host.getBoundingClientRect();
                        return {
                            x: rect.x,
                            y: rect.y,
                            width: rect.width,
                            height: rect.height,
                            viewportWidth: innerWidth,
                            stageHeight: footer.getBoundingClientRect().top
                        };
                    })(),
                    windowSurface: (() => {
                        const shadow = document.querySelector(
                            '#qt-shadow-container'
                        )?.shadowRoot;
                        const desktop = shadow?.querySelector(
                            '.qt-decorated-window'
                        );
                        const root = document.documentElement.dataset;
                        if (!desktop || !root.fluentQtWindowWidth)
                            return null;
                        return {
                            x: Number(root.fluentQtWindowX),
                            y: Number(root.fluentQtWindowY),
                            width: Number(root.fluentQtWindowWidth),
                            height: Number(root.fluentQtWindowHeight),
                            maximized: root.fluentQtWindowMaximized === 'true',
                            frameless: desktop.classList.contains('frameless')
                        };
                    })()
                })"""
            )
            gallery_seconds = time.perf_counter() - gallery_started
            if gallery_state["loaded"] != "true":
                raise RuntimeError(f"Gallery loader did not complete: {gallery_state}")
            if gallery_state["smoke"] != "pass" or gallery_state["error"]:
                raise RuntimeError(f"Gallery smoke failed: {gallery_state}")
            if page.locator("canvas").count() != 1:
                raise RuntimeError(
                    "Gallery canvas is missing from the Qt loader shadow tree"
                )
            if abs(gallery_state["nativeDpr"] - device_scale_factor) > 0.01:
                raise RuntimeError(
                    "Browser native DPR does not match the requested device scale: "
                    f"{gallery_state}"
                )
            if render_scale == "native":
                expected_render_dpr = device_scale_factor
                expected_render_mode = "native"
                expected_render_profile = "native"
            elif render_scale == "default":
                expected_render_dpr = adaptive_render_dpr(
                    device_scale_factor, viewport_width, viewport_height
                )
                expected_render_mode = (
                    "performance"
                    if expected_render_dpr < device_scale_factor
                    else "native"
                )
                expected_render_profile = "adaptive"
            else:
                try:
                    requested_render_dpr = float(render_scale)
                except ValueError as error:
                    raise RuntimeError(
                        f"Invalid --render-scale value: {render_scale}"
                    ) from error
                if requested_render_dpr <= 0:
                    raise RuntimeError("--render-scale must be positive")
                expected_render_dpr = min(
                    device_scale_factor, max(1.0, requested_render_dpr)
                )
                expected_render_mode = (
                    "performance"
                    if expected_render_dpr < device_scale_factor
                    else "native"
                )
                expected_render_profile = "fixed"
            if abs(gallery_state["renderDpr"] - expected_render_dpr) > 0.01:
                raise RuntimeError(
                    "Gallery render DPR does not match the selected profile: "
                    f"{gallery_state}"
                )
            if gallery_state["renderMode"] != expected_render_mode:
                raise RuntimeError(
                    "Gallery render mode does not match the selected profile: "
                    f"{gallery_state}"
                )
            if gallery_state["renderProfile"] != expected_render_profile:
                raise RuntimeError(
                    "Gallery render profile does not match the selected profile: "
                    f"{gallery_state}"
                )
            if gallery_state["pixelBudget"] != DEFAULT_RENDER_PIXEL_BUDGET:
                raise RuntimeError(
                    f"Gallery pixel budget is incorrect: {gallery_state}"
                )
            expected_window_mode = (
                (
                    "maximized"
                    if viewport_width < 948 or viewport_height < 678
                    else "windowed"
                )
                if window_mode == "default"
                else window_mode
            )
            if gallery_state["windowMode"] != expected_window_mode:
                raise RuntimeError(
                    "Gallery window mode does not match the selected profile: "
                    f"{gallery_state}"
                )
            stage = gallery_state["stageSurface"]
            if (
                abs(stage["x"]) > 1
                or abs(stage["y"]) > 1
                or abs(stage["width"] - stage["viewportWidth"]) > 2
                or abs(stage["height"] - stage["stageHeight"]) > 2
            ):
                raise RuntimeError(
                    "Qt browser stage does not fill the available page area: "
                    f"{gallery_state}"
                )
            surface = gallery_state["windowSurface"]
            if surface is None or not surface["frameless"]:
                raise RuntimeError(
                    "Gallery did not create a Fluent-owned top-level window: "
                    f"{gallery_state}"
                )
            if expected_window_mode == "windowed":
                if (
                    surface["maximized"]
                    or
                    surface["x"] < 16
                    or surface["y"] < 16
                    or surface["width"] >= stage["viewportWidth"] - 32
                    or surface["height"] >= stage["stageHeight"] - 32
                ):
                    raise RuntimeError(
                        "Windowed Gallery is not visibly inset inside the Qt stage: "
                        f"{gallery_state}"
                    )
            elif (
                not surface["maximized"]
                or abs(surface["x"]) > 1
                or abs(surface["y"]) > 1
                or abs(surface["width"] - stage["viewportWidth"]) > 2
                or abs(surface["height"] - stage["stageHeight"]) > 2
            ):
                raise RuntimeError(
                    "Maximized Gallery does not fill the browser stage: "
                    f"{gallery_state}"
                )

            licenses = page.goto(
                f"{base_url}/app/licenses.html", wait_until="domcontentloaded"
            )
            if licenses is None or not licenses.ok:
                raise RuntimeError("Could not load the WebAssembly license page")
            license_text = page.locator("body").inner_text()
            for expected in (
                "Qt 6.9.3",
                "Emscripten 3.1.70",
                "GPL version 3",
                "Noto Sans SC",
            ):
                if expected not in license_text:
                    raise RuntimeError(
                        f"License/build page is missing expected text: {expected}"
                    )

            hello_url = f"{base_url}/app/hello-world/index.html"
            hello = page.goto(hello_url, wait_until="domcontentloaded")
            if hello is None or not hello.ok:
                raise RuntimeError(f"Hello World request failed: {hello_url}")
            page.locator("canvas").wait_for(state="visible", timeout=60_000)
            page.wait_for_function(
                "document.documentElement.dataset.fluentQtLoaded === 'true'",
                timeout=60_000,
            )
            page.wait_for_timeout(1_000)
            hello_state = page.evaluate(
                """() => {
                    const host = document.querySelector('#qt-container');
                    const stage = host.getBoundingClientRect();
                    const shadow = document.querySelector(
                        '#qt-shadow-container'
                    )?.shadowRoot;
                    const desktop = shadow?.querySelector(
                        '.qt-decorated-window'
                    );
                    const root = document.documentElement.dataset;
                    return {
                        loaded: document.documentElement.dataset.fluentQtLoaded,
                        error: document.documentElement.dataset.fluentQtError || '',
                        stageX: stage.x,
                        stageY: stage.y,
                        stageWidth: stage.width,
                        stageHeight: stage.height,
                        x: Number(root.fluentQtWindowX ?? -1),
                        y: Number(root.fluentQtWindowY ?? -1),
                        width: Number(root.fluentQtWindowWidth ?? 0),
                        height: Number(root.fluentQtWindowHeight ?? 0),
                        viewportWidth: innerWidth,
                        viewportHeight: innerHeight,
                        frameless: desktop?.classList.contains('frameless') || false
                    };
                }"""
            )
            if (
                hello_state["loaded"] != "true"
                or hello_state["error"]
                or abs(hello_state["stageX"]) > 1
                or abs(hello_state["stageY"]) > 1
                or abs(hello_state["stageWidth"] - hello_state["viewportWidth"]) > 2
                or hello_state["x"] < 16
                or hello_state["y"] < 16
                or hello_state["width"] >= hello_state["viewportWidth"] - 32
                or hello_state["height"] >= hello_state["stageHeight"] - 32
                or not hello_state["frameless"]
            ):
                raise RuntimeError(
                    "Minimal UILib app is not a visible windowed consumer: "
                    f"{hello_state}"
                )

            if page_errors:
                raise RuntimeError("Browser page error(s): " + " | ".join(page_errors))
            if local_request_failures:
                raise RuntimeError(
                    "Local asset request failure(s): "
                    + " | ".join(local_request_failures)
                )

            browser.close()
    except Exception:
        if console_messages:
            print("Browser console tail:", file=sys.stderr)
            for message in console_messages[-30:]:
                print(f"  {message}", file=sys.stderr)
        raise
    finally:
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)

    print(
        "WebAssembly browser smoke passed: "
        f"mode={mode}, browser_elapsed={gallery_seconds:.2f}s, "
        f"viewport={viewport_width}x{viewport_height}, "
        f"dpr={gallery_state['renderDpr']}/{gallery_state['nativeDpr']} "
        f"({gallery_state['renderMode']}), window={gallery_state['windowMode']}, "
        f"gallery={gallery_state['detail']}, "
        "hello_world=windowed"
    )


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    try:
        require_files(root)
        run_smoke(
            root,
            args.mode,
            args.device_scale_factor,
            args.render_scale,
            args.window_mode,
            args.viewport_width,
            args.viewport_height,
        )
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
