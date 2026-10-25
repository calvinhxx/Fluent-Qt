"""Exercise asynchronous suggestions with browser input and Qt accessibility."""

from __future__ import annotations

import json
import time
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_auto_suggest_contract(browser: object, base_url: str, artifact_dir: Path) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 2200}, device_scale_factor=2)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    ax = page.context.new_cdp_session(page)
    checks = []

    def control(name: str) -> object:
        return page.locator(f'[id$=".{name}"][aria-hidden="false"]')

    def click(name: str) -> None:
        target = control(name)
        expect(target).to_be_visible()
        bounds = target.bounding_box()
        if bounds is None:
            raise RuntimeError(f"Missing control bounds: {name}")
        page.mouse.click(bounds["x"] + bounds["width"] / 2, bounds["y"] + bounds["height"] / 2)

    def value(name: str, text: str) -> None:
        page.wait_for_function(
            """({name, text}) => [...document.querySelector('#qt-shadow-container').shadowRoot
                .querySelectorAll(`[id$=".${name}"][aria-hidden="false"]`)]
                .some(n => n.value === text || n.textContent === text || n.getAttribute('aria-label') === text)""",
            arg={"name": name, "text": text}, timeout=10_000,
        )

    def query(text: str) -> None:
        click("asyncProjectSearch")
        page.keyboard.press("ControlOrMeta+A")
        # Commit the requested query in one real input event. Per-character
        # IPC can outlast the debounce on a busy browser and consume the
        # one-shot failure on an intermediate query instead of this one.
        page.keyboard.insert_text(text)

    def closed() -> None:
        expect(control("AutoSuggestBoxStatusTitle")).to_have_count(0)
        expect(visible_cells()).to_have_count(0)
        expect(control("AutoSuggestBoxSuggestionPopup")).to_have_count(0)

    def visible_cells() -> object:
        return page.locator('[id*=".fluentWasmVisibleListCell_"][aria-hidden="false"]')

    def observe_loading(title: str, row_count: int) -> None:
        # Observe the whole transient state in one DOM turn. Separate browser
        # round trips can run after the 350 ms demo response has already arrived.
        page.evaluate("""({title, rowCount}) => {
            const root = document.querySelector('#qt-shadow-container').shadowRoot;
            window.__asyncLoadingObserver?.disconnect();
            window.__asyncLoadingObserved = false;
            const observe = () => {
                const titles = [...root.querySelectorAll('[id$=".AutoSuggestBoxStatusTitle"][aria-hidden="false"]')];
                const loading = titles.some(n => [n.value, n.textContent, n.getAttribute('aria-label')].includes(title));
                const rows = root.querySelectorAll('[id*=".fluentWasmVisibleListCell_"][aria-hidden="false"]').length;
                const actions = root.querySelectorAll('[id$=".AutoSuggestBoxStatusAction"][aria-hidden="false"]').length;
                if (loading && rows === rowCount && actions === 0)
                    window.__asyncLoadingObserved = true;
            };
            window.__asyncLoadingObserver = new MutationObserver(observe);
            window.__asyncLoadingObserver.observe(root, {subtree:true, childList:true, attributes:true, characterData:true});
            observe();
        }""", {"title": title, "rowCount": row_count})

    def loading_observed() -> None:
        page.wait_for_function("window.__asyncLoadingObserved === true", timeout=10_000)
        page.evaluate("window.__asyncLoadingObserver.disconnect()")

    try:
        page.goto(f"{base_url}/app/index.html?route=auto-suggest-box&window-mode=maximized&wasm-smoke=interaction-state",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        wait_for_gallery_ready(page)
        activation = page.locator(
            "#qt-shadow-container .qt-window-a11y-container:visible "
            "> button.hidden-visually-read-by-screen-reader"
        ).first
        activation.wait_for(state="attached")
        activation.evaluate("button => button.click()")

        observe_loading("Searching projects…", 0)
        query("Aurora")
        loading_observed()
        page.screenshot(path=str(artifact_dir / "loading.png"))
        value("AutoSuggestBoxStatusAction", "Load more")
        # Semantic rows are coalesced after the model update; observe their
        # publication before requesting Chromium's computed accessibility tree.
        expect(visible_cells()).to_have_count(4)
        nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        exposed = [n.get("name", {}).get("value") for n in nodes if not n.get("ignored")]
        for text in ("Aurora Workspace", "Aurora Mobile", "Aurora API", "Aurora Docs", "Load more"):
            if text not in exposed:
                raise RuntimeError(f"Suggestion missing from Chromium accessibility tree: {text}")
        checks.append("results-and-action-in-accessibility-tree")
        page.wait_for_timeout(250)
        ids = visible_cells().evaluate_all("nodes => nodes.map(node => node.id)")
        page.evaluate("""ids => {
            const expected = new Set(ids);
            window.__stableSuggestionMutations = [];
            window.__stableSuggestionObserver = new MutationObserver(records => {
                for (const record of records)
                    if (expected.has(record.target.id))
                        window.__stableSuggestionMutations.push(record.attributeName);
            });
            window.__stableSuggestionObserver.observe(
                document.querySelector('#qt-shadow-container').shadowRoot,
                {subtree: true, attributes: true,
                 attributeFilter: ['role', 'aria-description', 'aria-posinset',
                                   'aria-setsize', 'aria-selected', 'aria-disabled']});
        }""", ids)
        for cell in visible_cells().all():
            bounds = cell.bounding_box()
            if bounds:
                page.mouse.move(bounds["x"] + 12, bounds["y"] + bounds["height"] / 2)
                page.wait_for_timeout(180)
        mutations = page.evaluate("""() => {
            window.__stableSuggestionObserver.disconnect();
            return window.__stableSuggestionMutations;
        }""")
        if mutations:
            raise RuntimeError(f"Hover-only painting rewrote suggestion semantics: {mutations}")
        checks.append("hover-paint-does-not-republish-unchanged-accessibility-slots")
        page.screenshot(path=str(artifact_dir / "results.png"))
        for cell in visible_cells().all():
            expect(cell).to_have_attribute("role", "option")
            expect(cell).to_have_attribute("aria-disabled", "false")
        for name in ("Aurora Workspace", "Aurora Mobile"):
            page.keyboard.press("ArrowDown")
            selected = visible_cells().and_(page.locator('[aria-selected="true"]'))
            expect(selected).to_have_count(1)
            expect(selected).to_have_text(name)
            expect(control("asyncProjectSearch")).to_have_attribute("aria-activedescendant", selected.get_attribute("id"))
            nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
            selected_names = [node.get("name", {}).get("value") for node in nodes
                              if not node.get("ignored") and node.get("role", {}).get("value") == "option"
                              and any(p.get("name") == "selected" and p.get("value", {}).get("value") is True
                                      for p in node.get("properties", []))]
            if selected_names != [name]:
                raise RuntimeError(f"Selected option missing from Chromium AX: {selected_names}")
        selected_id = selected.get_attribute("id")
        page.keyboard.press("Escape")
        closed()
        expect(control("asyncProjectSearch")).not_to_have_attribute("aria-activedescendant", selected_id)
        expect(control("asyncProjectSearch")).to_have_attribute("aria-expanded", "false")
        query("Aurora")
        value("AutoSuggestBoxStatusAction", "Load more")
        checks.append("option-role-selection-active-descendant-and-dismissal")
        observe_loading("Loading more…", 4)
        page.keyboard.press("Tab")
        page.keyboard.press("Space")
        loading_observed()
        expect(control("AutoSuggestBoxStatusTitle")).to_have_count(0)
        # Wait for the model to finish appending, then exercise real key input.
        # The submitted label below verifies the selected item; Qt 6.9's field
        # proxy can lag the painted preview text during keyboard navigation.
        for _ in range(8):
            page.keyboard.press("ArrowDown", delay=50)
            page.evaluate("() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))")
        page.keyboard.press("Enter")
        value("asyncSearchResult", "Selected: Aurora Support")
        checks.append("keyboard-pagination-and-selection")

        query("zzz")
        value("AutoSuggestBoxStatusTitle", "No projects found")
        page.screenshot(path=str(artifact_dir / "empty.png"))
        page.keyboard.press("Escape")
        closed()
        click("asyncFailNext")
        expect(control("asyncFailNext")).to_have_attribute("checked", "true")
        query("Canvas")
        value("AutoSuggestBoxStatusTitle", "Couldn’t load projects")
        value("AutoSuggestBoxStatusAction", "Retry")
        page.screenshot(path=str(artifact_dir / "retry.png"))
        click("AutoSuggestBoxStatusAction")
        expect(visible_cells()).to_have_count(2)
        page.keyboard.press("ArrowDown", delay=50)
        page.keyboard.press("Enter")
        value("asyncSearchResult", "Selected: Canvas")
        checks.append("empty-reopen-failure-retry")

        query("A")
        page.wait_for_timeout(300)
        query("Canvas")
        page.wait_for_timeout(1100)
        expect(visible_cells()).to_have_count(2)
        page.keyboard.press("ArrowDown", delay=50)
        value("asyncProjectSearch", "Canvas")
        page.keyboard.press("Enter")
        value("asyncSearchResult", "Selected: Canvas")
        checks.append("late-response-cannot-replace-new-query")
        # Model resets reuse the same small set of browser presentation slots.
        for text, count in (("Aurora", 4), ("Canvas", 2), ("Delta", 1), ("Aurora", 4), ("Canvas", 2)):
            query(text)
            expect(visible_cells()).to_have_count(count)
        nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
        exposed = [n.get("name", {}).get("value") for n in nodes if not n.get("ignored")]
        if "Aurora Workspace" in exposed or "Aurora Support" in exposed:
            raise RuntimeError("Old query rows remain in the accessibility tree")
        slot_count = page.locator('[id*=".fluentWasmVisibleListCell_"]').count()
        if slot_count > 7:
            raise RuntimeError(f"Suggestion proxies grew across model resets: {slot_count}")
        checks.append("bounded-proxies-and-no-stale-accessible-rows")
        for outside in (False, True):
            query("A")
            page.wait_for_timeout(300)
            if outside:
                page.mouse.click(1320, 1000)
            else:
                page.keyboard.press("Escape")
            page.wait_for_timeout(1000)
            closed()
        checks.append("escape-and-outside-dismissal-invalidate-pending-work")
        # Exercise late activation after both ordinary and deliberately slow
        # startup. No accessibility proxy is enabled to find or focus the field.
        for cpu_rate in (1, 8):
            run_late_accessibility_contract(browser, base_url, artifact_dir / f"late-cpu-{cpu_rate}", cpu_rate)
        checks.append("late-accessibility-after-normal-and-slow-startup")
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
            ), indent=2), encoding="utf-8")
        raise
    finally:
        page.close()


