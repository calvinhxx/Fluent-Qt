"""Exercise atomic tables and toolbar composition with browser input."""

from __future__ import annotations

import json
import re
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_data_grid_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 1300}, device_scale_factor=2)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    column_order = ["Project", "Owner", "Status", "Priority"]
    accessibility_checks = []
    toolbar_geometry = {}
    facet_geometry = []
    ax = page.context.new_cdp_session(page)

    def control(name: str) -> object:
        return page.locator(f'[id$=".{name}"][aria-hidden="false"]')

    def click(name: str) -> None:
        target = control(name)
        pointer(target)

    def painted_bounds(target: object) -> dict:
        # Qt publishes canvas geometry as inline dimensions. A transparent
        # accessibility input's DOM outer box also includes browser padding and
        # borders, which are not part of the painted Fluent control.
        return target.evaluate("""node => {
            const rect = node.getBoundingClientRect();
            return {x: rect.x, y: rect.y, width: parseFloat(node.style.width),
                    height: parseFloat(node.style.height)};
        }""")

    def pointer(target: object) -> None:
        expect(target).to_be_visible()
        bounds = target.bounding_box()
        if bounds is None:
            raise RuntimeError("Missing browser control bounds")
        page.mouse.click(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2)

    def item(name: str) -> object:
        # Owner/Status also name the facet triggers. Wait for the newly opened
        # column menu rather than clicking a same-named toolbar control.
        return page.get_by_role("menuitem", name=name, exact=True).and_(
            page.locator('[id^="QApplication.dataGridColumnsMenu."]')
        ).last

    def open_column(name: str) -> None:
        click("dataGridColumns")
        check_items(column_order)
        pointer(item(name))
        expect(column_menu()).to_be_visible()
        check_items(["Visible", "Pin to start", "Move earlier", "Move later"])

    def column_menu() -> object:
        return page.get_by_role("menubar").and_(page.locator('[id^="QApplication.dataGridColumnsMenu."]')).last

    def check_items(names: list[str]) -> None:
        for name in names:
            target = item(name)
            expect(target).to_be_visible()
            expect(target).to_have_attribute("aria-hidden", "false")
        # DOM role queries alone do not prove that Chromium exposes the items
        # to assistive technology. Check its actual accessibility tree too.
        nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        exposed = [node.get("name", {}).get("value") for node in nodes
                   if not node.get("ignored") and node.get("role", {}).get("value") == "menuitem"]
        missing = set(names) - set(exposed)
        if missing:
            raise RuntimeError(f"Reopened menu items missing from Chromium AX tree: {sorted(missing)}")
        accessibility_checks.append({"expected": list(names), "exposed_menuitems": exposed})

    def command(name: str) -> None:
        pointer(item(name))

    def close_columns() -> None:
        page.keyboard.press("Escape")
        page.keyboard.press("Escape")

    def column_state(visible: str, pinned: str) -> None:
        expect(control("dataGridColumns")).to_have_attribute(
            "aria-description", f"Visible columns: {visible}. Pinned columns: {pinned}."
        )

    def feedback(text: str) -> None:
        page.wait_for_function(
            """text => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".fluentToastMessage"][aria-hidden="false"]')]
                .some(node => (node.value ?? node.textContent).includes(text))""",
            arg=text, timeout=10_000,
        )

    def summary(text: str) -> None:
        page.wait_for_function(
            """text => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".dataGridSummary"]')]
                .some(node => (node.value ?? node.textContent).includes(text))""",
            arg=text, timeout=10_000,
        )

    try:
        page.goto(f"{base_url}/app/index.html?route=data-grid-tool-bar&window-mode=maximized&wasm-smoke=interaction-state",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")

        grid = control("workspaceDataGrid").filter(has_text=re.compile(r"^Projects$"))
        expect(grid).to_be_visible()
        sort_header = control("workspaceDataGrid").filter(has_text=re.compile(r"^Project$"))
        for name in ("dataGridSearch", "dataGridFilter_status", "dataGridFilter_owner",
                     "dataGridClearFilters", "dataGridColumns"):
            target = control(name)
            expect(target).to_be_visible()
            bounds = painted_bounds(target)
            if bounds["height"] != 32:
                raise RuntimeError(f"Toolbar control does not use the shared height: {name}: {bounds}")
            toolbar_geometry[name] = bounds
        tops = [bounds["y"] for bounds in toolbar_geometry.values()]
        if max(tops) - min(tops) > 1:
            raise RuntimeError(f"Toolbar control edges are not aligned: {toolbar_geometry}")

        # Establish native browser-to-Qt keyboard focus through the existing
        # search workflow before opening a transient cell editor.
        click("dataGridSearch")
        page.keyboard.type("Aurora")
        summary("1 of 8 projects")
        click("dataGridClearFilters")
        summary("8 of 8 projects")

        def edit_priority(value: str, finish: str) -> None:
            header = control("workspaceDataGrid").filter(has_text=re.compile(r"^Priority$"))
            row = control("workspaceDataGrid").filter(has_text=re.compile(r"^Aurora$"))
            column_bounds, row_bounds = header.bounding_box(), row.bounding_box()
            if not column_bounds or not row_bounds:
                raise RuntimeError("Missing numeric cell bounds")
            # Give real press/release events a duration: zero-duration bursts
            # can overtake Qt WASM's focus hand-off after the preceding editor.
            page.mouse.dblclick(column_bounds["x"] + column_bounds["width"] / 2,
                                row_bounds["y"] + row_bounds["height"] / 2, delay=60)
            editor = control("fluentDataGridNumberEditor")
            expect(editor).to_be_visible()
            page.keyboard.press("Control+A")
            page.keyboard.type(value)
            page.screenshot(path=str(artifact_dir / f"number-editor-{finish}.png"))
            if finish == "focus-out":
                click("dataGridSearch")
            else:
                page.keyboard.press(finish)
            expect(editor).to_have_count(0)

        def changed_priority(expected: str) -> None:
            expect(control("workspaceDataGrid").filter(has_text=re.compile(rf"^{expected}$"))).to_be_visible()
            nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
            exposed = [node.get("name", {}).get("value") for node in nodes if not node.get("ignored")]
            if expected not in exposed:
                raise RuntimeError(f"Edited Priority is missing from Chromium's AX tree: {expected}")

        edit_priority("10", "Tab")
        changed_priority("10")
        edit_priority("55", "Escape")
        changed_priority("10")
        edit_priority("7", "focus-out")
        changed_priority("7")
        edit_priority("1", "Enter")

        def first_project_before(first: str, last: str) -> None:
            page.wait_for_function(
                """([first, last]) => {
                    const nodes = [...document.querySelector('#qt-shadow-container').shadowRoot
                        .querySelectorAll('[aria-hidden="false"]')];
                    const a = nodes.find(n => n.textContent.trim() === first);
                    const b = nodes.find(n => n.textContent.trim() === last);
                    return a && b && a.getBoundingClientRect().top < b.getBoundingClientRect().top;
                }""", arg=[first, last], timeout=5000,
            )

        first_project_before("Beacon", "Fjord")
        pointer(sort_header)
        first_project_before("Fjord", "Beacon")
        page.screenshot(path=str(artifact_dir / "header-sorted-descending.png"))
        pointer(sort_header)
        first_project_before("Beacon", "Fjord")
        for key in ("status", "owner"):
            click("dataGridFilter_" + key)
            expect(control("MultiSelectComboBox.Search")).to_be_visible()
            anchor = painted_bounds(control("dataGridFilter_" + key))
            search = painted_bounds(control("MultiSelectComboBox.Search"))
            # The search field is inset 4 logical pixels from the visible card;
            # the popup's outer shadow is deliberately excluded from this gap.
            card_left, card_top = search["x"] - 4, search["y"] - 4
            gap = card_top - anchor["y"] - anchor["height"]
            if abs(card_left - anchor["x"]) > 1 or abs(gap - 8) > 1:
                raise RuntimeError(f"Filter card is not leading-aligned with an 8 px gap: {anchor}, {search}")
            facet_geometry.append({"filter": key, "anchor": anchor, "search": search, "visible_gap": gap})
            page.screenshot(path=str(artifact_dir / f"{key}-popup-spacing.png"))
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Space")
            page.keyboard.press("Escape")
        summary("1 of 8 projects")
        page.screenshot(path=str(artifact_dir / "filtered.png"))
        click("dataGridSaveView")
        feedback("View saved for this demo")
        summary("1 of 8 projects")
        click("dataGridClearFilters")
        summary("8 of 8 projects")
        click("dataGridRestoreView")
        summary("1 of 8 projects")
        click("dataGridColumns")
        check_items(column_order)
        page.keyboard.press("ArrowDown")
        page.keyboard.press("ArrowRight")
        check_items(["Visible", "Pin to start", "Move earlier", "Move later"])
        page.screenshot(path=str(artifact_dir / "columns.png"))
        # Every reopening discovers current action proxies. Never reuse a
        # cached row position when a menu item is missing from the AX tree.
        command("Pin to start")
        column_state("Project, Owner, Status, Priority", "Project")
        open_column("Status")
        command("Visible")
        column_state("Project, Owner, Priority", "Project")
        open_column("Priority")
        command("Move earlier")
        column_order[:] = ["Project", "Owner", "Priority", "Status"]
        open_column("Priority")
        command("Move earlier")
        column_order[:] = ["Project", "Priority", "Owner", "Status"]
        column_state("Project, Priority, Owner", "Project")
        click("dataGridSaveView")
        open_column("Status")
        command("Visible")
        open_column("Project")
        command("Pin to start")
        column_state("Project, Priority, Owner, Status", "None")
        click("dataGridRestoreView")
        column_state("Project, Priority, Owner", "Project")
        open_column("Status")
        page.screenshot(path=str(artifact_dir / "restored-columns.png"))
        close_columns()
        expect(item("Visible")).to_have_count(0)
        expect(item("Project")).to_have_count(0)
        # Escape and outside-click closures must also republish both levels.
        for outside in (False, True):
            open_column("Project")
            if outside:
                page.mouse.click(1300, 1200)
            else:
                close_columns()
            expect(item("Visible")).to_have_count(0)
            expect(item("Project")).to_have_count(0)
        # The table route stays independent of the toolbar workspace. Its first
        # sample has a horizontal range and a fixed leading column; both panes
        # must contain vertical wheel input instead of moving the outer page.
        page.goto(f"{base_url}/app/index.html?route=data-grid&window-mode=maximized&wasm-smoke=interaction-state",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        page.locator("#qt-shadow-container .qt-window-a11y-container:visible "
                     "> button.hidden-visually-read-by-screen-reader").first.evaluate("button => button.click()")

        expect(control("dataGridSearch")).to_have_count(0)
        surface = control("gallerySampleCardPreview").first
        expect(surface).to_be_visible()
        bounds = surface.bounding_box()
        if bounds is None:
            raise RuntimeError("Missing atomic DataGrid preview bounds")
        outer = page.locator(
            '[role="scrollbar"][aria-hidden="false"][id*=".galleryContentScrollArea."]'
            '[id$=".fluentScrollViewFloatingVerticalBar"]'
        )
        before = float(outer.get_attribute("aria-valuenow") or 0)
        table = control("gallerySamplePreviewWidget").filter(has_text=re.compile(r"^Pinned projects$"))
        expect(table).to_be_visible()
        table_bounds = table.bounding_box()
        if table_bounds is None:
            raise RuntimeError("Missing atomic DataGrid table bounds")
        table_left = table_bounds["x"]
        if abs(table_left - bounds["x"] - 20) > 1:
            raise RuntimeError("The capped DataGrid preview must stay left aligned")
        for offset in (70, 400):
            page.mouse.move(table_left + offset, bounds["y"] + 75)
            page.mouse.wheel(0, 350)
            page.wait_for_timeout(400)
            after = float(outer.get_attribute("aria-valuenow") or 0)
            if after != before:
                raise RuntimeError(f"DataGrid wheel escaped its pane: outer {before} -> {after}")
        page.screenshot(path=str(artifact_dir / "atomic-wheel-contained.png"))
        # Qt's WASM table proxy exposes cells, not the overlay scrollbar. Native
        # geometry tests cover its range; capture the real bar through pointer input.
        page.mouse.move(table_left + 450, table_bounds["y"] + table_bounds["height"] - 4)
        page.screenshot(path=str(artifact_dir / "atomic-scroll-track.png"))
        page.mouse.click(table_left + 290, table_bounds["y"] + 54)
        page.keyboard.press("F2")
        editor = control("fluent::textfields::LineEdit")
        expect(editor).to_be_visible()
        expect(editor).to_have_text("Maya")
        page.keyboard.press("End")
        page.keyboard.insert_text(" edited")
        expect(editor).to_have_text("Maya edited")
        expect(control("fluentLineEditClearButton")).to_have_count(0)
        page.screenshot(path=str(artifact_dir / "atomic-inset-editor.png"))
        page.keyboard.press("Escape")
        expect(editor).to_have_count(0)
        # Start in the pinned Project column, then cross into Owner and back.
        page.mouse.click(table_left + 90, table_bounds["y"] + 54)
        page.keyboard.press("F2")
        expect(editor).to_be_visible()
        expect(editor).to_have_text("Aurora")
        page.keyboard.press("Tab")
        expect(editor).to_be_visible()
        expect(editor).to_have_text("Maya")
        page.keyboard.press("Shift+Tab")
        expect(editor).to_be_visible()
        expect(editor).to_have_text("Aurora")
        page.keyboard.press("Escape")
        expect(editor).to_have_count(0)
        # A positive control proves this page can scroll and that the test did
        # not pass because the outer view had no range or a cover blocked input.
        page.mouse.move(1320, 1000)
        page.mouse.wheel(0, 350)
        page.wait_for_function(
            "([bar, before]) => Number(bar.getAttribute('aria-valuenow')) > before",
            arg=[outer.element_handle(), before], timeout=5000,
        )
        if errors:
            raise RuntimeError("DataGrid browser errors: " + "; ".join(errors))
        (artifact_dir / "result.json").write_text(json.dumps({
            "status": "pass", "checks": ["header-click-sort-both-directions", "search", "combined-facets", "clear", "save-restore", "column-menu", "pin-column", "hide-column", "move-column", "restore-columns", "reopened-menu-accessibility", "atomic-table-route", "wheel-containment-main-and-pinned", "leading-alignment", "fluent-editor-keyboard-cancel"],
            "accessibility_openings": accessibility_checks,
            "toolbar_geometry": toolbar_geometry,
            "facet_geometry": facet_geometry,
            "number_editor": ["NumberBox", "Tab commits", "Escape cancels", "focus-out commits", "Enter commits", "edited value updates Chromium AX tree"],
            "pinned_keyboard": ["Tab reaches Owner", "Shift+Tab returns to Project"],
            "page_errors": errors,
        }, indent=2) + "\n", encoding="utf-8")
    except Exception as error:
        (artifact_dir / "result.json").write_text(json.dumps({
            "status": "fail", "error": str(error), "page_errors": errors,
        }, indent=2) + "\n", encoding="utf-8")
        page.screenshot(path=str(artifact_dir / "failure.png"))
        snapshot = page.locator("#qt-shadow-container [aria-hidden='false'], #qt-shadow-container [role='menuitem']")
        (artifact_dir / "failure-dom.json").write_text(json.dumps(snapshot.evaluate_all(
            """nodes => nodes.map(n => ({
                id: n.id, role: n.getAttribute('role'),
                text: n.textContent, value: n.value,
                rect: n.getBoundingClientRect().toJSON(),
                description: n.getAttribute('aria-description')
            }))"""
        ), indent=2), encoding="utf-8")
        raise
    finally:
        page.close()
