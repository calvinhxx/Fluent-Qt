#!/usr/bin/env python3
"""Check all document navigation, isolated themes and real Qt component previews."""

import argparse
import json
from pathlib import Path
import re
from urllib.parse import urljoin

from playwright.sync_api import expect, sync_playwright

from component_inventory import config
from test_documentation_browser import assert_navigation_styles


SCREENSHOT_COMPONENTS = {'line-chart', 'data-grid', 'property-panel', 'number-box', 'content-dialog', 'tree-select'}


def document_theme(page):
    return page.evaluate('''() => ({
        theme: document.documentElement.dataset.theme,
        background: getComputedStyle(document.body).backgroundColor,
        color: getComputedStyle(document.body).color,
        saved: localStorage.getItem('fluent-qt-theme')
    })''')


def check_document_routes(browser, url, documents, output, errors):
    """Exercise each localized page while keeping the homepage and a peer open."""
    records = []
    runtime_url = re.compile('^' + re.escape(urljoin(url, 'gallery/')))
    for prefix, website_theme in (('', 'light'), ('zh-CN/', 'dark')):
        context = browser.new_context(viewport={'width': 1440, 'height': 1000},
                                      color_scheme='light', reduced_motion='reduce')
        context.on('page', lambda page: page.on('pageerror', lambda error: errors.append(str(error))))
        context.route(runtime_url, lambda route: route.fulfill(
            status=503, content_type='text/html', body='<h1>Unavailable</h1>'))
        home = context.new_page()
        home.goto(urljoin(url, prefix))
        # Set a saved preference through the real homepage control.
        for _ in range(3 if website_theme == 'light' else 1):
            home.locator('[data-theme-toggle]').click()
        expect(home.locator('html')).to_have_attribute('data-theme', website_theme)
        peer = context.new_page()
        peer.goto(urljoin(url, prefix + 'api/checkbox/'))
        expect(peer.locator('[data-component]')).to_have_attribute('data-preview-theme', website_theme)
        homepage = document_theme(home)
        page = context.new_page()
        opposite = 'dark' if website_theme == 'light' else 'light'
        for doc in documents:
            path = prefix + 'api/' + doc['id'] + '/'
            print('Checking document ' + path, flush=True)
            page.set_viewport_size({'width': 1440, 'height': 1000})
            response = page.goto(urljoin(url, path))
            assert response.status == 200, path
            root = page.locator('[data-component]')
            expect(root).to_have_count(1)
            expect(root).to_have_attribute('data-component', doc['id'])
            expect(root).to_have_attribute('data-preview-theme', website_theme)
            expect(root).to_have_attribute('data-live-state', 'error')
            expect(page.locator('.live-stage iframe')).to_have_count(0)
            expect(page.locator('#example-0-cpp, #example-0-python')).to_have_count(2)
            expect(page.locator('.directory-groups a[aria-current="page"]')).to_have_attribute('href', './')
            assert_navigation_styles(page)
            original = document_theme(page)
            assert original['theme'] == original['saved'] == website_theme, (path, original)
            # Each theme switch is made with the visible control, including every state recipe.
            for preview in doc['previews']:
                if len(doc['previews']) > 1:
                    root.locator(f'.state-tabs [data-state="{preview["id"]}"]').click()
                for theme in (website_theme, opposite):
                    root.locator(f'button[data-preview-theme="{theme}"]').click()
                    expect(root).to_have_attribute('data-preview-theme', theme)
                    expect(root.locator(f'button[data-preview-theme="{theme}"]')).to_have_attribute('aria-pressed', 'true')
                    page.wait_for_function('''theme => {
                        const img = document.querySelector('.preview-panel:not([hidden]) img');
                        return img?.complete && img.naturalWidth > 0 && img.dataset.imageReady === theme
                            && img.currentSrc.includes('-' + theme + '.png');
                    }''', arg=theme)
                    assert document_theme(page) == original, (path, theme, 'document theme changed')
                    assert document_theme(home) == homepage, (path, theme, 'homepage theme changed')
                    expect(peer.locator('[data-component]')).to_have_attribute('data-preview-theme', website_theme)
            if doc['id'] in SCREENSHOT_COMPONENTS and not prefix:
                page.screenshot(path=str(output / (doc['id'] + '-document-desktop.png')))
            # Compact navigation and theme controls remain usable on every translated route.
            page.set_viewport_size({'width': 390, 'height': 844})
            expect(page.locator('.page-outline-menu')).not_to_have_attribute('open', '')
            expect(page.locator('.documentation-directory')).not_to_have_attribute('open', '')
            for theme in (website_theme, opposite):
                root.locator(f'button[data-preview-theme="{theme}"]').click()
                expect(root).to_have_attribute('data-preview-theme', theme)
                page.wait_for_function('''theme => {
                    const img = document.querySelector('.preview-panel:not([hidden]) img');
                    return img?.complete && img.naturalWidth > 0 && img.dataset.imageReady === theme;
                }''', arg=theme)
            geometry = root.evaluate('''root => {
                const toolbar = root.querySelector('.preview-toolbar').getBoundingClientRect();
                const controls = root.querySelector('.theme-options').getBoundingClientRect();
                return {overflow:document.documentElement.scrollWidth > innerWidth + 1,
                    contained:controls.left >= toolbar.left && controls.right <= toolbar.right
                        && controls.top >= toolbar.top && controls.bottom <= toolbar.bottom};
            }''')
            assert not geometry['overflow'] and geometry['contained'], (path, geometry)
            assert document_theme(page) == original and document_theme(home) == homepage, path
            if doc['id'] in SCREENSHOT_COMPONENTS and prefix:
                page.screenshot(path=str(output / (doc['id'] + '-document-mobile.png')))
            records.append({'path': path, 'website_theme': website_theme,
                            'states': len(doc['previews']), 'viewports': [1440, 390]})
        # Global changes still propagate; the explicitly chosen preview remains local.
        home.locator('[data-theme-toggle]').click()
        changed = home.locator('html').get_attribute('data-theme')
        expect(page.locator('html')).to_have_attribute('data-theme', changed)
        expect(page.locator('[data-component]')).to_have_attribute('data-preview-theme', opposite)
        expect(peer.locator('[data-component]')).to_have_attribute('data-preview-theme', changed)
        page.reload()
        expect(page.locator('[data-component]')).to_have_attribute('data-preview-theme', changed)
        page.emulate_media(forced_colors='active')
        expect(page.locator('[data-component]')).to_have_attribute('data-preview-theme', 'high-contrast')
        for theme in ('light', 'dark'):
            expect(page.locator(f'button[data-preview-theme="{theme}"]')).to_be_disabled()
        assert home.locator('html').get_attribute('data-theme') == changed
        context.close()
    return records


