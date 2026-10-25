"""Exercise the shared file-browser sample with browser input and resizing."""

from __future__ import annotations

import json
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_list_detail_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    # Keep the 600 px sample and its preview controls visible below the Gallery
    # teaching block. Qt 6.9's AX child positions can lag behind outer scrolling.
    page = browser.new_page(viewport={"width": 1440, "height": 2000}, device_scale_factor=2)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))

    def control(name: str) -> object:
        # Qt's browser accessibility bridge exposes the native pane but does
        # not create a DOM proxy for QListView itself.
        if name == "fileBrowserList":
            return page.locator(
                '[id*=".fileBrowserLayout."][id$=".listDetailListPane"][aria-hidden="false"]'
            )
        return page.locator(f'[id$=".{name}"][aria-hidden="false"]')

    def click(name: str) -> None:
        target = control(name)
        expect(target).to_be_visible()
        bounds = target.bounding_box()
        if bounds is None:
            raise RuntimeError(f"Missing browser bounds: {name}")
        back = control("fileBrowserBack")
        if name == "fileBrowserNarrow" and back.count():
            # The sample is centered after narrowing. The newly shown back
            # button has a current position; persistent child proxies can
            # still contain the wide sample's previous horizontal origin.
            fresh = back.bounding_box()
            page.mouse.click(fresh["x"] + 12, bounds["y"] + bounds["height"] / 2)
            return
        if name == "fileBrowserEditor":
            # Qt 6.9's child proxies can retain the previous position after a
            # resize. Use the persistent host origin and the freshly shown
            # list pane to locate the actual native editor, without changing
            # proxy or application state. Both positions are inside its body.
            host = control("fileBrowserLayout").bounding_box()
            if control("fileBrowserList").count():
                pane = control("fileBrowserList").bounding_box()
                x = pane["x"] + pane["width"] + 32
            else:
                x = back.bounding_box()["x"] + 24
            page.mouse.click(x, host["y"] + 100)
            return
        page.mouse.click(bounds["x"] + 24, bounds["y"] + bounds["height"] / 2)

    def draft_is_retained() -> None:
        # Qt exposes the current editor value through its focused IME input;
        # the accessibility input can remain empty after the editor loses focus.
        click("fileBrowserEditor")
        page.wait_for_function(
            """() => [document, document.querySelector('#qt-shadow-container').shadowRoot]
                .some(root => [...root.querySelectorAll('[role="textbox"], textarea, input')]
                    .some(node => (node.value ?? node.textContent).includes('Browser draft kept')))""",
            timeout=10_000,
        )

    def list_pixels(filename: str, bounds: dict[str, float]) -> bytes:
        if bounds is None:
            raise RuntimeError("Missing list pane geometry")
        page.mouse.move(1320, 180)
        page.wait_for_timeout(350)  # Settle the existing list hover/scroll motion.
        return page.screenshot(path=str(artifact_dir / filename), clip={
            "x": bounds["x"] + 16, "y": bounds["y"] + 8,
            "width": bounds["width"] - 48, "height": bounds["height"] - 16,
        })

    try:
        page.goto(f"{base_url}/app/index.html?route=list-detail-view&window-mode=maximized&wasm-smoke=component-regressions",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        page.wait_for_function("document.documentElement.dataset.fluentQtComponentRegressions", timeout=20_000)
        regressions = page.evaluate("JSON.parse(document.documentElement.dataset.fluentQtComponentRegressions)")
        (artifact_dir / "component-regressions.json").write_text(json.dumps(regressions, indent=2), encoding="utf-8")
        if regressions["status"] != "pass":
            raise RuntimeError(f"Shared component regressions: {regressions}")
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")

        width = control("listDetailPreviewWidth")
        basic = control("basicListDetailView")
        def basic_control(pane: str, widget_class: str) -> object:
            return page.locator(f'[id*=".basicListDetailView.{pane}."]'
                                f'[id$=".{widget_class}"][aria-hidden="false"]')

        note = basic_control("listDetailDetailPane", "fluent::textfields::LineEdit")
        open_note = basic_control("listDetailListPane", "fluent::basicinput::Button")
        back_to_notes = basic_control("listDetailDetailPane", "fluent::basicinput::Button")

        def note_is_retained() -> None:
            # LineEdit uses Qt's focused IME input for its value, while its
            # persistent accessibility proxy carries the accessible name.
            page.wait_for_function("""() =>
                [document, document.querySelector('#qt-shadow-container').shadowRoot]
                    .some(root => [...root.querySelectorAll('input, textarea')]
                        .some(node => node.value === 'A note kept across layouts'))""")

        def click_basic(target: object) -> None:
            expect(target).to_be_visible()
            bounds = target.bounding_box()
            page.mouse.click(bounds["x"] + 24, bounds["y"] + bounds["height"] / 2)
        expect(width).to_be_visible()
        expect(note).to_be_visible()
        wide_width = basic.bounding_box()["width"]
        click("listDetailPreviewWidth")
        page.keyboard.press("End")
        page.keyboard.press("Enter")
        expect(note).to_have_count(0)
        expect(open_note).to_be_visible()
        page.screenshot(path=str(artifact_dir / "basic-narrow-list.png"))
        click_basic(open_note)
        expect(note).to_be_visible()
        click_basic(note)
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.type("A note kept across layouts")
        page.screenshot(path=str(artifact_dir / "basic-narrow-detail.png"))
        click_basic(back_to_notes)
        expect(note).to_have_count(0)
        click_basic(open_note)
        click_basic(note)
        note_is_retained()
        click("listDetailPreviewWidth")
        page.keyboard.press("Home")
        page.keyboard.press("Enter")
        expect(open_note).to_be_visible()
        expect(note).to_be_visible()
        expect(back_to_notes).to_have_count(0)
        page.keyboard.press("Tab")
        page.keyboard.press("Tab")
        note_is_retained()
        if basic.bounding_box()["width"] != wide_width:
            raise RuntimeError("Basic preview did not restore its wide geometry")
        page.screenshot(path=str(artifact_dir / "basic-wide-restored.png"))
        expect(control("fileBrowserList")).to_be_visible()
        # Native item views prefer AccessibleTextRole over the visible label.
        # The WASM presentation slots must preserve the same model semantics.
        ax = page.context.new_cdp_session(page)
        page.wait_for_function(
            """() => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id*=".fluentWasmVisibleListCell_"][aria-hidden="false"]')]
                .some(node => node.textContent === 'layout-notes.md, modified 10:42')""",
            timeout=10_000,
        )
        exposed = {node.get("name", {}).get("value")
                   for node in ax.send("Accessibility.getFullAXTree")["nodes"]
                   if not node.get("ignored")}
        if "layout-notes.md, modified 10:42" not in exposed:
            raise RuntimeError("File model AccessibleTextRole is missing from Chromium AX")
        click("fileBrowserEditor")
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.type("Browser draft kept")
        draft_is_retained()
        bounds = control("fileBrowserList").bounding_box()
        top = list_pixels("list-top.png", bounds)
        page.mouse.move(bounds["x"] + 80, bounds["y"] + 100)
        page.mouse.wheel(0, 320)
        scrolled = list_pixels("list-scrolled.png", bounds)
        if top == scrolled:
            raise RuntimeError("Browser wheel input did not scroll the file list")
        page.screenshot(path=str(artifact_dir / "wide.png"))
        click("fileBrowserNarrow")
        expect(control("fileBrowserBack")).to_be_visible()
        expect(control("fileBrowserList")).to_have_count(0)
        draft_is_retained()
        page.screenshot(path=str(artifact_dir / "narrow-detail.png"))
        click("fileBrowserBack")
        expect(control("fileBrowserList")).to_be_visible()
        expect(control("fileBrowserEditor")).to_have_count(0)
        page.screenshot(path=str(artifact_dir / "narrow-list.png"))
        page.keyboard.press("Enter")
        expect(control("fileBrowserBack")).to_be_visible()
        draft_is_retained()
        click("fileBrowserNarrow")
        expect(control("fileBrowserList")).to_be_visible()
        expect(control("fileBrowserEditor")).to_be_visible()
        draft_is_retained()
        # Compare the same observed wide rectangle. A stale AX proxy can still
        # report its compact origin even though the canvas has returned wide.
        if list_pixels("list-restored.png", bounds) != scrolled:
            raise RuntimeError("File list pixels changed after returning to the same wide geometry")
        if errors:
            raise RuntimeError("List/detail browser errors: " + "; ".join(errors))
        (artifact_dir / "result.json").write_text(
            json.dumps({"status": "pass", "checks": ["basic-width-selector", "basic-draft-retained",
                        "wide-edit", "narrow-detail", "back-to-list",
                        "keyboard-reopen", "wide-restore", "draft-retained", "scroll-retained",
                        "model-accessible-name"],
                        "page_errors": errors}, indent=2)
            + "\n", encoding="utf-8")
    except Exception:
        page.screenshot(path=str(artifact_dir / "failure.png"))
        (artifact_dir / "failure-controls.json").write_text(json.dumps(
            page.locator("#qt-shadow-container").evaluate("""host =>
                [...host.shadowRoot.querySelectorAll('[id*="basicListDetailView"]')]
                    .map(node => ({id: node.id, role: node.getAttribute('role'),
                        label: node.getAttribute('aria-label'), hidden: node.getAttribute('aria-hidden'),
                        text: node.textContent, value: node.value}))"""), indent=2))
        raise
    finally:
        page.close()
