#!/usr/bin/env python3
"""Check the shared reading layout, full API groups and foundation navigation."""
import argparse
import json
from pathlib import Path
from urllib.parse import urljoin

from playwright.sync_api import expect, sync_playwright


def assert_navigation_styles(page):
    """Both directories use the same hierarchy, selection and indicator geometry."""
    styles = page.evaluate('''() => {
        const snapshot = (element, properties, pseudo = null) => {
            const style = getComputedStyle(element, pseudo);
            return Object.fromEntries(properties.map(key => [key, style[key]]));
        };
        const left = document.querySelector('.directory-groups a[aria-current="page"]');
        const right = document.querySelector('.page-navigation a[aria-current="location"]');
        const item = ['fontSize', 'fontWeight', 'lineHeight', 'color', 'backgroundColor', 'padding', 'borderRadius'];
        const heading = ['fontSize', 'fontWeight', 'lineHeight', 'color', 'padding'];
        const indicator = ['width', 'height', 'left', 'backgroundColor', 'borderRadius'];
        const list = ['margin', 'padding', 'borderLeft', 'gap'];
        return {
            left: snapshot(left, item), right: snapshot(right, item),
            leftHeading: snapshot(left.closest('details').querySelector('summary'), heading),
            rightHeading: snapshot(document.querySelector('.page-outline-menu > summary'), heading),
            leftIndicator: snapshot(left, indicator, '::before'),
            rightIndicator: snapshot(right, indicator, '::before'),
            leftList: snapshot(left.closest('ul'), list),
            rightList: snapshot(right.closest('nav'), list)
        };
    }''')
    for suffix in ('', 'Heading', 'Indicator', 'List'):
        assert styles['left' + suffix] == styles['right' + suffix], (page.url, suffix, styles)


