import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import vm from 'node:vm';

const source = await readFile(new URL('../../site/site.js', import.meta.url), 'utf8');
const template = await readFile(new URL('./index.template.html', import.meta.url), 'utf8');
const funding = await readFile(new URL('../../.github/FUNDING.yml', import.meta.url), 'utf8');
const setup = source.slice(source.indexOf('function setupSupport() {'), source.indexOf('\nfunction setupGalleryPreview()'));
const strings = vm.runInNewContext(`(${source.slice(source.indexOf('{'), source.indexOf('\n};\n\nconst fallbackVersion') + 2)})`);

class Element {
  dataset = {};
  hidden = false;
  children = [];
  listeners = new Map();
  selectors = {};
  addEventListener(type, handler) {
    const handlers = this.listeners.get(type) || [];
    handlers.push(handler);
    this.listeners.set(type, handlers);
  }
  emit(type, event = {}) { this.listeners.get(type)?.forEach(handler => handler(event)); }
  querySelector(selector) { return this.selectors[selector]; }
  replaceChildren(...children) { this.children = children; }
}

class TestImage extends Element {
  complete = false;
  naturalWidth = 0;
  alt = 'Collection QR code';
  cloneNode() { return new TestImage(); }
  removeAttribute() {}
  settle(success) {
    this.complete = true;
    this.naturalWidth = success ? 828 : 0;
    this.emit(success ? 'load' : 'error');
  }
}

function fixture(page = 'https://example.test/', locale = 'en', missing = '', search = '') {
  const window = new Element();
  window.location = { search };
  const controls = new Element(), instruction = new Element();
  const cnyFieldset = new Element(), kofiActions = new Element();
  const mobileHelp = new Element(), kofiFallback = new Element();
  controls.hidden = true;
  kofiActions.hidden = true;
  let amount = '19', method = locale === 'en' ? 'kofi' : 'wechat';
  controls.querySelector = selector => {
    if (selector === '[data-support-cny]') return cnyFieldset;
    if (selector === '[data-support-kofi-actions]') return kofiActions;
    const name = selector.includes('support-amount') ? 'amount' : 'method';
    if (missing === name) return null;
    const value = selector.match(/\[value="([^"]+)"\]/)?.[1];
    if (value) {
      return {
        set checked(on) {
          if (!on) return;
          if (name === 'amount') amount = value;
          else method = value;
        }
      };
    }
    return { value: name === 'amount' ? amount : method };
  };
  const payments = [['wechat', 'jpg'], ['alipay', 'jpg'], ['kofi', 'png']].map(([name, ext]) => {
    const payment = new Element(), viewport = new Element(), generic = new TestImage();
    const code = new Element(), loading = new Element(), error = new Element();
    generic.src = new URL(`${locale === 'zh' ? '../' : ''}assets/support/${name}.${ext}`, page).href;
    generic.complete = true;
    generic.naturalWidth = 828;
    viewport.children = [generic];
    viewport.selectors.img = generic;
    payment.dataset.supportPayment = name;
    payment.selectors = {
      '[data-support-code]': code, '.support-code-window': viewport,
      '[data-support-loading]': loading, '[data-support-error]': error
    };
    return { payment, viewport, code, loading, error, generic };
  });
  const root = new Element();
  root.selectors = {
    '[data-support-controls]': controls,
    '[data-support-instruction]': instruction,
    '[data-support-mobile-help]': mobileHelp,
    '.support-kofi-fallback': kofiFallback
  };
  if (missing === 'controls') delete root.selectors['[data-support-controls]'];
  if (missing === 'instruction') delete root.selectors['[data-support-instruction]'];
  root.querySelectorAll = () => payments.map(entry => entry.payment);
  vm.runInNewContext(`${setup}\nsetupSupport();`, {
    document: { querySelector: selector => selector === '[data-support]' ? root : null },
    window, URL, URLSearchParams,
    dictionary: () => strings[locale]
  });
  return {
    payments, controls, instruction, cnyFieldset, kofiActions, mobileHelp, kofiFallback, window,
    get method() { return method; },
    choose(nextMethod, nextAmount, event = 'change') {
      method = nextMethod;
      amount = nextAmount;
      (event === 'pageshow' ? window : controls).emit(event);
      return payments.find(entry => entry.payment.dataset.supportPayment === method);
    }
  };
}

