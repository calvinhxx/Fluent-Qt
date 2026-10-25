"""Exercise shared CommandBar/FluentMenu cascades through browser input."""

from __future__ import annotations

import json
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_command_submenus_contract(
    browser: object,
    base_url: str,
    artifact_dir: Path,
) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    results = []
    cases = (
        ("command-bar", "primary"),
        ("command-bar", "overflow"),
        ("command-bar-flyout", "primary"),
        ("command-bar-flyout", "secondary"),
    )
    for route, location in cases:
        case = f"{route}-{location}"
        page = browser.new_page(
            viewport={"width": 1440, "height": 1000}, device_scale_factor=2
        )
        errors = []
        page.on("pageerror", lambda error: errors.append(str(error)))
        cached_page_offset = 0.0

        def pointer(locator: object, *, click: bool = False, page_control: bool = False) -> None:
            expect(locator).to_be_visible()
            box = locator.bounding_box()
            if box is None:
                raise RuntimeError(f"{route}: missing command/menu bounds")
            x, y = box["x"] + box["width"] / 2, box["y"] + box["height"] / 2
            if page_control:
                y -= cached_page_offset
            page.mouse.move(x, y, steps=12)
            if click:
                page.mouse.click(x, y)

        def item(name: str) -> object:
            # An expanded command surface can also contain "Sort by". The
            # newly opened native menu is the last visible popup in Qt's tree.
            return page.get_by_role("menuitem", name=name, exact=True).last

        def expect_order(value: str) -> None:
            page.wait_for_function(
                """expected => [...document.querySelector('#qt-shadow-container')
                    .shadowRoot.querySelectorAll('[role="textbox"], textarea, input')]
                    .some(element => (element.value ?? element.textContent) === expected)""",
                arg=f"Order: {value}",
            )

        try:
            page.goto(
                f"{base_url}/app/index.html?route={route}"
                "&window-mode=maximized&wasm-smoke=command-submenus",
                wait_until="domcontentloaded",
            )
            page.wait_for_function(
                "document.documentElement.dataset.fluentQtLoaded === 'true'",
                timeout=120_000,
            )
            wait_for_gallery_ready(page)
            # Discover labels through Qt's accessibility tree. Activation is
            # setup; every command/menu operation below uses browser input.
            activation = page.locator(
                "#qt-shadow-container .qt-window-a11y-container:visible "
                "> button.hidden-visually-read-by-screen-reader"
            ).first
            activation.wait_for(state="attached")
            activation.evaluate("button => button.click()")
            entry = (page.get_by_role("button", name="Open view commands", exact=True)
                     if route == "command-bar-flyout" else item("View"))
            expect(entry).to_be_visible()
            before = entry.bounding_box()
            scrollbar = page.locator(
                '[role="scrollbar"][aria-hidden="false"][id*=".galleryContentScrollArea."]'
                '[id$=".fluentScrollViewFloatingVerticalBar"]'
            )
            scroll_before = float(scrollbar.get_attribute("aria-valuenow") or 0)
            # The ordinary command sample is first; cascades have their own
            # final card. Scroll the real Qt page instead of moving AX proxies.
            page.mouse.move(1300, 750)
            page.mouse.wheel(0, 10000)
            # Wait for real movement and its animated position to settle, rather
            # than continuing with an offscreen target when no wheel was handled.
            page.wait_for_function(
                """([bar, initial]) => new Promise(resolve => {
                    let previous = initial;
                    let unchanged = 0;
                    const observe = () => {
                        const current = Number(bar.getAttribute('aria-valuenow'));
                        unchanged = current > initial && current === previous
                            ? unchanged + 1 : 0;
                        previous = current;
                        if (unchanged >= 2) resolve(current);
                        else requestAnimationFrame(observe);
                    };
                    requestAnimationFrame(observe);
                })""",
                arg=[scrollbar.element_handle(), scroll_before],
                timeout=5_000,
            )
            after = entry.bounding_box()
            if before is None or after is None:
                raise RuntimeError(f"{route}: missing sample entry geometry")
            scroll_delta = float(scrollbar.get_attribute("aria-valuenow") or 0) - scroll_before
            # Qt WASM can retain child AX coordinates when an ancestor scrolls.
            # Account only for the offset not already reflected in the proxy;
            # read the real scrollbar without moving the canvas or AX elements.
            cached_page_offset = scroll_delta - (before["y"] - after["y"])
            if route == "command-bar-flyout":
                pointer(entry, click=True, page_control=True)
                page.wait_for_timeout(350)
            elif location == "overflow":
                expect(item("View")).to_be_visible()
                leading = item("View").bounding_box()
                candidates = [
                    button.bounding_box()
                    for button in page.get_by_role(
                        "button", name="More commands", exact=True
                    ).all()
                ]
                candidates = [box for box in candidates if box is not None]
                if leading is None or not candidates:
                    raise RuntimeError("CommandBar has no initial command geometry")
                more = min(candidates, key=lambda box: abs(box["y"] - leading["y"]))
                pointer(page.get_by_role("checkbox").filter(has_text="Compact width"),
                        click=True, page_control=True)
                expect(item("View")).to_have_count(0)
                page.wait_for_timeout(350)
                page.screenshot(path=str(artifact_dir / f"{case}-compact.png"))
                # Qt WASM caches the proxy's old x after child relayout. More is
                # now the leading command: use that observed leading position,
                # preserving the real canvas and accessibility DOM unchanged.
                x = leading["x"] + more["width"] / 2
                y = leading["y"] - cached_page_offset + more["height"] / 2
                page.mouse.move(x, y, steps=12)
                page.mouse.click(x, y)
                page.wait_for_timeout(350)
            if location == "secondary":
                pointer(item("Sort by"), click=True)
            else:
                pointer(item("View"), click=True,
                        page_control=route == "command-bar" and location == "primary")
                pointer(item("Sort by"))
            pointer(item("Order"))
            expect(item("Descending")).to_be_visible()
            # Outlast Qt's delayed submenu-close path after crossing both parents.
            page.wait_for_timeout(700)
            expect(item("Descending")).to_be_visible()
            aligned = ["Order", "Ascending"]
            if location != "secondary":
                aligned.insert(0, "Sort by")
            bounds = [item(name).bounding_box() for name in aligned]
            if any(box is None for box in bounds):
                raise RuntimeError(f"{case}: missing cascade row geometry")
            centers = [box["y"] + box["height"] / 2 for box in bounds]
            if max(centers) - min(centers) > 2:
                raise RuntimeError(f"{case}: cascade rows do not align: {centers}")
            page.screenshot(path=str(artifact_dir / f"{case}-cascade.png"))
            page.keyboard.press("Escape")
            expect(item("Descending")).to_have_count(0)
            expect(item("Order")).to_be_visible()
            page.keyboard.press("ArrowDown")
            page.keyboard.press("ArrowRight")
            # The same submenu now has a new browser window. Its cached action
            # proxies must be exposed again, not just its painted surface.
            expect(item("Ascending")).to_be_visible()
            expect(item("Descending")).to_be_visible()
            expect(item("Descending")).to_have_attribute("aria-hidden", "false")
            nodes = page.context.new_cdp_session(page).send("Accessibility.getFullAXTree")["nodes"]
            names = {node.get("name", {}).get("value") for node in nodes
                     if not node.get("ignored") and node.get("role", {}).get("value") == "menuitem"}
            if not {"Ascending", "Descending"}.issubset(names):
                raise RuntimeError(f"{case}: reopened submenu absent from Chromium AX tree")
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Enter")
            # Prove execution through the application's observable result;
            # merely reopening a popup does not establish successful input.
            expect_order("Descending")
            if route == "command-bar-flyout":
                expect(item("View")).to_have_count(0)
            if errors:
                raise RuntimeError(f"{route}: browser errors: {errors}")
            results.append({
                "route": route,
                "location": location,
                "status": "pass",
                "input": "browser mouse and keyboard",
                "page_scroll_delta": scroll_delta,
                "cascade_row_centers": centers,
            })
        except Exception:
            page.screenshot(path=str(artifact_dir / f"{case}-failure.png"))
            (artifact_dir / f"{case}-failure.txt").write_text(page.locator("body").aria_snapshot())
            raise
        finally:
            page.close()
    (artifact_dir / "result.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Command submenu browser contract passed: three levels, hover, Escape, "
          "leaf selection, overflow and flyout")