def check_legacy_entry(browser, url):
    cases = [
        ('zh', 'api/?q=Button', 'zh-CN/api/button/?q=Button'),
        ('en', 'api/', 'api/foundation/'),
        ('zh', 'api/?lang=en', 'api/foundation/'),
        ('en', 'api/?lang=zh&view=headers', 'zh-CN/api/foundation/?view=headers'),
        ('en', 'zh-CN/api/', 'zh-CN/api/foundation/'),
        (None, 'api/?q=number', 'api/foundation/?q=number'),
        ('invalid', 'api/', 'api/foundation/'),
        ('blocked', 'api/', 'api/foundation/'),
    ]
    for saved, entry, destination in cases:
        context = browser.new_context()
        if saved == 'blocked':
            context.add_init_script("Storage.prototype.getItem = () => { throw new Error('storage unavailable'); };")
        elif saved is not None:
            context.add_init_script('localStorage.setItem("fluent-qt-api-language", ' + json.dumps(saved) + ');')
        page = context.new_page()
        page.goto(urljoin(url, entry))
        expect(page).to_have_url(urljoin(url, destination))
        expect(page.locator('h1')).to_be_visible()
        context.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8766/')
    parser.add_argument('--output', type=Path, default=Path('build/component-docs/documentation-review'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records, errors = [], []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        check_legacy_entry(browser, args.url)
        for route, width, theme in [('button', 1440, 'light'), ('button', 768, 'light'),
                ('button', 390, 'dark'), ('foundation', 1248, 'light'),
                ('foundation-typography', 390, 'light'), ('reference/motion-policy', 1440, 'dark'),
                ('line-chart', 1024, 'light')]:
            context = browser.new_context(viewport={'width': width, 'height': 1000}, color_scheme=theme,
                                          permissions=['clipboard-read', 'clipboard-write'])
            page = context.new_page()
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(urljoin(args.url, 'zh-CN/api/' + route + '/'))
            expect(page.locator('h1')).to_be_visible()
            expect(page.locator('[download], .imports, .run-instructions, .download-links')).to_have_count(0)
            bounds = page.evaluate('''() => {
                const rect = selector => document.querySelector(selector).getBoundingClientRect().toJSON();
                return {brand:rect('.documentation-brand'), nav:rect('.documentation-nav'),
                    sidebar:rect('.sidebar'), main:rect('.document-main'), outline:rect('.page-outline'),
                    intro:rect('.document-intro'), body:rect('.document-body'),
                    overflow:document.documentElement.scrollWidth > innerWidth + 1};
            }''')
            assert not bounds['overflow'], (route, width, bounds)
            assert abs(bounds['brand']['x'] - bounds['sidebar']['x']) <= 1, bounds
            assert abs(bounds['nav']['right'] - bounds['main']['right']) <= 1, bounds
            expect(page.locator('.sidebar .page-navigation')).to_have_count(0)
            outline = page.locator('.page-outline-menu')
            if width >= 1200:
                expect(outline).to_have_attribute('open', '')
                assert bounds['outline']['left'] >= bounds['body']['right'], bounds
                assert_navigation_styles(page)
            else:
                expect(outline).not_to_have_attribute('open', '')
                assert bounds['intro']['bottom'] <= bounds['outline']['top'] < bounds['body']['top'], bounds
            directory = page.locator('.documentation-directory')
            if width <= 960:
                expect(directory).not_to_have_attribute('open', '')
                expect(directory.locator('> summary')).to_be_visible()
            if page.locator('[data-component]').count():
                expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
            else:
                expect(page.locator('iframe, .preview')).to_have_count(0)
            name = route.replace('/', '-') + '-' + str(width) + '-' + theme
            page.screenshot(path=str(args.output / (name + '.png')))
            if width < 1200:
                outline.locator('> summary').focus()
                outline.locator('> summary').press('Enter')
                expect(outline).to_have_attribute('open', '')
                outline.locator('a').first.click()
                expect(outline).not_to_have_attribute('open', '')
            if route == 'button' and width == 1440:
                code = page.locator('.usage-example').first
                code.get_by_role('tab', name='PySide6', exact=True).click()
                shown = code.locator('[role="tabpanel"]:not([hidden]) code').inner_text()
                assert shown.startswith('import fluentqt\n')
                code.locator('[data-copy]').click()
                assert page.evaluate('navigator.clipboard.readText()') == shown
                filter = page.locator('[data-api-filter]')
                filter.fill('setIconGlyph')
                expect(page.locator('[data-api-member]:visible')).to_have_count(2)
                expect(page.locator('[data-api-member]:visible').first).to_contain_text('int pixelSize')
                filter.fill('setTheme')
                expect(page.locator('.api-inherited[open]')).to_have_count(1)
                expect(page.locator('[data-api-member]:visible').first).to_contain_text('setTheme')
                filter.fill('no-such-api-xyz')
                expect(page.locator('[data-api-empty]')).to_be_visible()
                filter.press('Escape')
                expect(page.locator('[data-api-empty]')).to_be_hidden()
                page.locator('#api').scroll_into_view_if_needed()
                expect(page.locator('.page-navigation a[href="#api"]')).to_have_attribute('data-section-active', 'true')
                page.screenshot(path=str(args.output / 'button-api.png'))
            if route == 'foundation':
                page.locator('[data-directory-search]').fill('MotionPolicy')
                page.locator('[data-directory-search]').press('Enter')
                expect(page).to_have_url(urljoin(args.url, 'zh-CN/api/reference/motion-policy/'))
                expect(page.locator('[data-api-member]').filter(has_text='resolvedDuration').first).to_be_visible()
            records.append({'route': route, 'width': width, 'theme': theme, 'bounds': bounds})
            context.close()
        browser.close()
    assert not errors, errors
    (args.output / 'result.json').write_text(json.dumps({'status': 'pass', 'cases': records, 'errors': errors}, indent=2) + '\n')
    print('Documentation layout, API filtering, code copying and foundation navigation passed.')


if __name__ == '__main__':
    main()