test('locale defaults select WeChat Pay in Chinese and Ko-fi in English', () => {
  const zh = fixture('https://example.test/Fluent-Qt/zh-CN/', 'zh');
  assert.equal(zh.method, 'wechat');
  assert.equal(zh.cnyFieldset.hidden, false);
  assert.equal(zh.kofiActions.hidden, true);
  assert.equal(zh.payments[0].payment.hidden, false);
  assert.equal(zh.payments[2].payment.hidden, true);
  assert.equal(zh.mobileHelp.textContent, strings.zh['support.mobileHelp']);
  assert.equal(zh.kofiFallback.hidden, true);

  const en = fixture('https://example.test/Fluent-Qt/', 'en');
  assert.equal(en.method, 'kofi');
  assert.equal(en.cnyFieldset.hidden, true);
  assert.equal(en.kofiActions.hidden, false);
  assert.equal(en.payments[0].payment.hidden, true);
  assert.equal(en.payments[2].payment.hidden, false);
  assert.equal(en.instruction.textContent, strings.en['support.kofiInstruction']);
  assert.equal(en.mobileHelp.textContent, strings.en['support.kofiMobileHelp']);
  assert.equal(en.kofiFallback.hidden, true);
});

test('all channel and tier selections keep QR codes and copy aligned in both locales and subpaths', async () => {
  for (const [page, locale] of [['https://example.test/', 'en'], ['https://example.test/Fluent-Qt/zh-CN/', 'zh']]) {
    const f = fixture(page, locale);
    for (const method of ['wechat', 'alipay']) {
      for (const amount of ['19', '49', '99', 'any']) {
        const selected = f.choose(method, amount);
        const file = `${method}${amount === 'any' ? '' : `-${amount}`}.jpg`;
        const expected = new URL(`${locale === 'zh' ? '../' : ''}assets/support/${file}`, page).href;
        const image = selected.viewport.children[0];
        assert.equal(image.src, expected);
        assert.equal(f.payments.filter(entry => !entry.payment.hidden).length, 1);
        assert.equal(selected.payment.hidden, false);
        assert.equal(f.cnyFieldset.hidden, false);
        assert.equal(f.kofiActions.hidden, true);
        assert.equal(image.loading, 'eager', 'hidden pending images must still load');
        assert.equal(image.dataset.supportFixed === '', amount !== 'any');
        assert.equal(f.instruction.textContent, strings[locale][amount === 'any' ? 'support.anyInstruction' : 'support.instruction'].replace('{amount}', amount));
        assert.equal(f.mobileHelp.textContent, strings[locale]['support.mobileHelp']);
        image.settle(true);
        assert.equal(image.hidden, false);
        assert.equal(selected.loading.hidden, true);
        assert.equal(selected.error.hidden, true);
        assert.ok((await readFile(new URL(`../../site/assets/support/${file}`, import.meta.url))).length > 0);
      }
    }
    for (const amount of ['19', 'any']) {
      const kofi = f.choose('kofi', amount);
      const expected = new URL(`${locale === 'zh' ? '../' : ''}assets/support/kofi.png`, page).href;
      assert.equal(kofi.viewport.children[0], kofi.generic);
      assert.equal(kofi.viewport.children[0].src, expected);
      assert.equal(f.payments.filter(entry => !entry.payment.hidden).length, 1);
      assert.equal(kofi.payment.hidden, false);
      assert.equal(f.cnyFieldset.hidden, true);
      assert.equal(f.kofiActions.hidden, false);
      assert.equal(f.instruction.textContent, strings[locale]['support.kofiInstruction']);
      assert.equal(f.mobileHelp.textContent, strings[locale]['support.kofiMobileHelp']);
      assert.ok((await readFile(new URL('../../site/assets/support/kofi.png', import.meta.url))).length > 0);
    }
  }
});

test('the homepage, modal checkout, and GitHub Sponsor link to the same Ko-fi recipient', () => {
  const handle = /^ko_fi: (\S+)$/m.exec(funding)?.[1];
  assert.ok(handle, 'GitHub Sponsor needs a Ko-fi handle');
  const kofiLinks = [...template.matchAll(/href="(https:\/\/ko-fi\.com\/[^"]+)"/g)].map(match => match[1]);
  assert.ok(kofiLinks.length >= 3, 'Ko-fi actions, dialog header, and no-script fallback should link directly');
  for (const href of kofiLinks) assert.equal(href, `https://ko-fi.com/${handle}`);
  const embed = template.match(/<dialog class="support-kofi-dialog"[\s\S]*?<template>\s*<iframe src="([^"]+)"/);
  assert.ok(embed, 'the third-party checkout should start in an inert template inside the modal dialog');
  const url = new URL(embed[1].replaceAll('&amp;', '&'));
  assert.equal(url.origin + url.pathname, `https://ko-fi.com/${handle}/`);
  assert.equal(url.searchParams.get('embed'), 'true');
});

