#!/usr/bin/env python3
"""Check real component previews and keyboard focus at integer/fractional scales."""

import argparse
from io import BytesIO
import json
from pathlib import Path
from urllib.parse import urljoin

from PIL import Image, ImageChops
from playwright.sync_api import expect, sync_playwright

from component_inventory import config
from test_component_catalog_browser import post_selection


FOCUS_CLASSES = {
    'checkbox': 'CheckBox', 'combobox': 'ComboBox', 'rating-control': 'RatingControl',
    'hyperlink-button': 'HyperlinkButton', 'slider': 'Slider',
}


def refresh_state(page, frame, selection):
    revision = frame.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState || "{}").revision || 0')
    post_selection(page, selection)
    frame.wait_for_function('''args => {
        const state = JSON.parse(document.documentElement.dataset.fluentQtComponentState || '{}');
        return state.route === args.route && state.revision > args.revision;
    }''', arg={'route': selection['route'], 'revision': revision})
    return frame.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')


def target_for(state, class_name=None):
    candidates = [item for item in state['focusTargets'] if item['fullyVisible']]
    if class_name:
        return next((item for item in candidates if item['class'].split('::')[-1] == class_name), None)
    focused = next((item for item in candidates if item['focused']), None)
    return focused or next((item for item in candidates if item['class'].startswith('fluent::')), next(iter(candidates), None))


def seek_focus(page, frame, selection, class_name, wanted):
    for _ in range(24):
        state = refresh_state(page, frame, selection)
        target = target_for(state, class_name)
        if target is not None and target['focused'] == wanted:
            return target
        page.keyboard.press('Tab')
    raise AssertionError((selection, class_name, 'could not reach requested focus state', state['focusTargets']))


def region(image, target, scale, inset=0):
    return image.crop((round((target['x'] + inset) * scale), round((target['y'] + inset) * scale),
                       round((target['x'] + target['width'] - inset) * scale),
                       round((target['y'] + target['height'] - inset) * scale)))


def settle_render(frame):
    frame.evaluate('() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))')


def run(url, output, scales, routes=None):
    documents = [doc for doc in config() if not routes or doc['id'] in routes]
    records, errors = [], []
    output.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as p:
        browser = p.chromium.launch()
        for scale in scales:
            context = browser.new_context(viewport={'width': 1440, 'height': 1100}, device_scale_factor=scale)
            page = context.new_page()
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(urljoin(url, 'zh-CN/api/button/'))
            expect(page.locator('[data-component]')).to_have_attribute('data-live-state', 'ready', timeout=90000)
            page.locator('[data-component]').evaluate("root => root.style.setProperty('--preview-height', '640px')")
            iframe = page.locator('.live-stage iframe')
            frame = iframe.element_handle().content_frame()
            frame.wait_for_function('JSON.parse(document.documentElement.dataset.fluentQtComponentState || "{}").revision > 0')
            for doc in documents:
                preview = doc['previews'][0]
                selection = {'route': doc['id'], 'sample': preview['sample'], 'state': preview['id']}
                state = refresh_state(page, frame, selection)
                assert state['contentWidth'] > 0 and state['contentHeight'] > 0, selection
                for theme in ('light', 'dark'):
                    print(f'{scale}x {theme} {doc["id"]}', flush=True)
                    page.locator(f'button[data-preview-theme="{theme}"]').click()
                    frame.wait_for_function('theme => document.documentElement.dataset.fluentQtGalleryHostTheme === theme', arg=theme)
                    bounds = iframe.bounding_box()
                    page.mouse.click(bounds['x'] + 8, bounds['y'] + 8)
                    page.mouse.move(20, 20)
                    state = refresh_state(page, frame, selection)
                    class_name = FOCUS_CLASSES.get(doc['id'])
                    target = target_for(state, class_name)
                    keyboard_checked = False
                    if target is not None:
                        if class_name:
                            target = seek_focus(page, frame, selection, class_name, False)
                            settle_render(frame)
                            resting = Image.open(BytesIO(iframe.screenshot())).convert('RGB')
                        page.keyboard.press('Tab')
                        target = seek_focus(page, frame, selection, class_name, True)
                        keyboard_checked = True
                    elif class_name:
                        raise AssertionError((selection, 'missing expected focus target', state))
                    settle_render(frame)
                    capture = iframe.screenshot(path=str(output / f'{doc["id"]}-{theme}-{scale}x.png'))
                    if class_name:
                        focused = Image.open(BytesIO(capture)).convert('RGB')
                        assert ImageChops.difference(region(resting, target, scale), region(focused, target, scale)).getbbox(), (selection, scale, theme, 'missing focus cue')
                        if doc['id'] == 'rating-control':
                            before, after = region(resting, target, scale), region(focused, target, scale)
                            background = before.getpixel((0, 0))
                            # Preserve star/caption ink; the cue may occupy blank space before the caption.
                            changed_ink = [index for index, (a, b) in enumerate(zip(before.getdata(), after.getdata()))
                                           if max(abs(c - d) for c, d in zip(a, background)) > 8
                                           and max(abs(c - d) for c, d in zip(a, b)) > 2]
                            assert not changed_ink, (selection, scale, theme, 'focus overlaps star ink', len(changed_ink))
                    assert not errors, errors
                    records.append({'component': doc['id'], 'scale': scale, 'theme': theme,
                                    'focus_targets': len(state['focusTargets']), 'keyboard_checked': keyboard_checked,
                                    'focused_class': target['class'] if target else None})
            context.close()
        browser.close()
    result = {'status': 'pass', 'components': len(documents), 'scales': scales, 'cases': records, 'errors': errors}
    (output / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f'{len(documents)} components passed {len(records)} theme/scale cases.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8766/')
    parser.add_argument('--output', type=Path, default=Path('build/component-docs/focus-review'))
    parser.add_argument('--scales', type=float, nargs='+', default=[1, 1.25, 1.5, 2])
    parser.add_argument('--routes', nargs='+')
    args = parser.parse_args()
    run(args.url, args.output, args.scales, args.routes)


if __name__ == '__main__':
    main()
