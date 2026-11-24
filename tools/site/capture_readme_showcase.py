#!/usr/bin/env python3
"""Record an optional WebP demo from Page Studio, the WASM Gallery and native screenshots."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import ThreadingHTTPServer
from io import BytesIO
import json
import math
from pathlib import Path
from tempfile import TemporaryDirectory
import threading

from PIL import Image, ImageDraw, ImageFilter, ImageFont
from playwright.sync_api import expect, sync_playwright

from serve import SiteHandler


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WASM_ROOT = ROOT / "build/wasm/app"
DEFAULT_OUTPUT = ROOT / "build/readme-showcase/hero-showcase.webp"
FONT_PATH = ROOT / "res/fonts/FluentQtUIDisplay-Semibold.ttf"

CANVAS_WIDTH = 1280
CANVAS_HEIGHT = 788
INNER_WIDTH = 1152
INNER_HEIGHT = 720
INNER_X = (CANVAS_WIDTH - INNER_WIDTH) // 2
INNER_Y = 50
CORNER_RADIUS = 20

SPATIAL_HOST_HTML = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <style>
    html, body, iframe {
      margin: 0;
      width: 100%;
      height: 100%;
      border: 0;
      overflow: hidden;
      background: #141820;
    }
  </style>
</head>
<body>
  <iframe
    id="spatial-frame"
    src="/gallery/?embed=site&amp;showcase=spatial&amp;window-mode=maximized&amp;render-scale=1&amp;host-theme=dark&amp;host-session=1"
  ></iframe>
  <script>
    window.fluentQtShowcaseReady = false;
    window.addEventListener("message", function (event) {
      var frame = document.getElementById("spatial-frame");
      if (
        event.origin === window.location.origin &&
        event.source === frame.contentWindow &&
        event.data &&
        event.data.source === "fluent-qt-gallery" &&
        event.data.state === "ready"
      ) {
        frame.contentWindow.postMessage(
          { source: "fluent-qt-site", type: "theme", theme: "dark" },
          window.location.origin
        );
        frame.contentWindow.postMessage(
          { source: "fluent-qt-site", type: "showcase-visibility", active: true },
          window.location.origin
        );
        frame.contentWindow.postMessage(
          { source: "fluent-qt-site", type: "showcase-motion", reduced: false },
          window.location.origin
        );
        window.fluentQtShowcaseReady = true;
      }
    });
  </script>
</body>
</html>
""".encode("utf-8")


class ShowcaseHandler(SiteHandler):
    """Serve the static site plus a clean full-bleed host page for SpatialView capture."""

    def log_message(self, *_args):
        pass

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/__showcase_spatial__":
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(SPATIAL_HOST_HTML)))
            self.end_headers()
            self.wfile.write(SPATIAL_HOST_HTML)
            return
        super().do_GET()