test('clicking the Ko-fi panel trigger mounts the modal checkout once, preserves it, and allows modifier clicks', () => {
  const root = new Element(), trigger = new Element(), dialog = new Element(), checkout = new Element();
  root.selectors['[data-kofi-open]'] = trigger;
  let mounts = 0, opens = 0, closes = 0;
  dialog.showModal = () => { opens++; };
  dialog.close = () => {
    closes++;
    dialog.emit('close', { target: dialog });
  };
  checkout.remove = () => {
    dialog.replaceChildren();
    delete dialog.selectors.iframe;
  };
  dialog.selectors.template = {
    content: { cloneNode: () => checkout },
    replaceWith(node) {
      mounts++;
      dialog.replaceChildren(node);
      dialog.selectors.iframe = node;
      delete dialog.selectors.template;
    }
  };
  vm.runInNewContext(`${setup}\nsetupSupport();`, {
    document: { querySelector: selector => selector === '[data-support]' ? root : dialog }
  });
  assert.equal(mounts, 0);
  assert.equal(opens, 0);

  let modifierPrevented = false;
  trigger.emit('click', { metaKey: true, preventDefault() { modifierPrevented = true; } });
  assert.equal(modifierPrevented, false);
  assert.equal(mounts, 0);
  assert.equal(opens, 0);

  for (const expectedOpens of [1, 2]) {
    let prevented = false;
    trigger.emit('click', { preventDefault() { prevented = true; } });
    assert.equal(prevented, true);
    assert.equal(mounts, 1);
    assert.equal(opens, expectedOpens);
    assert.equal(dialog.children[0], checkout);

    dialog.emit('click', { target: checkout });
    assert.equal(closes, expectedOpens - 1);
    dialog.emit('click', { target: dialog });
    assert.equal(closes, expectedOpens);
    assert.equal(dialog.children[0], checkout, 'closing must retain the same checkout');
  }
});

test('each custom GitHub funding link opens its CNY tier with a greeting until the visitor changes it', () => {
  // Each short path redirects to `?fuel=<tier>#support`; the Python tests cover the redirect pages.
  const tiers = [...funding.matchAll(/^\s*- \S+\/fuel\/([a-z-]+)\/$/gm)].map(match => match[1]);
  assert.equal(tiers.length, 4, 'GitHub accepts at most four custom funding links, one per tier');
  const amounts = ['19', '49', '99', 'any'];
  for (const locale of ['en', 'zh']) {
    tiers.forEach((tier, index) => {
      const amount = amounts[index];
      const f = fixture('https://example.test/', locale, '', `?fuel=${tier}`);
      const label = strings[locale][amount === 'any' ? 'support.any' : `support.tier${amount}`];
      const greeting = strings[locale][amount === 'any' ? 'support.linkedAnyInstruction' : 'support.linkedInstruction'];
      assert.equal(f.method, 'wechat', 'CNY tier links must activate WeChat Pay even when English defaults to Ko-fi');
      assert.equal(f.cnyFieldset.hidden, false);
      assert.equal(f.kofiActions.hidden, true);
      assert.equal(f.instruction.textContent, greeting.replace('{amount}', amount).replace('{tier}', label));
      assert.ok(f.payments[0].viewport.children[0].src.endsWith(amount === 'any' ? '/wechat.jpg' : `/wechat-${amount}.jpg`));
      f.choose('alipay', '49');
      assert.equal(f.instruction.textContent, strings[locale]['support.instruction'].replace('{amount}', '49'));
    });
  }
  // Unknown and inherited names keep each locale's default channel and copy.
  for (const fuel of ['espresso', 'constructor', '__proto__']) {
    const en = fixture('https://example.test/', 'en', '', `?fuel=${fuel}`);
    assert.equal(en.method, 'kofi');
    assert.equal(en.instruction.textContent, strings.en['support.kofiInstruction']);
    const zh = fixture('https://example.test/zh-CN/', 'zh', '', `?fuel=${fuel}`);
    assert.equal(zh.method, 'wechat');
    assert.equal(zh.instruction.textContent, strings.zh['support.instruction'].replace('{amount}', '19'));
  }
});

