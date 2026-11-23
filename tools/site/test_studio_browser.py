#!/usr/bin/env python3
"""End-to-end Playwright verification for Fluent-Qt Page Studio (WASM and schematic fallback)."""

from __future__ import annotations

import argparse
import base64
import json
from contextlib import ExitStack
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from pathlib import Path
import sys
from playwright.sync_api import sync_playwright


def select_fluent_combo(page, combo_id: str, value: str) -> None:
    page.locator(f'[data-combo-id="{combo_id}"] .fluent-combo-trigger').click()
    opt = page.locator(f'.fluent-combo-menu[data-combo-for="{combo_id}"] [role="option"][data-value="{value}"]')
    opt.wait_for(state="visible", timeout=5000)
    opt.click()


def ensure_palette_category_open(page, category_id: str) -> None:
    cat = page.locator(f'.palette-cat[data-category-id="{category_id}"]')
    if cat.get_attribute("open") is None:
        summary = cat.locator("> summary")
        summary.scroll_into_view_if_needed()
        summary.click()


def verify_homepage_entry_points(page, base_url: str, output_dir: Path) -> None:
    page.goto(base_url + "zh-CN/", wait_until="domcontentloaded")
    page.wait_for_selector("html[data-i18n-ready]", timeout=10000)
    page.evaluate("() => localStorage.setItem('fluent-qt-theme', 'light')")
    page.reload(wait_until="domcontentloaded")
    page.wait_for_selector("html[data-i18n-ready]", timeout=10000)
    for sel in ('[data-i18n="nav.studio"]', '.hero-studio-action', '[data-analytics-event="path_studio"]'):
        loc = page.locator(sel)
        if not loc.is_visible():
            raise RuntimeError(f"Expected Studio homepage entry point visible: {sel}")
    path_copy = page.locator('[data-i18n="path.studioCopy"]').inner_text()
    if "99" not in path_copy or "97" in path_copy:
        raise RuntimeError(f"Expected homepage Studio card to mention 99 widgets, got: {path_copy}")
    output_dir.mkdir(parents=True, exist_ok=True)
    page.screenshot(path=str(output_dir / "homepage-studio-entry.png"))
    page.locator('.hero-studio-action').click()
    page.wait_for_url("**/studio/?lang=zh", timeout=10000)


