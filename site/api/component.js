"use strict";

function setupDirectory() {
  const directory = document.querySelector('.documentation-directory');
  const search = directory?.querySelector('[data-directory-search]');
  if (!search) return;
  const entries = [...directory.querySelectorAll('[data-directory-entry]')];
  const groups = [...directory.querySelectorAll('[data-directory-group]')];
  const defaults = new Map(groups.map(group => [group, group.open]));
  const status = directory.querySelector('[data-directory-status]');
  const compact = matchMedia('(max-width: 960px)');
  const chinese = document.documentElement.lang === 'zh-CN';
  const query = new URLSearchParams(location.search);
  search.value = query.get('q') || '';
  function filter(updateUrl = false) {
    const value = search.value.trim().toLocaleLowerCase();
    entries.forEach(entry => { entry.hidden = value !== '' && !entry.dataset.search.includes(value); });
    groups.forEach(group => {
      group.hidden = !group.querySelector('[data-directory-entry]:not([hidden])');
      group.open = value !== '' ? !group.hidden : defaults.get(group);
    });
    const count = entries.filter(entry => !entry.hidden).length;
    status.hidden = !value;
    status.textContent = count ? (chinese ? `${count} 个结果` : `${count} results`)
      : (chinese ? '没有匹配的文档' : 'No matching documentation');
    if (updateUrl) {
      const url = new URL(location.href);
      if (value) url.searchParams.set('q', search.value.trim());
      else url.searchParams.delete('q');
      history.replaceState(null, '', url);
    }
  }
  function layout() { directory.open = !compact.matches || Boolean(search.value); }
  search.addEventListener('input', () => filter(true));
  search.addEventListener('keydown', event => {
    if (event.key === 'Escape') { search.value = ''; filter(true); }
    if (event.key === 'Enter' && search.value.trim()) {
      event.preventDefault();
      const queryValue = search.value.trim().toLocaleLowerCase();
      const visible = entries.filter(entry => !entry.hidden);
      const best = visible.find(entry => entry.querySelector('a').textContent.toLocaleLowerCase().startsWith(queryValue))
        || visible.find(entry => entry.dataset.kind === 'component')
        || visible[0];
      best?.querySelector('a').click();
    }
  });
  document.addEventListener('keydown', event => {
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') {
      event.preventDefault(); directory.open = true; search.focus(); search.select();
    }
  });
  compact.addEventListener('change', layout);
  filter(); layout();
  if (query.get('view') === 'headers' && !search.value) {
    groups.forEach(group => { group.open = group.hasAttribute('data-header-group'); });
    directory.open = true;
    directory.querySelector('[data-header-group]').scrollIntoView({ block: 'nearest' });
  }
}
setupDirectory();

function setupApiFilter() {
  const filter = document.querySelector('[data-api-filter]');
  if (!filter) return;
  const root = filter.closest('#api');
  const members = [...root.querySelectorAll('[data-api-member]')];
  const terms = new Map(members.map(member => [member, member.textContent.toLocaleLowerCase()]));
  const groups = [...root.querySelectorAll('[data-api-group]')];
  const inherited = [...root.querySelectorAll('.api-inherited')];
  const defaults = new Map(inherited.map(group => [group, group.open]));
  filter.addEventListener('input', () => {
    const query = filter.value.trim().toLocaleLowerCase();
    members.forEach(member => { member.hidden = query !== '' && !terms.get(member).includes(query); });
    groups.forEach(group => { group.hidden = !group.querySelector('[data-api-member]:not([hidden])'); });
    inherited.forEach(group => {
      group.hidden = query !== '' && !group.querySelector('[data-api-member]:not([hidden])');
      group.open = query !== '' ? !group.hidden : defaults.get(group);
    });
    root.querySelector('[data-api-empty]').hidden = members.some(member => !member.hidden);
  });
  filter.addEventListener('keydown', event => {
    if (event.key === 'Escape') { filter.value = ''; filter.dispatchEvent(new Event('input')); }
  });
}
setupApiFilter();

