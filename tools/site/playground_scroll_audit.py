"""Wheel/drag checks against the real collection canvas, using its reported row geometry."""
from playwright.sync_api import expect


def verify_collection_scroll(page, frame, state, edit, reset, point, remember, route, output, screenshots):
    reset()
    edit('headerText', 'Scrollable items')
    if route != 'tree-view':
        data = [list(row) for row in state()['values']['modelData']]
        template = data[0]
        data += [[f'Scroll row {i + 1}', *template[1:]] for i in range(len(data), 18)]
        edit('modelData', data)
    if route == 'grid-view':
        edit('maxColumns', 1)
        edit('itemHeight', 80)
    if route == 'flow-view':
        edit('itemWidth', 240)
        edit('itemHeight', 80)

    def rows():
        return {h['index']: h for h in state()['preview']['hitTargets'] if h['action'] == 'model-item'}

    def wheel(delta, pause=60):
        page.mouse.move(*point(.5, .5))
        page.mouse.wheel(delta if route == 'tree-view' else 0, 0 if route == 'tree-view' else delta)
        page.wait_for_timeout(pause)

    # The fixed six-node tree fits vertically; its deliberately wide column scrolls horizontally.
    axis = 'x' if route == 'tree-view' else 'y'

    # A large input reaches the top, then a quiet period settles its elastic tail.
    wheel(-3000, 600)
    baseline = rows()
    assert 0 in baseline, (route, 'top row unavailable', baseline)
    values = state()['values']
    source = (state()['cpp'], state()['python'])
    preview_box = page.locator('.playground-preview').bounding_box()
    outer_scroll = page.evaluate('window.scrollY')
    wheel(-800, 50)
    stretched = rows()
    assert 0 in stretched, (route, 'boundary input hid the whole list')
    assert 0 <= stretched[0][axis] - baseline[0][axis] <= 100, (route, 'unbounded elastic travel')
    if axis == 'y':
        page.mouse.wheel(100, 0)
        page.wait_for_timeout(40)
        assert rows()[0]['y'] > baseline[0]['y'], (route, 'unused horizontal axis cancelled vertical recovery')
        remember('unused-horizontal-axis-preserves-elastic-return')
    page.wait_for_timeout(600)
    assert rows()[0][axis] == baseline[0][axis], (route, 'boundary did not settle')

    wheel(-150, 160)
    wheel(100, 70)
    recovered = rows()
    shared = baseline.keys() & recovered.keys()
    assert shared and any(recovered[i][axis] < baseline[i][axis] for i in shared), (route, 'reverse input swallowed')
    page.wait_for_timeout(450)
    assert rows() == recovered, (route, 'content moved again after reverse input ended')
    assert state()['values'] == values and (state()['cpp'], state()['python']) == source
    assert page.evaluate('window.scrollY') == outer_scroll
    assert page.locator('.playground-preview').bounding_box() == preview_box
    remember('scroll-boundary-cap-reverse-recovery-stable-selection-and-layout')

    # Drag the visible overlay thumb while the wheel has stretched the content.
    wheel(-3000, 600)
    wheel(-150, 35)
    first = baseline[0]
    t = state()['preview']['target']
    start = (point(.1, 1 - 8 / t['height']) if axis == 'x'
             else point(1 - 5 / t['width'], (first['y'] - t['y'] + 8) / t['height']))
    scale = page.locator('.playground-live-frame').bounding_box()['width'] / state()['preview']['width']
    page.mouse.move(*start)
    page.mouse.down()
    page.mouse.move(start[0] + (75 * scale if axis == 'x' else 0),
                    start[1] + (75 * scale if axis == 'y' else 0), steps=5)
    page.wait_for_timeout(60)
    dragged = rows()
    page.mouse.up()
    page.wait_for_timeout(500)
    assert rows() == dragged, (route, 'thumb release left a second elastic movement')
    assert dragged != baseline, (route, 'thumb drag did not move content')
    assert state()['values'] == values
    remember('overlay-thumb-drag-cancels-wheel-elastic-offset')

    # HTML properties and the native collection must retain separate scroll positions.
    panel = page.locator('.playground-tabpanel:not([hidden])')
    panel_box = panel.bounding_box()
    panel_before = panel.evaluate('(el)=>el.scrollTop')
    page.mouse.move(panel_box['x'] + panel_box['width'] / 2, panel_box['y'] + panel_box['height'] / 2)
    page.mouse.wheel(0, 180)
    page.wait_for_timeout(150)
    if panel.evaluate('(el)=>el.scrollHeight > el.clientHeight'):
        assert panel.evaluate('(el)=>el.scrollTop') > panel_before
    assert rows() == dragged, (route, 'property scroll moved the native collection')
    panel_after = panel.evaluate('(el)=>el.scrollTop')
    wheel(-3000, 600)
    assert panel.evaluate('(el)=>el.scrollTop') == panel_after
    assert page.evaluate('window.scrollY') == outer_scroll
    remember('native-and-property-scroll-containment')

    if screenshots:
        for theme in ('light', 'dark'):
            page.emulate_media(color_scheme=theme)
            expect(page.locator('.playground-popover')).to_have_attribute('data-theme', theme)
            wheel(80, 600)
            page.locator('.playground-preview').screenshot(path=str(output / f'{route}-scroll-{theme}.png'))
        page.emulate_media(color_scheme='light')
    if route == 'file-list-view':
        desktop = page.viewport_size
        page.set_viewport_size({'width': 390, 'height': 844})
        page.wait_for_timeout(200)
        wheel(-3000, 600)
        narrow_top = rows()
        wheel(100, 300)
        narrow_scrolled = rows()
        shared = narrow_top.keys() & narrow_scrolled.keys()
        assert shared and any(narrow_scrolled[i]['y'] < narrow_top[i]['y'] for i in shared)
        box = page.locator('.playground-popover').bounding_box()
        assert box['x'] >= 0 and box['x'] + box['width'] <= 391
        if screenshots:
            page.screenshot(path=str(output / 'file-list-view-scroll-narrow.png'))
        remember('narrow-native-scroll')
        page.set_viewport_size(desktop)
        page.wait_for_timeout(200)
    reset()
