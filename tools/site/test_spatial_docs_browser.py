#!/usr/bin/env python3
"""Exercise real Spatial component documents, their local modes and teardown."""

import argparse
import json
from pathlib import Path
from urllib.parse import urljoin

from playwright.sync_api import expect, sync_playwright
from test_component_docs_interactive import native_state, wait_example
from test_component_focus_browser import refresh_state, settle_render
from test_component_catalog_browser import document_theme, post_selection


def wait_spatial(frame, enabled):
    frame.wait_for_function(
        'enabled => JSON.parse(document.documentElement.dataset.fluentQtComponentState || "{}").spatial?.enabled === enabled',
        arg=enabled)


def click_named(page, frame, selection, name, fraction=0.5):
    state = refresh_state(page, frame, selection)
    control = next(c for c in state['focusTargets'] if c['name'] == name)
    box = page.locator('.live-stage iframe').bounding_box()
    page.mouse.click(box['x'] + control['x'] + control['width'] * fraction,
                     box['y'] + control['y'] + control['height'] / 2)


def run(url, output):
    output.mkdir(parents=True, exist_ok=True)
    errors, records = [], []
    with sync_playwright() as p:
        browser = p.chromium.launch()
        try:
            for width, scale in [(1440, 1), (390, 2)]:
                context = browser.new_context(viewport={'width': width, 'height': 1100},
                                              device_scale_factor=scale, reduced_motion='no-preference')
                try:
                    page = context.new_page()
                    page.on('pageerror', lambda error: errors.append(str(error)))
                    for route, sample in [('spatial-view', 'spatial-view-scene'), ('spatial-item', 'spatial-item-pose')]:
                        selection = {'route': route, 'sample': sample, 'state': 'basic'}
                        page.goto(urljoin(url, f'zh-CN/api/{route}/'))
                        frame = wait_example(page)
                        page.locator('.live-stage iframe').scroll_into_view_if_needed()
                        wait_spatial(frame, True)
                        theme_before = document_theme(page)
                        for theme in ('light', 'dark'):
                            page.locator(f'button[data-preview-theme="{theme}"]').click()
                            page.locator('.live-stage iframe').scroll_into_view_if_needed()
                            frame.wait_for_function('theme => document.documentElement.dataset.fluentQtGalleryHostTheme === theme', arg=theme)
                            wait_spatial(frame, True)
                            frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).spatial.backend === "webgl"')
                            assert document_theme(page) == theme_before
                            click_named(page, frame, selection, 'showcaseSpatialMode', 0.1)
                            wait_spatial(frame, False)
                            click_named(page, frame, selection, 'showcaseSpatialMode', 0.1)
                            wait_spatial(frame, True)
                            if route == 'spatial-view':
                                before = native_state(frame)['spatial']['distance']
                                click_named(page, frame, selection, 'spatialViewDistance', 0.8 if theme == 'light' else 0.3)
                                frame.wait_for_function('before => JSON.parse(document.documentElement.dataset.fluentQtComponentState).spatial.distance !== before', arg=before)
                            page.mouse.move(5, 5)
                            settle_render(frame)
                            page.locator('[data-component]').screenshot(path=str(output / f'{route}-{theme}-{width}.png'))
                            records.append({'route': route, 'theme': theme, 'width': width, 'scale': scale,
                                            'spatial': native_state(frame)['spatial']})
                        # Reduced motion preserves the native 2D policy, then allows an explicit restart.
                        page.emulate_media(reduced_motion='reduce')
                        wait_spatial(frame, False)
                        page.emulate_media(reduced_motion='no-preference')
                        frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).focusTargets.some(c => c.name === "showcaseSpatialMode")')
                        click_named(page, frame, selection, 'showcaseSpatialMode', 0.1)
                        wait_spatial(frame, True)
                        # Host High Contrast keeps the same 2D contract and restores the local switch.
                        for theme in ('high-contrast', 'light'):
                            page.evaluate('''theme => {
                                document.querySelector('.live-stage iframe').contentWindow.postMessage(
                                    {source: 'fluent-qt-site', type: 'theme', theme}, location.origin);
                            }''', theme)
                            frame.wait_for_function('theme => document.documentElement.dataset.fluentQtGalleryHostTheme === theme', arg=theme)
                            if theme == 'high-contrast':
                                wait_spatial(frame, False)
                            else:
                                frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).focusTargets.some(c => c.name === "showcaseSpatialMode")')
                                click_named(page, frame, selection, 'showcaseSpatialMode', 0.1)
                                wait_spatial(frame, True)
                        # Reuse the mounted runtime: old mode controls and callbacks must be discarded.
                        post_selection(page, {'route': 'button', 'sample': 'button-styles', 'state': 'styles'})
                        frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState).route === "button"')
                        assert 'spatial' not in native_state(frame)
                        post_selection(page, selection)
                        wait_spatial(frame, True)
                        state = refresh_state(page, frame, selection)
                        assert sum(c['name'] == 'showcaseSpatialMode' for c in state['focusTargets']) == 1
                finally:
                    context.close()
            assert not errors, errors
        finally:
            browser.close()
    (output / 'result.json').write_text(json.dumps({'checks': records, 'page_errors': errors}, indent=2) + '\n')
    print(f'PASS: {len(records)} Spatial document cases; local 3D mode, WebGL, themes, motion policy and teardown')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8766/')
    parser.add_argument('--output', type=Path, default=Path('build/spatial-docs-browser'))
    args = parser.parse_args()
    run(args.url, args.output)
