// Own the browser lifecycle only. All controls and 3D rendering live in C++.
export function createSpatialShowcase(root, { theme, strings }) {
  if (!root) return null;
  const document = root.ownerDocument;
  const window = document.defaultView;
  const frame = root.querySelector('[data-showcase-frame]');
  const start = root.querySelector('[data-showcase-start]');
  const stop = root.querySelector('[data-showcase-stop]');
  const message = root.querySelector('[data-showcase-message]');
  const detail = root.querySelector('[data-showcase-detail]');
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)');
  let state = 'waiting';
  let session = 0;
  let timer = 0;
  let visible = !window.IntersectionObserver;
  const cleanups = [];

  function listen(target, type, handler) {
    target.addEventListener(type, handler);
    cleanups.push(() => target.removeEventListener(type, handler));
  }
  function post(type, values) {
    if (state !== 'ready') return;
    frame.contentWindow?.postMessage({ source: 'fluent-qt-site', type, ...values }, window.location.origin);
  }
  function syncTheme() { post('theme', { theme: theme() }); }
  function syncVisibility() {
    post('showcase-visibility', { active: visible && !document.hidden });
  }
  function render(next) {
    state = next;
    root.dataset.showcaseState = state;
    const keys = state === 'waiting' ? ['spatial.runTitle', 'spatial.runCopy']
      : state === 'error' ? ['gallery.errorTitle', 'gallery.errorDetail']
      : state === 'slow' ? ['gallery.slowTitle', 'gallery.slowDetail']
      : ['gallery.loadingTitle', 'gallery.loadingDetail'];
    message.textContent = strings()[keys[0]];
    detail.textContent = strings()[keys[1]];
    start.textContent = strings()[state === 'error' ? 'gallery.retryAction' : 'spatial.run'];
    start.disabled = !['waiting', 'error'].includes(state);
    stop.hidden = state === 'waiting';
    frame.tabIndex = state === 'ready' ? 0 : -1;
    frame.setAttribute('aria-hidden', String(state !== 'ready'));
  }
  function release() {
    window.clearTimeout(timer);
    ++session; // Ignore late ready/error messages from the discarded runtime.
    frame.removeAttribute('src');
    render('waiting');
  }
  function load() {
    if (!['waiting', 'error'].includes(state)) return;
    const url = new URL(root.dataset.showcaseSrc, window.location.href);
    url.searchParams.set('host-theme', theme());
    url.searchParams.set('host-session', String(++session));
    render('loading');
    window.clearTimeout(timer);
    timer = window.setTimeout(() => { if (state === 'loading') render('slow'); }, 30000);
    frame.src = url.href;
  }
  listen(start, 'click', load);
  listen(stop, 'click', () => { release(); start.focus(); });
  listen(document, 'visibilitychange', syncVisibility);
  listen(reduced, 'change', () => post('showcase-motion', { reduced: reduced.matches }));
  listen(window, 'message', event => {
    if (event.origin !== window.location.origin || event.source !== frame.contentWindow
        || event.data?.source !== 'fluent-qt-gallery'
        || event.data.session !== String(session) || state === 'waiting') return;
    if (event.data.state === 'ready') {
      window.clearTimeout(timer);
      render('ready');
      syncTheme();
      syncVisibility();
      post('showcase-motion', { reduced: reduced.matches });
    } else if (['error', 'exit'].includes(event.data.state)) {
      window.clearTimeout(timer);
      render('error');
    }
  });
  listen(frame, 'load', () => {
    if (!['loading', 'slow'].includes(state)) return;
    try {
      if (!frame.contentDocument?.querySelector('#qt-container')) {
        window.clearTimeout(timer);
        render('error');
      }
    } catch { /* Cross-origin custom hosts must use the explicit ready protocol. */ }
  });
  const observer = window.IntersectionObserver && new window.IntersectionObserver(entries => {
    visible = entries.some(entry => entry.isIntersecting);
    syncVisibility();
  });
  observer?.observe(root);
  function destroy() {
    release();
    observer?.disconnect();
    cleanups.splice(0).forEach(cleanup => cleanup());
  }
  listen(window, 'pagehide', event => {
    if (event.persisted) post('showcase-visibility', { active: false });
    else destroy();
  });
  listen(window, 'pageshow', syncVisibility);
  render('waiting');
  return { syncTheme, destroy };
}
