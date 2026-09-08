import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

// Load the browser module without changing the repository's Node module mode.
const source = await readFile(new URL("../../site/hero-particles.js", import.meta.url), "utf8");
const { createHeroParticles } = await import(`data:text/javascript;base64,${Buffer.from(source).toString("base64")}`);

class Target {
  listeners = new Map();
  dataset = {};
  attributes = {};
  addEventListener(type, handler) {
    if (!this.listeners.has(type)) this.listeners.set(type, new Set());
    this.listeners.get(type).add(handler);
  }
  removeEventListener(type, handler) { this.listeners.get(type)?.delete(handler); }
  emit(type, event = {}) { this.listeners.get(type)?.forEach((handler) => handler(event)); }
  setAttribute(name, value) { this.attributes[name] = value; }
  get listenerCount() { return [...this.listeners.values()].reduce((sum, set) => sum + set.size, 0); }
}

function fixture({ width = 1280, height = 950, reduced = false, stored = "", canvasAvailable = true } = {}) {
  const hero = new Target();
  const toggle = new Target();
  toggle.hidden = true;
  const root = new Target();
  root.dataset.theme = "light";
  const document = new Target();
  document.hidden = false;
  document.documentElement = root;
  const window = new Target();
  window.devicePixelRatio = 3;
  const frames = new Map();
  let frameId = 0;
  let now = 0;
  window.requestAnimationFrame = (callback) => { frames.set(++frameId, callback); return frameId; };
  window.cancelAnimationFrame = (id) => frames.delete(id);
  window.sessionStorage = { getItem: () => stored, setItem: (_, value) => { stored = value; } };
  window.getComputedStyle = () => ({ getPropertyValue: () => root.dataset.theme === "dark" ? "#72e4f5" : "#168c89" });
  const reducedQuery = new Target();
  reducedQuery.matches = reduced;
  const forcedQuery = new Target();
  forcedQuery.matches = false;
  window.matchMedia = (query) => query.includes("reduced-motion") ? reducedQuery : forcedQuery;
  const observers = {};
  for (const name of ["IntersectionObserver", "ResizeObserver", "MutationObserver"]) {
    window[name] = class {
      constructor(callback) { this.callback = callback; observers[name] = this; }
      observe() {}
      disconnect() { this.disconnected = true; }
    };
  }
  const context = {
    draws: 0, dots: 0, coordinates: [],
    clearRect() { this.draws += 1; this.dots = 0; this.coordinates = []; },
    setTransform() {}, beginPath() {}, moveTo() {}, lineTo() {}, stroke() {}, fill() {},
    arc(x, y, radius) {
      assert.ok(Number.isFinite(x) && Number.isFinite(y) && radius > 0);
      this.dots += 1;
      this.coordinates.push([x, y]);
    }
  };
  const canvas = {
    getContext: () => canvasAvailable ? context : null,
    getBoundingClientRect: () => ({ width, height, top: 80, left: 0 })
  };
  hero.ownerDocument = document;
  hero.closest = () => null;
  document.defaultView = window;
  hero.querySelector = (selector) => selector === ".hero-particles" ? canvas : toggle;
  const controller = createHeroParticles(hero);
  return {
    hero, toggle, root, document, window, frames, context, canvas, controller, observers,
    get stored() { return stored; },
    visible(value) { observers.IntersectionObserver.callback([{ isIntersecting: value }]); },
    advance(milliseconds = 20) {
      now += milliseconds;
      const callbacks = [...frames.values()];
      frames.clear();
      callbacks.forEach((callback) => callback(now));
    },
    reduced(value) { reducedQuery.matches = value; reducedQuery.emit("change"); },
    forced(value) { forcedQuery.matches = value; forcedQuery.emit("change"); },
    theme(value) { root.dataset.theme = value; observers.MutationObserver.callback(); },
    resize(w, h) { width = w; height = h; observers.ResizeObserver.callback(); },
    targets: [hero, toggle, root, document, window, reducedQuery, forcedQuery]
  };
}

test("animation runs only while the hero and document are visible, with one pending frame", () => {
  const f = fixture();
  assert.equal(f.frames.size, 0);
  f.visible(true);
  assert.equal(f.frames.size, 1);
  f.visible(true);
  f.theme("dark");
  assert.equal(f.frames.size, 1);
  const before = f.context.draws;
  f.advance();
  assert.ok(f.context.draws > before);
  f.visible(false);
  assert.equal(f.frames.size, 0);
  assert.equal(f.hero.dataset.particleState, "suspended");
  f.visible(true);
  f.document.hidden = true;
  f.document.emit("visibilitychange");
  assert.equal(f.frames.size, 0);
  f.document.hidden = false;
  f.document.emit("visibilitychange");
  assert.equal(f.frames.size, 1);
  f.controller.destroy();
});

test("pause persists and is not overridden by resize, theme, or visibility changes", () => {
  const f = fixture();
  f.visible(true);
  f.toggle.emit("click");
  assert.equal(f.toggle.attributes["aria-pressed"], "false");
  assert.equal(f.stored, "paused");
  f.resize(390, 1050);
  f.theme("dark");
  f.visible(false);
  f.visible(true);
  assert.equal(f.frames.size, 0);
  f.toggle.emit("click");
  assert.equal(f.frames.size, 1);
  assert.equal(f.stored, "playing");
  f.controller.destroy();
  const restored = fixture({ stored: "paused" });
  restored.visible(true);
  assert.equal(restored.frames.size, 0);
  restored.controller.destroy();
});

