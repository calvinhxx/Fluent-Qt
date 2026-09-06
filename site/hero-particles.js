const TAU = Math.PI * 2;
const MAX_PIXELS = 2_500_000;

// The decoration is optional; the page and its links never depend on Canvas.
export function createHeroParticles(hero) {
  if (!hero) return null;
  const document = hero.ownerDocument;
  const window = document.defaultView;
  const canvas = hero.querySelector(".hero-particles");
  const toggle = hero.querySelector("[data-particle-toggle]");
  const context = canvas?.getContext("2d");
  if (!context || !toggle || !window.IntersectionObserver || !window.ResizeObserver || !window.MutationObserver) return null;

  const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const forcedColors = window.matchMedia("(forced-colors: active)");
  const cleanups = [];
  const pointer = { x: 0, y: 0, strength: 0, active: false };
  let width = 0;
  let height = 0;
  let colors = [];
  let particles = [];
  let frame = 0;
  let previousTime = 0;
  let elapsed = 0;
  let visible = false;
  let pageActive = true;
  let destroyed = false;
  let paused = false;

  try { paused = window.sessionStorage.getItem("fluentqt-hero-motion") === "paused"; } catch {}

  function listen(target, type, handler) {
    if (target.addEventListener) {
      target.addEventListener(type, handler, { passive: true });
      cleanups.push(() => target.removeEventListener(type, handler));
    } else {
      target.addListener(handler);
      cleanups.push(() => target.removeListener(handler));
    }
  }

  function highContrast() {
    return forcedColors.matches || document.documentElement.dataset.theme === "high-contrast";
  }

  function enabled() {
    return !paused && !reducedMotion.matches && !highContrast();
  }

  function position(angle, lane, spread = 0) {
    const mobile = width < 700;
    const radiusX = width * (mobile ? 0.87 : 0.52);
    const radiusY = height * (mobile ? 0.25 : 0.34);
    const x = Math.cos(angle) * (radiusX + spread * 28);
    const y = Math.sin(angle) * (radiusY + spread * 22);
    const tilt = -0.3 + lane * 0.12;
    return {
      x: width * (mobile ? 0.7 : 0.79) + x * Math.cos(tilt) - y * Math.sin(tilt),
      y: height * (mobile ? 0.59 : 0.4) + x * Math.sin(tilt) + y * Math.cos(tilt)
        + Math.sin(angle * 3 + elapsed * 0.18 + lane) * height * 0.025
    };
  }

  function draw() {
    context.clearRect(0, 0, width, height);
    if (highContrast() || !width || !height) return;

    for (let lane = 0; lane < 3; lane += 1) {
      context.strokeStyle = colors[lane];
      context.fillStyle = colors[lane];
      context.lineWidth = 0.8;
      context.globalAlpha = 0.32;
      context.beginPath();
      for (let step = 0; step <= 100; step += 1) {
        const point = position(step / 100 * TAU, lane);
        if (step === 0) context.moveTo(point.x, point.y);
        else context.lineTo(point.x, point.y);
      }
      context.stroke();

      context.globalAlpha = 0.85;
      context.beginPath();
      for (const particle of particles[lane]) {
        const angle = particle.phase + elapsed * (0.1 + lane * 0.025);
        const point = position(angle, lane, particle.spread);
        if (pointer.strength > 0.01) {
          const dx = point.x - pointer.x;
          const dy = point.y - pointer.y;
          const influence = Math.exp(-(dx * dx + dy * dy) / 24000) * pointer.strength;
          point.x += (dx * 0.2 - dy * 0.12) * influence;
          point.y += (dy * 0.2 + dx * 0.12) * influence;
        }
        const radius = particle.size * (0.75 + (Math.sin(angle) + 1) * 0.25);
        context.moveTo(point.x + radius, point.y);
        context.arc(point.x, point.y, radius, 0, TAU);
      }
      context.fill();

      // Short highlights travel along each ribbon; no pairwise particle links.
      context.beginPath();
      for (let spark = 0; spark < 7; spark += 1) {
        const angle = spark / 7 * TAU + elapsed * (0.18 + lane * 0.035) + lane;
        for (let step = 0; step <= 6; step += 1) {
          const point = position(angle - step * 0.006, lane);
          if (step === 0) context.moveTo(point.x, point.y);
          else context.lineTo(point.x, point.y);
        }
      }
      context.lineWidth = 6;
      context.globalAlpha = 0.1;
      context.stroke();
      context.lineWidth = 1.8;
      context.globalAlpha = 1;
      context.stroke();
    }
    context.globalAlpha = 1;
  }

  function tick(time) {
    frame = 0;
    if (destroyed || !enabled() || !visible || document.hidden || !pageActive) return;
    const interval = 1000 / (width < 700 ? 30 : 60);
    const delta = previousTime ? time - previousTime : interval;
    if (delta >= interval - 0.5) {
      elapsed += Math.min(delta, 50) / 1000;
      previousTime = time;
      pointer.strength += ((pointer.active ? 1 : 0) - pointer.strength) * 0.12;
      draw();
    }
    frame = window.requestAnimationFrame(tick);
  }

  function stop() {
    if (frame) window.cancelAnimationFrame(frame);
    frame = 0;
    previousTime = 0;
    pointer.active = false;
    pointer.strength = 0;
  }

  function sync() {
    if (destroyed) return;
    const running = enabled() && visible && !document.hidden && pageActive;
    hero.dataset.particleState = highContrast() ? "hidden" :
      (!enabled() ? "paused" : (running ? "running" : "suspended"));
    toggle.disabled = reducedMotion.matches || highContrast();
    toggle.setAttribute("aria-pressed", String(enabled()));
    if (running) {
      if (!frame) frame = window.requestAnimationFrame(tick);
    } else {
      stop();
    }
  }

  function resize() {
    if (destroyed) return;
    const bounds = canvas.getBoundingClientRect();
    width = bounds.width;
    height = bounds.height;
    const scale = Math.min(window.devicePixelRatio || 1, 2,
      Math.sqrt(MAX_PIXELS / Math.max(1, width * height)));
    canvas.width = Math.floor(width * scale);
    canvas.height = Math.floor(height * scale);
    context.setTransform(scale, 0, 0, scale, 0, 0);
    const count = width < 700 ? 90 : 190;
    particles = Array.from({ length: 3 }, (_, lane) =>
      Array.from({ length: count }, (_, index) => ({
        phase: index / count * TAU + lane * 0.43,
        spread: Math.sin(index * 127.1 + lane * 31.7) * 1.8,
        size: 0.9 + ((index * 17) % 11) / 11 * 1.3
      })));
    pointer.active = false;
    pointer.strength = 0;
    draw();
    sync();
  }

  function updateTheme() {
    if (destroyed) return;
    const style = window.getComputedStyle(hero);
    colors = ["--particle-teal", "--particle-blue", "--particle-cyan"]
      .map((name) => style.getPropertyValue(name).trim());
    draw();
    sync();
  }

  const intersection = new window.IntersectionObserver((entries) => {
    visible = entries.some((entry) => entry.isIntersecting);
    sync();
  });
  const sizeObserver = new window.ResizeObserver(resize);
  const themeObserver = new window.MutationObserver(updateTheme);
  intersection.observe(hero);
  sizeObserver.observe(hero);
  themeObserver.observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });

  listen(toggle, "click", () => {
    paused = !paused;
    try { window.sessionStorage.setItem("fluentqt-hero-motion", paused ? "paused" : "playing"); } catch {}
    sync();
  });
  listen(hero, "pointermove", (event) => {
    if (!enabled() || !visible || event.pointerType === "touch") return;
    const bounds = canvas.getBoundingClientRect();
    pointer.x = event.clientX - bounds.left;
    pointer.y = event.clientY - bounds.top;
    pointer.active = true;
  });
  listen(hero, "pointerleave", () => { pointer.active = false; });
  listen(document, "visibilitychange", sync);
  listen(reducedMotion, "change", sync);
  listen(forcedColors, "change", updateTheme);
  listen(window, "resize", resize);
  listen(window, "pagehide", (event) => {
    pageActive = false;
    if (event.persisted) sync();
    else destroy();
  });
  listen(window, "pageshow", () => {
    pageActive = true;
    resize();
  });

  function destroy() {
    if (destroyed) return;
    destroyed = true;
    stop();
    intersection.disconnect();
    sizeObserver.disconnect();
    themeObserver.disconnect();
    cleanups.forEach((cleanup) => cleanup());
    toggle.hidden = true;
    hero.dataset.particleState = "paused";
    context.clearRect(0, 0, width, height);
  }

  updateTheme();
  resize();
  toggle.hidden = false;
  return { destroy };
}
