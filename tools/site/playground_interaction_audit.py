"""Real canvas input, property readback, source isolation and lifecycle contract."""
from __future__ import annotations
import json
import io
import importlib.util
import traceback
from pathlib import Path
from playwright.sync_api import expect
ROOT = Path(__file__).resolve().parents[2]
STATE = 'JSON.parse(document.documentElement.dataset.fluentQtPlaygroundState)'


def run(browser, base_url, output, screenshots=False, routes=None, exhaustive=True):
    output.mkdir(parents=True, exist_ok=True)
    specs = [{'component': 'button'}] + json.loads((ROOT / 'app/assets/playgrounds/components.json').read_text(encoding='utf-8'))
    if routes is not None: specs = [spec for spec in specs if spec['component'] in routes]
    context = browser.new_context(viewport={'width':1440,'height':1000}, device_scale_factor=2,
                                  permissions=['clipboard-read','clipboard-write'])
    page = context.new_page(); errors=[]; console=[]; results=[]; cases=[]
    page.on('pageerror', lambda e: errors.append(str(e)))
    page.on('console', lambda e: console.append(e.text) if e.type in ('error','warning') else None)
    for spec in specs:
        route=spec['component']; checked=[]; seen=set(); start_errors=len(errors)
        try:
            page.goto(base_url+'zh-CN/api/'+route+'/')
            page.locator('[data-component]').scroll_into_view_if_needed()
            expect(page.locator('[data-component]')).to_have_attribute('data-live-state','ready',timeout=90000)
            original=page.locator('.live-stage iframe').element_handle().content_frame()
            original.wait_for_function('document.documentElement.dataset.fluentQtComponentState')
            before=original.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')
            source=page.locator('.document-main .code-example').first.inner_text()
            page.locator('[data-playground-toggle]').click()
            expect(page.locator('.playground-copy')).to_be_enabled(timeout=60000)
            frame=page.locator('.playground-live-frame').element_handle().content_frame()
            def state(): return frame.evaluate(STATE)
            def settled():
                expect(page.locator('.playground-copy')).to_be_enabled(timeout=15000)
                s=state(); assert not s.get('error'), s.get('error'); assert s['preview']['live']; return s
            initial=settled(); assert initial['schema']['component']==route
            assert initial['schema']['interactive']; assert 'url' not in initial['preview']
            cases.append({'case':route+'/default','state':initial})
            def wait_value(name,value):
                frame.wait_for_function('args => JSON.stringify('+STATE+'.values[args[0]]) === JSON.stringify(args[1])',arg=[name,value],timeout=10000)
                return settled()
            def edit(name,value):
                s=settled(); f=next(f for f in s['schema']['properties'] if f['id']==name)
                category=page.locator('[data-property="'+name+'"]').evaluate('(el)=>el.closest("[role=tabpanel]").id.replace("panel-category-", "")')
                page.locator(f'.playground-category-tab[data-tab="{category}"]').click()
                if s['values'][name]==value:return s
                pair=s['schema'].get('stateTextPair',{})
                if name in (pair.get('on'),pair.get('off')) and page.locator('.playground-text-mode').get_attribute('aria-expanded')=='false':
                    page.locator('.playground-text-mode').click();s=settled()
                control=page.locator('#playground-'+name)
                follow=page.locator(f'[data-follow-theme="{name}"]')
                if follow.count() and follow.is_checked(): follow.uncheck(); settled()
                rev=state()['revision']
                if f['type']=='multi-enum' and f.get('singleSelection'):
                    choice=value[0] if value else ''
                    control.locator('input').evaluate_all('(els,value)=>els.find(el=>el.value===value).click()',choice)
                elif f['type']=='multi-enum':
                    for checkbox in control.locator('input').all():
                        if checkbox.is_checked() != (checkbox.input_value() in value):
                            checkbox.set_checked(checkbox.input_value() in value); settled()
                elif f['type']=='table':
                    while control.locator('tbody tr').count()>len(value):
                        control.locator('.playground-data-remove').last.click();settled()
                    while control.locator('tbody tr').count()<len(value):
                        control.locator('.playground-data-add').click();settled()
                    for row,cells in enumerate(value):
                        for col,cell in enumerate(cells):
                            editor=control.locator(f'input[data-row="{row}"][data-column="{col}"]')
                            text='' if cell is None else str(cell)
                            if editor.input_value()!=text:
                                editor.fill(text);expect(page.locator('.playground-copy')).to_be_enabled()
                    return wait_value(name,value)
                elif f['type']=='boolean' and f.get('control')=='segments': control.locator('[data-value="'+str(value).lower()+'"]').click()
                elif f['type']=='boolean': control.set_checked(value)
                elif control.evaluate('(el)=>el.tagName')=='SELECT':
                    page.locator(f'[data-property="{name}"] .playground-dropdown-trigger').click()
                    page.locator('.playground-dropdown-menu:not([hidden]) [data-value]').evaluate_all('(els,value)=>els.find(el=>el.dataset.value===String(value)).click()',value)
                elif f['type']=='enum': control.locator(f'[data-value="{value}"]').click()
                elif f['type'] in ('text','string-list'):control.fill('\n'.join(value) if isinstance(value,list) else value)
                else:
                    expect(control).to_be_enabled()
                    control.evaluate('(el,v)=>{el.value=v===null?"":String(v);el.dispatchEvent(new Event(el.type==="range"?"input":"change",{bubbles:true}));}',value)
                frame.wait_for_function('r=>'+STATE+'.revision>r',arg=rev,timeout=15000)
                return settled()
            def reset():
                page.locator('.playground-reset').click(); return settled()
            def point(fx=.5,fy=.5,widget=None):
                s=state();t=widget or s['preview']['target'];box=page.locator('.playground-live-frame').bounding_box();scale=box['width']/s['preview']['width']
                return box['x']+(t['x']+t['width']*fx)*scale,box['y']+(t['y']+t['height']*fy)*scale
            def click(fx=.5,fy=.5,widget=None):page.mouse.click(*point(fx,fy,widget));page.wait_for_timeout(120)
            def remember(name):
                s=settled();checked.append(name);cases.append({'case':route+'/'+name,'state':s});return s
            # Genuine browser input reaches Qt. Values are read back, never synthesized by the test.
            if route in ('button','compound-button','repeat-button','split-button','toggle-button','toggle-split-button'):
                edit('checkable',True);edit('checked',False);click(.25)
                wait_value('checked',True);expect(page.locator('#playground-checked')).to_be_checked()
                assert 'setChecked(true)' in state()['cpp']; remember('native-click-sync')
                edit('enabled',False);click(.25);assert state()['values']['checked'];remember('disabled-blocks-click')
            elif route=='checkbox':
                edit('tristate',True);edit('checkState','Unchecked');click(.15);wait_value('checkState','PartiallyChecked');remember('native-tristate')
            elif route=='radio-button':
                edit('checked',False);click(.15);wait_value('checked',True);remember('native-selection')
            elif route=='toggle-switch':
                before_value=state()['values']['isOn'];click(.15);wait_value('isOn',not before_value);remember('native-toggle')
            elif route=='file-drop-zone':
                click(widget=next(w for w in state()['preview']['controls'] if w['class'].endswith('Button')))
                frame.wait_for_function(STATE+'.events.length > 0')
                assert state()['events'][-1]['name']=='browseRequested';remember('native-browse-request')
                edit('enabled',False);events=state()['events'];click();assert state()['events']==events;remember('disabled-blocks-browse-request')
            elif route=='rating-control':
                click(15/state()['preview']['target']['width']);wait_value('value',1);remember('native-rating')
                edit('isReadOnly',True);click(.75);assert state()['values']['value']==1;remember('readonly-blocks-rating')
                edit('isReadOnly',False);edit('enabled',False);click(.75);assert state()['values']['value']==1;remember('disabled-blocks-rating')
            elif route in ('line-edit','auto-suggest-box','tag-input','text-edit','password-box'):
                name={'text-edit':'plainText','password-box':'password'}.get(route,'text')
                edit('readOnly',False);click(.35,.55);page.keyboard.press('ControlOrMeta+A');page.keyboard.insert_text('真实输入 Abc');wait_value(name,'真实输入 Abc');remember('native-text-input')
                if route=='text-edit':
                    edit('tabChangesFocus',False);click(.35,.55);page.keyboard.press('ControlOrMeta+End');page.keyboard.press('Tab')
                    wait_value(name,'真实输入 Abc\t');remember('native-tab-inserts-text')
                    page.keyboard.press('Backspace');wait_value(name,'真实输入 Abc')
                    # Chromium can lose its DOM key target while the Qt editor still has focus.
                    frame.evaluate('document.activeElement.blur()')
                    click(.35,.55)
                    page.keyboard.press('ControlOrMeta+End')
                    page.keyboard.insert_text('文')
                    wait_value(name,'真实输入 Abc文');remember('native-editor-refocus-input')
                    page.keyboard.press('Backspace');wait_value(name,'真实输入 Abc')
                    page.keyboard.press('Control+Tab');page.wait_for_timeout(100)
                    assert page.evaluate('document.activeElement !== document.querySelector(".playground-live-frame")')
                    remember('native-editor-control-tab-exits')
                if route=='auto-suggest-box' and state()['preview']['hasOpenOverlay']:
                    # Suggestions now overlay the form like a native popup; dismiss before editing it.
                    page.keyboard.press('Escape')
                    frame.wait_for_function('!'+STATE+'.preview.hasOpenOverlay')
                edit('readOnly',True);click(.35,.55);page.keyboard.press('ControlOrMeta+A');page.keyboard.insert_text('blocked');assert state()['values'][name]=='真实输入 Abc';remember('readonly-blocks-input')
            elif route=='combobox':
                edit('editable',True);click(.3);page.keyboard.press('ControlOrMeta+A');page.keyboard.insert_text('Custom input');wait_value('currentText','Custom input');remember('native-editable-text')
                page.keyboard.press('Enter');page.wait_for_timeout(180);assert 'Custom input' in state()['values']['items'];remember('native-model-insertion')
            elif route=='number-box':
                click(.3,.75);page.keyboard.press('ControlOrMeta+A');page.keyboard.insert_text('37');page.keyboard.press('Enter')
                wait_value('value',37);remember('native-number-commit')
                page.keyboard.press('ControlOrMeta+A');page.keyboard.press('Backspace');page.keyboard.press('Enter')
                wait_value('value',None);remember('native-empty-number')
                assert 'quiet_NaN' in state()['cpp'] and 'float("nan")' in state()['python']
            elif route=='multi-select-combobox':
                edit('isOpen',True)
                click(widget=next(h for h in state()['preview']['hitTargets'] if h['action']=='model-item' and h['index']==0))
                wait_value('selectedRows',['Design']);remember('native-popup-selection')
            elif route=='tree-select':
                choices=page.locator('#playground-selectedKeys')
                expect(choices.locator('input[type="radio"]')).to_have_count(4)
                for key in ('Design','Engineering','Research','Design',''):
                    choices.locator(f'label:has(input[value="{key}"])').click()
                    wait_value('selectedKeys',[key] if key else [])
                    expect(choices.locator('input:checked')).to_have_count(1)
                    expect(choices.locator(f'input[value="{key}"]')).to_be_checked()
                    if key:
                        assert next(w for w in state()['preview']['controls'] if w.get('name')=='TreeSelectSingleInput')['text']==key
                        for language in ('cpp','python'):
                            line=next(line for line in state()[language].splitlines() if 'setSelectedKeys(' in line)
                            assert key in line and not any(other in line for other in ('Design','Engineering','Research') if other!=key)
                    remember('single-choice-'+(key or 'clear'))
                    if screenshots and key=='Engineering':page.locator('.playground-popover').screenshot(path=str(output/'tree-select-single.png'))
                edit('selectionMode','Multiple')
                expect(choices.locator('input[type="checkbox"]')).to_have_count(3)
                edit('selectedKeys',['Design','Engineering']);wait_value('selectedKeys',['Design','Engineering'])
                remember('multiple-choices-sync')
                edit('selectionMode','Single');wait_value('selectedKeys',['Design'])
                expect(choices.locator('input[type="radio"]')).to_have_count(4)
                choices.locator('label:has(input[value="Engineering"])').click();wait_value('selectedKeys',['Engineering'])
                remember('return-to-single-replaces-existing-selection')
                page.keyboard.press('ArrowRight');wait_value('selectedKeys',['Research'])
                remember('single-choice-keyboard-sync')
                click(.95);frame.wait_for_function(STATE+'.preview.hasOpenOverlay')
                click(widget=next(h for h in state()['preview']['hitTargets'] if h['action']=='model-item' and h['index']==0))
                wait_value('selectedKeys',['Design'])
                expect(choices.locator('input[value="Design"]')).to_be_checked()
                frame.wait_for_function('!'+STATE+'.preview.hasOpenOverlay');remember('native-tree-choice-sync')
                edit('readOnly',True);click(.95)
                assert not state()['preview']['hasOpenOverlay'] and state()['values']['selectedKeys']==['Design']
                remember('readonly-blocks-native-selection')
                edit('readOnly',False);edit('enabled',False);click(.95)
                assert not state()['preview']['hasOpenOverlay'] and state()['values']['selectedKeys']==['Design']
                remember('disabled-blocks-native-selection')
                reset();expect(choices.locator('input[value=""]')).to_be_checked();remember('reset-clears-single-choice')
                if screenshots:
                    page.locator('.playground-category-tab[data-tab="basic"]').click()
                    page.emulate_media(color_scheme='dark');page.set_viewport_size({'width':390,'height':800})
                    page.wait_for_timeout(180)
                    choices.scroll_into_view_if_needed()
                    box=page.locator('.playground-popover').bounding_box()
                    assert box['x']>=0 and box['x']+box['width']<=391 and box['y']+box['height']<=801
                    page.screenshot(path=str(output/'tree-select-dark-narrow.png'))
                    page.emulate_media(color_scheme='light');page.set_viewport_size({'width':1440,'height':1000})
            elif route=='image-viewer':
                before_zoom=state()['values']['zoomFactor']
                click(.4,.4);page.keyboard.down('Control');page.mouse.wheel(0,-120);page.keyboard.up('Control')
                frame.wait_for_function('before => '+STATE+'.values.fitMode === "Manual" && '+STATE+'.values.zoomFactor > before',arg=before_zoom)
                page.wait_for_timeout(250);remember('native-wheel-zoom')
                assert float(page.locator('#playground-zoomFactor').input_value()) == state()['values']['zoomFactor']
            elif route in ('slider','range-slider','annotated-scrollbar'):
                before_values=state()['values'];click(.45);page.keyboard.press('End');page.wait_for_timeout(160)
                assert state()['values']!=before_values,(route,'native click/key did not change value');remember('native-pointer-key')
            elif route=='dropdown-button':
                click(.95);frame.wait_for_function(STATE+'.preview.hasOpenOverlay');remember('native-menu-open')
                page.keyboard.press('Escape');page.wait_for_timeout(180);assert not state()['preview']['hasOpenOverlay'];remember('native-menu-escape')
                click(.95);frame.wait_for_function(STATE+'.preview.hasOpenOverlay');page.keyboard.press('ArrowDown');page.keyboard.press('Enter')
                frame.wait_for_function(STATE+'.events.length > 0');assert state()['events'][-1]['name'] in ('Copy','Share');remember('native-menu-action')
            elif route in ('pivot','selector-bar','stepper','pips-pager','flip-view'):
                name={'stepper':'currentIndex','pips-pager':'selectedPageIndex','flip-view':'currentIndex'}.get(route,'selectedIndex')
                before_value=state()['values'][name]
                hits=[h for h in state()['preview'].get('hitTargets',[]) if h['action']=='select' and h['index']!=before_value]
                click(widget=hits[0]) if hits else click(.85,.5)
                page.wait_for_timeout(200)
                if state()['values'][name]==before_value:page.keyboard.press('ArrowRight');page.wait_for_timeout(150)
                assert state()['values'][name]!=before_value,(route,'navigation did not change');remember('native-navigation')
            if route in ('split-button','toggle-split-button'):
                edit('enabled',True);click(.95);frame.wait_for_function(STATE+'.preview.hasOpenOverlay');remember('native-secondary-menu')
                page.keyboard.press('Escape');page.wait_for_timeout(180)
            if route in ('data-grid','file-list-view','flow-view','grid-view','list-view','timeline','tree-view'):
                if state()['values']['selectionMode']=='None':edit('selectionMode','Single')
                hits=[h for h in state()['preview'].get('hitTargets',[]) if h['action']=='model-item']
                assert hits, 'No visible model item'
                click(widget=hits[0]);frame.wait_for_function(STATE+'.values.selectedIndexes.length > 0');remember('native-model-selection')
                assert 'selectionModel()' in state()['cpp']
                edit('editable',True)
                hit=next(h for h in state()['preview']['hitTargets'] if h['action']=='model-item')
                click(.25,.5,hit);page.keyboard.press('F2')
                page.keyboard.press('ControlOrMeta+A');page.keyboard.insert_text('Native row');page.keyboard.press('Enter')
                frame.wait_for_function(STATE+'.values.modelData.some(row=>row[0] === "Native row")')
                assert 'Native row' in state()['cpp'] and 'Native row' in state()['python']
                remember('native-cell-edit-sync-both-exports')
                edit('editable',False);click(.25,.5,hit);page.keyboard.press('F2');page.keyboard.insert_text('blocked')
                assert not any('blocked' in row[0] for row in state()['values']['modelData'])
                remember('editing-disabled-preserves-data')
            if route=='data-grid':
                edit('sortingEnabled',True)
                before_order=state()['values']['sortOrder']
                t=state()['preview']['target']
                click(.2,15/t['height'])
                frame.wait_for_function('old=>'+STATE+'.values.sortOrder!==old',arg=before_order)
                assert 'sortByColumn(' in state()['cpp'] and 'sortByColumn(' in state()['python']
                remember('native-header-sort-sync')
            if route=='tree-view':
                hit=next(h for h in state()['preview']['hitTargets'] if h['action']=='model-item')
                click(22/hit['width'],.5,hit)
                frame.wait_for_function(STATE+'.values.expandedIndexes.length < 2')
                # Exercise two single-click actions, not Qt's separate double-click gesture.
                page.wait_for_timeout(550)
                click(22/hit['width'],.5,hit)
                frame.wait_for_function(STATE+'.values.expandedIndexes.length === 2')
                click(.25,.5,hit);page.keyboard.press('ArrowLeft')
                frame.wait_for_function(STATE+'.values.expandedIndexes.length < 2')
                page.keyboard.press('ArrowRight')
                frame.wait_for_function(STATE+'.values.expandedIndexes.length === 2')
                remember('native-tree-pointer-and-keyboard-collapse-expand-sync')
            if route in ('file-list-view','flow-view','grid-view','list-view','timeline','tree-view'):
                scroll_spec = importlib.util.spec_from_file_location('playground_scroll_audit', Path(__file__).with_name('playground_scroll_audit.py'))
                scroll_audit = importlib.util.module_from_spec(scroll_spec)
                scroll_spec.loader.exec_module(scroll_audit)
                scroll_audit.verify_collection_scroll(page, frame, state, edit, reset, point, remember,
                                                      route, output, screenshots)
            if route=='transfer':
                source_list=next(w for w in state()['preview']['controls'] if w['name']=='Transfer.Source.List')
                hit=next(h for h in state()['preview']['hitTargets'] if h['action']=='model-item'
                         and source_list['x']<=h['x']<source_list['x']+source_list['width']
                         and source_list['y']<=h['y']<source_list['y']+source_list['height'])
                before_keys=state()['values']['selectedKeys'];click(widget=hit)
                click(widget=next(w for w in state()['preview']['controls'] if w['name']=='Transfer.Add'))
                frame.wait_for_function('n=>'+STATE+'.values.selectedKeys.length===n',arg=len(before_keys)+1)
                remember('native-transfer-add-sync')
                search=next(w for w in state()['preview']['controls'] if w['name']=='Transfer.Source.Search')
                click(widget=search);page.keyboard.insert_text('Native search');wait_value('sourceFilter','Native search')
                remember('native-transfer-search-sync')
            if route=='split-view':
                handle=next(w for w in state()['preview']['controls'] if w['class']=='QWidget' and w['width']==state()['values']['handleWidth'])
                x,y=point(widget=handle);before_size=state()['values']['paneSize0']
                page.mouse.move(x,y);page.mouse.down();page.mouse.move(x+45,y,steps=8);page.mouse.up()
                frame.wait_for_function('old=>'+STATE+'.values.paneSize0!==old',arg=before_size)
                remember('native-divider-drag-sync')
            if route=='drawer-view':
                click();page.keyboard.press('Escape');wait_value('isOpen',False)
                edit('isOpen',True)
                assert page.locator('.playground-preview-content').evaluate('(el)=>getComputedStyle(el).overflow')=='hidden'
                remember('native-drawer-dismiss-sync-and-contained-scrim')
                page.evaluate('''() => {
                    window.drawerFrames=[];window.traceDrawer=true;
                    const capture=()=>{const r=document.querySelector('.playground-preview').getBoundingClientRect();
                        drawerFrames.push([r.x,r.y,r.width,r.height]);if(traceDrawer)requestAnimationFrame(capture);};capture();
                }''')
                for edge in ('Left','Top','Right','Bottom'):
                    edit('edge',edge);page.wait_for_timeout(250)
                    click(widget=next(w for w in state()['preview']['controls'] if w['name']=='drawerClose'))
                    wait_value('isOpen',False);page.wait_for_timeout(250)
                    click(widget=next(w for w in state()['preview']['controls'] if w['name']=='drawerOpen'))
                    wait_value('isOpen',True);page.wait_for_timeout(250)
                    viewport=state()['preview'];surface=viewport['surface'];target=viewport['target']
                    assert target['x']>=surface['x'] and target['y']>=surface['y']
                    assert target['x']+target['width']<=surface['x']+surface['width']
                    assert target['y']+target['height']<=surface['y']+surface['height']
                    remember('native-drawer-buttons-'+edge.lower())
                frames=page.evaluate('() => {traceDrawer=false;return drawerFrames;}')
                assert all(max(f[i] for f in frames)-min(f[i] for f in frames)<=1 for i in range(4)), 'drawer animation changed preview layout'
                remember('drawer-animation-keeps-preview-layout')
                if screenshots:
                    edit('edge','Right');page.wait_for_timeout(250)
                    page.locator('.playground-popover').screenshot(path=str(output/'drawer-details-light.png'))
                    page.locator('.playground-preview').screenshot(path=str(output/'drawer-preview.png'))
                    click(widget=next(w for w in state()['preview']['controls'] if w['name']=='drawerClose'));wait_value('isOpen',False);page.wait_for_timeout(250)
                    page.locator('.playground-popover').screenshot(path=str(output/'drawer-background.png'))
                    edit('isOpen',True);edit('edge','Top');edit('drawerLength',120);page.wait_for_timeout(250)
                    page.locator('.playground-popover').screenshot(path=str(output/'drawer-short-top.png'))
                    page.mouse.move(*point(.8,.8));page.mouse.wheel(0,300);page.wait_for_timeout(250)
                    page.locator('.playground-popover').screenshot(path=str(output/'drawer-short-top-scrolled.png'))
                    reset();page.emulate_media(color_scheme='dark');page.wait_for_timeout(250)
                    page.locator('.playground-popover').screenshot(path=str(output/'drawer-details-dark.png'))
                    page.set_viewport_size({'width':390,'height':800});page.wait_for_timeout(250)
                    page.screenshot(path=str(output/'drawer-details-narrow.png'))
                    page.set_viewport_size({'width':1440,'height':1000});page.emulate_media(color_scheme='light')
            if 'seriesData' in initial['values'] and 'currentPoint' in initial['values']:
                click(.45,.6);page.keyboard.press('End')
                wait_value('currentPoint',len(state()['values']['seriesData'])-1)
                assert 'setCurrentPoint(' in state()['cpp'] and 'setCurrentPoint(' in state()['python']
                remember('native-chart-cursor-sync')
            if route=='tab-view':
                edit('tabsClosable',True);edit('addTabButtonVisible',True)
                count=len(state()['values']['tabs'])
                click(widget=next(h for h in state()['preview']['hitTargets'] if h['action']=='add-tab'))
                frame.wait_for_function('n=>'+STATE+'.values.tabs.length===n',arg=count+1);remember('native-tab-add')
                click(widget=next(h for h in state()['preview']['hitTargets'] if h['action']=='close-tab'))
                frame.wait_for_function('n=>'+STATE+'.values.tabs.length===n',arg=count);remember('native-tab-close')
            if route=='stack-view':
                click(widget=next(w for w in state()['preview']['controls'] if w.get('text')=='Open details'))
                wait_value('detailsOpen',True);page.wait_for_timeout(350);remember('native-stack-push')
                click(widget=next(w for w in state()['preview']['controls'] if w.get('text')=='Back'))
                wait_value('detailsOpen',False);page.wait_for_timeout(350);remember('native-stack-pop')
            if route=='property-panel':
                edit('readOnly',False)
                target=next(w for w in state()['preview']['controls'] if w['class'].endswith('LineEdit') and w.get('text')==state()['values']['value_title'])
                click(widget=target);page.keyboard.press('ControlOrMeta+A');page.keyboard.insert_text('Native form')
                wait_value('value_title','Native form');remember('native-property-form')
            # A successful setter/readback is not proof that its visible effect works.
            # Capture only the actual component, excluding event counters and editor chrome.
            def pixels(name, hovered=False):
                from PIL import Image, ImageDraw
                page.mouse.move(*(point() if hovered else (1400, 950)))
                page.wait_for_timeout(180)
                s=state(); t=s['preview']['target']; box=page.locator('.playground-live-frame').bounding_box()
                scale=box['width']/s['preview']['width']
                clip={'x':box['x']+t['x']*scale,'y':box['y']+t['y']*scale,
                      'width':t['width']*scale,'height':t['height']*scale}
                png=page.screenshot(clip=clip)
                if screenshots:(output/(route+'-'+name+'.png')).write_bytes(png)
                img=Image.open(io.BytesIO(png)).convert('RGB')
                if route=='button':
                    # The rounded corners expose the web stage's dotted background.
                    # Its phase changes when the dialog recenters; retain the straight
                    # focus-ring edges and text, but exclude those background corners.
                    corner=round(4*img.width/t['width'])
                    draw=ImageDraw.Draw(img)
                    for x in (0,img.width-corner):
                        for y in (0,img.height-corner):
                            draw.rectangle((x,y,x+corner-1,y+corner-1),fill='black')
                return img.size, img.tobytes()
            if route=='horizontal-bar-chart':
                reset();table=page.locator('#playground-seriesData')
                row=len(state()['values']['seriesData']);table.locator('.playground-data-add').click()
                frame.wait_for_function('n=>'+STATE+'.values.seriesData.length===n',arg=row+1)
                value=table.locator(f'input[data-row="{row}"][data-column="1"]')
                label=table.locator(f'input[data-row="{row}"][data-column="2"]')
                expect(value).to_have_value('0');value.focus();page.keyboard.press('End');value.press_sequentially('345')
                expect(value).to_have_value('345')
                frame.wait_for_function('r=>'+STATE+'.values.seriesData[r][1]===345',arg=row)
                remember('numeric-leading-zero-removed-during-typing')
                if screenshots:page.locator('.playground-popover').screenshot(path=str(output/'numeric-345.png'))
                value.fill('-00012.50');expect(value).to_have_value('-12.50')
                label.click();expect(value).to_have_value('-12.5')
                frame.wait_for_function('r=>'+STATE+'.values.seriesData[r][1]===-12.5',arg=row)
                value.fill('');value.press_sequentially('-0.05');expect(value).to_have_value('-0.05')
                frame.wait_for_function('r=>'+STATE+'.values.seriesData[r][1]===-0.05',arg=row)
                value.fill('');value.press_sequentially('0.05');expect(value).to_have_value('0.05')
                frame.wait_for_function('r=>'+STATE+'.values.seriesData[r][1]===0.05',arg=row)
                value.fill('');value.press_sequentially('1e2');label.click();expect(value).to_have_value('100')
                frame.wait_for_function('r=>'+STATE+'.values.seriesData[r][1]===100',arg=row)
                remember('numeric-negative-decimal-and-exponent-editing')
                value.fill('');label.fill('0345')
                frame.wait_for_function('r=>'+STATE+'.values.seriesData[r][1]===null && '+STATE+'.values.seriesData[r][2]==="0345"',arg=row)
                expect(value).to_have_value('');expect(label).to_have_value('0345')
                remember('numeric-empty-gap-and-text-leading-zero-preserved')
            if route in ('chart-view','data-grid'):
                reset();name='seriesData' if route=='chart-view' else 'modelData'
                original_data=state()['values'][name]
                changed=[list(row) for row in original_data]
                changed[0][1 if route=='chart-view' else 0]=73 if route=='chart-view' else 'Panel row'
                before_pixels=pixels('data-before');edit(name,changed)
                assert pixels('data-after')!=before_pixels, 'model data did not change the preview'
                if route=='chart-view':
                    editor=page.locator('#playground-seriesData input[data-row="1"][data-column="0"]')
                    editor.fill(str(changed[0][0]-1))
                    assert not editor.evaluate('(el)=>el.checkValidity()')
                    assert state()['values']['seriesData']==changed, 'invalid X order reached the native model'
                    editor.fill(str(changed[1][0]));wait_value(name,changed)
                    changed[0][1]=None;edit(name,changed)
                    assert 'quiet_NaN' in state()['cpp'] and 'float("nan")' in state()['python']
                edit(name,[]);assert state()['values'][name]==[]
                if route=='data-grid':expect(page.locator('[data-property="placeholderText"]')).to_be_visible()
                reset();assert state()['values'][name]==original_data
                remember('data-pixels-empty-reset-and-validation')
            if 'icon' in initial['values'] and 'iconRotation' in initial['values']:
                reset();edit('icon','None')
                expect(page.locator('[data-property="fluentLayout"]')).to_be_hidden()
                for name in ('iconRotation','iconScale'):
                    expect(page.locator('[data-property="'+name+'"]')).to_be_hidden()
                    expect(page.locator('#playground-'+name)).to_be_disabled()
                if route=='dropdown-button':
                    expect(page.locator('[data-group="dropdown-indicator"]')).to_be_visible()
                    expect(page.locator('#playground-chevronSize')).to_be_enabled()
                    if screenshots:page.locator('.playground-popover').screenshot(path=str(output/'dropdown-no-main-icon.png'))
                    edit('chevronGlyph','None')
                    expect(page.locator('[data-property="chevronSize"]')).to_be_hidden()
                    edit('chevronGlyph','ChevronDown')
                edit('icon','Save');edit('fluentLayout','IconAbove');edit('iconRotation',0)
                icon_before=pixels('icon-transform-before');edit('iconRotation',90)
                assert pixels('icon-transform-after')!=icon_before, 'visible icon rotation has no pixel effect'
                edit('fluentLayout','TextOnly')
                expect(page.locator('[data-property="iconRotation"]')).to_be_hidden()
                edit('icon','None');assert state()['values']['iconRotation']==90
                edit('icon','Save');edit('fluentLayout','IconAbove')
                expect(page.locator('[data-property="iconRotation"]')).to_be_visible()
                assert float(page.locator('#playground-iconRotation').input_value())==90
                if route=='dropdown-button' and screenshots:
                    page.locator('.playground-popover').screenshot(path=str(output/'dropdown-with-main-icon.png'))
                remember('icon-group-prerequisites-preserve-values-and-pixels')
                layouts=['TextOnly','IconBefore','IconAfter','IconOnly','IconAbove','IconBelow']
                layout_field=next(f for f in state()['schema']['properties'] if f['id']=='fluentLayout')
                assert len(layout_field['options'])==6 and set(layout_field['options'])==set(layouts), (route,layout_field['options'])
                for placement in layouts:
                    edit('fluentLayout',placement)
                    assert placement in state()['cpp'] and placement in state()['python']
                edit('fluentLayout','IconBefore');edit('iconRotation',0)
                horizontal=pixels('horizontal-icon-default')
                edit('iconRotation',90)
                assert pixels('horizontal-icon-rotated')!=horizontal
                edit('iconRotation',0);edit('iconScale',0.6)
                assert pixels('horizontal-icon-scaled')!=horizontal
                edit('iconScale',1);edit('iconOffsetX',5);edit('iconOffsetY',-3)
                assert pixels('horizontal-icon-offset')!=horizontal
                assert 'setIconOffset(QPoint(5, -3))' in state()['cpp']
                assert 'setIconOffset(QPoint(5, -3))' in state()['python']
                remember('all-six-layouts-and-horizontal-icon-transforms')
                reset()
            elif route=='font-icon':
                reset();edit('glyph','None')
                for name in ('iconSize','rotation','color'):
                    expect(page.locator('[data-property="'+name+'"]')).to_be_hidden()
                edit('glyph','Save')
                for name in ('iconSize','rotation','color'):
                    expect(page.locator('[data-property="'+name+'"]')).to_be_visible()
                remember('font-icon-group-requires-glyph');reset()
                page.locator('.playground-close').click();page.locator('[data-playground-toggle]').click()
                expect(page.locator('.playground-copy')).to_be_enabled(timeout=60000)
                frame=page.locator('.playground-live-frame').element_handle().content_frame()
                expect(page.locator('.playground-category-tab[data-tab="icon"]')).to_have_attribute('aria-selected','true')
                expect(page.locator('[data-property="glyph"]')).to_be_visible()
                remember('reopen-selects-first-populated-category')
            elif route=='auto-suggest-box':
                reset();edit('queryIconVisible',False)
                expect(page.locator('[data-property="queryButtonPlacement"]')).to_be_hidden()
                edit('queryIconVisible',True)
                expect(page.locator('[data-property="queryButtonPlacement"]')).to_be_visible()
                remember('query-placement-requires-query-icon');reset()
            if route=='button':
                reset(); edit('checkable',False)
                for theme in ('light','dark'):
                    page.emulate_media(color_scheme=theme)
                    expect(page.locator('.playground-popover')).to_have_attribute('data-theme',theme)
                    for style in ('Standard','Accent','Subtle'):
                        edit('enabled',True); edit('fluentStyle',style); edit('focusVisual',False)
                        off=pixels(theme+'-'+style+'-idle-off')
                        edit('focusVisual',True)
                        expect(page.locator('#playground-focusVisual-note')).to_contain_text('Tab')
                        idle=pixels(theme+'-'+style+'-idle-on')
                        assert idle==off, 'focusVisual must not focus an idle button'
                        click()
                        focused=pixels(theme+'-'+style+'-focused')
                        assert any(w['focused'] for w in state()['preview']['controls']), 'real Qt focus missing'
                        assert focused!=idle, 'focus ring has no visible effect after native focus'
                        # Moving focus back to the editor must remove the cue without clearing the property.
                        page.locator('.playground-category-tab[data-tab="appearance"]').click()
                        assert pixels(theme+'-'+style+'-blurred')==idle, 'focus ring remained after blur'
                        page.locator('.playground-close').focus();page.keyboard.press('Tab')
                        assert pixels(theme+'-'+style+'-keyboard-focused')==focused, 'Tab did not focus the real preview'
                        if screenshots and theme=='light' and style=='Subtle':
                            page.locator('.playground-popover').screenshot(path=str(output/'button-focus-guidance.png'))
                        edit('enabled',False)
                        disabled=pixels(theme+'-'+style+'-disabled-on')
                        edit('focusVisual',False)
                        assert pixels(theme+'-'+style+'-disabled-off')==disabled, 'disabled button painted a focus ring'
                page.emulate_media(color_scheme='light')
                remember('focus-ring-pixels-rest-focused-blurred-disabled-light-dark-all-styles')
            elif route in ('hyperlink-button','checkbox'):
                reset()
                name='showUnderline' if route=='hyperlink-button' else 'hoverBackgroundEnabled'
                edit(name,False);rest=pixels('rest-off');hover=pixels('hover-off',True)
                edit(name,True)
                assert pixels('rest-on')==rest, 'hover option changed the resting appearance'
                assert pixels('hover-on',True)!=hover, 'hover option has no visible effect while hovered'
                remember('hover-option-pixels-rest-and-hover')
            elif route=='toggle-switch':
                reset();edit('isOn',False)
                mode=page.locator('.playground-text-mode')
                expect(mode).to_have_attribute('aria-expanded','false')
                expect(page.locator('[data-property="offContent"]')).to_be_hidden()
                # One ordinary edit must survive a genuine preview click in either direction.
                shared=page.locator('#playground-onContent')
                shared.fill('自动保存')
                wait_value('onContent','自动保存');wait_value('offContent','自动保存')
                click();wait_value('isOn',True)
                expect(shared).to_have_value('自动保存')
                assert state()['values']['onContent']==state()['values']['offContent']=='自动保存'
                assert 'setOnContent(QStringLiteral("自动保存"))' in state()['cpp']
                assert 'setOffContent(QStringLiteral("自动保存"))' in state()['cpp']
                click();wait_value('isOn',False)
                expect(shared).to_have_value('自动保存')
                # Separate labels are explicit and survive returning to shared editing.
                mode.click();settled()
                expect(mode).to_have_attribute('aria-expanded','true')
                expect(page.locator('[data-property="offContent"]')).to_be_visible()
                edit('onContent','Active');edit('offContent','Stopped')
                mode.click();settled()
                assert state()['values']['onContent']==state()['values']['offContent']=='自动保存'
                mode.click();settled()
                expect(shared).to_have_value('Active')
                expect(page.locator('#playground-offContent')).to_have_value('Stopped')
                edit('onContent','');edit('isOn',True)
                assert state()['values']['onContent']=='', 'separate mode must preserve intentional empty text'
                mode.click();settled()
                shared.fill('');wait_value('onContent','');wait_value('offContent','')
                shared.fill('Keep text')
                page.locator('#playground-isOn [data-value="false"]').click()
                wait_value('isOn',False);wait_value('onContent','Keep text');wait_value('offContent','Keep text')
                off_radio=page.locator('#playground-isOn [data-value="false"]');off_radio.focus()
                page.keyboard.press('ArrowRight');wait_value('isOn',True)
                edit('enabled',False);click();assert state()['values']['isOn']
                remember('shared-switch-text-survives-toggle-and-separate-mode-preserves-labels')
            elif route=='progress-bar':
                reset();edit('showPaused',True);edit('isIndeterminate',True)
                expect(page.locator('#playground-value')).to_be_disabled()
                expect(page.locator('#playground-value-note')).to_be_visible()
                edit('isIndeterminate',False)
                expect(page.locator('#playground-value')).to_be_enabled()
                before_pixels=pixels('determinate-before')
                edit('value',75)
                assert pixels('determinate-after')!=before_pixels
                edit('isIndeterminate',True)
                expect(page.locator('#playground-value')).to_be_disabled()
                assert state()['values']['value']==75
                remember('determinate-value-pixels-and-indeterminate-condition')
            elif route=='command-bar':
                reset();edit('labelPosition','Right');with_labels=pixels('labels-right')
                edit('labelPosition','Collapsed')
                assert pixels('labels-collapsed')!=with_labels, 'command fixture needs icons to demonstrate label collapsing'
                assert 'setIcon(' in state()['cpp'] and 'setIcon(' in state()['python'], 'icon setup missing from an export'
                remember('command-label-pixels-and-icon-export')
            reset()
            # Every schema field remains reachable and agrees with Qt after property edits.
            fields=initial['schema']['properties']
            for f in fields if exhaustive else []:
                name=f['id'];s=reset()
                prerequisites=(
                    'icon','glyph','chevronGlyph','queryIconVisible','fluentLayout',
                    'enabled','readOnly','isOpen','searchEnabled','isActive',
                    'indeterminate','isIndeterminate','showError','active','animationEnabled',
                    'displayMode','presentation','transitionAnimationEnabled','transitionType',
                    'autoResizeEnabled','wordWrap','tickPosition','sortingEnabled','dim',
                    'interactive','selectionMode','editable','checkable','compactThreshold',
                    'breadcrumbSize','labelPosition','validationMessage','initials',
                    'date','startDateTime','endDateTime','monthVisible','dayVisible',
                    'yearVisible','tabsClosable','alwaysExpanded','currentIndex','text',
                    'password','plainText','searchText','selectedRows','selectedKeys',
                    'selectedItems','value',
                )
                fixture=f.get('effectFixture',{})
                for key in sorted(fixture, key=lambda key: prerequisites.index(key) if key in prerequisites else len(prerequisites)):
                    s=edit(key,fixture[key])
                if f.get('enabledWhen')=='checkable':s=edit('checkable',True)
                if f.get('enabledWhen')=='editable':s=edit('editable',True)
                if f.get('adapter')=='model-selection' and s['values']['selectionMode']=='None':s=edit('selectionMode','Single')
                if f.get('enabledWhen') in ('hasIcon','hasVisibleIcon','stackedIcon'):
                    s=edit('icon','Save')
                    if f.get('enabledWhen')!='hasIcon':s=edit('fluentLayout','IconAbove')
                f=next(item for item in s['schema']['properties'] if item['id']==name)
                category=page.locator('[data-property="'+name+'"]').evaluate('(el)=>el.closest("[role=tabpanel]").id.replace("panel-category-", "")')
                page.locator(f'.playground-category-tab[data-tab="{category}"]').click()
                input=page.locator('#playground-'+name)
                if input.is_disabled():
                    # A dependency or singleton range can correctly prevent this edit.
                    seen.add(name);continue
                kind=f['type'];v=s['values'][name]
                if kind=='enum' or (kind=='text' and f.get('options')): variants=f['options']
                elif f.get('control')=='model-select':variants=[c['value'] for c in f['choices'] if c.get('enabled',True)]
                elif kind=='table':
                    variant=[list(row) for row in v]
                    if variant:
                        if f.get('adapter')=='chart-data':variant[0][1]=(variant[0][1] or 0)+13
                        else:variant[0][0]='Edited row'
                    variants=[variant]
                elif kind=='boolean':variants=[not v]
                elif kind=='multi-enum':variants=[[option] for option in f['options']] if f['options'] else [[]]
                elif kind=='string-list':variants=[['First','Second']]
                elif kind=='number':variants=[f['maximum'] if f['maximum']!=v else f['minimum']]
                elif kind=='color':variants=['#ffed4b2e']
                elif kind=='date':variants=['2026-12-25']
                elif kind=='time':variants=['14:30:00']
                elif kind=='datetime':variants=['2026-12-25T14:30:00']
                else:variants=[('Edited 文字' if name!='language' else 'python')[:f.get('maximumLength',320)]]
                for variant in variants:
                    updated=edit(name,variant)
                    assert name in updated['values'];assert updated['cpp'] and updated['python']
                    if kind=='enum' or f.get('control')=='model-select':assert updated['values'][name]==variant,(name,variant,updated['values'][name])
                    if kind=='string-list':
                        input.evaluate('(el)=>{el.focus();const n=el.value.length;el.setSelectionRange(n,n);}')
                        page.keyboard.press('Enter');page.wait_for_timeout(180)
                        assert input.input_value().endswith('\n'),(name,input.input_value())
                        page.keyboard.type('Third');updated=wait_value(name,['First','Second','Third'])
                        assert input.input_value().endswith('\nThird'),(name,input.input_value())
                    cases.append({'case':route+'/property/'+name,'requested':variant,'state':updated})
                seen.add(name)
            reset()
            if exhaustive:checked.append('all-property-editors')
            if screenshots and route in ('button','rating-control','combobox','multi-select-combobox','range-slider','property-panel','data-grid','dropdown-button'):
                page.locator('.playground-popover').screenshot(path=str(output/(route+'.png')))
            # Native and property edits must not reconfigure the original sample.
            after=original.evaluate('JSON.parse(document.documentElement.dataset.fluentQtComponentState)')
            assert after['controls']==before['controls'], 'original controls changed'
            assert after['clicks']==before['clicks'], 'original click count changed'
            assert page.locator('.document-main .code-example').first.inner_text()==source
            checked.append('original-unchanged')
            if route=='button':
                for language in ('cpp','python'):
                    page.locator(f'.playground-code-tab[data-lang="{language}"]').click()
                    page.locator('.playground-copy').click()
                    page.wait_for_function('async text => (await navigator.clipboard.readText()) === text',arg=state()[language])
                checked.append('exact-fresh-copy-both-languages')
                click();page.keyboard.press('Tab');page.wait_for_timeout(80)
                assert page.evaluate('document.activeElement !== document.querySelector(".playground-live-frame")'), 'Tab did not exit Qt canvas'
                click();page.keyboard.press('Shift+Tab');page.wait_for_timeout(80)
                assert page.evaluate('document.activeElement !== document.querySelector(".playground-live-frame")'), 'Shift+Tab did not exit Qt canvas'
                for _ in range(18):
                    page.keyboard.press('Tab')
                    assert page.evaluate('document.querySelector(".playground-popover").contains(document.activeElement)')
                checked.append('native-tab-boundaries-and-modal-focus')
                for theme in ('dark','light'):
                    page.emulate_media(color_scheme=theme)
                    expect(page.locator('.playground-popover')).to_have_attribute('data-theme',theme)
                    page.wait_for_timeout(120)
                    if screenshots:page.locator('.playground-popover').screenshot(path=str(output/f'button-{theme}-desktop.png'))
                page.set_viewport_size({'width':390,'height':800})
                page.wait_for_timeout(150)
                box=page.locator('.playground-popover').bounding_box()
                assert box['x']>=0 and box['x']+box['width']<=391 and box['y']>=0 and box['y']+box['height']<=801,box
                if screenshots:page.screenshot(path=str(output/'button-narrow.png'))
                page.emulate_media(forced_colors='active',reduced_motion='reduce')
                expect(page.locator('.playground-popover')).to_have_attribute('data-theme','high-contrast')
                if screenshots:page.screenshot(path=str(output/'button-high-contrast-narrow.png'))
                page.emulate_media(forced_colors='none',reduced_motion='no-preference',color_scheme='light')
                page.set_viewport_size({'width':1440,'height':1000})
                checked.append('light-dark-narrow-high-contrast')
            page.locator('.playground-close').click();expect(page.locator('.playground-live-frame')).to_have_count(0)
            expect(page.locator('[data-playground-toggle]')).to_be_focused();checked.append('runtime-disposed-focus-restored')
            assert not errors[start_errors:],errors[start_errors:]
            results.append({'route':route,'status':'pass','fields':sorted(seen),'checks':checked})
        except Exception as error:
            try:(output/(route+'-failure.json')).write_text(json.dumps({'console':console[-30:],'status':page.locator('.playground-status').text_content(),'state':state()},ensure_ascii=False,indent=2),encoding='utf-8')
            except Exception:pass
            try:page.screenshot(path=str(output/(route+'-failure.png')))
            except Exception:pass
            results.append({'route':route,'status':'fail','error':traceback.format_exc(),'fields':sorted(seen),'checks':checked})
        print(route,results[-1]['status'],results[-1].get('error','')[:220],flush=True)
        (output/'result.json').write_text(json.dumps({'results':results,'errors':errors},ensure_ascii=False,indent=2),encoding='utf-8')
        (output/'cases.json').write_text(json.dumps({'cases':cases},ensure_ascii=False,indent=2),encoding='utf-8')
    context.close()
    failures=[r for r in results if r['status']!='pass']
    if failures:raise AssertionError(f'{len(failures)} playground contracts failed: '+', '.join(r['route'] for r in failures))
    return results
