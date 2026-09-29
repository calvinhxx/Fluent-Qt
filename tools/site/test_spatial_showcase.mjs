import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const source = await readFile(new URL('../../site/spatial-showcase.js', import.meta.url), 'utf8');
const { createSpatialShowcase } = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);

class Target {
  listeners = new Map();
  dataset = {};
  attrs = {};
  addEventListener(type, fn) {
    if (!this.listeners.has(type)) this.listeners.set(type, new Set());
    this.listeners.get(type).add(fn);
  }
  removeEventListener(type, fn) { this.listeners.get(type)?.delete(fn); }
  emit(type, event = {}) { this.listeners.get(type)?.forEach(fn => fn(event)); }
  setAttribute(key, value) { this.attrs[key] = value; }
  removeAttribute(key) { delete this[key]; }
  focus() { this.focused = true; }
}

function fixture() {
  const root = new Target(), document = new Target(), window = new Target();
  const frame = new Target(), start = new Target(), stop = new Target();
  const message = new Target(), detail = new Target(), media = new Target();
  const messages = [], timers = new Map();
  let observer, theme = 'light', timerId = 0;
  root.ownerDocument = document;
  document.defaultView = window;
  root.dataset.showcaseSrc = '../gallery/?embed=site&showcase=spatial&render-scale=native';
  root.querySelector = selector => ({
    '[data-showcase-frame]': frame, '[data-showcase-start]': start, '[data-showcase-stop]': stop,
    '[data-showcase-message]': message, '[data-showcase-detail]': detail
  })[selector];
  window.location = { origin: 'https://site.test', href: 'https://site.test/zh-CN/' };
  window.matchMedia = () => media;
  window.setTimeout = fn => { timers.set(++timerId, fn); return timerId; };
  window.clearTimeout = id => timers.delete(id);
  window.IntersectionObserver = class {
    constructor(fn) { observer = fn; }
    observe() {}
    disconnect() { observer = null; }
  };
  frame.contentWindow = { postMessage: (data, origin) => messages.push({ ...data, origin }) };
  frame.contentDocument = { querySelector: () => ({}) };
  const controller = createSpatialShowcase(root, { theme: () => theme, strings: () => new Proxy({}, {get: (_, key) => key}) });
  function receive(state, session = new URL(frame.src).searchParams.get('host-session'), overrides = {}) {
    window.emit('message', {origin: window.location.origin, source: frame.contentWindow,
      data: {source: 'fluent-qt-gallery', state, session}, ...overrides});
  }
  return { root, frame, start, stop, window, document, media, messages, timers, controller, receive,
    visible: value => observer([{ isIntersecting: value }]), theme: value => { theme = value; controller.syncTheme(); } };
}

test('Spatial is opt-in and the initial URL retains native pixel density and host theme', () => {
  const f = fixture();
  f.visible(true);
  assert.equal(f.frame.src, undefined);
  f.theme('dark');
  f.start.emit('click');
  const url = new URL(f.frame.src);
  assert.equal(url.pathname, '/gallery/');
  assert.equal(url.searchParams.get('render-scale'), 'native');
  assert.equal(url.searchParams.get('host-theme'), 'dark');
  assert.equal(f.root.dataset.showcaseState, 'loading');
  f.controller.destroy();
});

test('only the current same-origin frame session can report readiness', () => {
  const f = fixture(); f.start.emit('click');
  f.receive('ready', '1', {origin:'https://elsewhere.test'});
  f.receive('ready', '1', {source:{}});
  f.receive('ready', 'old');
  assert.equal(f.root.dataset.showcaseState, 'loading');
  f.receive('ready');
  assert.equal(f.root.dataset.showcaseState, 'ready');
  assert.equal(f.frame.tabIndex, 0);
  assert.equal(f.frame.attrs['aria-hidden'], 'false');
  assert.equal(f.timers.size, 0);
  f.controller.destroy();
});

test('theme, reduced motion, viewport and document visibility reach the real Qt host', () => {
  const f = fixture(); f.start.emit('click'); f.receive('ready');
  f.visible(true);
  assert.equal(f.messages.at(-1).active, true);
  f.document.hidden = true; f.document.emit('visibilitychange');
  assert.equal(f.messages.at(-1).active, false);
  f.theme('high-contrast');
  assert.equal(f.messages.at(-1).theme, 'high-contrast');
  f.media.matches = true; f.media.emit('change');
  assert.equal(f.messages.at(-1).reduced, true);
  assert.ok(f.messages.every(value => value.origin === 'https://site.test'));
  f.controller.destroy();
});

test('stop releases the iframe, restores focus and rejects stale callbacks after restarting', () => {
  const f = fixture(); f.start.emit('click'); const old = new URL(f.frame.src).searchParams.get('host-session');
  f.receive('ready'); f.stop.emit('click');
  assert.equal(f.frame.src, undefined);
  assert.equal(f.start.focused, true);
  assert.equal(f.frame.tabIndex, -1);
  f.start.emit('click'); f.receive('ready', old);
  assert.equal(f.root.dataset.showcaseState, 'loading');
  f.receive('ready'); assert.equal(f.root.dataset.showcaseState, 'ready');
  f.controller.destroy();
});

test('slow loading and missing runtime expose retry without pretending to be ready', () => {
  const f = fixture(); f.start.emit('click');
  [...f.timers.values()][0]();
  assert.equal(f.root.dataset.showcaseState, 'slow');
  f.frame.contentDocument.querySelector = () => null; f.frame.emit('load');
  assert.equal(f.root.dataset.showcaseState, 'error');
  assert.equal(f.start.disabled, false);
  f.start.emit('click');
  assert.equal(f.root.dataset.showcaseState, 'loading');
  f.controller.destroy();
});

test('page cache suspension preserves the session; navigation removes handlers and timers', () => {
  const f = fixture(); f.start.emit('click'); f.receive('ready'); f.visible(true);
  f.window.emit('pagehide', {persisted:true});
  assert.equal(f.messages.at(-1).active, false);
  assert.ok(f.frame.src);
  f.window.emit('pageshow'); assert.equal(f.messages.at(-1).active, true);
  f.window.emit('pagehide', {persisted:false});
  assert.equal(f.frame.src, undefined);
  assert.equal(f.timers.size, 0);
  for (const target of [f.window,f.document,f.start,f.stop,f.frame,f.media])
    assert.equal([...target.listeners.values()].reduce((n, set) => n + set.size, 0), 0);
});