function setupPageOutline() {
  const menu = document.querySelector('.page-outline-menu');
  const compact = matchMedia('(max-width: 1199px)');
  const adapt = () => { if (menu) menu.open = !compact.matches; };
  compact.addEventListener('change', adapt);
  adapt();
  const links = [...document.querySelectorAll('.page-navigation a[href^="#"]')];
  if (!links.length) return;
  links.forEach(link => link.addEventListener('click', () => {
    if (compact.matches && menu) menu.open = false;
  }));
  const targets = links.map(link => document.getElementById(link.getAttribute('href').slice(1))).filter(Boolean);
  const activate = id => {
    links.forEach(link => {
      if (link.getAttribute('href') === `#${id}`) {
        link.dataset.sectionActive = 'true';
        link.setAttribute('aria-current', 'location');
      } else {
        delete link.dataset.sectionActive;
        link.removeAttribute('aria-current');
      }
    });
  };
  let pending = false;
  function update() {
    pending = false;
    const boundary = document.querySelector('.documentation-header').getBoundingClientRect().bottom + 24;
    let current = targets[0];
    for (const target of targets) {
      if (target.getBoundingClientRect().top <= boundary) current = target;
      else break;
    }
    if (current) activate(current.id);
  }
  function schedule() {
    if (!pending) { pending = true; requestAnimationFrame(update); }
  }
  window.addEventListener('scroll', schedule, { passive: true });
  window.addEventListener('resize', schedule, { passive: true });
  document.addEventListener('toggle', schedule, true);
  document.addEventListener('input', schedule);
  update();
}
setupPageOutline();