def post_selection(page, selection):
    page.evaluate('''selection => {
        const iframe = document.querySelector('.live-stage iframe');
        const session = new URL(iframe.src).searchParams.get('host-session');
        iframe.contentWindow.postMessage({source:'fluent-qt-site', type:'component-select', session, selection}, location.origin);
    }''', selection)


def check_initial_dialog(page, url, output):
    """Open immediately: late window setup must not cover an existing overlay."""
    page.goto(urljoin(url, 'zh-CN/api/content-dialog/'))
    expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
    frame = page.locator('.live-stage iframe').element_handle().content_frame()
    frame.wait_for_function('document.documentElement.dataset.fluentQtComponentState')
    state = frame.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')
    button = next(b for b in state['controls'] if b['text'] == 'Show dialog')
    bounds = page.locator('.live-stage iframe').bounding_box()
    page.mouse.click(bounds['x'] + button['x'] + button['width'] / 2,
                     bounds['y'] + button['y'] + button['height'] / 2)
    frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).clicks > 0')
    # Cross the deferred window-layout turn and the dialog entrance animation.
    frame.wait_for_timeout(350)
    selection = {key: state[key] for key in ('route', 'sample', 'state')}
    post_selection(page, selection)
    frame.wait_for_function('''() => JSON.parse(document.documentElement.dataset.fluentQtComponentState)
        .dialogs.some(d => d.open && d.visible && d.exposed && d.title === 'Save your work?')''')
    page.locator('.live-stage iframe').screenshot(path=str(output / 'content-dialog-open.png'))
    page.keyboard.press('Escape')
    frame.wait_for_timeout(350)
    post_selection(page, selection)
    frame.wait_for_function('''() => JSON.parse(document.documentElement.dataset.fluentQtComponentState)
        .dialogs.every(d => !d.open && !d.visible)''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8766/')
    parser.add_argument('--output', type=Path, default=Path('build/component-docs/catalog-browser'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    documents = config()
    records, errors = [], []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        routes = check_document_routes(browser, args.url, documents, args.output, errors)
        context = browser.new_context(viewport={'width': 1440, 'height': 1000}, color_scheme='light')
        page = context.new_page()
        page.on('pageerror', lambda error: errors.append(str(error)))
        home = context.new_page()
        home.goto(urljoin(args.url, 'zh-CN/'))
        homepage = document_theme(home)
        check_initial_dialog(page, args.url, args.output)
        page.goto(urljoin(args.url, 'zh-CN/api/button/'))
        expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
        frame = page.locator('.live-stage iframe').element_handle().content_frame()
        original = document_theme(page)
        first_load = frame.evaluate('document.documentElement.dataset.fluentQtLoadStartedAt')
        for doc in documents:
            preview = doc['previews'][0]
            selection = {'route': doc['id'], 'sample': preview['sample'], 'state': preview['id']}
            print('Loading ' + doc['id'], flush=True)
            page.evaluate('''() => {
                document.querySelector('[data-component]').style.setProperty('--preview-height', '600px');
            }''')
            post_selection(page, selection)
            frame.wait_for_function('''selection => {
                const s = JSON.parse(document.documentElement.dataset.fluentQtComponentState || '{}');
                return s.route === selection.route && s.sample === selection.sample && s.contentWidth > 0 && s.contentHeight > 0;
            }''', arg=selection, timeout=20000)
            state = frame.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')
            expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready')
            for theme in ('dark', 'light'):
                page.locator(f'button[data-preview-theme="{theme}"]').click()
                frame.wait_for_function('theme => document.documentElement.dataset.fluentQtGalleryHostTheme === theme', arg=theme)
                assert document_theme(page) == original, (doc['id'], 'document theme changed')
                assert document_theme(home) == homepage, (doc['id'], 'homepage theme changed')
                assert frame.evaluate('document.documentElement.dataset.fluentQtLoadStartedAt') == first_load
                assert frame.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState).route') == doc['id']
                if doc['id'] in SCREENSHOT_COMPONENTS:
                    page.locator('.live-stage iframe').screenshot(path=str(args.output / (doc['id'] + '-live-' + theme + '.png')))
            assert not errors, errors
            records.append({'component': doc['id'], 'sample': preview['sample'],
                            'width': state['contentWidth'], 'height': state['contentHeight'],
                            'widgets': state['widgetCount'], 'themes': ['dark', 'light']})
            page.locator('.live-stage iframe').screenshot(path=str(args.output / (doc['id'] + '.png')))
            print('Loaded ' + doc['id'], flush=True)
        # Revisit the original sample after disposing all of the other factories.
        page.reload()
        expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
        expect(page.locator('.live-stage iframe')).to_have_count(1)
        context.close()
        browser.close()
    (args.output / 'result.json').write_text(json.dumps({'status': 'pass', 'pages': len(documents) * 2,
        'documents': routes, 'previews': records, 'dialog_open_close': True, 'page_errors': errors}, indent=2) + '\n')
    print(f'All {len(documents)} real component factories and {len(routes)} localized documents passed navigation and isolated-theme checks.')


if __name__ == '__main__':
    main()
