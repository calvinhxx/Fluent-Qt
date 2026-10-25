"""Check property edits, numeric dragging, PNG/CSV downloads and host clipboard."""

from __future__ import annotations

import csv
import io
import json
import struct
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_chart_workbench_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 2200}, device_scale_factor=2)
    # Qt 6.9 uses the native File System Access picker when showOpenFilePicker
    # exists. Headless Chromium aborts that OS dialog. Exercise Qt's real
    # download fallback, also used by browsers without File System Access.
    page.add_init_script("""
        Object.defineProperty(window, 'showOpenFilePicker', {value: undefined, configurable: true});
        Object.defineProperty(window, 'showSaveFilePicker', {value: undefined, configurable: true});
    """)
    page.context.grant_permissions(["clipboard-read", "clipboard-write"], origin=base_url)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    console_errors = []
    page.on("console", lambda message: console_errors.append(message.text)
            if message.type in {"error", "warning"} else None)

    def control(name: str) -> object:
        return page.locator(f'[id$=".{name}"][aria-hidden="false"]').last

    def click(name: str) -> None:
        exports = {"chartSavePng": "Save image as PNG…", "chartSaveCsv": "Save data as CSV…",
                   "chartCopyImage": "Copy image", "chartCopyData": "Copy data"}
        if name in exports:
            click("chartExportMenuButton")
            target = page.get_by_role("menuitem", name=exports[name], exact=True).last
        else:
            target = control(name)
        expect(target).to_be_visible()
        bounds = target.bounding_box()
        if not bounds:
            raise RuntimeError(f"Missing geometry: {name}")
        page.mouse.click(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2)

    def feedback(text: str) -> None:
        page.wait_for_function(
            """text => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".fluentToastMessage"][aria-hidden="false"]')]
                .some(node => (node.value ?? node.textContent).includes(text))""",
            arg=text, timeout=10_000,
        )

    def status(text: str) -> None:
        page.wait_for_function(
            """text => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".chartWorkbenchStatus"]')]
                .some(node => (node.value ?? node.textContent).includes(text))""",
            arg=text, timeout=10_000,
        )

    def document_title_is(text: str) -> None:
        page.wait_for_function(
            """text => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".propertyDocumentTitle"][aria-hidden="false"]')]
                .some(node => (node.value ?? node.textContent) === text)""",
            arg=text, timeout=10_000,
        )

    def qt_widget_rect(locator: object) -> dict:
        # Qt writes the widget dimensions into inline CSS. Chromium adds its
        # default input border/padding to the invisible content-box proxy, so
        # getBoundingClientRect alone is larger than the painted Qt editor.
        return locator.evaluate("""node => {
            const bounds = node.getBoundingClientRect();
            const width = parseFloat(node.style.width);
            const height = parseFloat(node.style.height);
            if (!Number.isFinite(width) || !Number.isFinite(height))
                throw new Error('Qt widget geometry is missing');
            return {x: bounds.x, y: bounds.y, width, height};
        }""")

    def number(expected: str) -> None:
        page.wait_for_function(
            """value => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".propertyEditor_maximum"]')]
                .some(node => (node.getAttribute('aria-valuenow') ?? node.value) === value)""",
            arg=expected, timeout=10_000,
        )

    def download(button: str, name: str) -> bytes:
        with page.expect_download() as event:
            click(button)
        result = event.value
        if result.suggested_filename != name:
            raise RuntimeError(f"Unexpected download name: {result.suggested_filename}")
        result.save_as(artifact_dir / name)
        return (artifact_dir / name).read_bytes()

    try:
        page.goto(f"{base_url}/app/index.html?route=property-panel&window-mode=maximized&wasm-smoke=interaction-state",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")

        document_editor = page.locator(
            '[id*=".documentProperties."][id$=".propertyEditor_title"][aria-hidden="false"]'
        )
        editor_bounds = document_editor.bounding_box()
        if not editor_bounds:
            raise RuntimeError("Document title editor has no browser bounds")
        page.mouse.click(editor_bounds["x"] + 30, editor_bounds["y"] + editor_bounds["height"] / 2)
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.type("Planning brief <draft>")
        document_title_is("Planning brief <draft>")
        page.screenshot(path=str(artifact_dir / "document-title.png"))
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.press("Backspace")
        document_title_is("Untitled document")
        reset = page.locator(
            '[id*=".documentProperties."][id$=".propertyReset_title"][aria-hidden="false"]'
        )
        # Focus and editing may scroll the Gallery. Compare both controls in
        # the current viewport instead of retaining the pre-focus position.
        editor_bounds = document_editor.bounding_box()
        reset_bounds = reset.bounding_box()
        if not editor_bounds or not reset_bounds:
            raise RuntimeError("Document title editor or reset has no browser bounds")
        editor_rect = qt_widget_rect(document_editor)
        reset_rect = qt_widget_rect(reset)
        if abs(reset_rect["y"] + reset_rect["height"] / 2
               - editor_rect["y"] - editor_rect["height"] / 2) > 1:
            raise RuntimeError(f"Document title reset is not centered beside the input: "
                               f"editor={editor_rect}, reset={reset_rect}")
        if reset_rect["x"] - editor_rect["x"] - editor_rect["width"] < 4:
            raise RuntimeError("Document title reset has no separation from the input")
        page.mouse.click(reset_bounds["x"] + reset_bounds["width"] / 2,
                         reset_bounds["y"] + reset_bounds["height"] / 2)
        document_title_is("Quarterly report")
        number("80")
        bounds = control("propertyEditor_maximum").bounding_box()
        if not bounds:
            raise RuntimeError("Missing NumberBox geometry")
        x, y = bounds["x"] + 16, bounds["y"] + 16
        page.mouse.move(x, y)
        page.mouse.down()
        page.mouse.move(x + 40, y, steps=5)
        number("100")
        page.keyboard.press("Escape")
        page.mouse.up()
        number("80")
        page.mouse.move(x, y)
        page.mouse.down()
        page.mouse.move(x + 40, y, steps=5)
        page.mouse.up()
        number("100")
        click("propertyReset_maximum")
        number("80")
        click("propertyEditor_title")
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.type("Weekly latency")
        expect(control("propertyReset_title")).to_be_enabled()
        click("propertyReset_title")
        click("chartExportScope")
        page.keyboard.press("ArrowDown")
        page.keyboard.press("Enter")
        status("4 original rows")
        data = download("chartSaveCsv", "response-time.csv")
        rows = list(csv.reader(io.StringIO(data.decode("utf-8"))))
        if len(rows) != 5 or [row[2] for row in rows[1:]] != ["1", "2", "3", "4"]:
            raise RuntimeError(f"CSV did not preserve the visible raw rows: {rows}")
        png = download("chartSavePng", "response-time.png")
        if not png.startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError("Image download is not PNG")
        width, height = struct.unpack(">II", png[16:24])
        if width < 520 or height < 640:
            raise RuntimeError(f"Unexpected PNG resolution: {width} x {height}")
        click("chartCopyData")
        feedback("Data copied")
        if page.evaluate("navigator.clipboard.readText()").replace("\r\n", "\n") != data.decode().replace("\r\n", "\n"):
            raise RuntimeError("Browser clipboard differs from exported CSV")
        click("chartCopyImage")
        feedback("Image copied")
        types = page.evaluate("navigator.clipboard.read().then(items => items.flatMap(item => item.types))")
        if "image/png" not in types:
            raise RuntimeError(f"Clipboard image type missing: {types}")
        # A rejected browser promise must not produce a success message.
        page.evaluate("() => { navigator.clipboard.write = () => Promise.reject(new Error('test denial')); }")
        click("chartCopyImage")
        feedback("Copy unavailable")
        status("4 original rows")
        expect(page.locator('[id$=".fluentToastMessage"][aria-hidden="false"]')).to_have_count(1)
        page.screenshot(path=str(artifact_dir / "workbench.png"))
        page.goto(f"{base_url}/app/index.html?route=chart-view&window-mode=maximized&wasm-smoke=interaction-state",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        page.locator("#qt-shadow-container .qt-window-a11y-container:visible "
                     "> button.hidden-visually-read-by-screen-reader").first.evaluate("button => button.click()")

        click("chartCopyDataButton")
        feedback("Data copied")
        copied = page.evaluate("navigator.clipboard.readText()")
        if not copied.startswith("series,name,x,y,label,unit"):
            raise RuntimeError("Atomic chart copy did not preserve the CSV header")
        click("chartCopyDataButton")
        feedback("Data copied")
        expect(page.locator('[id$=".fluentToastMessage"][aria-hidden="false"]')).to_have_count(1)
        page.screenshot(path=str(artifact_dir / "chart-copy-toast.png"))

        if errors:
            raise RuntimeError("Chart workbench browser errors: " + "; ".join(errors))
        (artifact_dir / "result.json").write_text(json.dumps({
            "status": "pass", "checks": ["document-title-edit-clear-reset", "reset-editor-alignment", "scrub-commit", "scrub-cancel", "reset", "title-edit",
                "raw-range-csv-download", "png-download", "clipboard-text", "clipboard-image", "copy-denial", "toast-feedback", "atomic-chart-copy", "no-toast-stacking"],
            "file_save_path": "qt-download-fallback", "csv_rows": len(rows) - 1,
            "png_size": [width, height], "page_errors": errors,
            "document_reset_geometry": {"editor": editor_rect, "reset": reset_rect},
        }, indent=2) + "\n", encoding="utf-8")
    except Exception:
        page.screenshot(path=str(artifact_dir / "failure.png"))
        (artifact_dir / "failure.json").write_text(json.dumps({
            "page_errors": errors, "console_errors": console_errors,
            "file_picker_available": page.evaluate("typeof window.showSaveFilePicker"),
        }, indent=2) + "\n", encoding="utf-8")
        raise
    finally:
        page.close()