def build_backdrop() -> Image.Image:
    """Create the framed canvas with a soft gradient backdrop and rounded drop shadow."""
    backdrop = Image.new("RGBA", (CANVAS_WIDTH, CANVAS_HEIGHT), (244, 249, 253, 255))
    draw = ImageDraw.Draw(backdrop)
    top = (247, 251, 255)
    bottom = (232, 241, 249)
    for y in range(CANVAS_HEIGHT):
        t = y / max(1, CANVAS_HEIGHT - 1)
        color = tuple(int(top[i] * (1.0 - t) + bottom[i] * t) for i in range(3)) + (255,)
        draw.line([(0, y), (CANVAS_WIDTH, y)], fill=color)

    shadow = Image.new("RGBA", (CANVAS_WIDTH, CANVAS_HEIGHT), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.rounded_rectangle(
        (INNER_X, INNER_Y + 6, INNER_X + INNER_WIDTH, INNER_Y + INNER_HEIGHT + 6),
        radius=CORNER_RADIUS,
        fill=(18, 38, 58, 72),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(12))
    backdrop.alpha_composite(shadow)
    return backdrop


def build_rounded_mask() -> Image.Image:
    """Supersampled rounded rectangle alpha mask for crisp window corners."""
    scale = 4
    mask = Image.new("L", (INNER_WIDTH * scale, INNER_HEIGHT * scale), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle(
        (0, 0, INNER_WIDTH * scale - 1, INNER_HEIGHT * scale - 1),
        radius=CORNER_RADIUS * scale,
        fill=255,
    )
    return mask.resize((INNER_WIDTH, INNER_HEIGHT), Image.Resampling.LANCZOS)


def load_badge_font() -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    if FONT_PATH.is_file():
        return ImageFont.truetype(str(FONT_PATH), 15)
    return ImageFont.load_default()


def resize_capture(image: Image.Image) -> Image.Image:
    rgb = image.convert("RGB")
    if rgb.size != (INNER_WIDTH, INNER_HEIGHT):
        rgb = rgb.resize((INNER_WIDTH, INNER_HEIGHT), Image.Resampling.LANCZOS)
    return rgb


def compose_frame(
    content: Image.Image,
    label: str,
    backdrop: Image.Image,
    mask: Image.Image,
    font: ImageFont.FreeTypeFont | ImageFont.ImageFont,
    dark_badge: bool = False,
) -> Image.Image:
    """Composite a 1152x720 scene image onto the backdrop with a rounded border and label pill."""
    frame = backdrop.copy()
    card = content.convert("RGBA")
    card.putalpha(mask)
    frame.alpha_composite(card, (INNER_X, INNER_Y))

    overlay = Image.new("RGBA", (CANVAS_WIDTH, CANVAS_HEIGHT), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    draw.rounded_rectangle(
        (INNER_X, INNER_Y, INNER_X + INNER_WIDTH, INNER_Y + INNER_HEIGHT),
        radius=CORNER_RADIUS,
        outline=(205, 218, 230, 215),
        width=1,
    )

    text_box = draw.textbbox((0, 0), label, font=font)
    text_w = text_box[2] - text_box[0]
    text_h = text_box[3] - text_box[1]
    pad_x = 16
    pad_y = 7
    pill_w = text_w + pad_x * 2
    pill_h = text_h + pad_y * 2
    pill_x1 = (CANVAS_WIDTH - pill_w) // 2
    pill_x2 = pill_x1 + pill_w
    pill_y1 = (INNER_Y - pill_h) // 2
    pill_y2 = pill_y1 + pill_h

    fill = (15, 23, 42, 232) if not dark_badge else (22, 32, 48, 240)
    border = (255, 255, 255, 68)
    draw.rounded_rectangle(
        (pill_x1, pill_y1, pill_x2, pill_y2),
        radius=pill_h // 2,
        fill=fill,
        outline=border,
        width=1,
    )
    text_x = pill_x1 + pad_x - text_box[0]
    text_y = pill_y1 + pad_y - text_box[1]
    draw.text((text_x, text_y), label, font=font, fill=(248, 250, 252, 255))

    frame.alpha_composite(overlay)
    return frame.convert("RGB")


def add_crossfade(
    frames: list[Image.Image],
    durations: list[int],
    start_img: Image.Image,
    end_img: Image.Image,
    steps: int = 4,
    step_ms: int = 80,
) -> None:
    for i in range(1, steps + 1):
        alpha = i / (steps + 1)
        frames.append(Image.blend(start_img, end_img, alpha))
        durations.append(step_ms)


def capture_raw_scenes(base_url: str) -> dict[str, object]:
    """Drive Page Studio and SpatialView in headless Chromium and return raw PIL captures."""
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True)
        try:
            context = browser.new_context(
                viewport={"width": 1440, "height": 900},
                device_scale_factor=1,
                reduced_motion="no-preference",
            )
            page = context.new_page()

            # Scene 2 & 3: Page Studio (Live C++ WASM + Theme switch + 1-Click Export)
            page.goto(base_url + "studio/?lang=en&template=analytics", wait_until="domcontentloaded")
            page.wait_for_selector('#runtime-badge[data-state="wasm"]', timeout=45000)
            page.wait_for_selector("#canvas-shell.is-wasm-ready", timeout=20000)
            page.wait_for_function(
                """() => {
                    const loader = document.querySelector('#wasm-loader');
                    return Boolean(loader && getComputedStyle(loader).visibility === 'hidden');
                }""",
                timeout=15000,
            )
            page.wait_for_selector('[data-canvas-node="chart_line"]', timeout=20000)
            studio_light = resize_capture(Image.open(BytesIO(page.screenshot())))

            page.locator('[data-canvas-node="chart_line"]').click()
            expect(page.locator('.tree-row[data-node-id="chart_line"]')).to_have_attribute("aria-selected", "true")
            studio_selected = resize_capture(Image.open(BytesIO(page.screenshot())))

            page.locator("#artboard-theme-dark").click()
            page.locator('.artboard-swatch[data-artboard-accent="#00B7C3"]').click()
            expect(page.locator("#artboard-theme-dark")).to_have_attribute("aria-pressed", "true")
            expect(page.locator('.artboard-swatch[data-artboard-accent="#00B7C3"]')).to_have_attribute("aria-pressed", "true")
            studio_dark = resize_capture(Image.open(BytesIO(page.screenshot())))

            page.locator("#btn-code").click()
            page.wait_for_selector("#code-drawer:not([hidden])", timeout=10000)
            exports = {}
            for fmt, marker in (("cpp", "#include"), ("python", "from PySide6"), ("ui", "<ui version=")):
                page.locator(f'.drawer-tab[data-format="{fmt}"]').click()
                expect(page.locator("#code-output code")).to_contain_text(marker)
                exports[f"export_{fmt}"] = resize_capture(Image.open(BytesIO(page.screenshot())))

            # Scene 4: SpatialView (Live 3D WebGL widgets)
            spatial_page = context.new_page()
            spatial_page.goto(base_url + "__showcase_spatial__", wait_until="domcontentloaded")
            spatial_page.wait_for_function("() => window.fluentQtShowcaseReady === true", timeout=45000)
            spatial_page.wait_for_timeout(450)

            spatial_frames: list[Image.Image] = []
            steps = 12
            for idx in range(steps):
                t = idx / max(1, steps - 1)
                x = 280 + (1160 - 280) * t
                y = 450 - math.sin(t * math.pi) * 180
                spatial_page.mouse.move(x, y)
                spatial_page.wait_for_timeout(90)
                spatial_frames.append(resize_capture(Image.open(BytesIO(spatial_page.screenshot()))))

            spatial_page.close()
            context.close()
        finally:
            browser.close()

    return {
        "studio_light": studio_light,
        "studio_selected": studio_selected,
        "studio_dark": studio_dark,
        **exports,
        "spatial_frames": spatial_frames,
    }


def save_animated_webp(frames: list[Image.Image], durations: list[int], output: Path, max_bytes: int) -> int:
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=output.parent) as directory:
        candidate = Path(directory) / "showcase.webp"
        for scale in (1, 0.88):
            images = frames
            if scale < 1:
                dimensions = tuple(max(1, round(n * scale)) for n in frames[0].size)
                images = [frame.resize(dimensions, Image.Resampling.LANCZOS) for frame in frames]
            for quality in (82, 74, 66):
                images[0].save(candidate, save_all=True, append_images=images[1:],
                               duration=durations, loop=0, quality=quality, method=4)
                size = candidate.stat().st_size
                if size <= max_bytes:
                    with Image.open(candidate) as encoded:
                        if not encoded.is_animated:
                            raise ValueError("The recording must contain multiple distinct frames")
                    candidate.replace(output)
                    return size
    raise ValueError(f"Could not encode the recording within the {max_bytes}-byte asset budget")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wasm-root", type=Path, default=DEFAULT_WASM_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    wasm_root = args.wasm_root.resolve()
    for name in ("index.html", "fluent_qt_gallery.js", "fluent_qt_gallery.wasm"):
        if not (wasm_root / name).is_file():
            parser.error(f"Missing {wasm_root / name}; build fluent_qt_gallery with the wasm preset.")

    gallery_light = resize_capture(Image.open(ROOT / "site/assets/gallery/gallery-home-real.png"))
    gallery_dark = resize_capture(Image.open(ROOT / "site/assets/gallery/gallery-home-dark.png"))

    handler = partial(ShowcaseHandler, wasm_root=wasm_root)
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base_url = f"http://127.0.0.1:{server.server_port}/"
        captured = capture_raw_scenes(base_url)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

    backdrop = build_backdrop()
    mask = build_rounded_mask()
    font = load_badge_font()

    frame_gallery_light = compose_frame(gallery_light, "Gallery · Light Mica", backdrop, mask, font)
    frame_gallery_dark = compose_frame(gallery_dark, "Gallery · Dark Mica", backdrop, mask, font, dark_badge=True)
    frame_studio_light = compose_frame(captured["studio_light"], "Page Studio · Live C++ WASM", backdrop, mask, font)
    frame_studio_selected = compose_frame(captured["studio_selected"], "Page Studio · Live Inspector", backdrop, mask, font)
    frame_studio_dark = compose_frame(captured["studio_dark"], "Page Studio · Dark + Accent", backdrop, mask, font, dark_badge=True)
    frame_export_cpp = compose_frame(captured["export_cpp"], "1-Click Export · C++17", backdrop, mask, font, dark_badge=True)
    frame_export_python = compose_frame(captured["export_python"], "1-Click Export · PySide6", backdrop, mask, font, dark_badge=True)
    frame_export_ui = compose_frame(captured["export_ui"], "1-Click Export · Qt Designer .ui", backdrop, mask, font, dark_badge=True)
    frames_spatial = [
        compose_frame(raw, "SpatialView · Live 3D Widgets", backdrop, mask, font, dark_badge=True)
        for raw in captured["spatial_frames"]
    ]

    frames: list[Image.Image] = []
    durations: list[int] = []

    # Scene 1: Gallery Light -> Dark Mica (~2.2s)
    frames.append(frame_gallery_light)
    durations.append(950)
    add_crossfade(frames, durations, frame_gallery_light, frame_gallery_dark, steps=4, step_ms=75)
    frames.append(frame_gallery_dark)
    durations.append(950)

    # Scene 2: Page Studio Live C++ WASM (~2.6s)
    add_crossfade(frames, durations, frame_gallery_dark, frame_studio_light, steps=3, step_ms=75)
    frames.append(frame_studio_light)
    durations.append(850)
    frames.append(frame_studio_selected)
    durations.append(750)
    add_crossfade(frames, durations, frame_studio_selected, frame_studio_dark, steps=3, step_ms=75)
    frames.append(frame_studio_dark)
    durations.append(850)

    # Scene 3: 1-Click Export C++17 -> PySide6 -> .ui (~2.2s)
    frames.append(frame_export_cpp)
    durations.append(700)
    frames.append(frame_export_python)
    durations.append(850)
    frames.append(frame_export_ui)
    durations.append(700)

    # Scene 4: Live 3D SpatialView (~2.2s) & seamless loop back to Scene 1
    add_crossfade(frames, durations, frame_export_ui, frames_spatial[0], steps=3, step_ms=75)
    for idx, spatial_frame in enumerate(frames_spatial):
        frames.append(spatial_frame)
        durations.append(450 if idx == len(frames_spatial) - 1 else 90)
    add_crossfade(frames, durations, frames_spatial[-1], frame_gallery_light, steps=4, step_ms=75)

    budget = json.loads((ROOT / "tools/dev/repository-size-budget.json").read_text(encoding="utf-8"))
    byte_size = save_animated_webp(frames, durations, args.output, budget["max_file_bytes"])
    with Image.open(args.output) as verify_img:
        print(
            f"Generated {args.output}: {verify_img.size[0]}x{verify_img.size[1]}, "
            f"{verify_img.n_frames} frames, {sum(durations) / 1000:.2f}s loop, {byte_size / 1024:.1f} KiB"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
