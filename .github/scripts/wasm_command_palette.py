"""Exercise shared commands and overlay modality/backdrop controls in the browser Gallery."""

from __future__ import annotations

import json
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready
from wasm_drawer_lifecycle import run_drawer_lifecycle_contract


def run_command_palette_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect, TimeoutError as PlaywrightTimeoutError

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 900}, device_scale_factor=2)
    errors = []
    current_route = "command-palette"
    drawer_lifecycle = None
    page.on("pageerror", lambda error: errors.append(str(error)))

    # Native transitions and their accessibility proxies settle on separate
    # frames. Observe completion even on slow software-rendered runners.
    def expect_visible(target: object) -> None:
        expect(target).to_be_visible(timeout=30_000)

    def expect_absent(target: object) -> None:
        expect(target).to_have_count(0, timeout=30_000)

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
        expect_visible(button)
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
        wait_for_gallery_ready(page)
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")

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
        expect_visible(search)
        page.screenshot(path=str(artifact_dir / "default-modal-blur.png"))
        page.keyboard.type("inbox")
        page.keyboard.press("Enter")
        text_is("Current page: Inbox")
        expect_absent(search)
        page.set_viewport_size({"width": 1440, "height": 2800})
        open_route("command-palette")
        click(open_button)
        expect_visible(search)
        page.wait_for_timeout(300)
        page.screenshot(path=str(artifact_dir / "blurred-open.png"))
        page.keyboard.type("clear")
        page.keyboard.press("Enter")
        # An empty notebook has nothing to clear, so the palette stays open.
        expect_visible(search)
        page.keyboard.press("Escape")
        expect_absent(search)
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
        expect_absent(search)
        page.screenshot(path=str(artifact_dir / "executed.png"))
        click(open_button)
        page.keyboard.type("clear")
        page.keyboard.press("Enter")
        text_is("0 notes")
        expect_absent(search)
        # Toggle the same checked action from the toolbar and restore the
        # sample's initial geometry before using cached Qt AX child bounds.
        click(page.get_by_role("button", name="Show details", exact=True))
        expect_absent(page.locator('[id$=".Gallery.CommandPalette.Status"][aria-hidden="false"]'))
        for destination in ("Inbox", "Home"):
            click(open_button)
            page.keyboard.type(destination.lower())
            page.keyboard.press("Enter")
            text_is(destination)
            expect_absent(search)
        # Both samples fit in this viewport; keep the real page stationary so
        # Qt's cached AX child coordinates remain valid throughout focus changes.
        edit_button = page.get_by_role("button", name="Search editing commands", exact=True)
        click(edit_button)
        expect_visible(search)
        page.keyboard.type("cut")
        page.keyboard.press("Enter")
        text_is(" the release notes")
        click(page.get_by_role("button", name="Reset text", exact=True))
        click(edit_button)
        expect_visible(search)
        page.screenshot(path=str(artifact_dir / "editing.png"))
        page.keyboard.press("Escape")
        expect_absent(search)
        # Programmatic setText does not immediately update Qt's HTML input-
        # method value. A second physical edit proves both Reset and Escape:
        # cutting the first word must again leave the original suffix.
        click(edit_button)
        expect_visible(search)
        page.keyboard.type("cut")
        page.keyboard.press("Enter")
        text_is(" the release notes")
        click(page.get_by_role("button", name="Reset text", exact=True))
        # A modal outside press dismisses without activating the sidebar.
        # The runtime now exposes visible navigation rows as named tree items.
        sidebar = page.get_by_role("treeitem", name="Settings", exact=True)
        click(open_button)
        expect_visible(search)
        click(sidebar)
        expect_absent(search)
        expect_visible(open_button)
        page.screenshot(path=str(artifact_dir / "modal-dismissal.png"))
        click(sidebar)
        expect_absent(open_button)

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
            expect_visible(effects)
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
                        text_is(expected, timeout=5_000)
                        return
                    except PlaywrightTimeoutError as error:
                        if attempt == 4:
                            raise RuntimeError(
                                f"{route} {suffix} did not reach {expected!r} "
                                "after physical click and End"
                            ) from error

            adjust_slider("Blur", "Blur: 24 px")
            adjust_slider("Dim", "Dim: 60%")
            background = page.get_by_role("button", name="Background action", exact=True)
            click(background)
            text_is("Background clicks: 0")
            expect_visible(effects)
            page.screenshot(path=str(artifact_dir / f"{route}-effects.png"))
            click(control("DimEnabled"))
            click(background)
            text_is("Background clicks: 0")
            expect_visible(effects)
            click(control("Modal"))
            click(background)
            text_is("Background clicks: 1")
            expect_visible(effects)
            click(control("Modal"))
            click(sidebar)
            expect_visible(effects)
            expect_visible(opener_button)
            click(control("CloseOutside"))
            click(sidebar)
            expect_absent(effects)
            expect_visible(opener_button)
            click(opener_button)
            expect_visible(effects)
            page.keyboard.press("Escape")
            expect_absent(effects)
            if route == "drawer-view":
                drawer_lifecycle = run_drawer_lifecycle_contract(
                    page, opener_button, click, artifact_dir / "drawer-lifecycle"
                )
            click(sidebar)
            expect_absent(opener_button)
        if errors:
            raise RuntimeError(f"Command palette browser errors: {errors}")
        (artifact_dir / "result.json").write_text(json.dumps({
            "routes": ["command-palette", "popup", "dialog", "drawer-view"], "status": "pass", "input": "browser mouse and keyboard",
            "drawer_lifecycle": drawer_lifecycle,
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
