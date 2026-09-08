const TAU = Math.PI * 2;
const MAX_PIXELS = 2_500_000;
const MAX_PULSES = 3;
const PULSE_SECONDS = 1.6;
const RIBBON_STROKES = [[18, 0.025], [5, 0.08], [0.8, 0.3]];

// Stable seeds avoid visual jumps when the page is resized or restored.
function seed(index) {
  const value = Math.sin(index * 127.1 + 311.7) * 43758.5453;
  return value - Math.floor(value);
}

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
  let dust = [];
  let pulses = [];
  const point = { x: 0, y: 0, depth: 0 };
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

  // Three woven ribbons frame the Gallery. Reuse the projected point rather
  // than allocating an object for every dot and trail segment on every frame.
  function position(angle, lane, spread = 0) {
    const mobile = width < 700;
    const weave = Math.sin(angle * 2 + elapsed * 0.22 + lane * 1.8);
    const radiusX = width * (mobile ? 0.85 : 0.51);
    const radiusY = height * (mobile ? 0.24 : 0.33);
    const x = Math.cos(angle) * (radiusX + spread * 19);
    const y = Math.sin(angle) * (radiusY + spread * 14) + weave * height * 0.06;
    const tilt = -0.36 + lane * 0.16;
    point.x = width * (mobile ? 0.66 : 0.78) + x * Math.cos(tilt) - y * Math.sin(tilt);
    point.y = height * (mobile ? 0.63 : 0.42) + x * Math.sin(tilt) + y * Math.cos(tilt);
    point.depth = (Math.sin(angle + lane * 0.7) + 1) / 2;
    return point;
  }

  function displacePoint() {
    if (pointer.strength > 0.01) {
      const dx = point.x - pointer.x;
      const dy = point.y - pointer.y;
      const influence = Math.exp(-(dx * dx + dy * dy) / 26000) * pointer.strength;
      point.x += (dx * 0.24 - dy * 0.32) * influence;
      point.y += (dy * 0.24 + dx * 0.32) * influence;
    }
    for (const pulse of pulses) {
      const dx = point.x - pulse.x;
      const dy = point.y - pulse.y;
      const distance = Math.hypot(dx, dy);
      const age = elapsed - pulse.started;
      const offset = (distance - age * 280) / 38;
      const force = Math.exp(-offset * offset) * (1 - age / PULSE_SECONDS) * 24;
      if (distance > 1) {
        point.x += dx / distance * force;
        point.y += dy / distance * force;
      }
    }
  }

  function drawRibbon(lane) {
    const steps = width < 700 ? 72 : 120;
    context.beginPath();
    for (let step = 0; step <= steps; step += 1) {
      position(step / steps * TAU, lane);
      displacePoint();
      if (step === 0) context.moveTo(point.x, point.y);
      else context.lineTo(point.x, point.y);
    }
    // Layered strokes keep the light soft without per-particle blur filters.
    for (const [lineWidth, alpha] of RIBBON_STROKES) {
      context.lineWidth = lineWidth;
      context.globalAlpha = alpha;
      context.stroke();
    }
  }

  function drawParticles(lane) {
    for (let layer = 0; layer < 3; layer += 1) {
      context.globalAlpha = 0.32 + layer * 0.28;
      context.beginPath();
      for (const particle of particles[lane][layer]) {
        const angle = particle.phase + elapsed * (0.09 + lane * 0.025 + layer * 0.008);
        position(angle, lane, particle.spread);
        displacePoint();
        const radius = particle.size * (0.7 + point.depth * 0.55);
        context.moveTo(point.x + radius, point.y);
        context.arc(point.x, point.y, radius, 0, TAU);
      }
      context.fill();
    }
  }

  function drawTrails(lane) {
    // Staggered, fading segments read as travel rather than blinking. The cost
    // is linear and fixed; there is no all-pairs particle connection search.
    for (let tail = 3; tail >= 0; tail -= 1) {
      context.beginPath();
      for (let spark = 0; spark < 6; spark += 1) {
        const angle = spark / 6 * TAU + elapsed * (0.22 + lane * 0.035) + lane;
        for (let step = 0; step <= 4; step += 1) {
          position(angle - (tail * 4 + step) * 0.009, lane);
          displacePoint();
          if (step === 0) context.moveTo(point.x, point.y);
          else context.lineTo(point.x, point.y);
        }
      }
      context.lineWidth = 9 - tail;
      context.globalAlpha = 0.09 - tail * 0.02;
      context.stroke();
      context.lineWidth = 2.6 - tail * 0.5;
      context.globalAlpha = 0.95 - tail * 0.22;
      context.stroke();
    }
  }

  function draw() {
    context.clearRect(0, 0, width, height);
    if (highContrast() || !width || !height) return;

    context.fillStyle = colors[1];
    context.globalAlpha = 0.24;
    context.beginPath();
    for (const mote of dust) {
      const x = (mote.x * width + elapsed * mote.speed) % width;
      const y = mote.y * height + Math.sin(elapsed * 0.2 + mote.x * TAU) * 8;
      context.moveTo(x + mote.size, y);
      context.arc(x, y, mote.size, 0, TAU);
    }
    context.fill();

    for (let lane = 0; lane < 3; lane += 1) {
      context.strokeStyle = colors[lane];
      context.fillStyle = colors[lane];
      drawRibbon(lane);
      drawParticles(lane);
      drawTrails(lane);
    }
    for (const pulse of pulses) {
      const age = elapsed - pulse.started;
      const radius = Math.max(1, age * 280);
      context.strokeStyle = colors[2];
      context.lineWidth = 1;
      context.globalAlpha = 0.35 * (1 - age / PULSE_SECONDS) ** 2;
      context.beginPath();
      context.arc(pulse.x, pulse.y, radius, 0, TAU);
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
      pointer.strength += ((pointer.active ? 1 : 0) - pointer.strength) * (1 - Math.exp(-delta / 140));
      while (pulses.length && elapsed - pulses[0].started >= PULSE_SECONDS) pulses.shift();
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
    pulses = [];
  }

  function sync() {
    if (destroyed) return;
    const running = enabled() && visible && width > 0 && height > 0 && !document.hidden && pageActive;
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
    const count = width < 700 ? 42 : 110;
    particles = Array.from({ length: 3 }, (_, lane) =>
      Array.from({ length: 3 }, (_, layer) =>
        Array.from({ length: count }, (_, index) => ({
          phase: index / count * TAU + lane * 0.43 + layer * 0.17,
          spread: (seed(index + lane * count) - 0.5) * (1.2 + layer * 2.1),
          size: 0.55 + layer * 0.35 + seed(index + 19) * 0.8
        }))));
    dust = Array.from({ length: width < 700 ? 24 : 60 }, (_, index) => ({
      x: seed(index), y: seed(index + 81), size: 0.5 + seed(index + 17),
      speed: 1 + seed(index + 41) * 3
    }));
    pointer.active = false;
    pointer.strength = 0;
    pulses = [];
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
  listen(hero, "pointerdown", (event) => {
    if (!enabled() || !visible || event.pointerType === "touch" || event.button !== 0) return;
    if (event.target.closest("a, button, input, select, textarea, label")) return;
    const bounds = canvas.getBoundingClientRect();
    if (pulses.length === MAX_PULSES) pulses.shift();
    pulses.push({ x: event.clientX - bounds.left, y: event.clientY - bounds.top, started: elapsed });
  });
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
