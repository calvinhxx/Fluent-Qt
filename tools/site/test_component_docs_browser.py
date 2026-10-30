#!/usr/bin/env python3
"""Browser acceptance for the static Button/CheckBox documentation."""

import argparse
import json
import re
from pathlib import Path
from urllib.parse import urljoin

from playwright.sync_api import sync_playwright, expect


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8766/")
    parser.add_argument("--output", type=Path, default=Path("build/component-docs/browser"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    requests, errors, results, loading = [], [], [], []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        context = browser.new_context(viewport={"width": 1440, "height": 900}, device_scale_factor=1,
                                      permissions=["clipboard-read", "clipboard-write"])
        unavailable = lambda route: route.fulfill(status=503, content_type='text/html', body='<h1>Unavailable</h1>')
        runtime_url = re.compile('^' + re.escape(urljoin(args.url, 'gallery/')))
        context.route(runtime_url, unavailable)
        page = context.new_page()
        context.on("request", lambda request: requests.append(request.url))
        page.on("pageerror", lambda error: errors.append(str(error)))
        for component in ("button", "checkbox"):
            for language in ("en", "zh"):
                path = ("zh-CN/" if language == "zh" else "") + f"api/{component}/"
                response = page.goto(urljoin(args.url, path))
                assert response.status == 200
                expect(page.locator("h1")).to_contain_text("Button" if component == "button" else "CheckBox")
                expect(page.locator("html")).to_have_attribute("lang", "zh-CN" if language == "zh" else "en")
                expect(page.locator('.documentation-nav a')).to_have_text(['官网', '文档'] if language == 'zh' else ['Home', 'Documentation'])
                expect(page.locator('.documentation-header button')).to_have_count(0)
                expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'error')
                expect(page.locator('[data-retry-live]')).to_be_visible()
                page.wait_for_function("[...document.querySelectorAll('.preview-panel:not([hidden]) img')].every(i => i.complete && i.naturalWidth > 0)")
                assert page.locator("iframe, canvas").count() == 0
                dark = "深色" if language == "zh" else "Dark"
                light = "浅色" if language == "zh" else "Light"
                document_theme = page.locator('html').get_attribute('data-theme')
                saved_theme = page.evaluate("localStorage.getItem('fluent-qt-theme')")
                background = page.locator('body').evaluate('element => getComputedStyle(element).backgroundColor')
                page.get_by_role("button", name=dark, exact=True).click()
                expect(page.locator('[data-component]')).to_have_attribute('data-preview-theme', 'dark')
                expect(page.locator('html')).to_have_attribute('data-theme', document_theme)
                expect(page.locator('body')).to_have_css('background-color', background)
                assert page.evaluate("localStorage.getItem('fluent-qt-theme')") == saved_theme
                page.wait_for_function("document.querySelector('.preview-panel:not([hidden]) img').currentSrc.includes('-dark.png')")
                tab = page.locator('.state-tabs [role="tab"]').first
                tab.focus()
                tab.press("ArrowRight")
                expect(page.locator('.state-tabs [role="tab"]').nth(1)).to_have_attribute("aria-selected", "true")
                code = page.locator(".usage-example").first
                code.get_by_role("tab", name="PySide6", exact=True).click()
                displayed = code.locator('[role="tabpanel"]:not([hidden]) code').inner_text()
                code.locator("[data-copy]").click()
                expect(page.locator(".copy-status")).to_be_visible()
                expect(page.locator(".copy-status")).to_have_text(
                    page.locator("body").get_attribute("data-copy-success"))
                # Windows clipboard text uses CRLF; retain exact code comparison in LF form.
                copied = page.evaluate("navigator.clipboard.readText()").replace("\r\n", "\n")
                assert copied == displayed
                expect(page.locator('a[download], .imports, .run-instructions')).to_have_count(0)
                assert displayed.startswith('import fluentqt')
                if component == "button" and language == "en":
                    page.evaluate("scrollTo(0,0)")
                    page.screenshot(path=str(args.output / "button-en-dark.png"))
                page.get_by_role("button", name=light, exact=True).click()
                expect(page.locator('html')).to_have_attribute('data-theme', document_theme)
                page.wait_for_function("document.querySelector('.preview-panel:not([hidden]) img').dataset.imageReady === 'light'")
                if language == "zh":
                    if component == "checkbox":
                        page.get_by_role("tab", name="半选", exact=True).click()
                        page.wait_for_function("document.querySelector('#state-mixed img').dataset.imageReady === 'light'")
                    page.evaluate("scrollTo(0,0)")
                    page.screenshot(path=str(args.output / f"{component}-zh-desktop.png"))
                results.append(path)
        page.goto(urljoin(args.url, "api/?lang=zh&q=复选框"))
        expect(page.locator("h1")).to_have_text("CheckBox 复选框")
        expect(page.locator('[data-directory-entry]:not([hidden])')).to_have_count(1)
        expect(page.locator('[data-directory-search]')).to_have_value('复选框')
        page.locator('[data-directory-search]').fill('ChartView')
        expect(page.locator('[data-component-id="chart-view"]')).to_be_visible()
        page.locator('[data-directory-search]').fill('no-such-component-xyz')
        expect(page.locator('[data-directory-status]')).to_have_text('没有匹配的文档')
        page.locator('[data-directory-search]').press('Escape')
        expect(page.locator('[data-directory-search]')).to_have_value('')
        page.locator('[data-shell-link="home"]').click()
        expect(page.locator('[data-i18n="gallery.codeCopy"]')).to_have_text('切换查看 C++ / Python 代码')
        page.locator('.language-switch a[lang="en"]').click()
        page.locator('header [data-i18n="nav.api"]').click()
        expect(page).to_have_url(urljoin(args.url, "api/foundation/"))
        expect(page.locator('html')).to_have_attribute('lang', 'en')
        page.goto(urljoin(args.url, "zh-CN/api/"))
        expect(page).to_have_url(urljoin(args.url, "zh-CN/api/foundation/"))
        page.locator('[data-directory-search]').fill('复选框')
        page.locator('[data-directory-search]').press('Enter')
        expect(page).to_have_url(urljoin(args.url, "zh-CN/api/checkbox/"))
        page.screenshot(path=str(args.output / "directory-zh-desktop.png"))
        mobile = context.new_page()
        mobile.set_viewport_size({"width": 390, "height": 844})
        mobile.on("pageerror", lambda error: errors.append(str(error)))
        mobile.goto(urljoin(args.url, "zh-CN/api/checkbox/"))
        mobile.get_by_role("button", name="深色", exact=True).click()
        mobile.get_by_role("tab", name="半选", exact=True).click()
        mobile.wait_for_function("document.querySelector('#state-mixed img').dataset.imageReady === 'dark'")
        assert mobile.evaluate("document.documentElement.scrollWidth <= innerWidth + 1")
        mobile.screenshot(path=str(args.output / "checkbox-zh-mobile-dark.png"))
        mobile.goto(urljoin(args.url, "api/?lang=zh&q=按钮"))
        expect(mobile.locator('h1')).to_have_text('Button 按钮')
        expect(mobile.locator('.documentation-directory')).to_have_attribute('open', '')
        mobile.locator('[data-directory-search]').fill('复选框')
        mobile.locator('[data-directory-search]').press('Enter')
        expect(mobile.locator('h1')).to_have_text('CheckBox 复选框')
        expect(mobile.locator('.documentation-directory')).not_to_have_attribute('open', '')
        assert mobile.evaluate("document.documentElement.scrollWidth <= innerWidth + 1")
        mobile.locator('.documentation-directory > summary').click()
        expect(mobile.locator('[data-directory-search]')).to_be_visible()
        mobile.screenshot(path=str(args.output / 'directory-zh-mobile.png'))
        # The global controls stay on the homepage. Documents inherit their preference.
        preference_context = browser.new_context(color_scheme='light')
        preference_context.route(runtime_url, unavailable)
        home = preference_context.new_page()
        home.goto(urljoin(args.url, 'zh-CN/'))
        home.locator('[data-theme-toggle]').click()
        home.locator('[data-theme-toggle]').click()
        expect(home.locator('html')).to_have_attribute('data-theme', 'high-contrast')
        home.locator('header [data-i18n="nav.api"]').click()
        home.locator('[data-directory-search]').fill('Button')
        home.locator('[data-directory-search]').press('Enter')
        expect(home.locator('html')).to_have_attribute('data-theme', 'high-contrast')
        expect(home.locator('body')).to_have_css('background-color', 'rgb(0, 0, 0)')
        home.wait_for_function("document.querySelector('.preview-panel:not([hidden]) img').dataset.imageReady === 'dark'")
        home.screenshot(path=str(args.output / 'button-inherited-high-contrast.png'))
        settings = preference_context.new_page()
        settings.goto(urljoin(args.url, 'zh-CN/'))
        settings.locator('[data-theme-toggle]').click()
        expect(home.locator('html')).to_have_attribute('data-theme', 'light')
        settings.locator('[data-theme-toggle]').click()
        expect(home.locator('html')).to_have_attribute('data-theme', 'dark')
        home.emulate_media(forced_colors='active')
        expect(home.locator('html')).to_have_attribute('data-theme', 'high-contrast')
        expect(home.locator('button[data-preview-theme="light"]')).to_be_disabled()
        home.emulate_media(forced_colors='none')
        expect(home.locator('html')).to_have_attribute('data-theme', 'dark')
        settings.evaluate("localStorage.removeItem('fluent-qt-theme')")
        expect(home.locator('html')).to_have_attribute('data-theme', 'light')
        home.emulate_media(color_scheme='dark')
        expect(home.locator('html')).to_have_attribute('data-theme', 'dark')
        preference_context.close()
        static = browser.new_context(java_script_enabled=False, color_scheme="dark", viewport={"width": 1280, "height": 900})
        offline = static.new_page()
        offline.goto(urljoin(args.url, "zh-CN/api/button/"))
        expect(offline.locator("h1")).to_have_text("Button 按钮")
        expect(offline.locator(".preview-panel img").first).to_be_visible()
        expect(offline.locator("pre").first).to_be_visible()
        for path in ("api/?lang=zh", "zh-CN/api/button/", "zh-CN/api/checkbox/"):
            cold = browser.new_context(color_scheme="light")
            cold.route(runtime_url, unavailable)
            cold.on("request", lambda request: requests.append(request.url))
            first_load = cold.new_page()
            first_load.goto(urljoin(args.url, path), wait_until="networkidle")
            size = first_load.evaluate("""[...performance.getEntriesByType('navigation'),
                ...performance.getEntriesByType('resource')].reduce((sum, entry) => sum + entry.decodedBodySize, 0)""")
            assert 0 < size < 300_000, (path, size)
            contrast = first_load.locator('.directory-groups a[aria-current="page"]').first.evaluate("""element => {
                const style = getComputedStyle(element);
                const luminance = value => {
                    const rgb = value.match(/[\\d.]+/g).slice(0, 3).map(Number).map(v => v / 255)
                        .map(v => v <= .04045 ? v / 12.92 : ((v + .055) / 1.055) ** 2.4);
                    return rgb[0] * .2126 + rgb[1] * .7152 + rgb[2] * .0722;
                };
                const [dark, light] = [luminance(style.color), luminance(style.backgroundColor)].sort((a, b) => a - b);
                return (light + .05) / (dark + .05);
            }""")
            assert contrast >= 4.5, (path, contrast)
            loading.append({"path": path, "bytes": size, "active_text_contrast": round(contrast, 2)})
            cold.close()
        assert not any(".wasm" in request or "fluent_qt_gallery.js" in request for request in requests)
        assert not errors, errors
        static.close()
        context.close()
        browser.close()
    result = {"status": "pass", "pages": results, "checks": ["native images", "isolated light/dark previews without changing document or saved theme", "keyboard states",
              "C++/PySide6 code tabs", "exact clipboard copy", "no source download or build panels", "Chinese search",
              "homepage language inheritance", "two-link header", "shared theme including high contrast and system preferences",
              "390px layout and directory", "legacy entry redirects", "no-JavaScript reading", "offline runtime fallback",
              "document assets under 300 KB without runtime", "active text contrast"], "loading": loading, "page_errors": errors}
    (args.output / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print("Static component documentation browser acceptance passed")


if __name__ == "__main__":
    main()