def wait_wasm_settled(page) -> None:
    page.wait_for_selector('#runtime-badge[data-state="wasm"]', timeout=25000)
    page.wait_for_selector('#canvas-shell.is-wasm-ready', timeout=15000)
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            const loader = document.querySelector('#wasm-loader');
            return Boolean(
                shell &&
                shell.classList.contains('is-wasm-ready') &&
                !shell.classList.contains('is-wasm-loading') &&
                loader &&
                getComputedStyle(loader).visibility === 'hidden' &&
                getComputedStyle(loader).opacity === '0'
            );
        }""",
        timeout=8000,
    )


def run_wasm_checks(page, base_url: str, output_dir: Path) -> None:
    verify_homepage_entry_points(page, base_url, output_dir)
    wait_wasm_settled(page)
    page.wait_for_selector('#canvas-shell.is-wasm-ready [data-canvas-node="button_1"]', timeout=15000)

    if page.evaluate("() => document.documentElement.dataset.theme") != "light":
        raise RuntimeError("Studio did not inherit light theme from homepage")
    if page.evaluate("() => document.documentElement.lang") != "zh-CN":
        raise RuntimeError("Studio did not inherit zh-CN language from zh-CN homepage")

    for removed_sel in (
        "select",
        "#btn-theme",
        "#btn-lang",
        "#more-menu",
        "#template-select",
        "#scope-recommended",
        "#scope-all",
        "#palette-category",
    ):
        if page.locator(removed_sel).count() != 0:
            raise RuntimeError(f"Expected 0 elements matching removed control {removed_sel}")
    if page.locator("#selection-box").is_visible():
        raise RuntimeError("Expected initial root selection box to stay calm/hidden before user selection")
    if page.locator("#outline-tree .tree-row").count() != 2:
        raise RuntimeError(
            f"Expected starter canvas to have 2 outline rows (root + button_1), got {page.locator('#outline-tree .tree-row').count()}"
        )
    if "AnchorLayout" not in page.locator("#selected-api-hint").inner_text():
        raise RuntimeError(
            f"Expected root inspector hint to advertise AnchorLayout, got {page.locator('#selected-api-hint').inner_text()!r}"
        )
    if page.locator("#catalog-count").inner_text().strip() != "103":
        raise RuntimeError(f"Expected catalog count badge '103', got {page.locator('#catalog-count').inner_text()!r}")
    if page.locator(".palette-item").count() != 103:
        raise RuntimeError(f"Expected 103 palette items by default across categories, got {page.locator('.palette-item').count()}")
    if page.locator(".palette-cat").count() != 13:
        raise RuntimeError(f"Expected 13 category groups by default, got {page.locator('.palette-cat').count()}")
    if page.locator(".palette-cat[open]").count() != 1 or page.locator('.palette-cat[open][data-category-id="layout"]').count() != 1:
        raise RuntimeError("Expected only the first category ('layout') open by default")

    def assert_button_centered(stage_label: str, timeout: int = 8000) -> None:
        try:
            page.wait_for_function(
                """() => {
                    const s = document.querySelector('#canvas-shell')?.getBoundingClientRect();
                    const b = document.querySelector('[data-canvas-node="button_1"]')?.getBoundingClientRect();
                    if (!s || !b) return false;
                    return Math.abs((b.left + b.width / 2) - (s.left + s.width / 2)) <= 12
                        && Math.abs((b.top + b.height / 2) - (s.top + s.height / 2)) <= 12;
                }""",
                timeout=timeout,
            )
        except Exception as exc:
            shell_box = page.locator("#canvas-shell").bounding_box()
            btn_box = page.locator('[data-canvas-node="button_1"]').bounding_box()
            raise RuntimeError(
                f"Expected starter button_1 centered in {stage_label}: shell={shell_box}, btn={btn_box}"
            ) from exc

    assert_button_centered("desktop")
    output_dir.mkdir(parents=True, exist_ok=True)
    page.screenshot(path=str(output_dir / "studio-wasm-light.png"))

    # Verify starter button stays centered across Desktop (860) -> Tablet (620) -> Compact (400) -> Desktop (860)
    page.locator('.viewport-group [data-width="620"]').click()
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            return Boolean(shell && shell.dataset.viewportTier === 'medium' && shell.getBoundingClientRect().width <= 630);
        }""",
        timeout=8000,
    )
    page.wait_for_timeout(250)
    assert_button_centered("tablet")
    page.screenshot(path=str(output_dir / "studio-wasm-tablet.png"))

    page.locator('.viewport-group [data-width="400"]').click()
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            return Boolean(shell && shell.dataset.viewportTier === 'compact' && shell.getBoundingClientRect().width <= 410);
        }""",
        timeout=8000,
    )
    page.wait_for_timeout(250)
    assert_button_centered("compact")
    page.screenshot(path=str(output_dir / "studio-wasm-compact.png"))

    page.locator('.viewport-group [data-width="860"]').click()
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            return Boolean(shell && shell.dataset.viewportTier === 'expanded' && shell.getBoundingClientRect().width >= 760);
        }""",
        timeout=8000,
    )
    page.wait_for_timeout(200)

    # Figma free-canvas drag of button_1 from Center -> TopRight area + Undo restore
    shell_box = page.locator("#canvas-shell").bounding_box()
    btn_box = page.locator('[data-canvas-node="button_1"]').bounding_box()
    if not shell_box or not btn_box:
        raise RuntimeError("Missing geometry for starter free drag check")
    page.mouse.move(btn_box["x"] + btn_box["width"] / 2, btn_box["y"] + btn_box["height"] / 2)
    page.mouse.down()
    page.mouse.move(shell_box["x"] + shell_box["width"] - 90, shell_box["y"] + 84, steps=10)
    page.wait_for_selector("#drag-preview-box:not([hidden])", timeout=10000)
    page.wait_for_selector("#drag-ghost.is-docked", timeout=5000)
    page.mouse.up()
    page.wait_for_selector('#inspector-body [data-prop-key="anchorPosition"]', timeout=8000)
    page.wait_for_function(
        """() => {
            const s = document.querySelector('#canvas-shell')?.getBoundingClientRect();
            const b = document.querySelector('[data-canvas-node="button_1"]')?.getBoundingClientRect();
            return Boolean(s && b && b.left > s.left + s.width * 0.55 && b.top < s.top + 160);
        }""",
        timeout=8000,
    )
    page.locator("#btn-code").click()
    page.wait_for_selector("#code-drawer:not([hidden])")
    starter_cpp = page.locator("#code-output code").inner_text()
    if "AnchorLayout" not in starter_cpp or "addAnchoredWidget" not in starter_cpp:
        raise RuntimeError("Expected AnchorLayout + addAnchoredWidget in exported starter C++")
    page.locator("#btn-close-drawer").click()
    page.locator("#btn-undo").click()
    assert_button_centered("after-undo")

    # Verify collapsible category accordion expand/collapse & search snapshot restore
    charts_cat = page.locator('.palette-cat[data-category-id="charts"]')
    charts_cat.locator("> summary").scroll_into_view_if_needed()
    charts_cat.locator("> summary").click()
    if charts_cat.get_attribute("open") is None:
        raise RuntimeError("Clicking charts category tab did not expand it")
    charts_cat.locator("> summary").click()
    if charts_cat.get_attribute("open") is not None:
        raise RuntimeError("Clicking open charts category tab did not collapse it")

    page.locator("#palette-search").fill("PieChart")
    page.wait_for_function(
        "() => document.querySelector('#catalog-count').textContent.trim() === '1' && Boolean(document.querySelector('.palette-cat[data-category-id=\"charts\"]')?.open)",
        timeout=5000,
    )
    page.locator("#btn-clear-search").click()
    page.wait_for_function(
        "() => document.querySelector('#catalog-count').textContent.trim() === '103' && document.querySelectorAll('.palette-cat[open]').length === 1",
        timeout=5000,
    )

    # Verify Figma WYSIWYG palette drag onto root AnchorBox: floating ghost -> live preview -> Escape cancel -> drop & Undo
    ensure_palette_category_open(page, "basic-input")
    toggle_item = page.locator('.palette-item[data-type="ToggleSwitch"] .palette-item-info')
    toggle_item.scroll_into_view_if_needed()
    toggle_box = toggle_item.bounding_box()
    shell_box = page.locator("#canvas-shell").bounding_box()
    if not toggle_box or not shell_box:
        raise RuntimeError("Missing palette or canvas geometry for Figma palette drag verification")
    drag_before = page.locator("#outline-tree .tree-row").count()
    start_x = toggle_box["x"] + toggle_box["width"] / 2
    start_y = toggle_box["y"] + toggle_box["height"] / 2
    page.mouse.move(start_x, start_y)
    page.mouse.down()
    page.mouse.move(start_x + 28, start_y - 40, steps=6)
    page.wait_for_selector("#drag-ghost:not([hidden]):not(.is-docked)", timeout=5000)
    if page.locator("#drag-ghost-body .sch-node").count() < 1:
        raise RuntimeError("Expected rendered schematic preview inside floating #drag-ghost-body")
    page.screenshot(path=str(output_dir / "studio-wasm-dragging-floating.png"))

    page.mouse.move(shell_box["x"] + 160, shell_box["y"] + 120, steps=10)
    page.wait_for_selector("#drag-preview-box:not([hidden])", timeout=10000)
    page.wait_for_selector("#drag-ghost.is-docked", timeout=5000)
    page.wait_for_timeout(120)
    page.screenshot(path=str(output_dir / "studio-wasm-dragging-live.png"))

    page.keyboard.press("Escape")
    page.wait_for_function(
        "prev => document.querySelector('#drag-ghost')?.hidden && document.querySelector('#drag-preview-box')?.hidden && document.querySelectorAll('#outline-tree .tree-row').length === prev",
        arg=drag_before,
        timeout=8000,
    )

    page.mouse.move(start_x, start_y)
    page.mouse.down()
    page.mouse.move(shell_box["x"] + 160, shell_box["y"] + 120, steps=10)
    page.wait_for_selector("#drag-preview-box:not([hidden])", timeout=10000)
    page.mouse.up()
    page.wait_for_function(
        "prev => document.querySelectorAll('#outline-tree .tree-row').length === prev + 1 && Boolean(document.querySelector('[data-canvas-node=\"toggle_switch_1\"]'))",
        arg=drag_before,
        timeout=10000,
    )
    page.locator("#btn-undo").click()
    page.wait_for_function("() => !document.querySelector('[data-canvas-node=\"toggle_switch_1\"]')", timeout=8000)

    # Verify Arrow-key nudge on selected button_1 inside AnchorBox root (+8px Shift+ArrowRight / Shift+ArrowDown)
    page.locator('[data-canvas-node="button_1"]').click()
    page.wait_for_selector('#inspector-body .insp-category-tabs [data-insp-tab="icon"]', timeout=8000)
    page.evaluate("() => { if (document.activeElement instanceof HTMLElement) document.activeElement.blur(); }")
    before_nudge = page.locator('[data-canvas-node="button_1"]').bounding_box()
    page.keyboard.press("Shift+ArrowRight")
    page.keyboard.press("Shift+ArrowDown")
    page.wait_for_function(
        "orig => { const r = document.querySelector('[data-canvas-node=\"button_1\"]')?.getBoundingClientRect(); return Boolean(r && r.left >= orig.x + 6 && r.top >= orig.y + 6); }",
        arg=before_nudge,
        timeout=5000,
    )
    page.locator("#btn-undo").click()
    page.locator("#btn-undo").click()
    assert_button_centered("after-nudge-undo")

    # Verify Duplicate inside AnchorBox offsets child (+24, +24) cleanly without exact overlap
    page.locator('[data-canvas-node="button_1"]').click()
    b1_box = page.locator('[data-canvas-node="button_1"]').bounding_box()
    page.locator("#btn-duplicate").click()
    page.wait_for_selector('[data-canvas-node="button_2"]', timeout=8000)
    b2_box = page.locator('[data-canvas-node="button_2"]').bounding_box()
    if not b1_box or not b2_box or abs((b2_box["x"] - b1_box["x"]) - 24) > 6 or abs((b2_box["y"] - b1_box["y"]) - 24) > 6:
        raise RuntimeError(f"Expected duplicated AnchorBox child offset (+24, +24), got b1={b1_box}, b2={b2_box}")
    page.locator("#btn-undo").click()
    page.wait_for_function("() => !document.querySelector('[data-canvas-node=\"button_2\"]')", timeout=8000)

    # Verify Playground Button Inspector & C++/PySide6/.ui code export on starter AnchorBox
    page.locator('[data-canvas-node="button_1"]').click()
    page.wait_for_selector('#inspector-body .insp-category-tabs [data-insp-tab="icon"]', timeout=8000)
    text_input = page.locator('#inspector-body [data-prop-key="text"]')
    text_input.fill("Deploy Studio")
    text_input.dispatch_event("change")
    page.locator('#inspector-body .insp-category-tabs [data-insp-tab="icon"]').click()
    page.wait_for_selector('#inspector-body [data-prop-key="icon"]', timeout=5000)
    select_fluent_combo(page, "prop-combo-button_1-icon", "Save")
    page.wait_for_selector('#inspector-body [data-property="fluentLayout"]:not([hidden])', timeout=5000)
    page.wait_for_timeout(200)

    page.locator("#btn-code").click()
    page.wait_for_selector("#code-drawer:not([hidden])")
    for fmt in ("cpp", "python", "ui"):
        page.locator(f'.drawer-tab[data-format="{fmt}"]').click()
        code = page.locator("#code-output code").inner_text()
        if "Deploy Studio" not in code:
            raise RuntimeError(f"Expected 'Deploy Studio' in exported {fmt} code")
        if fmt in ("cpp", "python") and ("setIconGlyph" not in code or "AnchorLayout" not in code):
            raise RuntimeError(f"Expected setIconGlyph and AnchorLayout in exported {fmt} code")
    page.locator("#btn-close-drawer").click()

    # Play mode interaction on starter button_1
    hitbox = page.locator('[data-canvas-node="button_1"]')
    box = hitbox.bounding_box()
    if not box or box["width"] <= 0 or box["height"] <= 0:
        raise RuntimeError(f"Invalid WASM hitbox bounds for button_1: {box}")
    page.locator("#mode-play").click()
    page.wait_for_timeout(150)
    page.mouse.click(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
    page.wait_for_function(
        "() => document.querySelector('#event-pill').textContent.includes('button_1')",
        timeout=10000,
    )
    page.locator("#mode-edit").click()

    # Clear Canvas preserves AnchorLayout root; Restore Sample brings back button_1
    page.locator("#node-crumb button").first.click()
    page.locator("#btn-clear-canvas").click()
    page.wait_for_function("() => document.querySelectorAll('#outline-tree .tree-row').length === 1", timeout=8000)
    if "AnchorLayout" not in page.locator("#selected-api-hint").inner_text():
        raise RuntimeError("Clear Canvas unexpectedly lost AnchorLayout root")
    page.locator("#btn-restore-sample").click()
    page.wait_for_selector('[data-canvas-node="button_1"]', timeout=10000)

    # Expand Extensions -> Window -> Charts quick-add smoke verification
    ensure_palette_category_open(page, "extensions")
    for ext_type, node_id in (("ParticleBackdrop", "particle_backdrop_1"), ("SpatialView", "spatial_view_1"), ("QmlView", "qml_view_1")):
        btn = page.locator(f'.palette-item[data-type="{ext_type}"] .palette-add-btn')
        btn.scroll_into_view_if_needed()
        btn.click()
        page.wait_for_selector(f'[data-canvas-node="{node_id}"]', timeout=10000)
        page.locator("#btn-undo").click()
        page.wait_for_function("id => !document.querySelector(`[data-canvas-node=\"${id}\"]`)", arg=node_id, timeout=8000)

    ensure_palette_category_open(page, "windowing-foundation")
    win_btn = page.locator('.palette-item[data-type="Window"] .palette-add-btn')
    win_btn.scroll_into_view_if_needed()
    win_btn.click()
    page.wait_for_selector('[data-canvas-node="window_1"]', timeout=10000)
    page.locator("#btn-undo").click()
    page.wait_for_function("() => !document.querySelector('[data-canvas-node=\"window_1\"]')", timeout=8000)

    ensure_palette_category_open(page, "charts")
    pie_btn = page.locator('.palette-item[data-type="PieChart"] .palette-add-btn')
    pie_btn.scroll_into_view_if_needed()
    pie_btn.click()
    page.wait_for_selector('[data-canvas-node="pie_chart_1"]', timeout=10000)
    page.locator("#btn-undo").click()
    page.wait_for_function("() => !document.querySelector('[data-canvas-node=\"pie_chart_1\"]')", timeout=8000)

    # Test Dark theme + Simplified Chinese inheritance on starter canvas
    page.goto(base_url + "zh-CN/", wait_until="domcontentloaded")
    page.wait_for_selector("html[data-i18n-ready]", timeout=10000)
    page.evaluate("() => localStorage.setItem('fluent-qt-theme', 'dark')")
    page.locator('[data-i18n="nav.studio"]').click()
    page.wait_for_url("**/studio/?lang=zh", timeout=10000)
    wait_wasm_settled(page)
    page.wait_for_selector('[data-canvas-node="button_1"]', timeout=15000)
    if page.evaluate("() => document.documentElement.dataset.theme") != "dark":
        raise RuntimeError("Studio did not inherit dark theme from homepage")
    if page.locator("#btn-code").inner_text().strip() != "导出代码":
        raise RuntimeError("Expected Chinese Export Code button after entering from zh-CN homepage")
    page.wait_for_timeout(150)
    page.screenshot(path=str(output_dir / "studio-wasm-dark.png"))

    page.evaluate(
        """() => {
            localStorage.setItem('fluent-qt-theme', 'light');
            window.dispatchEvent(new StorageEvent('storage', { key: 'fluent-qt-theme', newValue: 'light' }));
        }"""
    )
    page.wait_for_function("() => document.documentElement.dataset.theme === 'light'", timeout=5000)

    # Test English inheritance from root homepage
    page.goto(base_url, wait_until="domcontentloaded")
    page.wait_for_selector("html[data-i18n-ready]", timeout=10000)
    page.locator('[data-i18n="nav.studio"]').click()
    page.wait_for_url("**/studio/?lang=en", timeout=10000)
    wait_wasm_settled(page)
    page.wait_for_selector('[data-canvas-node="button_1"]', timeout=15000)
    if page.evaluate("() => document.documentElement.lang") != "en":
        raise RuntimeError("Expected lang='en' when opening Studio from English homepage")
    if page.locator("#btn-code").inner_text().strip() != "Export Code":
        raise RuntimeError("Expected English 'Export Code' button after entering from EN homepage")

    # Deep link ?template=settings: multi-col Grid/HBox reflow, Play -> Edit sibling isolation, Field climb & Slider Inspector
    page.goto(base_url + "studio/?lang=zh&template=settings", wait_until="domcontentloaded")
    wait_wasm_settled(page)
    page.wait_for_selector('[data-canvas-node="btn_apply"]', timeout=15000)

    theme_box = page.locator('[data-canvas-node="field_theme"]').bounding_box()
    port_box = page.locator('[data-canvas-node="field_port"]').bounding_box()
    apply_box = page.locator('[data-canvas-node="btn_apply"]').bounding_box()
    reset_box = page.locator('[data-canvas-node="btn_reset"]').bounding_box()
    if not all((theme_box, port_box, apply_box, reset_box)):
        raise RuntimeError("Missing initial settings hitboxes for responsive check")
    if port_box["x"] <= theme_box["x"] + 100 or abs(port_box["y"] - theme_box["y"]) > 12:
        raise RuntimeError(f"Expected 2-col side-by-side grid in Desktop view, got theme={theme_box}, port={port_box}")
    if reset_box["x"] <= apply_box["x"] + 20:
        raise RuntimeError(f"Expected horizontal buttons in Desktop view, got apply={apply_box}, reset={reset_box}")

    page.locator('.viewport-group [data-width="400"]').click()
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            const t = document.querySelector('[data-canvas-node="field_theme"]')?.getBoundingClientRect();
            const p = document.querySelector('[data-canvas-node="field_port"]')?.getBoundingClientRect();
            const a = document.querySelector('[data-canvas-node="btn_apply"]')?.getBoundingClientRect();
            const r = document.querySelector('[data-canvas-node="btn_reset"]')?.getBoundingClientRect();
            const b = document.querySelector('[data-canvas-node="info_1"]')?.getBoundingClientRect();
            const c = document.querySelector('[data-canvas-node="card_1"]')?.getBoundingClientRect();
            const sRect = shell?.getBoundingClientRect();
            return Boolean(
                shell && shell.dataset.viewportTier === 'compact' &&
                sRect && sRect.width <= 410 &&
                t && p && p.top >= t.bottom - 4 && Math.abs(p.left - t.left) <= 8 &&
                a && r && r.top >= a.bottom - 4 &&
                b && c && b.right <= sRect.right - 2 && c.right <= sRect.right - 2
            );
        }""",
        timeout=10000,
    )
    page.locator('.viewport-group [data-width="860"]').click()
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            const t = document.querySelector('[data-canvas-node="field_theme"]')?.getBoundingClientRect();
            const p = document.querySelector('[data-canvas-node="field_port"]')?.getBoundingClientRect();
            return Boolean(
                shell && shell.dataset.viewportTier === 'expanded' &&
                t && p && p.left > t.left + 100 && Math.abs(p.top - t.top) <= 12
            );
        }""",
        timeout=10000,
    )

    # Regression: Play interaction on sibling B (switch_motion) must survive editing component A (slider_scale)
    sw_hit = page.locator('[data-canvas-node="switch_motion"]').bounding_box()
    rst_hit = page.locator('[data-canvas-node="btn_reset"]').bounding_box()
    if not sw_hit or not rst_hit:
        raise RuntimeError("Missing hitboxes for Play -> Edit isolation check")
    page.locator("#mode-play").click()
    page.wait_for_timeout(120)
    page.mouse.click(sw_hit["x"] + min(20, sw_hit["width"] / 3), sw_hit["y"] + sw_hit["height"] / 2)
    page.wait_for_function(
        "() => document.querySelector('#event-pill').textContent.includes('switch_motion')",
        timeout=10000,
    )
    page.mouse.click(rst_hit["x"] + rst_hit["width"] / 2, rst_hit["y"] + rst_hit["height"] / 2)
    page.wait_for_function(
        "() => document.querySelector('#event-pill').textContent.includes('btn_reset')",
        timeout=10000,
    )
    page.locator("#mode-edit").click()

    page.locator('[data-canvas-node="slider_scale"]').click()
    page.wait_for_selector('#inspector-body .insp-category-tabs [data-insp-tab="icon"]', timeout=8000)
    page.locator('#inspector-body .insp-category-tabs [data-insp-tab="icon"]').click()
    page.wait_for_selector('#inspector-body [data-prop-key="handleSize"]', timeout=5000)
    page.locator('#inspector-body [data-prop-key="handleSize"]').evaluate(
        """el => {
            el.value = '24';
            el.dispatchEvent(new Event('input', { bubbles: true }));
            el.dispatchEvent(new Event('change', { bubbles: true }));
        }"""
    )
    page.wait_for_function(
        "() => document.querySelector('#inspector-body [data-property=\"handleSize\"] output')?.textContent.includes('24')",
        timeout=5000,
    )
    page.wait_for_timeout(120)

    page.locator('[data-canvas-node="switch_motion"]').click()
    page.wait_for_selector('#inspector-body [data-prop-key="isOn"] [data-value="true"]', timeout=8000)
    if page.locator('#inspector-body [data-prop-key="isOn"] [data-value="true"]').get_attribute("aria-checked") != "false":
        raise RuntimeError("Editing slider_scale reset sibling switch_motion isOn back to true")

    page.locator("#btn-code").click()
    page.wait_for_selector("#code-drawer:not([hidden])")
    cpp_code = page.locator("#code-output code").inner_text()
    if "setHandleSize(24)" not in cpp_code:
        raise RuntimeError("Expected setHandleSize(24) in exported C++ after editing slider_scale")
    if "switch_motion->setIsOn(true)" in cpp_code:
        raise RuntimeError("switch_motion unexpectedly reset to setIsOn(true) in exported C++")
    page.locator("#btn-close-drawer").click()

    page.locator("#btn-undo").click()
    page.locator("#btn-undo").click()
    page.wait_for_function(
        "() => document.querySelector('#inspector-body [data-prop-key=\"isOn\"] [data-value=\"true\"]')?.getAttribute('aria-checked') === 'true'",
        timeout=8000,
    )
    page.locator("#node-crumb button").first.click()
    page.wait_for_timeout(120)

    # Full Field ancestry climb to Grid & HBox reorder checks
    ensure_palette_category_open(page, "basic-input")
    tog_info = page.locator('.palette-item[data-type="ToggleSwitch"] .palette-item-info')
    tog_info.scroll_into_view_if_needed()
    t_box = tog_info.bounding_box()
    s_loc = page.locator('[data-canvas-node="slider_scale"]')
    s_loc.scroll_into_view_if_needed()
    s_box = s_loc.bounding_box()
    if not t_box or not s_box:
        raise RuntimeError("Missing geometry for Field ancestry climb check in settings template")
    page.mouse.move(t_box["x"] + t_box["width"] / 2, t_box["y"] + t_box["height"] / 2)
    page.mouse.down()
    page.mouse.move(s_box["x"] + s_box["width"] * 0.78, s_box["y"] + s_box["height"] / 2, steps=8)
    page.wait_for_selector("#drag-preview-box:not([hidden])", timeout=10000)
    container_tag = page.locator("#drop-container-tag").inner_text()
    if "Grid" not in container_tag and "网格" not in container_tag:
        raise RuntimeError(f"Expected full-Field ancestry climb to Grid container, got tag={container_tag!r}")
    page.keyboard.press("Escape")
    page.wait_for_function("() => Boolean(document.querySelector('#drag-preview-box')?.hidden)", timeout=8000)

    a_loc = page.locator('[data-canvas-node="btn_apply"]')
    r_loc = page.locator('[data-canvas-node="btn_reset"]')
    a_loc.scroll_into_view_if_needed()
    a_box = a_loc.bounding_box()
    r_box = r_loc.bounding_box()
    if not a_box or not r_box or r_box["x"] <= a_box["x"]:
        raise RuntimeError("Unexpected initial order of btn_apply and btn_reset before canvas reorder drag")
    page.mouse.move(r_box["x"] + r_box["width"] / 2, r_box["y"] + r_box["height"] / 2)
    page.mouse.down()
    page.mouse.move(a_box["x"] + 8, a_box["y"] + a_box["height"] / 2, steps=10)
    page.wait_for_selector("#drag-preview-box:not([hidden])", timeout=10000)
    page.mouse.up()
    page.wait_for_function(
        """() => {
            const a = document.querySelector('[data-canvas-node="btn_apply"]')?.getBoundingClientRect();
            const r = document.querySelector('[data-canvas-node="btn_reset"]')?.getBoundingClientRect();
            return Boolean(a && r && r.left < a.left);
        }""",
        timeout=10000,
    )
    page.locator("#btn-undo").click()
    page.wait_for_function(
        """() => {
            const a = document.querySelector('[data-canvas-node="btn_apply"]')?.getBoundingClientRect();
            const r = document.querySelector('[data-canvas-node="btn_reset"]')?.getBoundingClientRect();
            return Boolean(a && r && r.left > a.left);
        }""",
        timeout=10000,
    )

    # Verify Slider playground-aligned inspector: category tabs, tickPosition -> tickInterval visibleWhen, reset properties
    page.locator('[data-canvas-node="slider_scale"]').click()
    page.wait_for_selector('#inspector-body .insp-category-tabs [data-insp-tab="appearance"]', timeout=8000)
    page.locator('#inspector-body .insp-category-tabs [data-insp-tab="appearance"]').click()
    page.wait_for_selector('#inspector-body [data-prop-key="tickPosition"]', timeout=5000)
    if not page.locator('#inspector-body [data-property="tickInterval"]').is_hidden():
        raise RuntimeError("Expected Slider tickInterval to stay hidden while tickPosition is NoTicks")
    select_fluent_combo(page, "prop-combo-slider_scale-tickPosition", "TicksBothSides")
    page.wait_for_selector('#inspector-body [data-property="tickInterval"]:not([hidden])', timeout=5000)
    page.wait_for_timeout(180)
    page.screenshot(path=str(output_dir / "studio-wasm-inspector-slider.png"))
    if page.locator("#btn-reset-props").is_disabled():
        raise RuntimeError("Expected #btn-reset-props to be enabled after editing slider_scale")
    page.locator("#btn-reset-props").click()
    page.wait_for_function("() => document.querySelector('#btn-reset-props')?.disabled === true", timeout=5000)

    # Verify ?template=analytics deep link still works cleanly
    page.goto(base_url + "studio/?lang=zh&template=analytics", wait_until="domcontentloaded")
    wait_wasm_settled(page)
    page.wait_for_selector('[data-canvas-node="chart_line"]', timeout=15000)


