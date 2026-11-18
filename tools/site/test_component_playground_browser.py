#!/usr/bin/env python3
"""Exercise the documentation controls against the actual Qt WebAssembly button."""

import argparse
import importlib.util
from contextlib import contextmanager
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading
from urllib.parse import parse_qs, urljoin, urlsplit

ROOT = Path(__file__).resolve().parents[2]


@contextmanager
def website_server(wasm_root, site_root=ROOT / "site"):
    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def translate_path(self, path):
            if urlsplit(path).path.startswith("/gallery/"):
                previous = self.directory
                self.directory = str(wasm_root / "app")
                try:
                    return super().translate_path(path[len("/gallery"):])
                finally:
                    self.directory = previous
            return super().translate_path(path)

        def end_headers(self):
            self.send_header("Cache-Control", "no-store")
            super().end_headers()

    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(Handler, directory=str(site_root)))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{server.server_port}/"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


def load_audit():
    spec = importlib.util.spec_from_file_location('playground_interaction_audit', Path(__file__).with_name('playground_interaction_audit.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_loading_states(browser, base_url, output, screenshots=False):
    """Delay/fail the independent runtime without changing the real sample."""
    from playwright.sync_api import expect

    output.mkdir(parents=True, exist_ok=True)
    context = browser.new_context(viewport={'width': 1440, 'height': 1000}, color_scheme='light')
    page = context.new_page()
    errors, held, checks = [], [], []
    mode = 'hold'
    page.on('pageerror', lambda error: errors.append(str(error)))

    def runtime_request(route):
        query = parse_qs(urlsplit(route.request.url).query)
        if query.get('playground') == ['interactive'] and route.request.is_navigation_request():
            if mode == 'hold':
                held.append(route)
                return
            if mode == 'fail':
                route.fulfill(status=503, content_type='text/html', body='<html>Service unavailable</html>')
                return
        route.continue_()

    page.route('**/gallery/?**', runtime_request)
    popup = page.locator('.playground-popover')

    def screenshot(name):
        if screenshots:
            page.screenshot(path=str(output / f'{name}.png'))

    def assert_loading():
        expect(popup).to_have_attribute('data-preview-state', 'loading')
        expect(popup).to_have_attribute('aria-busy', 'true')
        expect(page.locator('.playground-preview-state')).to_have_text('正在加载预览…重新尝试')
        expect(page.locator('.playground-preview-spinner')).to_be_visible()
        expect(page.locator('.playground-preview img')).to_have_count(0)
        expect(page.locator('.playground-status')).to_be_hidden()
        expect(page.locator('.playground-category-tabs')).to_be_hidden()
        expect(page.locator('.playground-category-panels')).to_be_hidden()
        expect(page.locator('.playground-code-text')).to_be_empty()
        expect(page.locator('.playground-copy')).to_be_disabled()
        expect(page.locator('.playground-fields-placeholder')).to_be_visible()

    try:
        page.goto(urljoin(base_url, 'zh-CN/api/button/'))
        page.locator('[data-component]').scroll_into_view_if_needed()
        expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
        original = page.locator('.live-stage iframe').element_handle().content_frame()
        original.wait_for_function('document.documentElement.dataset.fluentQtComponentState')
        original_before = original.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')
        page.locator('[data-playground-toggle]').click()
        expect(page.locator('.playground-live-frame')).to_have_count(1)
        assert_loading()
        screenshot('loading-light')
        page.emulate_media(color_scheme='dark')
        expect(popup).to_have_attribute('data-theme', 'dark')
        screenshot('loading-dark')
        page.emulate_media(color_scheme='light', reduced_motion='reduce')
        assert page.locator('.playground-preview-spinner').evaluate('(el)=>getComputedStyle(el).animationName') == 'none'
        checks.append('reduced-motion-static-indicator')
        page.set_viewport_size({'width': 390, 'height': 844})
        page.wait_for_function('''() => {
          const rect = document.querySelector('.playground-popover').getBoundingClientRect();
          return rect.left >= 0 && rect.right <= innerWidth;
        }''')
        screenshot('loading-narrow')
        box = popup.bounding_box()
        assert box['x'] >= 0 and box['x'] + box['width'] <= 390
        page.locator('[data-view="code"]').click()
        expect(page.locator('.playground-code-placeholder')).to_be_visible()
        screenshot('loading-narrow-code')
        page.set_viewport_size({'width': 1440, 'height': 1000})
        page.emulate_media(forced_colors='active')
        expect(popup).to_have_attribute('data-theme', 'high-contrast')
        screenshot('loading-high-contrast')
        page.emulate_media(forced_colors='none', reduced_motion='no-preference')
        mode = 'pass'
        for route in held:
            route.continue_()
        held.clear()
        expect(popup).to_have_attribute('data-preview-state', 'ready', timeout=60000)
        expect(page.locator('.playground-copy')).to_be_enabled()
        expect(page.locator('.playground-preview-state')).to_be_hidden()
        expect(page.locator('.playground-code-placeholder')).to_be_hidden()
        screenshot('ready')
        checks.append('delayed-load-ready-and-responsive-themes')

        # Reopening must not expose stale fields/code while a new runtime starts.
        page.locator('#playground-text').fill('Previous preview')
        expect(page.locator('.playground-code-text')).to_contain_text('Previous preview')
        page.locator('.playground-close').click()
        mode = 'hold'
        page.locator('[data-playground-toggle]').click()
        expect(page.locator('.playground-live-frame')).to_have_count(1)
        assert_loading()
        page.keyboard.press('Escape')
        expect(popup).to_be_hidden()
        for route in held:
            route.abort()
        held.clear()
        expect(page.locator('[data-playground-toggle]')).to_be_focused()
        checks.append('reopen-clears-stale-content-and-close-during-load')

        mode = 'fail'
        page.locator('[data-playground-toggle]').click()
        expect(popup).to_have_attribute('data-preview-state', 'error', timeout=15000)
        expect(page.locator('.playground-preview-spinner')).to_be_hidden()
        expect(page.locator('.playground-preview-retry')).to_be_visible()
        expect(page.locator('.playground-preview-state')).to_contain_text('预览暂时无法加载')
        expect(page.locator('.playground-live-frame')).to_have_count(0)
        screenshot('load-error')
        mode = 'pass'
        page.locator('.playground-preview-retry').click()
        expect(popup).to_have_attribute('data-preview-state', 'ready', timeout=60000)
        expect(page.locator('#playground-text')).not_to_have_value('Previous preview')
        checks.append('failed-document-retry-ready')
        original_after = original.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')
        assert original_after['controls'] == original_before['controls'], 'original controls changed'
        assert original_after['clicks'] == original_before['clicks'], 'original click count changed'
        assert not errors, errors
        (output / 'loading-result.json').write_text(json.dumps({'passed': checks, 'pageErrors': errors}, indent=2) + '\n')
    finally:
        context.close()


def run_preview_entries(browser, base_url):
    from playwright.sync_api import expect

    page = browser.new_page(viewport={'width': 1280, 'height': 850})
    errors, scripts = [], []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.on('request', lambda request: scripts.append(request.url)
            if urlsplit(request.url).path.endswith('/playground.js') else None)
    try:
        page.goto(urljoin(base_url, 'api/qml-view/'))
        expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=60000)
        assert not scripts, 'Reading an example must not load the editor'
        trigger = page.locator('.document-heading [data-playground-toggle]')
        popup = page.locator('.playground-popover')
        page.route('**/playground.js*', lambda route: route.abort())
        trigger.click()
        expect(page.locator('[data-preview-error]')).to_be_visible()
        expect(trigger).to_have_attribute('aria-busy', 'false')
        page.unroute('**/playground.js*')
        trigger.press('Enter')
        expect(popup).to_have_attribute('data-preview-state', 'ready', timeout=60000)
        title = page.locator('[data-property="contextTitle"] input')
        title.fill('QML preview test')
        title.press('Tab')
        code = page.locator('.playground-code-text')
        expect(code).to_contain_text('QML preview test')
        page.locator('[data-property="source"] [role="combobox"]').click()
        page.get_by_role('option', name='Action.qml', exact=False).click()
        expect(code).to_contain_text('function advance()')
        page.locator('.playground-reset').click()
        expect(title).to_have_value('A shared title')
        expect(code).to_contain_text('property string previewTitle')
        page.keyboard.press('Escape')
        expect(trigger).to_be_focused()
        trigger.press('Enter')
        expect(popup).to_have_attribute('data-preview-state', 'ready', timeout=60000)
        assert len(scripts) == 2, scripts  # One failed request and one successful retry.

        page.set_viewport_size({'width': 320, 'height': 400})
        for route, suffix in (('checkbox', ''), ('checkbox', '?playground=1'), ('button', '?playground=1')):
            page.goto(urljoin(base_url, f'zh-CN/api/{route}/{suffix}'))
            if not suffix:
                page.locator('[data-playground-toggle]').click()
            expect(popup).to_have_attribute('data-preview-state', 'ready', timeout=60000)
            page.locator('.playground-close').click()
            expect(page.locator('[data-playground-toggle]')).to_be_focused()
        assert not errors, errors
    finally:
        page.close()


def run(browser, base_url, output, screenshots=False):
    spec = importlib.util.spec_from_file_location('playground_layout', Path(__file__).with_name('test_playground_layout_browser.py'))
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
    layout.run(browser, base_url, output / 'layout', screenshots)
    run_loading_states(browser, base_url, output / 'loading', screenshots)
    run_preview_entries(browser, base_url)
    return load_audit().run(browser, base_url, output, screenshots, routes=['button'])


def main():
    from playwright.sync_api import sync_playwright
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wasm-root', type=Path, default=ROOT / 'build/wasm')
    parser.add_argument('--url')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/component-playground/browser')
    parser.add_argument('--screenshots', action='store_true')
    args = parser.parse_args()
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True, ignore_default_args=['--hide-scrollbars'])
        try:
            if args.url:
                run(browser, args.url, args.output, args.screenshots)
            else:
                with website_server(args.wasm_root) as url:
                    run(browser, url, args.output, args.screenshots)
        finally:
            browser.close()


if __name__ == '__main__':
    main()
