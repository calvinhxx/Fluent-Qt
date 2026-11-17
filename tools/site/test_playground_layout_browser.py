#!/usr/bin/env python3
"""Check property reachability and stable layout around real Qt preview input."""
import argparse
import importlib.util
import json
from pathlib import Path
from urllib.parse import urljoin

ROOT = Path(__file__).resolve().parents[2]
STATE = 'JSON.parse(document.documentElement.dataset.fluentQtPlaygroundState)'
GEOMETRY = '''() => Object.fromEntries(['.playground-popover', '.playground-preview', '.playground-category-tabs', '.playground-category-panels', '.playground-live-frame'].map(selector => {
  const e = document.querySelector(selector), r = e.getBoundingClientRect();
  return [selector, {x:r.x,y:r.y,width:r.width,height:r.height}];
}))'''


def run(browser, base_url, output, screenshots=False):
    from playwright.sync_api import expect
    output.mkdir(parents=True, exist_ok=True)
    context = browser.new_context(viewport={'width':1440,'height':900}, device_scale_factor=2, color_scheme='light')
    page = context.new_page(); errors=[]; checks=[]
    page.on('pageerror', lambda error: errors.append(str(error)))

    def open_route(route):
        page.goto(urljoin(base_url, 'zh-CN/api/'+route+'/?playground=1'))
        expect(page.locator('.playground-popover')).to_have_attribute('data-preview-state','ready',timeout=90000)
        expect(page.locator('.playground-copy')).to_be_enabled(timeout=90000)
        page.wait_for_timeout(300)  # Finish the one-time dialog entrance, before tracing interactions.
        return page.locator('.playground-live-frame').element_handle().content_frame()

    def capture(name):
        if screenshots:page.screenshot(path=str(output/(name+'.png')))

    def assert_reachable():
        panel=page.locator('.playground-tabpanel:not([hidden])')
        info=panel.evaluate('e=>({height:e.clientHeight,scroll:e.scrollHeight,overflow:getComputedStyle(e).overflowY,gutter:getComputedStyle(e).scrollbarGutter})')
        bounds=panel.bounding_box(); modal=page.locator('.playground-popover').bounding_box()
        assert info['height']>=120, info
        assert bounds['y']+bounds['height']<=modal['y']+modal['height']-8, (bounds,modal)
        assert info['overflow']=='auto' and info['gutter']=='stable', info
        first=panel.locator('.playground-field').first.bounding_box()
        assert first['y']<bounds['y']+bounds['height'], (first,bounds)
        return panel,info

    def point(frame, fx=.9, fy=.5, widget=None):
        state=frame.evaluate(STATE); target=widget or state['preview']['target']
        box=page.locator('.playground-live-frame').bounding_box();scale=box['width']/state['preview']['width']
        return box['x']+(target['x']+target['width']*fx)*scale,box['y']+(target['y']+target['height']*fy)*scale

    def start_trace():
        page.evaluate('''geometry => {
          window.layoutSamples=[]; window.layoutTracing=true;
          window.paintClips=[];
          const frame=document.querySelector('.playground-live-frame');
          if (!frame.originalViewportCallback) {
            frame.originalViewportCallback=frame.fluentQtViewport;
            frame.fluentQtViewport=(session, viewport)=>{
              frame.originalViewportCallback(session,viewport);
              if (window.layoutTracing && viewport.hasVisibleOverlay) {
                const insets=getComputedStyle(frame).clipPath.match(/[\d.]+/g).map(Number);
                const clip=insets.length>2?insets[2]:insets[0];
                const available=parseFloat(frame.style.height)-clip;
                paintClips.push({available,needed:viewport.height,open:viewport.hasOpenOverlay,overflow:getComputedStyle(document.querySelector('.playground-preview')).overflow});
              }
            };
          }
          const measure=eval('('+geometry+')');
          const sample=()=>{layoutSamples.push(measure());if(layoutTracing)requestAnimationFrame(sample);}; sample();
        }''',GEOMETRY)

    def finish_trace(label):
        page.wait_for_timeout(300)
        samples=page.evaluate('layoutTracing=false; layoutSamples')
        assert len(samples)>2
        for selector in samples[0]:
            for key in ('x','y','width','height'):
                values=[s[selector][key] for s in samples]
                assert max(values)-min(values)<1, (label,selector,key,min(values),max(values))
        paint_clips=page.evaluate('paintClips')
        for clip in paint_clips:
            assert clip['overflow']=='visible' and clip['available']>=clip['needed'], (label,clip)
        if 'combobox-select' in label:
            assert paint_clips, 'Popup was not synchronized before paint'
        checks.append({'case':label,'frames':len(samples),'paintClips':paint_clips,'geometry':samples[-1]})

    try:
        frame=open_route('color-picker')
        assert_reachable();capture('color-picker-desktop')
        start_trace()
        page.locator('#playground-alphaEnabled').uncheck();expect(page.locator('.playground-copy')).to_be_enabled()
        page.locator('#playground-alphaEnabled').check();expect(page.locator('.playground-copy')).to_be_enabled()
        finish_trace('color-picker-alpha-keeps-layout')
        page.emulate_media(color_scheme='dark');page.wait_for_timeout(180);capture('color-picker-dark')
        page.emulate_media(color_scheme='light')
        for name,width,height in [('short',1100,600),('narrow',390,720)]:
            page.set_viewport_size({'width':width,'height':height});page.wait_for_timeout(200)
            assert_reachable();capture('color-picker-'+name)
            checks.append({'case':'color-picker-properties-visible-'+name})
        page.set_viewport_size({'width':1440,'height':900})
        frame=open_route('combobox')
        for index in (1,2,0):
            start_trace();page.mouse.click(*point(frame))
            frame.wait_for_function(STATE+'.preview.hasOpenOverlay',timeout=10000)
            if index==1:capture('combobox-popup')
            state=frame.evaluate(STATE)
            item=next(t for t in state['preview']['hitTargets'] if t['action']=='model-item' and t['index']==index)
            page.mouse.click(*point(frame,.5,.5,item))
            frame.wait_for_function('i=>'+STATE+'.values.currentIndex===i && !'+STATE+'.preview.hasOpenOverlay',arg=index)
            finish_trace('combobox-select-'+str(index))
        page.set_viewport_size({'width':1100,'height':600});page.wait_for_timeout(200)
        panel,info=assert_reachable()
        assert info['scroll']>info['height'],info
        box=panel.bounding_box();page.mouse.move(box['x']+box['width']-20,box['y']+box['height']/2)
        page.mouse.wheel(0,800)
        page.wait_for_function('document.querySelector(".playground-tabpanel:not([hidden])").scrollTop > 0')
        expect(page.locator('#playground-contentPaddingH')).to_be_in_viewport()
        capture('combobox-scrolled');checks.append({'case':'overflow-scrollbar-and-last-property-reachable'})
        page.set_viewport_size({'width':1440,'height':900})
        frame=open_route('button');start_trace()
        for _ in range(3):page.mouse.click(*point(frame,.5));page.wait_for_timeout(80)
        finish_trace('button-click-feedback-keeps-layout')
        for route in ('dropdown-button','tree-select','date-picker'):
            if route=='date-picker':page.set_viewport_size({'width':1100,'height':600})
            frame=open_route(route);start_trace();page.mouse.click(*point(frame))
            frame.wait_for_function(STATE+'.preview.hasOpenOverlay',timeout=10000)
            state=frame.evaluate(STATE);box=page.locator('.playground-live-frame').bounding_box()
            scale=box['width']/state['preview']['width']
            bottom=max(w['y']+w['height'] for w in state['preview']['controls'])
            assert bottom*scale<=box['height']+1, (route,bottom,scale,box)
            page.keyboard.press('Escape')
            frame.wait_for_function('!'+STATE+'.preview.hasOpenOverlay',timeout=10000)
            finish_trace(route+'-popup-keeps-layout')
        assert not errors,errors
        (output/'result.json').write_text(json.dumps({'status':'pass','checks':checks,'errors':errors},indent=2)+'\n')
    except Exception:
        page.screenshot(path=str(output/'failure.png'))
        (output/'failure.json').write_text(json.dumps({'checks':checks,'geometry':page.evaluate(GEOMETRY),'errors':errors},indent=2)+'\n')
        raise
    finally:context.close()


def main():
    from playwright.sync_api import sync_playwright
    spec=importlib.util.spec_from_file_location('browser_contract',Path(__file__).with_name('test_component_playground_browser.py'))
    contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wasm-root',type=Path,default=ROOT/'build/wasm')
    parser.add_argument('--output',type=Path,default=ROOT/'build/playground-layout/browser')
    parser.add_argument('--screenshots',action='store_true');args=parser.parse_args()
    with contract.website_server(args.wasm_root.resolve()) as url, sync_playwright() as p:
        browser=p.chromium.launch(headless=True,ignore_default_args=['--hide-scrollbars'])
        try:run(browser,url,args.output,args.screenshots)
        finally:browser.close()


if __name__=='__main__':main()