def verify_loading_state(browser, base_url: str, output_dir: Path) -> None:
    loading_context = browser.new_context(viewport={"width": 1440, "height": 900})
    loading_page = loading_context.new_page()
    held_routes = []
    loading_errors: list[str] = []
    loading_page.on("pageerror", lambda err: loading_errors.append(f"pageerror: {err}"))

    def intercept_wasm(route):
        if route.request.method == "HEAD":
            route.continue_()
            return
        held_routes.append(route)

    loading_page.add_init_script("try { localStorage.setItem('fluent-qt-theme', 'light'); } catch (_) {}")
    loading_page.route("**/fluent_qt_gallery.wasm*", intercept_wasm)
    try:
        loading_page.goto(base_url + "studio/?lang=zh", wait_until="domcontentloaded")
        loading_page.wait_for_selector("#wasm-loader", state="visible", timeout=8000)
        layout_ok = loading_page.evaluate(
            """() => {
                const shell = document.querySelector('#canvas-shell');
                const loader = document.querySelector('#wasm-loader');
                const wasm = document.querySelector('#wasm-stage');
                const sch = document.querySelector('#schematic-stage');
                if (!shell || !loader || !wasm || !sch) return { ok: false, reason: 'missing stage nodes' };
                const s = shell.getBoundingClientRect();
                const l = loader.getBoundingClientRect();
                const w = wasm.getBoundingClientRect();
                const c = sch.getBoundingClientRect();
                const schOpacity = getComputedStyle(sch).opacity;
                return {
                    ok: shell.classList.contains('is-wasm-loading')
                        && !shell.classList.contains('is-wasm-ready')
                        && !shell.classList.contains('is-static-only')
                        && Math.abs(l.top - s.top) <= 2
                        && Math.abs(w.top - s.top) <= 2
                        && Math.abs(c.top - s.top) <= 2
                        && l.height >= 500
                        && schOpacity === '0',
                    shellTop: s.top,
                    loaderTop: l.top,
                    wasmTop: w.top,
                    schTop: c.top,
                    schOpacity,
                    loaderHeight: l.height,
                    cls: shell.className,
                };
            }"""
        )
        if not layout_ok.get("ok"):
            raise RuntimeError(f"WASM loading overlay layout mismatch: {layout_ok}")

        output_dir.mkdir(parents=True, exist_ok=True)
        loading_page.wait_for_timeout(200)
        loading_page.screenshot(path=str(output_dir / "studio-loading-light.png"))

        # Verify inherited theme updates loading overlay cleanly on storage event
        loading_page.evaluate(
            """() => {
                localStorage.setItem('fluent-qt-theme', 'dark');
                window.dispatchEvent(new StorageEvent('storage', { key: 'fluent-qt-theme', newValue: 'dark' }));
            }"""
        )
        loading_page.wait_for_function("() => document.documentElement.dataset.theme === 'dark'", timeout=5000)
        loading_page.wait_for_timeout(180)
        loading_page.screenshot(path=str(output_dir / "studio-loading-dark.png"))
        for _ in range(50):
            if held_routes:
                break
            loading_page.wait_for_timeout(100)
        if not held_routes:
            raise RuntimeError("Expected WASM GET request to be intercepted during loading verification")
        if loading_errors:
            raise RuntimeError(f"Loading state page errors: {loading_errors}")
    finally:
        for route in held_routes:
            try:
                route.abort()
            except Exception:
                pass
        try:
            loading_page.unroute_all(behavior="ignoreErrors")
        except Exception:
            pass
        loading_context.close()