function highlightCodeBlocks() {
  if (!window.CSS?.highlights || !window.Highlight) return;
  const buckets = { kw: [], str: [], cmt: [], num: [], typ: [] };
  const pattern = /(\/\/[^\n]*|#[^\n]*)|("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|\b(auto|const|constexpr|static|explicit|virtual|override|final|noexcept|int|double|float|qreal|bool|void|return|class|struct|enum|public|protected|private|using|namespace|new|true|false|nullptr|import|from|def|None|True|False|lambda|for|in|if|else)\b|\b([A-Z][A-Za-z0-9_]*)\b|\b(\d+(?:\.\d+)?)\b/g;
  document.querySelectorAll('.code-example pre code, .api-member dt code').forEach(block => {
    const node = block.firstChild;
    if (!node || node.nodeType !== Node.TEXT_NODE) return;
    const text = node.nodeValue;
    for (const match of text.matchAll(pattern)) {
      const range = new Range();
      range.setStart(node, match.index);
      range.setEnd(node, match.index + match[0].length);
      if (match[1]) buckets.cmt.push(range);
      else if (match[2]) buckets.str.push(range);
      else if (match[3]) buckets.kw.push(range);
      else if (match[4]) buckets.typ.push(range);
      else if (match[5]) buckets.num.push(range);
    }
  });
  Object.entries(buckets).forEach(([key, ranges]) => {
    if (ranges.length) CSS.highlights.set(`fluent-${key}`, new Highlight(...ranges));
  });
}
highlightCodeBlocks();

function createInteractivePreview() {
  const root = document.querySelector('[data-component]');
  if (!root) return null;
  const stage = root.querySelector('.live-stage');
  const pictures = root.querySelector('.preview-stage');
  const retry = root.querySelector('[data-retry-live]');
  const feedback = root.querySelector('.live-feedback');
  const status = root.querySelector('[data-live-status]');
  const reduced = matchMedia('(prefers-reduced-motion: reduce)');
  let frame, timer, session = 0, visible = false, state = 'idle';
  let outerFocus;
  const selected = () => {
    const tab = root.querySelector('.state-tabs [aria-selected="true"]');
    return { route: root.dataset.component, sample: tab.dataset.sample, state: tab.dataset.state };
  };
  function post(type, values = {}) {
    frame?.contentWindow?.postMessage({ source: 'fluent-qt-site', type, session: String(session), ...values }, location.origin);
  }
  function render(next) {
    state = next;
    root.dataset.liveState = next;
    const running = next === 'loading' || next === 'ready';
    pictures.hidden = next === 'ready';
    stage.hidden = !running;
    stage.setAttribute('aria-busy', String(next === 'loading'));
    feedback.hidden = next !== 'loading' && next !== 'error';
    retry.hidden = next !== 'error';
    status.textContent = next === 'loading' ? root.dataset.loadingLabel
      : next === 'error' ? root.dataset.failedLabel : '';
    if (frame) frame.tabIndex = next === 'ready' ? 0 : -1;
  }
  function release(next = 'idle') {
    ++session;
    clearTimeout(timer);
    frame?.remove();
    frame = null;
    render(next);
  }
  function syncVisibility() {
    if (visible && !document.hidden && state === 'idle') begin();
    post('component-visibility', { active: visible && !document.hidden });
  }
  const previewTheme = () => root.dataset.previewTheme || document.documentElement.dataset.theme;
  function syncTheme() { post('theme', { theme: previewTheme() }); }
  function begin() {
    if (state === 'loading' || state === 'ready') return;
    release();
    outerFocus = document.activeElement;
    const url = new URL(root.dataset.runtimeUrl, location.href);
    Object.entries({ embed: 'site', preview: 'component', ...selected(), 'host-theme': previewTheme(),
      'host-session': String(session), 'render-scale': 'native' }).forEach(([key, value]) => url.searchParams.set(key, value));
    frame = document.createElement('iframe');
    frame.title = root.dataset.frameTitle;
    const current = frame;
    frame.addEventListener('load', () => {
      if (frame !== current || state !== 'loading') return;
      try {
        if (!current.contentDocument?.documentElement.hasAttribute('data-fluent-qt-loaded')) release('error');
      } catch { release('error'); }
    });
    frame.addEventListener('error', () => { if (frame === current) release('error'); });
    frame.src = url.href;
    stage.replaceChildren(frame);
    render('loading');
    timer = setTimeout(() => release('error'), 45000);
  }
  retry.addEventListener('click', begin);
  root.addEventListener('selectionchange', () => {
    if (frame) post('component-select', { selection: selected() });
  });
  window.addEventListener('message', event => {
    if (!frame || event.origin !== location.origin || event.source !== frame.contentWindow
        || event.data?.source !== 'fluent-qt-gallery' || event.data.session !== String(session)) return;
    if (event.data.state === 'ready') {
      clearTimeout(timer);
      render('ready');
      post('component-select', { selection: selected() });
      syncTheme();
      syncVisibility();
      post('component-motion', { reduced: reduced.matches });
      if (document.activeElement === frame && outerFocus?.isConnected && outerFocus !== document.body)
        outerFocus.focus({ preventScroll: true });
    } else if (event.data.state === 'error' || event.data.state === 'exit') release('error');
  });
  document.addEventListener('visibilitychange', syncVisibility);
  document.addEventListener('focusin', event => {
    if (state === 'loading' && event.target !== frame) outerFocus = event.target;
  });
  reduced.addEventListener('change', () => post('component-motion', { reduced: reduced.matches }));
  const observer = window.IntersectionObserver && new IntersectionObserver(entries => { visible = entries[0].isIntersecting; syncVisibility(); });
  observer?.observe(root);
  window.addEventListener('pagehide', event => {
    if (event.persisted) post('component-visibility', { active: false });
    else { release(); observer?.disconnect(); }
  });
  window.addEventListener('pageshow', syncVisibility);
  render('idle');
  if (!observer) { visible = true; syncVisibility(); }
  return { syncTheme };
}
const interactivePreview = createInteractivePreview();

function updateImage(img) {
  const panel = img.closest(".preview-panel");
  const selected = img.closest('[data-component]')?.dataset.previewTheme || document.documentElement.dataset.theme;
  const theme = selected === 'light' ? 'light' : 'dark';
  const ready = img.complete && img.naturalWidth > 0 && img.currentSrc.includes(`-${theme}.png`);
  panel.setAttribute("aria-busy", String(!ready));
  img.dataset.imageReady = ready ? theme : "";
}
document.querySelectorAll(".preview-panel img").forEach(img => {
  const panel = img.closest(".preview-panel");
  const chinese = document.documentElement.lang === "zh-CN";
  panel.dataset.loadingLabel = chinese ? "正在加载预览…" : "Loading preview…";
  panel.dataset.errorLabel = chinese ? "预览加载失败，请刷新页面重试。" : "Preview unavailable. Refresh the page to retry.";
  img.addEventListener("load", () => { delete panel.dataset.imageError; updateImage(img); });
  img.addEventListener("error", () => { panel.setAttribute("aria-busy", "false"); panel.dataset.imageError = "true"; });
});

const systemThemeQuery = matchMedia('(prefers-color-scheme: dark)');
const forcedColorsQuery = matchMedia('(forced-colors: active)');
let previewThemeOverride = null;
function siteTheme() {
  try {
    const saved = localStorage.getItem('fluent-qt-theme');
    if (['light', 'dark', 'high-contrast'].includes(saved)) return saved;
  } catch { /* Use the system theme when storage is unavailable. */ }
  return systemThemeQuery.matches ? 'dark' : 'light';
}
function syncPreviewTheme() {
  const preview = document.querySelector('[data-component]');
  if (!preview) return;
  const theme = forcedColorsQuery.matches ? 'high-contrast'
    : previewThemeOverride || document.documentElement.dataset.theme;
  preview.dataset.previewTheme = theme;
  preview.querySelectorAll("source[data-dark-src]").forEach(source => {
    source.media = theme !== "light" ? "(min-width: 0px)" : "(max-width: 0px)";
  });
  preview.querySelectorAll("button[data-preview-theme]").forEach(button => {
    button.setAttribute("aria-pressed", String(button.dataset.previewTheme === theme));
    button.disabled = forcedColorsQuery.matches;
  });
  preview.querySelectorAll(".preview-panel img").forEach(img => {
    delete img.closest(".preview-panel").dataset.imageError;
    updateImage(img);
  });
  requestAnimationFrame(() => preview.querySelectorAll(".preview-panel img").forEach(updateImage));
  interactivePreview?.syncTheme();
}

document.querySelectorAll("button[data-preview-theme]").forEach(button => {
  button.addEventListener("click", () => {
    previewThemeOverride = button.dataset.previewTheme;
    syncPreviewTheme();
  });
});
function inheritTheme() {
  document.documentElement.dataset.theme = forcedColorsQuery.matches ? 'high-contrast' : siteTheme();
  syncPreviewTheme();
}
window.addEventListener('storage', event => {
  if (event.key === 'fluent-qt-theme' || event.key === null) inheritTheme();
});
window.addEventListener('pageshow', inheritTheme);
systemThemeQuery.addEventListener('change', inheritTheme);
forcedColorsQuery.addEventListener('change', inheritTheme);
inheritTheme();

document.querySelectorAll("[data-tabs]").forEach(group => {
  const tabs = [...group.querySelectorAll('[role="tab"]')];
  function select(tab, focus = false) {
    tabs.forEach(candidate => {
      const active = candidate === tab;
      candidate.setAttribute("aria-selected", String(active));
      candidate.tabIndex = active ? 0 : -1;
      document.getElementById(candidate.getAttribute("aria-controls")).hidden = !active;
    });
    if (focus) tab.focus();
    group.dispatchEvent(new Event('selectionchange'));
    requestAnimationFrame(() => group.querySelectorAll("img").forEach(updateImage));
  }
  tabs.forEach((tab, index) => {
    tab.addEventListener("click", () => select(tab));
    tab.addEventListener("keydown", event => {
      let next;
      if (event.key === "ArrowRight") next = (index + 1) % tabs.length;
      if (event.key === "ArrowLeft") next = (index - 1 + tabs.length) % tabs.length;
      if (event.key === "Home") next = 0;
      if (event.key === "End") next = tabs.length - 1;
      if (next !== undefined) { event.preventDefault(); select(tabs[next], true); }
    });
  });
});

let noticeTimer;
function notice(message) {
  const status = document.querySelector(".copy-status");
  clearTimeout(noticeTimer);
  status.textContent = message;
  status.hidden = false;
  noticeTimer = setTimeout(() => { status.hidden = true; }, 2400);
}
document.querySelectorAll("[data-copy]").forEach(button => {
  button.addEventListener("click", async () => {
    const code = button.closest("[data-tabs]").querySelector('[role="tabpanel"]:not([hidden]) code');
    try {
      await navigator.clipboard.writeText(code.textContent);
      notice(document.body.dataset.copySuccess);
    } catch {
      const range = document.createRange();
      range.selectNodeContents(code);
      const selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      notice(document.body.dataset.copyError);
    }
  });
});

function setupReleaseHistory() {
  const form = document.querySelector('[data-release-filters]');
  if (!form) return;
  const entries = [...document.querySelectorAll('[data-release-entry]')];
  const normalize = value => value.toLocaleLowerCase().replace(/[-_\s]/g, '');
  const terms = new Map(entries.map(entry => [entry, normalize(entry.dataset.releaseSearch)]));
  const query = form.querySelector('[data-release-query]');
  const from = form.querySelector('[data-release-from]');
  const breaking = form.querySelector('[data-release-breaking]');
  const result = form.querySelector('[data-release-result]');
  const empty = document.querySelector('[data-release-empty]');
  const chinese = document.documentElement.lang === 'zh-CN';
  const params = new URLSearchParams(location.search);
  query.value = params.get('change') || '';
  from.value = [...from.options].some(option => option.value === params.get('from')) ? params.get('from') : '';
  breaking.checked = params.get('breaking') === '1';
  const newer = (version, baseline) => {
    if (!baseline || version === 'unreleased') return true;
    const a = version.split('.').map(Number), b = baseline.split('.').map(Number);
    for (let i = 0; i < 3; i++) {
      if (a[i] !== b[i]) return a[i] > b[i];
    }
    return false;
  };
  function filter(updateUrl = false) {
    const term = normalize(query.value.trim());
    let count = 0;
    entries.forEach(entry => {
      const visible = (!term || terms.get(entry).includes(term))
        && (!breaking.checked || entry.dataset.breaking === 'true') && newer(entry.dataset.version, from.value);
      entry.hidden = !visible;
      if (visible) count++;
      const outline = document.querySelector(`.page-navigation a[href="#${entry.id}"]`);
      if (outline) outline.hidden = !visible;
    });
    empty.hidden = count !== 0;
    result.textContent = chinese ? `${from.value ? `${from.value} 之后共` : '共'} ${count} 条记录`
      : `${count} ${count === 1 ? 'entry' : 'entries'}${from.value ? ` after ${from.value}` : ''}`;
    if (updateUrl) {
      const url = new URL(location.href);
      for (const [key, value] of [['change', query.value.trim()], ['from', from.value], ['breaking', breaking.checked ? '1' : '']]) {
        if (value) url.searchParams.set(key, value); else url.searchParams.delete(key);
      }
      history.replaceState(null, '', url);
    }
  }
  form.hidden = false;
  form.addEventListener('submit', event => event.preventDefault());
  form.addEventListener('input', () => filter(true));
  form.addEventListener('change', () => filter(true));
  form.addEventListener('reset', event => {
    event.preventDefault();
    query.value = '';
    from.value = '';
    breaking.checked = false;
    filter(true);
  });
  window.addEventListener('hashchange', () => {
    let anchor;
    try { anchor = decodeURIComponent(location.hash.slice(1)); } catch { return; }
    const entry = document.getElementById(anchor)?.closest('[data-release-entry]');
    if (entry?.hidden) { form.reset(); queueMicrotask(() => entry.scrollIntoView({block: 'start'})); }
  });
  filter();
}
setupReleaseHistory();