def run_late_accessibility_contract(browser: object, base_url: str, artifact_dir: Path, cpu_rate: int = 1) -> None:
    """Prove raw input reached Qt before judging late accessibility publication."""
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1440, "height": 2200}, device_scale_factor=2)
    ax = page.context.new_cdp_session(page)
    if cpu_rate != 1:
        ax.send("Emulation.setCPUThrottlingRate", {"rate": cpu_rate})
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    evidence = {"cpu_rate": cpu_rate}
    expected = ["Aurora Workspace", "Aurora Mobile", "Aurora API", "Aurora Docs"]

    def observe() -> dict:
        return page.evaluate("""() => ({
            qt: JSON.parse(document.documentElement.dataset.fluentQtAutoSuggestInput || '{}'),
            browserActiveElement: document.activeElement?.tagName,
            browserInputValue: document.querySelector("body > input[type='text']")?.value,
            visibility: document.visibilityState
        })""")

    try:
        page.goto(f"{base_url}/app/index.html?route=auto-suggest-box&window-mode=maximized&wasm-smoke=auto-suggest-input",
                  wait_until="domcontentloaded")
        page.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'", timeout=120_000)
        evidence["loaded"] = observe()
        page.bring_to_front()
        # The loader-ready flag only means main() has returned. Splash and
        # deferred sample construction may still own input, especially on a
        # busy browser. Use the current page's Qt state and geometry instead of
        # sleeping four seconds and reusing another page's field rectangle.
        page.wait_for_function("""() => {
            const s = JSON.parse(document.documentElement.dataset.fluentQtAutoSuggestInput || '{}');
            return s.ready && s.point?.length === 2;
        }""", timeout=60_000)
        evidence["ready"] = observe()
        activation = page.locator("#qt-shadow-container .qt-window-a11y-container:visible "
                                  "> button.hidden-visually-read-by-screen-reader").first
        expect(activation).to_be_attached()
        point = evidence["ready"]["qt"]["point"]
        page.mouse.click(*point)
        page.wait_for_function("JSON.parse(document.documentElement.dataset.fluentQtAutoSuggestInput).focused === true",
                               timeout=10_000)
        # Headless Chromium can keep DOM focus on its canvas after Qt has
        # focused the real field. Focusing the input context is allowed only
        # after the independent Qt focus check above.
        if page.evaluate("document.activeElement?.tagName !== 'INPUT'"):
            page.locator("body > input[type='text']").focus()
        page.keyboard.insert_text("Aurora")
        page.wait_for_function("""expected => {
            const s = JSON.parse(document.documentElement.dataset.fluentQtAutoSuggestInput || '{}');
            return s.focused && s.text === 'Aurora' && s.open && s.loadState === 2
                && JSON.stringify(s.rows) === JSON.stringify(expected);
        }""", arg=expected, timeout=20_000)
        evidence["beforeActivation"] = observe()
        expect(activation).to_be_attached()
        page.screenshot(path=str(artifact_dir / "before-activation.png"))
        activation.evaluate("button => button.click()")
        # Wait for semantic publication, not an arbitrary 100 ms. The real AX
        # tree must contain each visible row exactly once; persistent absence
        # or duplication remains a failure with both Qt and browser evidence.
        deadline = time.monotonic() + 10
        while True:
            nodes = ax.send("Accessibility.getFullAXTree")["nodes"]
            names = [node.get("name", {}).get("value") for node in nodes
                     if not node.get("ignored") and node.get("role", {}).get("value") == "option"]
            counts = {name: names.count(name) for name in expected}
            if all(count == 1 for count in counts.values()):
                break
            if time.monotonic() >= deadline:
                raise RuntimeError(f"Late accessibility row publication failed: {counts}")
            page.wait_for_timeout(50)
        evidence["afterActivation"] = observe()
        evidence["rowCounts"] = counts
        if errors:
            raise RuntimeError(f"Late accessibility page errors: {errors}")
        page.screenshot(path=str(artifact_dir / "after-activation.png"))
        evidence["status"] = "pass"
    except Exception:
        evidence["status"] = "fail"
        evidence["failure"] = observe()
        evidence["ax"] = ax.send("Accessibility.getFullAXTree")["nodes"]
        page.screenshot(path=str(artifact_dir / "failure.png"))
        raise
    finally:
        evidence["page_errors"] = errors
        (artifact_dir / "result.json").write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        page.evaluate("document.documentElement.dataset.fluentQtAutoSuggestInputDone = 'true'")
        page.close()
