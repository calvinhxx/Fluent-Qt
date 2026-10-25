"""Verify hierarchical team selection with browser keyboard and Chinese composition."""

from __future__ import annotations

import json
import re
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_tree_select_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    result_path = artifact_dir / "result.json"
    result_path.unlink(missing_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 1600}, device_scale_factor=2)
    errors = []
    checks = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    ax = page.context.new_cdp_session(page)

    def control(name: str) -> object:
        return page.locator(f'[id$=".{name}"][aria-hidden="false"]')

    def click(name: str) -> None:
        target = control(name)
        expect(target).to_be_visible()
        bounds = target.bounding_box()
        if bounds is None:
            raise RuntimeError(f"Missing bounds for {name}")
        x = bounds["x"] + (bounds["width"] - 60 if name.startswith("project") else bounds["width"] / 2)
        page.mouse.click(x, bounds["y"] + bounds["height"] - 18)

    def summary(text: str) -> None:
        page.wait_for_function(
            """text => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll('[id$=".teamSelectionStatus"]')]
                .some(node => (node.value ?? node.textContent).includes(text))""",
            arg=text, timeout=10_000,
        )

    def tags() -> object:
        return page.get_by_role("button", name=re.compile(r"^Remove "))

    def text_value(name: str, value: str) -> None:
        page.wait_for_function(
            """({name,value}) => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll(`[id$=".${name}"][aria-hidden="false"]`)]
                .some(n => n.value === value || n.textContent === value)""",
            arg={"name": name, "value": value}, timeout=10_000,
        )

    try:
        page.goto(f"{base_url}/app/index.html?route=tree-select&window-mode=maximized&wasm-smoke=interaction-state", wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        page.locator("#qt-shadow-container .qt-window-a11y-container:visible "
                     "> button.hidden-visually-read-by-screen-reader").first.evaluate("button => button.click()")

        summary("Owner: Product engineering / Client apps")
        expect(tags()).to_have_count(1)
        click("projectOwningTeam")
        expect(page.get_by_role("tree", name="Owning team options", exact=True)).to_be_visible()
        ax.send("Input.imeSetComposition", {"text": "sheji", "selectionStart": 5, "selectionEnd": 5})
        page.wait_for_timeout(100)
        summary("Owner: Product engineering / Client apps")
        page.screenshot(path=str(artifact_dir / "preedit.png"))
        ax.send("Input.insertText", {"text": "设计"})
        text_value("TreeSelectSingleInput", "设计")
        summary("Owner: Product engineering / Client apps")
        page.keyboard.press("Enter")
        summary("Owner: Product engineering / Client apps")
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.insert_text("Design")
        text_value("TreeSelectSingleInput", "Design")
        page.keyboard.press("ArrowDown")
        page.keyboard.press("Enter")
        summary("Owner: Product engineering / Design")
        expect(page.get_by_role("tree", name="Owning team options", exact=True)).to_have_count(0)
        checks.append("Chinese composition commits query without activating a team")

        click("projectCollaboratingTeams")
        expect(page.get_by_role("tree", name="Collaborating teams options", exact=True)).to_be_visible()
        page.keyboard.insert_text("Design")
        text_value("TreeSelectMultipleInput", "Design")
        # Existing selection is the second Design team. Navigate to the first.
        page.keyboard.press("ArrowUp")
        page.keyboard.press("ArrowUp")
        page.keyboard.press("Enter")
        expect(tags()).to_have_count(2)
        summary("Product engineering / Design")
        summary("Brand and marketing / Design")
        page.screenshot(path=str(artifact_dir / "multiple-search.png"))
        combo = page.get_by_role("combobox", name="Collaborating teams", exact=True)
        tree = page.get_by_role("tree", name="Collaborating teams options", exact=True)
        expect(combo).to_have_attribute("aria-expanded", "true")
        expect(tree).to_be_visible()
        expect(page.get_by_role("treeitem", name="Design", exact=True)).to_have_count(2)
        expect(combo).to_have_attribute("aria-controls", tree.get_attribute("id"))
        nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        (artifact_dir / "selected-ax.json").write_text(json.dumps(nodes, ensure_ascii=False, indent=2))
        exposed = [n for n in nodes if not n.get("ignored")]
        if not any(n.get("role", {}).get("value") == "combobox"
                   and n.get("name", {}).get("value") == "Collaborating teams" for n in exposed):
            raise RuntimeError("TreeSelect combobox is absent from the Chromium accessibility tree")
        design = [n for n in exposed if n.get("role", {}).get("value") == "treeitem"
                  and n.get("name", {}).get("value") == "Design"]
        if len(design) != 2 or {n.get("description", {}).get("value") for n in design} != {
                "Product engineering / Design", "Brand and marketing / Design"}:
            raise RuntimeError("Filtered tree must expose both same-name teams with full paths")
        for node in design:
            properties = {p["name"]: p.get("value", {}).get("value") for p in node.get("properties", [])}
            if properties.get("level") != 2 or str(properties.get("checked")).lower() != "true":
                raise RuntimeError(f"Missing tree level or check state: {properties}")
        checks.append("Independent same-name selections retain distinct full paths and checked tree semantics")

        page.keyboard.press("Escape")
        expect(page.get_by_role("tree", name=re.compile(r"options$"))).to_have_count(0)
        page.keyboard.press("Backspace")
        expect(tags()).to_have_count(2)
        page.keyboard.press("Backspace")
        expect(tags()).to_have_count(1)
        checks.append("Escape keeps values; two-stage Backspace removes only the focused tag")

        click("projectCollaboratingTeams")
        expect(page.get_by_role("tree", name="Collaborating teams options")).to_be_visible()
        expect(page.get_by_role("treeitem", name="Design", exact=True)).to_have_count(2)
        page.keyboard.insert_text("no matching team")
        # Qt may create a lazy label's AX proxy before setObjectName runs.
        # Verify the user-facing text and actual AX semantics, not that ID.
        empty_message = page.locator('[aria-hidden="false"]').filter(
            has_text=re.compile(r"^No matching items$"))
        expect(empty_message).to_be_visible()
        empty_nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        if not any(not node.get("ignored") and any(
                node.get(field, {}).get("value") == "No matching items"
                for field in ("name", "value")) for node in empty_nodes):
            raise RuntimeError("Empty results message is missing from Chromium accessibility")
        page.keyboard.press("Enter")
        expect(tags()).to_have_count(1)
        page.keyboard.press("Tab")
        expect(page.get_by_role("tree", name=re.compile(r"options$"))).to_have_count(0)
        checks.append("Empty query results never create values; Tab exits")
        click("teamReadOnly")
        expect(tags()).to_have_count(0)
        expect(page.get_by_role("combobox", name="Owning team", exact=True)).to_have_attribute("aria-readonly", "true")
        click("projectOwningTeam")
        expect(page.get_by_role("tree", name=re.compile(r"options$"))).to_have_count(0)
        summary("Owner: Product engineering / Design")
        page.screenshot(path=str(artifact_dir / "readonly.png"))
        checks.append("Read-only retains values and prevents opening")
        if errors:
            raise RuntimeError(" | ".join(errors))
        result_path.write_text(json.dumps({"status": "pass", "checks": checks}, ensure_ascii=False, indent=2))
    except Exception as error:
        result_path.write_text(json.dumps({"status": "fail", "checks": checks, "error": str(error)}, ensure_ascii=False, indent=2))
        page.screenshot(path=str(artifact_dir / "failure.png"))
        (artifact_dir / "failure-dom.json").write_text(json.dumps(page.evaluate(
            """() => [...document.querySelector('#qt-shadow-container').shadowRoot.querySelectorAll('[aria-hidden="false"]')]
                .map(n => ({id:n.id,role:n.getAttribute('role'),name:n.getAttribute('aria-label'),value:n.value,text:n.textContent}))"""
        ), ensure_ascii=False, indent=2))
        raise
    finally:
        page.close()
