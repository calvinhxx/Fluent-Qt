#!/usr/bin/env python3
"""Check the website-to-document journey and physical input in real Qt WASM examples."""

import argparse
import json
from io import BytesIO
from pathlib import Path
from urllib.parse import urljoin

from PIL import Image
from playwright.sync_api import sync_playwright, expect
from test_component_focus_browser import refresh_state, settle_render


def native_state(frame):
    frame.wait_for_function("document.documentElement.dataset.fluentQtComponentState")
    return frame.evaluate("JSON.parse(document.documentElement.dataset.fluentQtComponentState)")


def click_control(page, frame, index=0):
    control = native_state(frame)["controls"][index]
    rectangle = page.locator('.live-stage iframe').bounding_box()
    page.mouse.click(rectangle["x"] + control["x"] + control["width"] / 2,
                     rectangle["y"] + control["y"] + control["height"] / 2)


def wait_state(frame, state):
    frame.wait_for_function("state => JSON.parse(document.documentElement.dataset.fluentQtComponentState || '{}').state === state", arg=state)


def wait_example(page):
    expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
    frame = page.locator('.live-stage iframe').element_handle().content_frame()
    native_state(frame)
    return frame


def check_radio_button_previews(browser, url, output):
    """Selection keeps geometry stable and rings clear at fractional pixel ratios."""
    for scale in (1, 1.25, 1.5, 2):
        context = browser.new_context(viewport={'width': 1664, 'height': 1000}, device_scale_factor=scale)
        page = context.new_page()
        page.goto(urljoin(url, 'zh-CN/api/radio-button/'))
        frame = wait_example(page)
        iframe = page.locator('.live-stage iframe')
        selection = {'route': 'radio-button', 'sample': 'radio-button-group', 'state': 'basic'}
        settle_render(frame)
        baseline = refresh_state(page, frame, selection)
        def geometry(state):
            return [(control['text'], control['x'], control['y'], control['width'], control['height'])
                    for control in state['controls']]
        positions = geometry(baseline)
        for theme in ('light', 'dark'):
            page.locator(f'button[data-preview-theme="{theme}"]').click()
            frame.wait_for_function('theme => document.documentElement.dataset.fluentQtGalleryHostTheme === theme', arg=theme)
            page.mouse.move(20, 20)
            pixels = Image.open(BytesIO(iframe.screenshot())).convert('RGB')
            medium = next(c for c in native_state(frame)['controls'] if c['text'] == 'Medium')
            # A complete ring must not paint into the widget's first two logical pixels.
            y = round((medium['y'] + medium['height'] / 2) * scale)
            background = pixels.getpixel((round((medium['x'] - 3) * scale), y))
            for offset in (0, 1):
                edge = pixels.getpixel((round((medium['x'] + offset) * scale), y))
                assert max(abs(a - b) for a, b in zip(edge, background)) <= 2, (scale, theme, edge, background)
            assert medium['height'] > 20, (scale, medium)
            for text in ('High', 'Low', 'Medium', 'Medium', 'High'):
                control = next(c for c in native_state(frame)['controls'] if c['text'] == text)
                bounds = iframe.bounding_box()
                page.mouse.click(bounds['x'] + control['x'] + 14,
                                 bounds['y'] + control['y'] + control['height'] / 2)
                frame.wait_for_function('text => JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls.some(c => c.text === text && c.checked)', arg=text)
                settle_render(frame)
                state = refresh_state(page, frame, selection)
                assert geometry(state) == positions, (scale, theme, text, positions, geometry(state))
                assert state['contentWidth'] == baseline['contentWidth']
        low = next(c for c in native_state(frame)['controls'] if c['text'] == 'Low')
        bounds = iframe.bounding_box()
        page.mouse.click(bounds['x'] + low['x'] + 14, bounds['y'] + low['y'] + low['height'] / 2)
        frame.wait_for_function("JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls[0].checked")
        page.keyboard.press('ArrowDown')
        frame.wait_for_function("JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls[1].checked")
        page.keyboard.press('Space')
        frame.wait_for_function("JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls.filter(c => c.checked).length === 1")
        page.mouse.move(20, 20)
        frame.wait_for_timeout(250)
        iframe.screenshot(path=str(output / f'radio-focus-{scale}x.png'))
        context.close()