test("reduced motion keeps a static image; contrast preferences remove decoration", () => {
  const f = fixture({ reduced: true });
  f.visible(true);
  assert.ok(f.context.dots > 0);
  assert.equal(f.frames.size, 0);
  assert.equal(f.toggle.disabled, true);
  f.reduced(false);
  assert.equal(f.frames.size, 1);
  f.theme("high-contrast");
  assert.equal(f.context.dots, 0);
  assert.equal(f.frames.size, 0);
  assert.equal(f.hero.dataset.particleState, "hidden");
  f.theme("light");
  f.forced(true);
  assert.equal(f.context.dots, 0);
  assert.equal(f.frames.size, 0);
  f.forced(false);
  assert.equal(f.frames.size, 1);
  f.reduced(true);
  assert.equal(f.frames.size, 0);
  f.controller.destroy();
});

test("mobile particle count and canvas pixel memory stay bounded through resize", () => {
  const f = fixture();
  const desktopDots = f.context.dots;
  assert.ok(desktopDots <= 1_100);
  assert.ok(f.canvas.width * f.canvas.height <= 2_500_000);
  f.resize(390, 1200);
  assert.ok(f.context.dots < desktopDots);
  assert.ok(f.context.dots <= 450);
  f.visible(true);
  let draws = f.context.draws;
  for (let n = 0; n < 120; n += 1) f.advance(1000 / 120);
  assert.ok(f.context.draws - draws <= 31, "mobile drawing is capped at 30 fps");
  f.resize(3840, 2160);
  assert.ok(f.canvas.width * f.canvas.height <= 2_500_000);
  assert.equal(f.frames.size, 1);
  f.controller.destroy();
});

test("pointer interaction changes the ribbon locally and ignores touch scrolling", () => {
  const f = fixture();
  const baseline = fixture();
  f.visible(true);
  baseline.visible(true);
  const [x, y] = f.context.coordinates[200];
  f.hero.emit("pointermove", { clientX: x + 25, clientY: y + 80, pointerType: "touch" });
  f.advance();
  baseline.advance();
  assert.deepEqual(f.context.coordinates, baseline.context.coordinates);
  f.hero.emit("pointermove", { clientX: x + 25, clientY: y + 80, pointerType: "mouse" });
  f.advance();
  baseline.advance();
  assert.notDeepEqual(f.context.coordinates, baseline.context.coordinates);
  f.controller.destroy();
  baseline.controller.destroy();
});

test("background clicks emit bounded, short-lived pulses without hijacking links or touch", () => {
  const f = fixture();
  f.visible(true);
  const dots = f.context.dots;
  const click = { clientX: 900, clientY: 400, pointerType: "mouse", button: 0, target: f.hero };
  f.hero.emit("pointerdown", { ...click, pointerType: "touch" });
  f.hero.emit("pointerdown", { ...click, button: 2 });
  f.hero.emit("pointerdown", { ...click, target: { closest: () => ({ tagName: "A" }) } });
  f.advance();
  assert.equal(f.context.dots, dots);

  for (let n = 0; n < 50; n += 1) f.hero.emit("pointerdown", click);
  f.advance();
  assert.equal(f.context.dots, dots + 3, "rapid clicks retain at most three pulses");
  for (let n = 0; n < 90; n += 1) f.advance();
  assert.equal(f.context.dots, dots, "pulses expire instead of accumulating");

  f.toggle.emit("click");
  f.hero.emit("pointerdown", click);
  assert.equal(f.frames.size, 0);
  f.toggle.emit("click");
  f.advance();
  assert.equal(f.context.dots, dots, "clicks while paused do not queue a pulse for resume");
  f.controller.destroy();
});

test("a collapsed hero does not schedule frames and can resume after layout", () => {
  const f = fixture({ width: 0, height: 0 });
  f.visible(true);
  assert.equal(f.frames.size, 0);
  assert.equal(f.context.dots, 0);
  f.resize(1280, 950);
  assert.equal(f.frames.size, 1);
  f.resize(0, 0);
  assert.equal(f.frames.size, 0);
  assert.equal(f.context.dots, 0);
  f.controller.destroy();
});

test("page cache restoration resumes; navigation tears down observers, listeners, and frames", () => {
  const f = fixture();
  f.visible(true);
  f.window.emit("pagehide", { persisted: true });
  assert.equal(f.frames.size, 0);
  f.window.emit("pageshow");
  assert.equal(f.frames.size, 1);
  f.window.emit("pagehide", { persisted: false });
  assert.equal(f.frames.size, 0);
  assert.ok(Object.values(f.observers).every((observer) => observer.disconnected));
  assert.ok(f.targets.every((target) => target.listenerCount === 0));
  assert.equal(f.toggle.hidden, true);
  f.controller.destroy();
});

test("missing Canvas and unavailable session storage do not break the page", () => {
  assert.equal(createHeroParticles(null), null);
  const missing = fixture({ canvasAvailable: false });
  assert.equal(missing.controller, null);
  assert.equal(missing.toggle.hidden, true);
  assert.equal(missing.frames.size, 0);
  const f = fixture();
  f.window.sessionStorage.setItem = () => { throw new Error("storage denied"); };
  f.visible(true);
  assert.doesNotThrow(() => f.toggle.emit("click"));
  assert.equal(f.frames.size, 0);
  f.controller.destroy();
});
