"""Exercise shared commands and overlay modality/backdrop controls in the browser Gallery."""

from __future__ import annotations

import json
from pathlib import Path


def run_command_palette_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect, TimeoutError as PlaywrightTimeoutError

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 900}, device_scale_factor=2)
    errors = []
    current_route = "command-palette"
    page.on("pageerror", lambda error: errors.append(str(error)))

    def text_is(value: str, timeout: int = 30_000) -> None:
        # Qt exposes focused editor text through its input-method element.
        # The separate accessibility input can remain empty, so inspect both
        # document and Qt shadow roots without writing to either input.
        page.wait_for_function(
            """expected => [document, document.querySelector('#qt-shadow-container').shadowRoot]
                .some(root => [...root.querySelectorAll('[role="textbox"], textarea, input')]
                    .some(element => (element.value ?? element.textContent) === expected))""",
            arg=value,
            timeout=timeout,
        )

    def click(button: object) -> None:
        expect(button).to_be_visible()
        bounds = button.evaluate("""element => {
            const rect = element.getBoundingClientRect();
            const dialog = [...element.getRootNode().querySelectorAll('dialog[id]')]
                .find(owner => element.id.startsWith(owner.id + '.'));
            // Qt 6.9.3 emits a closed HTML dialog proxy for a visible child
            // QDialog. Its sibling control proxies use dialog-local geometry.
            // Read that origin without changing proxies or application state.
            let dx = dialog && !dialog.open ? parseFloat(dialog.style.left) || 0 : 0;
            const dy = dialog && !dialog.open ? parseFloat(dialog.style.top) || 0 : 0;
            const drawer = element.getRootNode().querySelector('[id$=".Gallery.Drawer.Effects"]');
            if (drawer && element.id.startsWith(drawer.id + '.')) {
                // Qt only refreshes proxy geometry on ObjectShow, so an
                // animated drawer retains its closed origin. The sample
                // declares a right edge: locate its settled panel from the
                // host width and reported drawer width, keeping real input.
                const host = element.closest('.qt-window-a11y-container');
                const origin = parseFloat(drawer.style.left) || 0;
                const settled = host.clientWidth - parseFloat(drawer.style.width);
                dx += Math.min(origin, settled) - origin;
            }
            return {x: rect.x + dx, y: rect.y + dy, width: rect.width, height: rect.height};
        }""")
        if bounds is None:
            raise RuntimeError("Command palette sample has no button geometry")
        y = bounds["y"] + bounds["height"] / 2
        page.mouse.click(bounds["x"] + bounds["width"] / 2, y)

    def open_route(route: str) -> None:
        nonlocal current_route
        current_route = route
        page.goto(
            f"{base_url}/app/index.html?route={route}&window-mode=maximized"
            "&wasm-smoke=command-palette",
            wait_until="domcontentloaded",
        )
        page.wait_for_function(
            "document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000
        )
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")
        expect(page.locator('[id$=".gallerySplashScreen"][aria-hidden="false"]')).to_have_count(0, timeout=15_000)

    try:
        open_route("command-palette")
        open_button = page.get_by_role("button", name="Open command palette", exact=True)
        # Qt WASM puts QLineEdit's accessible name in HTML `name`, which is not
        # a browser accessible label. Use its stable native object identity;
        # input still goes through real browser keyboard events below.
        search = page.locator('input[id$=".FluentCommandPalette.Search"][aria-hidden="false"]')
        # The introductory palette has no explicit backdrop configuration.
        # Exercise its default modal appearance before the configurable sample.
        click(page.get_by_role("button", name="Search pages", exact=True))
        expect(search).to_be_visible()
        page.screenshot(path=str(artifact_dir / "default-modal-blur.png"))
        page.keyboard.type("inbox")
        page.keyboard.press("Enter")
        text_is("Current page: Inbox")
        expect(search).to_have_count(0)
        page.set_viewport_size({"width": 1440, "height": 2800})
        open_route("command-palette")
        click(open_button)
        expect(search).to_be_visible()
        page.wait_for_timeout(300)
        page.screenshot(path=str(artifact_dir / "blurred-open.png"))
        page.keyboard.type("clear")
        page.keyboard.press("Enter")
        # An empty notebook has nothing to clear, so the palette stays open.
        expect(search).to_be_visible()
        page.keyboard.press("Escape")
        expect(search).to_have_count(0)
        click(open_button)
        page.keyboard.type("properties")
        page.keyboard.press("Enter")
        text_is("0 notes")
        click(open_button)
        page.keyboard.type("new")
        page.wait_for_timeout(300)
        page.screenshot(path=str(artifact_dir / "search.png"))
        page.keyboard.press("Enter")
        text_is("1 note")
        expect(search).to_have_count(0)
        page.screenshot(path=str(artifact_dir / "executed.png"))
        click(open_button)
        page.keyboard.type("clear")
        page.keyboard.press("Enter")
        text_is("0 notes")
        expect(search).to_have_count(0)
        # Toggle the same checked action from the toolbar and restore the
        # sample's initial geometry before using cached Qt AX child bounds.
        click(page.get_by_role("button", name="Show details", exact=True))
        expect(page.locator('[id$=".Gallery.CommandPalette.Status"][aria-hidden="false"]')).to_have_count(0)
        for destination in ("Inbox", "Home"):
            click(open_button)
            page.keyboard.type(destination.lower())
            page.keyboard.press("Enter")
            text_is(destination)
            expect(search).to_have_count(0)
        # Both samples fit in this viewport; keep the real page stationary so
        # Qt's cached AX child coordinates remain valid throughout focus changes.
        edit_button = page.get_by_role("button", name="Search editing commands", exact=True)
        click(edit_button)
        expect(search).to_be_visible()
        page.keyboard.type("cut")
        page.keyboard.press("Enter")
        text_is(" the release notes")
        click(page.get_by_role("button", name="Reset text", exact=True))
        click(edit_button)
        expect(search).to_be_visible()
        page.screenshot(path=str(artifact_dir / "editing.png"))
        page.keyboard.press("Escape")
        expect(search).to_have_count(0)
        # Programmatic setText does not immediately update Qt's HTML input-
        # method value. A second physical edit proves both Reset and Escape:
        # cutting the first word must again leave the original suffix.
        click(edit_button)
        expect(search).to_be_visible()
        page.keyboard.type("cut")
        page.keyboard.press("Enter")
        text_is(" the release notes")
        click(page.get_by_role("button", name="Reset text", exact=True))
        # A modal outside press dismisses without activating the sidebar.
        sidebar = page.locator(
            '[id$=".galleryFooterNavigationTreeView"][aria-hidden="false"]'
        )
        click(open_button)
        expect(search).to_be_visible()
        click(sidebar)
        expect(search).to_have_count(0)
        expect(open_button).to_be_visible()
        page.screenshot(path=str(artifact_dir / "modal-dismissal.png"))
        click(sidebar)
        expect(open_button).to_have_count(0)

        # Keep counters outside centered cards and the right-hand drawer so
        # modal and modeless states exercise the same physical pointer target.
        page.set_viewport_size({"width": 1800, "height": 2800})
        for route, prefix, opener in (
            ("popup", "Gallery.Popup", "Preview background effects"),
            ("dialog", "Gallery.Dialog", "Preview background effects"),
            ("drawer-view", "Gallery.Drawer", "Preview background effects"),
        ):
            open_route(route)
            def control(suffix: str) -> object:
                return page.locator(f'[id$=".{prefix}.{suffix}"][aria-hidden="false"]')

            opener_button = page.get_by_role("button", name=opener, exact=True)
            click(opener_button)
            # Observe an actual interactive control; Qt's synthetic HTML
            # <dialog> container is closed even while the canvas card is open.
            effects = control("Modal")
            expect(effects).to_be_visible()
            if route == "drawer-view":
                page.wait_for_timeout(350)
            def adjust_slider(suffix: str, expected: str) -> None:
                # Drawer proxies retain the closed position during animation.
                # Wait for real input to reach the settled control instead of
                # assuming that one fixed delay covers slow browser frames.
                for attempt in range(5):
                    click(control(suffix))
                    page.keyboard.press("End")
                    try:
                        text_is(expected, timeout=1_000)
                        return
                    except PlaywrightTimeoutError:
                        if attempt == 4:
                            raise

            adjust_slider("Blur", "Blur: 24 px")
            adjust_slider("Dim", "Dim: 60%")
            background = page.get_by_role("button", name="Background action", exact=True)
            click(background)
            text_is("Background clicks: 0")
            expect(effects).to_be_visible()
            page.screenshot(path=str(artifact_dir / f"{route}-effects.png"))
            click(control("DimEnabled"))
            click(background)
            text_is("Background clicks: 0")
            expect(effects).to_be_visible()
            click(control("Modal"))
            click(background)
            text_is("Background clicks: 1")
            expect(effects).to_be_visible()
            click(control("Modal"))
            click(sidebar)
            expect(effects).to_be_visible()
            expect(opener_button).to_be_visible()
            click(control("CloseOutside"))
            click(sidebar)
            expect(effects).to_have_count(0)
            expect(opener_button).to_be_visible()
            click(opener_button)
            expect(effects).to_be_visible()
            page.keyboard.press("Escape")
            expect(effects).to_have_count(0)
            click(sidebar)
            expect(opener_button).to_have_count(0)
        if errors:
            raise RuntimeError(f"Command palette browser errors: {errors}")
        (artifact_dir / "result.json").write_text(json.dumps({
            "routes": ["command-palette", "popup", "dialog", "drawer-view"], "status": "pass", "input": "browser mouse and keyboard",
            "verified": ["keyword search", "shared checked state", "disabled command", "Escape", "note insertion and clearing", "editing target", "reset text", "workspace navigation", "blurred modal capture", "modal sidebar dismissal", "shared live blur and dim", "modal independent of dim", "sticky modal background blocking", "modeless background input"],
        }, indent=2) + "\n")
        print("Command palette and three overlay browser contracts passed: commands, modal navigation and backdrop controls")
    except Exception as error:
        (artifact_dir / "result.json").write_text(json.dumps({
            "route": current_route, "status": "fail", "error": str(error),
        }, indent=2) + "\n")
        page.screenshot(path=str(artifact_dir / "failure.png"))
        (artifact_dir / "failure.txt").write_text(page.locator("body").aria_snapshot())
        (artifact_dir / "failure-dom.html").write_text(page.evaluate(
            "document.querySelector('#qt-shadow-container').shadowRoot.innerHTML"
        ))
        raise
    finally:
        page.close()