def check_spatial_startup_recovery(browser, url, output):
    """A failed controller has a visible retry which starts the real Qt sample."""
    for locale, retry_text in [('zh-CN/', '重试'), ('', 'Retry')]:
        context = browser.new_context(viewport={'width': 1440, 'height': 960})
        try:
            page = context.new_page()
            pattern = '**/spatial-showcase.js?*'
            page.route(pattern, lambda route: route.abort())
            page.goto(urljoin(url, locale + '#spatial'))
            root = page.locator('[data-spatial-showcase]')
            expect(root).to_have_attribute('data-showcase-state', 'error')
            retry = root.locator('[data-showcase-start]')
            expect(retry).to_be_visible()
            expect(retry).to_have_text(retry_text)
            assert page.locator('[data-showcase-frame]').get_attribute('src') is None
            page.unroute(pattern)
            retry.click()
            expect(root).to_have_attribute('data-showcase-state', 'ready', timeout=90000)
            expect(retry).to_be_hidden()
            frame = page.locator('[data-showcase-frame]').element_handle().content_frame()
            frame.wait_for_function("document.documentElement.dataset.fluentQtShowcase === 'spatial'")
            page.locator('[data-showcase-stop]').click()
            expect(root).to_have_attribute('data-showcase-state', 'waiting')
            assert page.locator('[data-showcase-frame]').get_attribute('src') is None
            retry.click()
            expect(root).to_have_attribute('data-showcase-state', 'ready', timeout=90000)
            page.locator('[data-showcase-frame]').scroll_into_view_if_needed()
            page.screenshot(path=str(output / f'spatial-recovered-{locale.rstrip("/") or "en"}.png'))
        finally:
            context.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8766/')
    parser.add_argument('--output', type=Path, default=Path('build/component-docs/journey'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    checks, errors, requests = [], [], []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        context = browser.new_context(locale='en-US', viewport={'width': 1440, 'height': 960})
        page = context.new_page()
        page.on('pageerror', lambda error: errors.append(str(error)))
        context.on('request', lambda request: requests.append(request.url))
        page.goto(urljoin(args.url, 'zh-CN/'))
        expect(page.locator('header [data-i18n="nav.components"]')).to_have_count(0)
        assert not page.locator('a[href]').evaluate_all("links => links.some(a => new URL(a.href).pathname === '/gallery/')")
        page.locator('header [data-i18n="nav.api"]').click()
        expect(page).to_have_url(urljoin(args.url, 'zh-CN/api/foundation/'))
        expect(page.locator('.documentation-nav a')).to_have_text(['官网', '文档'])
        expect(page.locator('.documentation-header button')).to_have_count(0)
        expect(page.locator('[data-shell-link="components"]')).to_have_attribute('aria-current', 'page')
        page.locator('[data-shell-link="home"]').click()
        expect(page).to_have_url(urljoin(args.url, 'zh-CN/'))
        page.locator('header [data-i18n="nav.api"]').click()
        page.locator('[data-shell-link="home"]').click()
        page.locator('header [data-i18n="nav.downloads"]').click()
        assert page.url.endswith('/zh-CN/#downloads')
        page.goto(urljoin(args.url, 'zh-CN/api/button/'))
        checks.append('Documentation has only Home and Documentation links; downloads remain on the homepage')
        expect(page.locator('[data-start-live]')).to_have_count(0)
        page.locator('[data-directory-search]').focus()
        frame = wait_example(page)
        expect(page.locator('[data-directory-search]')).to_be_focused()
        expect(page.locator('.capture-label')).to_have_count(0)
        before = native_state(frame)['clicks']
        click_control(page, frame, 1)
        frame.wait_for_function('before => JSON.parse(document.documentElement.dataset.fluentQtComponentState).clicks > before', arg=before)
        first_load = frame.evaluate('document.documentElement.dataset.fluentQtLoadStartedAt')
        page.get_by_role('tab', name='交互状态', exact=True).click()
        wait_state(frame, 'states')
        assert len(native_state(frame)['controls']) == 5
        assert frame.evaluate('document.documentElement.dataset.fluentQtLoadStartedAt') == first_load
        document_theme = page.locator('html').get_attribute('data-theme')
        saved_theme = page.evaluate("localStorage.getItem('fluent-qt-theme')")
        page.get_by_role('button', name='深色', exact=True).click()
        frame.wait_for_function("document.documentElement.dataset.fluentQtGalleryHostTheme === 'dark'")
        expect(page.locator('[data-component]')).to_have_attribute('data-preview-theme', 'dark')
        expect(page.locator('html')).to_have_attribute('data-theme', document_theme)
        assert page.evaluate("localStorage.getItem('fluent-qt-theme')") == saved_theme
        assert frame.evaluate('document.documentElement.dataset.fluentQtLoadStartedAt') == first_load
        page.screenshot(path=str(args.output / 'button-live-dark.png'))
        checks.append('Button starts on visibility, preserves document focus, and reuses the runtime across states and themes')
        page.goto(urljoin(args.url, 'zh-CN/api/checkbox/'))
        frame = wait_example(page)
        expect(page.locator('[data-component]')).to_have_attribute('data-preview-theme', document_theme)
        frame.wait_for_function('theme => document.documentElement.dataset.fluentQtGalleryHostTheme === theme', arg=document_theme)
        click_control(page, frame)
        frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls[0].checkState === 2')
        page.keyboard.press('Space')
        frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls[0].checkState === 0')
        page.get_by_role('tab', name='半选', exact=True).click()
        wait_state(frame, 'mixed')
        assert native_state(frame)['controls'][0]['checkState'] == 1
        page.get_by_role('tab', name='禁用', exact=True).click()
        wait_state(frame, 'disabled')
        assert not native_state(frame)['controls'][0]['enabled']
        click_control(page, frame)
        assert native_state(frame)['controls'][0]['checkState'] == 0
        page.get_by_role('tab', name='全选组合', exact=True).click()
        wait_state(frame, 'select-all')
        click_control(page, frame)
        frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).controls.every(c => c.checkState === 2)')
        page.get_by_role('button', name='浅色', exact=True).click()
        frame.wait_for_function("document.documentElement.dataset.fluentQtGalleryHostTheme === 'light'")
        page.screenshot(path=str(args.output / 'checkbox-live-light.png'))
        checks.append('CheckBox physical click, Space, mixed state, disabled state and select-all')
        checks.append('Preview theme affects only its Qt runtime, does not persist, and resets on document navigation')
        page.set_viewport_size({'width': 390, 'height': 844})
        page.get_by_role('tab', name='键盘焦点', exact=True).click()
        wait_state(frame, 'focus')
        click_control(page, frame)
        page.keyboard.press('Space')
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth + 1')
        page.screenshot(path=str(args.output / 'checkbox-live-mobile.png'))
        page.locator('#usage').scroll_into_view_if_needed()
        page.locator('#appearance').scroll_into_view_if_needed()
        assert page.locator('.live-stage iframe').count() == 1
        checks.append('390px input and viewport suspension keep one runtime')
        unavailable = lambda route: route.fulfill(status=503, content_type='text/html', body='<h1>Unavailable</h1>')
        page.route('**/gallery/**', unavailable)
        page.reload()
        expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'error')
        expect(page.locator('.preview-stage')).to_be_visible()
        expect(page.locator('[data-retry-live]')).to_have_text('重试')
        expect(page.locator('.live-stage iframe')).to_have_count(0)
        page.unroute('**/gallery/**', unavailable)
        page.locator('[data-retry-live]').click()
        wait_example(page)
        checks.append('Unavailable runtime keeps the document readable; Retry starts the real example')
        check_radio_button_previews(browser, args.url, args.output)
        checks.append('RadioButton stable selection layout, clear edges, keyboard focus and exclusive grouping at 1x, 1.25x, 1.5x and 2x')
        # Check the homepage in a separate navigation context.
        home_context = browser.new_context(locale='en-US', viewport={'width': 1440, 'height': 960})
        landing = home_context.new_page()
        landing.on('pageerror', lambda error: errors.append(str(error)))
        landing.goto(urljoin(args.url, 'zh-CN/api/button/'))
        landing.locator('[data-shell-link="home"]').click()
        landing.locator('header [data-i18n="nav.gallery"]').click()
        expect(landing).to_have_url(urljoin(args.url, 'zh-CN/#gallery'))
        expect(landing.locator('[data-live-gallery]')).to_have_attribute('data-gallery-state', 'ready', timeout=90000)
        gallery = landing.locator('[data-gallery-frame]').element_handle().content_frame()
        gallery.wait_for_function("document.documentElement.dataset.fluentQtLoaded === 'true'")
        landing.locator('[data-gallery-frame]').scroll_into_view_if_needed()
        # GalleryWindow intentionally holds its startup splash for at least 1400 ms.
        # Include the following presentation transition in the visual capture.
        gallery.wait_for_timeout(2500)
        landing.screenshot(path=str(args.output / 'homepage-gallery.png'))
        expect(landing.locator('[data-gallery-load]')).to_be_hidden()
        landing.locator('#spatial').scroll_into_view_if_needed()
        expect(landing.locator('[data-spatial-showcase]')).to_have_attribute('data-showcase-state', 'ready', timeout=90000)
        expect(landing.locator('[data-showcase-start]')).to_be_hidden()
        spatial = landing.locator('[data-showcase-frame]').element_handle().content_frame()
        spatial.wait_for_function("document.documentElement.dataset.fluentQtShowcase === 'spatial'")
        landing.screenshot(path=str(args.output / 'homepage-spatial.png'))
        landing.locator('[data-showcase-stop]').click()
        landing.goto(urljoin(args.url, 'zh-CN/'))
        expect(landing.locator('.hero-message')).to_have_css('opacity', '1')
        landing.screenshot(path=str(args.output / 'homepage-zh-desktop.png'))
        landing.set_viewport_size({'width': 390, 'height': 844})
        landing.screenshot(path=str(args.output / 'homepage-zh-mobile.png'))
        landing.goto(urljoin(args.url, ''))
        landing.screenshot(path=str(args.output / 'homepage-en-mobile.png'))
        home_context.close()
        check_spatial_startup_recovery(browser, args.url, args.output)
        checks.append('Gallery navigation opens the homepage and starts its single embedded Gallery')
        checks.append('Homepage 3D example also starts on visibility; start controls remain hidden')
        checks.append('Spatial controller failure, retry and stop/restart recover in both languages')
        assert not errors, errors
        browser.close()
    result = {'status': 'pass', 'checks': checks, 'page_errors': errors}
    (args.output / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print('Website journey and real Qt WASM interaction passed')


if __name__ == '__main__':
    main()
