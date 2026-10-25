"""Exercise Field/FormController through real browser keyboard and pointer input."""

from __future__ import annotations

import json
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_form_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 1600}, device_scale_factor=2)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))

    def control(name: str) -> object:
        return page.locator(f'[id$=".{name}"][aria-hidden="false"]')

    def click(locator: object) -> None:
        expect(locator).to_be_visible()
        bounds = locator.bounding_box()
        if bounds is None:
            raise RuntimeError("Form control has no browser bounds")
        page.mouse.click(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2)

    def enter(name: str, text: str) -> None:
        click(control(name))
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.type(text)

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
                .querySelectorAll('[id$=".connectionStatus"]')]
                .some(node => (node.value ?? node.textContent).includes(text))""",
            arg=text,
            timeout=10_000,
        )

    try:
        page.goto(f"{base_url}/app/index.html?route=field&window-mode=maximized&wasm-smoke=form",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")

        enter("connectionInput_token", "demo-token")
        enter("connectionInput_address", "http://invalid.example")
        click(page.get_by_role("button", name="Save connection", exact=True))
        status("Enter a valid HTTPS address.")
        page.screenshot(path=str(artifact_dir / "invalid.png"))
        enter("connectionInput_address", "https://offline.example")
        click(page.get_by_role("button", name="Test connection", exact=True))
        status("Server unavailable.")
        enter("connectionInput_address", "https://api.example.com/v1")
        click(page.get_by_role("button", name="Save connection", exact=True))
        feedback("Connection saved for this demo")
        status("Connection verified.")
        page.screenshot(path=str(artifact_dir / "saved.png"))
        if errors:
            raise RuntimeError("Form browser errors: " + "; ".join(errors))
        (artifact_dir / "result.json").write_text(
            json.dumps({"status": "pass", "checks": ["invalid-input", "async-failure", "save"],
                        "page_errors": errors}, indent=2) + "\n", encoding="utf-8")
    except Exception:
        page.screenshot(path=str(artifact_dir / "failure.png"))
        raise
    finally:
        page.close()