test('late success or failure from a previous amount cannot change the active QR state', () => {
  const f = fixture('https://example.test/zh-CN/', 'zh'), selected = f.payments[0];
  const previous = selected.viewport.children[0];
  f.choose('wechat', '49');
  const current = selected.viewport.children[0];
  assert.notEqual(current, previous);
  assert.equal(current.hidden, true);
  assert.equal(selected.loading.hidden, false);
  previous.settle(true);
  assert.equal(current.hidden, true);
  assert.equal(selected.loading.hidden, false);
  current.settle(true);
  previous.settle(false);
  assert.equal(current.hidden, false);
  assert.equal(selected.code.hidden, false);
  assert.equal(selected.error.hidden, true);
});

test('failed fixed codes never fall back to generic; changing selection recovers', () => {
  const f = fixture('https://example.test/zh-CN/', 'zh'), selected = f.payments[0];
  selected.viewport.children[0].settle(false);
  assert.equal(selected.code.hidden, true);
  assert.equal(selected.error.hidden, false);
  f.choose('wechat', '99');
  assert.equal(selected.code.hidden, false);
  assert.equal(selected.error.hidden, true);
  assert.equal(selected.loading.hidden, false);
  selected.viewport.children[0].settle(true);
  f.choose('wechat', 'any');
  assert.equal(selected.viewport.children[0], selected.generic);
  assert.equal(selected.generic.hidden, false);
  assert.equal(selected.loading.hidden, true);
});

test('restored radio state selects the corresponding code on pageshow', () => {
  const f = fixture();
  const selected = f.choose('alipay', '49', 'pageshow');
  assert.equal(selected.payment.hidden, false);
  assert.ok(selected.viewport.children[0].src.endsWith('/alipay-49.jpg'));
});

test('missing controls or defaults keep static generic codes available without throwing', () => {
  for (const missing of ['controls', 'instruction', 'method', 'amount']) {
    const f = fixture('https://example.test/', 'en', missing);
    assert.equal(f.controls.hidden, true);
    assert.equal(f.kofiFallback.hidden, false);
    assert.equal(f.controls.listeners.size, 0);
    for (const entry of f.payments) {
      assert.equal(entry.payment.hidden, false);
      assert.equal(entry.viewport.children[0], entry.generic);
      assert.equal(entry.generic.hidden, false);
    }
  }
});

test('the early mask waits for deferred loading and releases the fallback when that script fails', () => {
  const earlyScript = template.match(/<script>\s*([\s\S]*?)<\/script>/)[1];
  const document = new Element();
  const attributes = new Map();
  document.documentElement = {
    dataset: {},
    setAttribute: (name, value) => attributes.set(name, value),
    removeAttribute: name => attributes.delete(name)
  };
  vm.runInNewContext(earlyScript, {
    document,
    matchMedia: () => ({ matches: false }),
    localStorage: { getItem: () => { throw new Error('Storage unavailable'); } }
  });
  assert.equal(attributes.has('data-support-pending'), true);
  // A failed deferred script still allows DOMContentLoaded, without setupSupport.
  document.emit('DOMContentLoaded');
  assert.equal(attributes.has('data-support-pending'), false);
});

test('a support initialization exception does not block the actual subsequent page startup calls', () => {
  const bootstrap = source.slice(
    source.indexOf('document.documentElement.setAttribute("data-i18n-ready"'),
    source.indexOf('\nimport("./hero-particles.js')
  );
  const calls = [], warnings = [];
  const context = {
    document: { documentElement: { setAttribute() {} } },
    releaseState: {},
    detectPlatform: () => 'other',
    console: { warn: (...args) => warnings.push(args) },
    setupSupport: () => { throw new Error('Simulated support initialization failure'); }
  };
  const initializers = ['loadAnalytics', 'setupNavigation', 'setupInteractions',
    'setupGalleryPreview', 'setupLiveGallery', 'setupReveal', 'initializeTheme', 'setupSpatialShowcase'];
  for (const name of initializers) context[name] = () => calls.push(name);
  vm.runInNewContext(bootstrap, context);
  assert.deepEqual(calls, initializers);
  assert.equal(warnings.length, 1);
  assert.match(warnings[0][1].message, /Simulated support/);
});