def verify_platform_shortcuts(browser, base_url: str) -> None:
    for platform, lang, modifier, hint, redo in (
        ("Windows", "en", "Control", "Ctrl+K", "Ctrl+Shift+Z"),
        ("Linux", "zh", "Control", "Ctrl+K", "Ctrl+Shift+Z"),
        ("macOS", "en", "Meta", "⌘K", "⇧⌘Z"),
    ):
        page = browser.new_page()
        try:
            page.add_init_script(f"""Object.defineProperty(navigator, 'userAgentData',
                {{value: {{platform: {json.dumps(platform)}}}}});
                localStorage.setItem('fluent-qt-studio-intro-v1', 'seen');""")
            page.goto(base_url + f"studio/?renderer=schematic&lang={lang}")
            page.wait_for_selector('#runtime-badge[data-state="schematic"]')
            assert page.locator("#btn-ai kbd").inner_text() == hint, platform
            assert redo in page.locator("#btn-redo").get_attribute("title"), platform
            page.keyboard.press(modifier + "+k")
            page.locator("#ai-modal:not([hidden])").wait_for()
        finally:
            page.close()


def run_schematic_checks(page, base_url: str, output_dir: Path) -> None:
    text = "Review <em>literal</em> & diagrams"
    document = {"schemaVersion": 1, "root": {"id": "root", "type": "VBox", "children": [
        {"id": "caption", "type": "TitleBar", "props": {"title": text}},
        {"id": "shell", "type": "Window", "props": {"title": text}},
        {"id": "qml", "type": "QmlView", "props": {"contextText": text}},
    ]}}
    token = base64.urlsafe_b64encode(json.dumps(document).encode()).decode().rstrip("=")
    page.goto(base_url + "studio/?renderer=schematic#state=v1." + token)
    stage = page.locator("#schematic-stage")
    stage.get_by_text(text, exact=True).first.wait_for()
    if stage.locator("em").count() or stage.inner_text().count(text) != 3:
        raise RuntimeError("Shared Studio captions must remain literal text")
    page.goto(base_url + "studio/?lang=zh&renderer=schematic", wait_until="domcontentloaded")
    page.wait_for_selector('#runtime-badge[data-state="schematic"]', timeout=10000)
    page.wait_for_selector('#schematic-stage > .sch-anchor-box [data-sch-id="button_1"]', timeout=10000)
    if not page.locator("#wasm-loader").is_hidden():
        raise RuntimeError("#wasm-loader should stay hidden in schematic mode")
    if page.locator(".palette-item").count() != 103:
        raise RuntimeError(f"Expected 103 palette items in schematic mode, got {page.locator('.palette-item').count()}")

    shell_box = page.locator("#canvas-shell").bounding_box()
    sch_btn_box = page.locator('[data-sch-id="button_1"]').bounding_box()
    if not shell_box or not sch_btn_box:
        raise RuntimeError("Missing schematic starter geometry")
    if abs((sch_btn_box["x"] + sch_btn_box["width"] / 2) - (shell_box["x"] + shell_box["width"] / 2)) > 16:
        raise RuntimeError(f"Expected schematic starter button_1 centered horizontally: shell={shell_box}, btn={sch_btn_box}")
    if not page.locator("#tour-layer").is_visible():
        raise RuntimeError("Expected first-visit #tour-layer to appear directly on clean starter canvas")

    # Test Figma live WYSIWYG pointer drag-and-drop in schematic mode (including mid-drag preview + drag_to)
    ensure_palette_category_open(page, "basic-input")
    before = page.locator("#outline-tree .tree-row").count()
    btn_item = page.locator('.palette-item[data-type="Button"]')
    btn_item.scroll_into_view_if_needed()
    b_box = btn_item.bounding_box()
    ov_box = page.locator("#edit-overlay").bounding_box()
    if b_box and ov_box:
        page.mouse.move(b_box["x"] + 40, b_box["y"] + b_box["height"] / 2)
        page.mouse.down()
        page.mouse.move(ov_box["x"] + 160, ov_box["y"] + 120, steps=6)
        page.wait_for_selector("#drag-preview-box:not([hidden])", timeout=5000)
        page.wait_for_selector("#drag-ghost.is-docked", timeout=5000)
        page.keyboard.press("Escape")
        page.wait_for_function("() => document.querySelector('#drag-preview-box')?.hidden", timeout=5000)

    btn_item.drag_to(page.locator("#edit-overlay"))
    page.wait_for_function(
        "prev => document.querySelectorAll('#outline-tree .tree-row').length > prev",
        arg=before,
        timeout=8000,
    )

    # Test Undo / Redo
    page.locator("#btn-undo").click()
    if page.locator("#outline-tree .tree-row").count() != before:
        raise RuntimeError("Undo did not revert dragged node in schematic mode")
    page.locator("#btn-redo").click()
    if page.locator("#outline-tree .tree-row").count() <= before:
        raise RuntimeError("Redo did not restore dragged node in schematic mode")
    page.locator("#btn-undo").click()

    # Test Play mode interaction on starter button_1
    page.locator("#mode-play").click()
    page.locator('[data-sch-id="button_1"] button').click()
    page.wait_for_function(
        "() => document.querySelector('#event-pill').textContent.includes('button_1')",
        timeout=5000,
    )
    page.locator("#mode-edit").click()

    output_dir.mkdir(parents=True, exist_ok=True)
    page.screenshot(path=str(output_dir / "studio-schematic-light.png"))

    # Verify schematic responsive reflow across Compact (400) -> Desktop (860) on ?template=settings
    page.goto(base_url + "studio/?lang=zh&renderer=schematic&template=settings", wait_until="domcontentloaded")
    page.wait_for_selector('[data-sch-id="field_theme"]', timeout=10000)
    page.locator('.viewport-group [data-width="400"]').click()
    page.wait_for_function(
        """() => {
            const shell = document.querySelector('#canvas-shell');
            const t = document.querySelector('[data-sch-id="field_theme"]')?.getBoundingClientRect();
            const p = document.querySelector('[data-sch-id="field_port"]')?.getBoundingClientRect();
            return Boolean(shell && shell.dataset.viewportTier === 'compact' && t && p && p.top >= t.bottom - 4 && Math.abs(p.left - t.left) <= 8);
        }""",
        timeout=5000,
    )
    page.locator('.viewport-group [data-width="860"]').click()

    # Changing the artboard theme must leave the surrounding site theme alone.
    page.goto(base_url + "studio/?lang=zh&renderer=schematic", wait_until="domcontentloaded")
    page.wait_for_selector('#schematic-stage > .sch-anchor-box [data-sch-id="button_1"]', timeout=10000)
    page.locator("#artboard-bar").wait_for(state="visible", timeout=5000)
    initial_outer_theme = page.evaluate("() => document.documentElement.dataset.theme")
    page.locator("#artboard-theme-dark").click()
    page.wait_for_function(
        "() => document.querySelector('#canvas-shell')?.dataset.canvasTheme === 'dark'",
        timeout=5000,
    )
    if page.evaluate("() => document.documentElement.dataset.theme") != initial_outer_theme:
        raise RuntimeError("Artboard dark toggle leaked into outer document theme")
    page.locator("#artboard-theme-light").click()
    page.wait_for_function(
        "() => document.querySelector('#canvas-shell')?.dataset.canvasTheme === 'light'",
        timeout=5000,
    )
    page.locator('[data-canvas-node="button_1"]').click()
    page.locator("#sel-ai-refine").wait_for(state="visible", timeout=5000)
    page.locator("#sel-ai-refine").click()
    page.wait_for_selector("#ai-modal:not([hidden])", timeout=5000)
    if page.locator('.ai-mode-group [data-ai-mode="refine"]').get_attribute("aria-pressed") != "true":
        raise RuntimeError("Expected #sel-ai-refine to open AI modal directly in refine mode")
    page.locator('.ai-mode-group [data-ai-mode="create"]').click()
    if page.locator("#ai-settings-details").evaluate("el => el.open"):
        raise RuntimeError("Expected AI Config disclosure to start collapsed")
    page.screenshot(path=str(output_dir / "studio-ai-modal.png"))

    # Without Worker/Key configured, clicking Generate prompts user & auto-expands AI Config
    page.locator("#ai-prompt-input").fill("创建一个带左侧导航、延迟趋势图和节点状态表的云端监控台")
    page.locator("#btn-ai-run").click()
    page.wait_for_function(
        "() => document.querySelector('#ai-status')?.dataset.tone === 'info' && document.querySelector('#ai-settings-details')?.open",
        timeout=5000,
    )
    page.screenshot(path=str(output_dir / "studio-ai-modal-settings.png"))

    # Switch to Custom API, quick-fill DeepSeek preset, and mock OpenAI-compatible response
    page.locator('[data-ai-provider="custom"]').click()
    page.locator('[data-ai-preset="deepseek"]').click()
    page.locator("#ai-api-key").fill("sk-test-local-only")
    # One streamed page exercises navigation, charts, tables and editor controls.
    document = {
        "design": {"theme": "dark", "accentColor": "#00B7C3"},
        "root": {"id": "root", "type": "NavigationView", "children": [
            {"id": "overview", "type": "VBox", "children": [
                {"id": "areachart", "type": "AreaChart", "props": {"values": [38, 41, 36]}},
                {"id": "datagrid", "type": "DataGrid", "props": {"modelData": [["api-gateway-us", "Healthy"]]}},
                {"id": "tabs", "type": "TabView", "props": {"items": ["main.cpp", "README.md"]}},
                {"id": "files", "type": "TreeView", "props": {"items": ["src/", "  main.cpp"]}},
                {"id": "editor", "type": "TextEdit", "props": {"plainText": "int main() {}"}},
            ]},
        ]},
    }
    chunk = {
        "id": "studio-test", "object": "chat.completion.chunk", "model": "deepseek-chat",
        "choices": [{"index": 0, "delta": {"content": json.dumps(document)}, "finish_reason": "stop"}],
        "usage": {"prompt_tokens": 100, "completion_tokens": 20, "prompt_tokens_details": {"cached_tokens": 80}},
    }
    page.route("https://api.deepseek.com/v1/chat/completions", lambda route: route.fulfill(
        content_type="text/event-stream", body=f"data: {json.dumps(chunk)}\n\ndata: [DONE]\n\n",
    ))
    page.locator("#btn-ai-run").click()
    page.wait_for_function("() => document.querySelector('#ai-modal')?.hidden", timeout=5000)
    page.wait_for_selector('#schematic-stage .sch-navview', timeout=5000)
    page.wait_for_function(
        "() => document.querySelector('#canvas-shell')?.dataset.canvasTheme === 'dark'",
        timeout=5000,
    )
    if page.evaluate("() => document.documentElement.dataset.theme") != initial_outer_theme:
        raise RuntimeError("AI-generated dark design leaked into outer website theme")
    page.wait_for_selector('#schematic-stage .sch-status-pill.is-ok', timeout=5000)
    page.wait_for_selector('#schematic-stage [data-sch-id="areachart"] polygon', timeout=5000)
    grid_text = page.locator('#schematic-stage [data-sch-id="datagrid"] .sch-table').inner_text()
    if "api-gateway-us" not in grid_text or "BasicInput" in grid_text:
        raise RuntimeError(f"Expected DataGrid to render real custom modelData rows, got: {grid_text!r}")
    event_text = page.locator("#event-pill").inner_text()
    if "80%" not in event_text:
        raise RuntimeError(f"Expected cache hit rate telemetry in event pill, got: {event_text!r}")
    for selector in (".sch-tree", ".sch-textedit", ".sch-tabbar"):
        page.locator(f"#schematic-stage {selector}").wait_for(state="attached")
    page.screenshot(path=str(output_dir / "studio-ai-generated.png"))

    # Undo cleanly restores starter canvas (root + button_1)
    page.locator("#btn-undo").click()
    page.wait_for_selector('#schematic-stage > .sch-anchor-box [data-sch-id="button_1"]', timeout=5000)

    # Verify direct First-Time 4-step Spotlight Tour (no redundant welcome card), persistence, and header replay
    page.evaluate("() => { localStorage.removeItem('fluent-qt-studio-intro-v1'); localStorage.setItem('fluent-qt-theme', 'light'); }")
    page.goto(base_url + "studio/?lang=zh&renderer=schematic", wait_until="domcontentloaded")
    page.wait_for_selector("#tour-layer:not([hidden])", timeout=5000)
    if page.locator("#studio-intro-card").count() != 0:
        raise RuntimeError("Expected redundant #studio-intro-card to be removed")
    if page.locator("#tour-step-badge").inner_text().strip() != "1 / 4":
        raise RuntimeError(f"Expected tour step 1 / 4 on first visit, got {page.locator('#tour-step-badge').inner_text()!r}")
    page.wait_for_timeout(280)
    page.screenshot(path=str(output_dir / "intro-tour-step1-light.png"))

    page.locator("#btn-tour-next").click()
    if page.locator("#tour-step-badge").inner_text().strip() != "2 / 4":
        raise RuntimeError("Expected tour step 2 / 4 after clicking Next")
    page.wait_for_timeout(280)
    page.screenshot(path=str(output_dir / "intro-tour-step2.png"))
    page.locator("#btn-tour-next").click()
    if page.locator("#tour-step-badge").inner_text().strip() != "3 / 4":
        raise RuntimeError("Expected tour step 3 / 4")
    page.locator("#btn-tour-next").click()
    if page.locator("#tour-step-badge").inner_text().strip() != "4 / 4":
        raise RuntimeError("Expected tour step 4 / 4")
    page.locator("#btn-tour-next").click()
    page.wait_for_function("() => document.querySelector('#tour-layer')?.hidden", timeout=5000)
    if page.evaluate("() => localStorage.getItem('fluent-qt-studio-intro-v1')") != "seen":
        raise RuntimeError("Finishing tour did not persist fluent-qt-studio-intro-v1 = 'seen'")

    # Reloading after 'seen' keeps tour hidden, while header #btn-intro replays the tour
    page.reload(wait_until="domcontentloaded")
    page.wait_for_selector('#schematic-stage > .sch-anchor-box [data-sch-id="button_1"]', timeout=5000)
    if not page.locator("#tour-layer").is_hidden():
        raise RuntimeError("Expected #tour-layer to remain hidden on returning visit")
    page.locator("#btn-intro").click()
    page.wait_for_selector("#tour-layer:not([hidden])", timeout=5000)
    page.keyboard.press("Escape")
    page.wait_for_function("() => document.querySelector('#tour-layer')?.hidden", timeout=5000)

    # Dark theme + ?intro=1 directly enters Spotlight Tour Step 1 / 4
    page.evaluate("() => localStorage.setItem('fluent-qt-theme', 'dark')")
    page.goto(base_url + "studio/?lang=zh&renderer=schematic&intro=1", wait_until="domcontentloaded")
    page.wait_for_selector("#tour-layer:not([hidden])", timeout=5000)
    page.wait_for_timeout(280)
    page.screenshot(path=str(output_dir / "intro-tour-step1-dark.png"))
    page.keyboard.press("Escape")
    page.wait_for_function("() => document.querySelector('#tour-layer')?.hidden", timeout=5000)
    page.evaluate("() => localStorage.setItem('fluent-qt-theme', 'light')")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8765/")
    parser.add_argument("--site-root", type=Path, help="Serve this local website on an ephemeral loopback port")
    parser.add_argument("--wasm", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/site-studio-browser"))
    args = parser.parse_args()

    url = args.url if args.url.endswith("/") else args.url + "/"
    errors: list[str] = []

    with ExitStack() as stack:
        if args.site_root:
            server = ThreadingHTTPServer(("127.0.0.1", 0), partial(SimpleHTTPRequestHandler, directory=args.site_root))
            stack.callback(server.server_close)
            Thread(target=server.serve_forever, daemon=True).start()
            stack.callback(server.shutdown)
            url = f"http://127.0.0.1:{server.server_port}/"
        p = stack.enter_context(sync_playwright())
        browser = p.chromium.launch()
        context = browser.new_context(viewport={"width": 1440, "height": 900})
        page = context.new_page()
        page.on("pageerror", lambda err: errors.append(f"pageerror: {err}"))
        page.on("console", lambda msg: errors.append(f"console.{msg.type}: {msg.text}") if msg.type == "error" else None)

        if args.wasm:
            verify_loading_state(browser, url, args.output)
            run_wasm_checks(page, url, args.output)
        else:
            verify_platform_shortcuts(browser, url)
            run_schematic_checks(page, url, args.output)

        browser.close()

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Studio browser verification passed ({'WASM' if args.wasm else 'schematic'})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
