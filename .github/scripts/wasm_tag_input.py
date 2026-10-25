"""Exercise tagged member selection through browser input and Qt accessibility."""

from __future__ import annotations

import json
import re
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_tag_input_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
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
            raise RuntimeError(f"Missing control bounds: {name}")
        # The tags occupy the start of this field; click its native editor.
        if name == "projectMemberInput":
            page.mouse.click(bounds["x"] + bounds["width"] - 20, bounds["y"] + bounds["height"] - 16)
        else:
            page.mouse.click(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2)

    def value(name: str, text: str) -> None:
        page.wait_for_function(
            """({name, text}) => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll(`[id$=".${name}"][aria-hidden="false"]`)]
                .some(n => n.value === text || n.textContent === text || n.getAttribute('aria-label') === text)""",
            arg={"name": name, "text": text}, timeout=10_000,
        )

    def tags() -> object:
        return page.get_by_role("button", name=re.compile(r"^Remove ")).and_(
            page.locator('[id*=".projectMemberInput."]')
        )

    def cells() -> object:
        return page.locator('[id*=".fluentWasmVisibleListCell_"][aria-hidden="false"]')

    def query(text: str) -> None:
        click("projectMemberInput")
        page.keyboard.press("ControlOrMeta+A")
        page.keyboard.insert_text(text)

    def reset() -> None:
        # Reset is setup, not a fixed Tab-order assumption after the field wraps
        # or shrinks. Keyboard exit is exercised separately below.
        page.get_by_role("button", name="Reset members", exact=True).evaluate("button => button.click()")
        expect(tags()).to_have_count(2)
        value("projectMemberInput", "")

    try:
        page.goto(f"{base_url}/app/index.html?route=tag-input&window-mode=maximized&wasm-smoke=interaction-state",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        page.locator("#qt-shadow-container .qt-window-a11y-container:visible "
                     "> button.hidden-visually-read-by-screen-reader").first.evaluate("button => button.click()")

        expect(tags()).to_have_count(2)
        expect(page.get_by_role("button", name="Remove Chen Chen · Design team", exact=True)).to_have_count(1)
        close = page.get_by_role("button", name="Remove Chen Chen · Design team", exact=True)
        bounds = close.bounding_box()
        if bounds is None or bounds["width"] > 28:
            raise RuntimeError(f"Tag removal must expose only the close button bounds: {bounds}")
        page.mouse.click(bounds["x"] - 24, bounds["y"] + bounds["height"] / 2)
        expect(tags()).to_have_count(2)
        page.mouse.click(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2)
        expect(tags()).to_have_count(1)
        reset()
        checks.append("tag-body-focuses-without-removal-and-only-close-button-deletes")
        page.screenshot(path=str(artifact_dir / "selected.png"))

        click("projectMemberInput")
        page.keyboard.press("Backspace")
        expect(tags()).to_have_count(2)
        ax.send("Input.imeSetComposition", {"text": "chen", "selectionStart": 4, "selectionEnd": 4})
        page.wait_for_timeout(350)
        expect(cells()).to_have_count(0)
        expect(control("AutoSuggestBoxStatusTitle")).to_have_count(0)
        expect(tags()).to_have_count(2)
        page.screenshot(path=str(artifact_dir / "ime-preedit.png"))
        ax.send("Input.insertText", {"text": "陈"})
        value("projectMemberInput", "陈")
        value("AutoSuggestBoxStatusTitle", "No members found")
        expect(tags()).to_have_count(2)
        query("Chen Chen")
        expect(cells()).to_have_count(2)
        disabled = cells().and_(page.locator('[aria-disabled="true"]'))
        expect(disabled).to_have_count(1)
        expect(disabled).to_have_attribute("role", "option")
        page.keyboard.press("ArrowDown")
        selected = cells().and_(page.locator('[aria-selected="true"]'))
        expect(selected).to_have_count(1)
        expect(selected).to_have_attribute("aria-disabled", "false")
        expect(disabled).to_have_attribute("aria-selected", "false")
        nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        disabled_options = [node for node in nodes if not node.get("ignored")
                            and node.get("role", {}).get("value") == "option"
                            and any(p.get("name") == "disabled" and p.get("value", {}).get("value") is True
                                    for p in node.get("properties", []))]
        if len(disabled_options) != 1:
            raise RuntimeError("Disabled suggestion missing from Chromium accessibility tree")
        checks.append("disabled-option-state-and-keyboard-skip")
        expect(tags()).to_have_count(2)
        value("projectMemberInput", "Chen Chen · Engineering team")
        page.screenshot(path=str(artifact_dir / "results.png"))
        page.keyboard.press("Enter")
        expect(tags()).to_have_count(3)
        expect(page.get_by_role("button", name="Remove Chen Chen · Engineering team", exact=True)).to_have_count(1)
        value("projectMemberInput", "")
        checks.append("Chinese-composition-from-focused-tag-defers-search-and-English-query-confirms-distinct-member")

        page.keyboard.press("Backspace")
        expect(tags()).to_have_count(3)
        page.keyboard.press("Backspace")
        expect(tags()).to_have_count(2)
        page.keyboard.press("ArrowRight")
        page.keyboard.press("ArrowLeft")
        page.keyboard.press("Tab")
        # Qt keeps browser text entry in its input-context element; verify the
        # receiving QWidget by its keyboard action, rather than DOM focus.
        page.keyboard.press("Space")
        expect(control("tagFailNext")).to_have_attribute("checked", "true")
        page.keyboard.press("Space")
        expect(control("tagFailNext")).to_have_attribute("checked", "false")
        checks.append("two-step-removal-arrow-return-and-tab-exit")

        query("alex@example.com； chen.dev@example.com; alex@example.com; unknown@example.com;")
        expect(tags()).to_have_count(4)
        value("projectMemberInput", "unknown@example.com")
        expect(page.get_by_role("button", name="Remove Alex Chen", exact=True)).to_have_count(1)
        expect(page.get_by_role("button", name="Remove Chen Chen · Engineering team", exact=True)).to_have_count(1)
        page.screenshot(path=str(artifact_dir / "bulk-values.png"))
        checks.append("delimited-input-resolves-identities-and-retains-unknown-text")

        page.keyboard.press("Tab")
        page.keyboard.press("Space")
        expect(control("tagFailNext")).to_have_attribute("checked", "true")
        query("wang")
        value("AutoSuggestBoxStatusTitle", "Couldn’t load members")
        expect(tags()).to_have_count(4)
        click("AutoSuggestBoxStatusAction")
        expect(cells()).to_have_count(1)
        page.keyboard.press("ArrowDown")
        page.keyboard.press("Enter")
        expect(tags()).to_have_count(5)
        checks.append("failure-retry-preserves-values")

        page.set_viewport_size({"width": 760, "height": 1600})
        page.wait_for_function("document.documentElement.dataset.fluentQtWindowWidth === '760'")
        expect(tags()).to_have_count(5)
        page.screenshot(path=str(artifact_dir / "narrow.png"))
        page.set_viewport_size({"width": 1440, "height": 1600})
        page.wait_for_function("document.documentElement.dataset.fluentQtWindowWidth === '1440'")
        expect(tags()).to_have_count(5)
        page.keyboard.press("Tab")
        page.keyboard.press("Tab")
        page.keyboard.press("Space")
        expect(control("tagReadOnly")).to_have_attribute("checked", "true")
        expect(tags()).to_have_count(0)
        value("tagSelectionStatus", "5 selected: Chen Chen · Design team, Lin Xiao, Alex Chen, Chen Chen · Engineering team, Ming Wang")
        page.keyboard.press("Shift+Tab")
        page.keyboard.press("Shift+Tab")
        page.keyboard.press("Backspace")
        page.keyboard.press("Enter")
        value("tagSelectionStatus", "5 selected: Chen Chen · Design team, Lin Xiao, Alex Chen, Chen Chen · Engineering team, Ming Wang")
        page.screenshot(path=str(artifact_dir / "readonly.png"))
        page.keyboard.press("Tab")
        page.keyboard.press("Tab")
        page.keyboard.press("Space")
        expect(tags()).to_have_count(5)
        checks.append("resize-and-readonly-preserve-values-without-remove-actions")

        reset()
        click("projectMemberInput")
        page.keyboard.press("Backspace")
        page.keyboard.press("Space")
        expect(tags()).to_have_count(1)
        value("projectMemberInput", "")
        page.keyboard.press("Backspace")
        page.keyboard.press("Enter")
        expect(tags()).to_have_count(0)
        value("projectMemberInput", "")
        checks.append("space-and-enter-activate-focused-remove-buttons")

        reset()
        for _ in range(3):
            query("Chen Chen")
            expect(cells()).to_have_count(2)
            page.keyboard.press("ArrowDown")
            page.keyboard.press("Enter")
            expect(tags()).to_have_count(3)
            reset()
        nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        (artifact_dir / "selected-ax.json").write_text(json.dumps(nodes, ensure_ascii=False, indent=2), encoding="utf-8")
        names = [node.get("name", {}).get("value") for node in nodes
                 if not node.get("ignored") and node.get("role", {}).get("value") == "button"]
        for expected in ("Remove Chen Chen · Design team", "Remove Lin Xiao"):
            if names.count(expected) != 1:
                raise RuntimeError(f"Missing or duplicated selected value: {expected}")
        if "Remove Chen Chen · Engineering team" in names:
            raise RuntimeError("Removed member remains in the browser accessibility tree")
        checks.append("rebuild-and-reopen-have-no-stale-accessible-tags")
        if errors:
            raise RuntimeError("Browser errors: " + "; ".join(errors))
        (artifact_dir / "result.json").write_text(json.dumps({
            "status": "pass", "checks": checks, "page_errors": errors,
        }, indent=2) + "\n", encoding="utf-8")
    except Exception as error:
        (artifact_dir / "result.json").write_text(json.dumps({
            "status": "fail", "error": str(error), "checks": checks, "page_errors": errors,
        }, indent=2) + "\n", encoding="utf-8")
        page.screenshot(path=str(artifact_dir / "failure.png"))
        (artifact_dir / "failure-dom.json").write_text(json.dumps(
            page.locator("#qt-shadow-container [aria-hidden='false']").evaluate_all(
                """nodes => nodes.map(n => ({id: n.id, role: n.getAttribute('role'),
                    text: n.textContent, value: n.value, label: n.getAttribute('aria-label')}))"""
            ), ensure_ascii=False, indent=2), encoding="utf-8")
        raise
    finally:
        page.close()
