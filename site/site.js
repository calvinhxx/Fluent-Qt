const defaultRuntimeStrings = {
  "a11y.systemHighContrastTheme": "System high contrast theme is active",
  "a11y.useDarkTheme": "Switch to dark theme",
  "a11y.useHighContrastTheme": "Switch to high contrast theme",
  "a11y.useLightTheme": "Switch to light theme",
  "a11y.openMenu": "Open navigation",
  "a11y.closeMenu": "Close navigation",
  "gallery.waitingTitle": "Loading Gallery…",
  "gallery.waitingDetail": "",
  "gallery.loadingTitle": "Loading…",
  "gallery.loadingDetail": "",
  "gallery.slowTitle": "Still loading…",
  "gallery.slowDetail": "",
  "gallery.readyTitle": "Gallery",
  "gallery.readyDetail": "",
  "gallery.errorTitle": "Could not load the example",
  "gallery.errorDetail": "Check your connection and try again.",
  "gallery.retryAction": "Retry",
  "copy.copy": "Copy",
  "copy.copied": "Copied",
  "copy.announcement": "Code copied to the clipboard",
  "copy.failed": "Copy failed; select the code manually",
  "support.kofiMobileHelp": "On your phone? Tap either button above to tip directly in your browser.",
  "support.mobileHelp": "On your phone? Take a quick screenshot and load it from your payment app’s album scanner.",
  "support.kofiInstruction": "Ko-fi USD channel ready—open the tip panel above, head to Ko-fi directly, or scan the QR code on your phone.",
  "support.linkedAnyInstruction": "Followed the little heart? {tier} is ready—scan and punch in whatever feels right. Every drop counts!",
  "support.linkedInstruction": "Followed the little heart? {tier} is locked in at ¥{amount}—scan, double-check the number, and you’re officially a legend.",
  "support.anyInstruction": "Freestyle code ready—scan and punch in whatever feels right. Every drop counts!",
  "support.instruction": "Locked in at ¥{amount}—scan, double-check the number, and you’re officially a legend.",
  "support.any": "Freestyle",
  "support.tier19": "Iced coffee fuel",
  "support.tier49": "Fresh context window",
  "support.tier99": "Max-reasoning night",
  "downloads.macChoice": "Choose macOS build",
  "downloads.openRelease": "Open latest release",
  "downloads.downloadFor": "Download for {platform}",
  "spatial.runTitle": "Loading example…",
  "spatial.runCopy": "",
  "spatial.stopped": "Example stopped",
  "spatial.reload": "Reload example"
};

function loadRuntimeStrings() {
  const payload = document.getElementById("site-i18n")?.textContent;
  if (!payload) return defaultRuntimeStrings;
  try {
    const parsed = JSON.parse(payload);
    if (parsed && typeof parsed === "object") {
      return { ...defaultRuntimeStrings, ...parsed };
    }
  } catch {
    // Fall back to English defaults when inline JSON is unavailable.
  }
  return defaultRuntimeStrings;
}

const runtimeStrings = loadRuntimeStrings();

const fallbackVersion = "Latest";
const latestReleaseUrl = "https://github.com/calvinhxx/Fluent-Qt/releases/latest";
const latestReleaseApi = "https://api.github.com/repos/calvinhxx/Fluent-Qt/releases/latest";
const releaseState = {
  architecture: "",
  language: document.documentElement.dataset.siteLanguage || (document.documentElement.lang.toLowerCase().startsWith("zh") ? "zh" : "en"),
  platform: "other",
  release: null
};

const metaThemeColor = document.querySelector("meta[name='theme-color']");
const siteHeader = document.querySelector(".site-header");
const menuButton = document.querySelector(".menu-button");
const themeToggle = document.querySelector("[data-theme-toggle]");
const copyAnnouncement = document.querySelector(".copy-announcement");
const systemThemeQuery = window.matchMedia("(prefers-color-scheme: dark)");
const forcedColorsQuery = window.matchMedia("(forced-colors: active)");
let followsSystemTheme = true;
let preferredTheme = "light";
let spatialShowcase = null;

const galleryRoot = document.querySelector("[data-live-gallery]");
const galleryFrame = document.querySelector("[data-gallery-frame]");
const galleryMessage = document.querySelector("[data-gallery-message]");
const galleryDetail = document.querySelector("[data-gallery-detail]");
const galleryLoadButtons = Array.from(document.querySelectorAll("[data-gallery-load]"));
let galleryState = "waiting";
let gallerySlowTimer = 0;
let galleryRetry = 0;

function dictionary() {
  return runtimeStrings;
}

function isLocalPreview() {
  return location.hostname === "localhost" || location.hostname === "127.0.0.1";
}

function loadAnalytics() {
  const token = document
    .querySelector("meta[name='fluent-qt-cloudflare-token']")
    ?.getAttribute("content")
    ?.trim();

  if (!token || isLocalPreview()) {
    return;
  }

  const script = document.createElement("script");
  script.defer = true;
  script.src = "https://static.cloudflareinsights.com/beacon.min.js";
  script.dataset.cfBeacon = JSON.stringify({ token });
  document.head.appendChild(script);
}

function trackEvent(name, properties = {}) {
  if (!name) {
    return;
  }
  if (typeof window.plausible === "function") {
    window.plausible(name, { props: properties });
  }
  if (window.umami && typeof window.umami.track === "function") {
    window.umami.track(name, properties);
  }
  if (typeof window.gtag === "function") {
    window.gtag("event", name, properties);
  }
}

function detectPlatform() {
  const source = `${navigator.userAgentData?.platform || ""} ${navigator.platform || ""} ${navigator.userAgent || ""}`.toLowerCase();
  if (source.includes("win")) return "windows";
  if (source.includes("mac")) return "mac";
  if (source.includes("linux")) return "linux";
  return "other";
}

async function detectArchitecture() {
  try {
    if (navigator.userAgentData?.getHighEntropyValues) {
      const values = await navigator.userAgentData.getHighEntropyValues(["architecture"]);
      return String(values.architecture || "").toLowerCase();
    }
  } catch {
    // Architecture is only a hint; platform defaults remain usable.
  }
  return "";
}

function mapRelease(release) {
  const assets = {};
  (release.assets || []).forEach((asset) => {
    const name = asset.name || "";
    const url = asset.browser_download_url || asset.url || "";
    if (/Windows-x64\.exe$/i.test(name)) assets.windowsX64 = url;
    else if (/Windows-arm64\.exe$/i.test(name)) assets.windowsArm = url;
    else if (/Darwin-arm64\.dmg$/i.test(name)) assets.macArm = url;
    else if (/Darwin-x86_64\.dmg$/i.test(name)) assets.macIntel = url;
    else if (/Linux-(?:x86_64|x64)\.deb$/i.test(name)) assets.linuxX64 = url;
    else if (/Linux-arm64\.deb$/i.test(name)) assets.linuxArm = url;
  });

  return {
    assets,
    tagName: release.tag_name || fallbackVersion,
    url: release.html_url || latestReleaseUrl
  };
}

function fallbackRelease() {
  return {
    assets: {},
    tagName: fallbackVersion,
    url: document.querySelector("[data-release-link]")?.href || latestReleaseUrl
  };
}

function isArm() {
  return /arm|aarch64/.test(releaseState.architecture);
}

function isX86() {
  return /x86|x64|amd64/.test(releaseState.architecture);
}

function primaryAssetKey() {
  if (releaseState.platform === "windows") return isArm() ? "windowsArm" : "windowsX64";
  if (releaseState.platform === "mac") {
    if (isX86()) return "macIntel";
    if (isArm()) return "macArm";
    return "macChoice";
  }
  if (releaseState.platform === "linux") return isArm() ? "linuxArm" : "linuxX64";
  return "release";
}

function primaryLabel(assetKey) {
  const values = dictionary();
  if (assetKey === "macChoice") {
    return values["downloads.macChoice"];
  }

  const platform = assetKey.startsWith("windows")
    ? "Windows"
    : assetKey.startsWith("mac")
      ? "macOS"
      : assetKey.startsWith("linux")
        ? "Linux"
        : "";

  if (!platform) {
    return values["downloads.openRelease"];
  }
  return values["downloads.downloadFor"].replace("{platform}", platform);
}

function primaryMeta(assetKey) {
  const labels = {
    windowsX64: "Windows · x64",
    windowsArm: "Windows · ARM64",
    macIntel: "macOS · x64",
    macArm: "macOS · ARM64",
    macChoice: "macOS · x64 / ARM64",
    linuxX64: "Linux · x64",
    linuxArm: "Linux · ARM64",
    release: "GitHub Releases"
  };
  return labels[assetKey] || labels.release;
}

function updateDownloads() {
  const release = releaseState.release || fallbackRelease();
  const preferredAssetKey = primaryAssetKey();
  const assetKey = release.assets[preferredAssetKey]
    ? preferredAssetKey
    : preferredAssetKey === "macChoice"
      ? "macChoice"
      : "release";
  const primaryUrl = release.assets[assetKey] || release.url;

  document.querySelectorAll("[data-release-asset]").forEach((link) => {
    const url = release.assets[link.dataset.releaseAsset];
    if (url) link.href = url;
  });
  document.querySelectorAll("[data-release-link]").forEach((link) => {
    link.href = release.url;
  });
  document.querySelectorAll("[data-release-version]").forEach((node) => {
    node.textContent = release.tagName;
  });
  document.querySelectorAll("[data-primary-download]").forEach((link) => {
    link.href = primaryUrl || release.url;
  });
  document.querySelectorAll("[data-primary-download-label]").forEach((node) => {
    node.textContent = primaryLabel(assetKey);
  });
  document.querySelectorAll("[data-primary-download-meta]").forEach((node) => {
    node.textContent = primaryMeta(assetKey);
  });
}

async function hydrateLatestRelease() {
  releaseState.release = fallbackRelease();
  updateDownloads();

  try {
    const response = await fetch(latestReleaseApi, {
      headers: { Accept: "application/vnd.github+json" }
    });
    if (response.ok) {
      releaseState.release = mapRelease(await response.json());
    }
  } catch {
    // Checked-in fallback links keep local and offline previews usable.
  }

  updateDownloads();
}

function savedTheme() {
  try {
    const value = localStorage.getItem("fluent-qt-theme");
    return normalizeTheme(value);
  } catch {
    return null;
  }
}

function storeTheme(theme) {
  try {
    localStorage.setItem("fluent-qt-theme", theme);
  } catch {
    // Theme switching still works when storage is disabled.
  }
}

function normalizeTheme(theme) {
  return theme === "light" || theme === "dark" || theme === "high-contrast"
    ? theme
    : null;
}

function effectiveTheme(theme) {
  return forcedColorsQuery.matches ? "high-contrast" : (normalizeTheme(theme) || "light");
}

function activeTheme() {
  return normalizeTheme(document.documentElement.dataset.theme) || "light";
}

function nextTheme(theme) {
  if (theme === "light") return "dark";
  if (theme === "dark") return "high-contrast";
  return "light";
}

function updateThemeToggle() {
  if (!themeToggle) return;
  const theme = activeTheme();
  const forced = forcedColorsQuery.matches;
  const label = dictionary()[forced
    ? "a11y.systemHighContrastTheme"
    : (theme === "light"
      ? "a11y.useDarkTheme"
      : (theme === "dark" ? "a11y.useHighContrastTheme" : "a11y.useLightTheme"))];
  themeToggle.disabled = forced;
  themeToggle.setAttribute("aria-pressed", theme === "high-contrast" ? "mixed" : String(theme === "dark"));
  themeToggle.setAttribute("aria-label", label);
  themeToggle.setAttribute("title", label);
}

function updateThemeMedia(theme) {
  const suffix = theme === "light" ? "Light" : "Dark";

  document.querySelectorAll("[data-theme-src-light][data-theme-src-dark]").forEach((image) => {
    const source = theme === "high-contrast" && image.dataset.themeSrcContrast
      ? image.dataset.themeSrcContrast
      : image.dataset[`themeSrc${suffix}`];
    if (source && image.getAttribute("src") !== source) image.setAttribute("src", source);
  });

  const previewLink = document.querySelector("[data-gallery-preview]");
  const heroImage = document.querySelector(".hero-window img");
  if (previewLink && heroImage) previewLink.href = heroImage.src;

  document.querySelectorAll("[data-theme-label]").forEach((label) => {
    const value = label.dataset[`themeLabel${suffix}`];
    if (value) label.textContent = value;
  });
}

function syncGalleryTheme() {
  spatialShowcase?.syncTheme();
  if (!galleryFrame?.contentWindow || !galleryFrame.getAttribute("src")) return;
  galleryFrame.contentWindow.postMessage({
    source: "fluent-qt-site",
    type: "theme",
    theme: activeTheme()
  }, window.location.origin);
}

function applyTheme(theme, shouldStore = false) {
  preferredTheme = normalizeTheme(theme) || "light";
  const effective = effectiveTheme(preferredTheme);
  document.documentElement.dataset.theme = effective;
  document.documentElement.style.colorScheme = effective === "light" ? "light" : "dark";
  metaThemeColor?.setAttribute(
    "content",
    effective === "high-contrast" ? "#000000" : (effective === "dark" ? "#070c12" : "#eef3f7")
  );
  updateThemeMedia(effective);
  updateThemeToggle();
  syncGalleryTheme();

  if (shouldStore) {
    followsSystemTheme = false;
    storeTheme(preferredTheme);
  }
}

function listenForMediaChange(query, handler) {
  if (typeof query.addEventListener === "function") query.addEventListener("change", handler);
  else query.addListener(handler);
}

function setupReveal() {
  const nodes = Array.from(document.querySelectorAll("[data-reveal]"));
  if (!nodes.length) return;

  if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    nodes.forEach((node) => node.classList.add("is-visible"));
    return;
  }

  nodes.forEach((node) => {
    const delay = Number(node.dataset.revealDelay || 0);
    if (delay > 0) node.style.setProperty("--reveal-delay", `${delay}ms`);
  });

  const observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-visible");
        observer.unobserve(entry.target);
      });
    },
    { rootMargin: "0px 0px -8% 0px", threshold: 0.12 }
  );

  nodes.forEach((node) => observer.observe(node));
}

function initializeTheme() {
  const storedTheme = savedTheme();
  followsSystemTheme = storedTheme === null;
  applyTheme(storedTheme || (systemThemeQuery.matches ? "dark" : "light"));
  listenForMediaChange(systemThemeQuery, (event) => {
    if (followsSystemTheme) applyTheme(event.matches ? "dark" : "light");
  });
  listenForMediaChange(forcedColorsQuery, () => applyTheme(preferredTheme));
}

function menuIsOpen() {
  return siteHeader?.dataset.menuOpen === "true";
}

function updateMenuLabel() {
  if (!menuButton) return;
  menuButton.setAttribute("aria-label", dictionary()[menuIsOpen() ? "a11y.closeMenu" : "a11y.openMenu"]);
}

function renderGalleryState() {
  if (!galleryRoot) return;
  const values = dictionary();
  const stateKeys = {
    waiting: ["gallery.waitingTitle", "gallery.waitingDetail"],
    loading: ["gallery.loadingTitle", "gallery.loadingDetail"],
    slow: ["gallery.slowTitle", "gallery.slowDetail"],
    ready: ["gallery.readyTitle", "gallery.readyDetail"],
    error: ["gallery.errorTitle", "gallery.errorDetail"]
  };
  const [messageKey, detailKey] = stateKeys[galleryState] || stateKeys.waiting;
  galleryRoot.dataset.galleryState = galleryState;
  document.documentElement.dataset.galleryLoadState = galleryState;
  if (galleryMessage) galleryMessage.textContent = values[messageKey];
  if (galleryDetail) { galleryDetail.textContent = values[detailKey]; galleryDetail.hidden = !values[detailKey]; }

  galleryLoadButtons.forEach((button) => {
    const label = button.querySelector("span") || button;
    label.textContent = values["gallery.retryAction"];
    button.hidden = galleryState !== "error";
    button.disabled = galleryState !== "error";
  });
}

function closeLanguageSwitch(restoreFocus = false) {
  const switcher = document.querySelector(".language-switch");
  if (!switcher || !switcher.open) return;
  switcher.open = false;
  if (restoreFocus) switcher.querySelector("summary")?.focus();
}

function setupLanguageSwitch() {
  const switcher = document.querySelector(".language-switch");
  if (!switcher) return;
  document.addEventListener("click", (event) => {
    if (switcher.open && !switcher.contains(event.target)) {
      closeLanguageSwitch(false);
    }
  });
  switcher.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && switcher.open) {
      event.stopPropagation();
      closeLanguageSwitch(true);
    }
  });
  switcher.querySelectorAll(".language-menu a").forEach((link) => {
    link.addEventListener("click", () => closeLanguageSwitch(false));
  });
}

function setMenuOpen(open) {
  if (!siteHeader || !menuButton) return;
  siteHeader.dataset.menuOpen = String(open);
  menuButton.setAttribute("aria-expanded", String(open));
  if (!open) closeLanguageSwitch(false);
  updateMenuLabel();
}

function setupNavigation() {
  setupLanguageSwitch();
  menuButton?.addEventListener("click", () => setMenuOpen(!menuIsOpen()));
  document.querySelectorAll(".nav-links a, .brand").forEach((link) => {
    link.addEventListener("click", () => setMenuOpen(false));
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") setMenuOpen(false);
  });
  window.matchMedia("(min-width: 1241px)").addEventListener?.("change", (event) => {
    if (event.matches) setMenuOpen(false);
  });

  const sectionLinks = Array.from(document.querySelectorAll('.nav-links a[href^="#"]'));
  const sections = sectionLinks
    .map((link) => ({ link, target: document.querySelector(link.getAttribute("href")) }))
    .filter((entry) => entry.target);
  if (sections.length) {
    let pending = false;
    const updateActiveSection = () => {
      pending = false;
      const offset = (siteHeader?.getBoundingClientRect().bottom || 84) + 72;
      let active = null;
      for (const entry of sections) {
        const rect = entry.target.getBoundingClientRect();
        if (rect.top <= offset && rect.bottom > offset) {
          active = entry.link;
        }
      }
      sectionLinks.forEach((link) => {
        if (link === active) link.setAttribute("aria-current", "location");
        else link.removeAttribute("aria-current");
      });
    };
    const scheduleUpdate = () => {
      if (!pending) {
        pending = true;
        window.requestAnimationFrame(updateActiveSection);
      }
    };
    window.addEventListener("scroll", scheduleUpdate, { passive: true });
    window.addEventListener("resize", scheduleUpdate, { passive: true });
    updateActiveSection();
  }
}

function fallbackCopy(text) {
  const textArea = document.createElement("textarea");
  textArea.value = text;
  textArea.setAttribute("readonly", "");
  textArea.style.position = "fixed";
  textArea.style.opacity = "0";
  document.body.appendChild(textArea);
  textArea.select();
  const copied = document.execCommand("copy");
  textArea.remove();
  return copied;
}

async function copyCode(button) {
  const code = document.getElementById(button.dataset.copyTarget);
  const label = button.querySelector("[data-copy-label]");
  if (!code || !label) return;

  let copied = fallbackCopy(code.textContent);
  if (!copied && navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(code.textContent);
      copied = true;
    } catch {
      copied = false;
    }
  }

  label.textContent = dictionary()[copied ? "copy.copied" : "copy.copy"];
  if (copyAnnouncement) {
    copyAnnouncement.textContent = dictionary()[copied ? "copy.announcement" : "copy.failed"];
  }
  trackEvent("copy_cmake", { success: String(copied) });
  window.setTimeout(() => {
    label.textContent = dictionary()["copy.copy"];
  }, 1500);
}

function galleryUrl(retry = false) {
  const url = new URL(galleryRoot.dataset.gallerySrc, window.location.href);
  url.searchParams.set("host-theme", activeTheme());
  if (retry) url.searchParams.set("site-retry", String(galleryRetry));
  return url.href;
}

function loadGallery(retry = false, trigger = "manual") {
  if (!galleryRoot || !galleryFrame) return;
  if (galleryState === "loading" || galleryState === "slow" || galleryState === "ready") return;

  if (retry) galleryRetry += 1;
  galleryState = "loading";
  renderGalleryState();
  window.clearTimeout(gallerySlowTimer);
  gallerySlowTimer = window.setTimeout(() => {
    if (galleryState === "loading") {
      galleryState = "slow";
      renderGalleryState();
    }
  }, 30000);
  galleryFrame.src = galleryUrl(retry);
  trackEvent("gallery_embed_start", { trigger, retry: String(galleryRetry) });
}

function setupLiveGallery() {
  if (!galleryRoot || !galleryFrame) return;
  renderGalleryState();
  galleryLoadButtons.forEach((button) => {
    button.addEventListener("click", () => loadGallery(galleryState === "error", "manual"));
  });

  window.addEventListener("message", (event) => {
    if (event.origin !== window.location.origin || event.source !== galleryFrame.contentWindow) return;
    if (event.data?.source !== "fluent-qt-gallery") return;
    if (event.data.state === "ready") {
      window.clearTimeout(gallerySlowTimer);
      galleryState = "ready";
      renderGalleryState();
      syncGalleryTheme();
      trackEvent("gallery_embed_ready", { load_ms: String(event.data.detail?.loadMs || "") });
    } else if (event.data.state === "error" || event.data.state === "exit") {
      window.clearTimeout(gallerySlowTimer);
      galleryState = "error";
      renderGalleryState();
      trackEvent("gallery_embed_error", { state: event.data.state });
    }
  });

  galleryFrame.addEventListener("load", () => {
    syncGalleryTheme();
    if (galleryState !== "loading" && galleryState !== "slow") return;
    try {
      if (!galleryFrame.contentDocument?.querySelector("#qt-container")) {
        window.clearTimeout(gallerySlowTimer);
        galleryState = "error";
        renderGalleryState();
      }
    } catch {
      // The Pages deployment is same-origin. A custom host can still use the
      // Gallery's explicit postMessage contract as the authoritative signal.
    }
  });

  if (!("IntersectionObserver" in window)) { loadGallery(false, "visible"); return; }
  const preloadMargin = navigator.connection?.saveData ? "0px" : "240px 0px";
  const observer = new IntersectionObserver((entries) => {
    if (!entries.some((entry) => entry.isIntersecting)) return;
    observer.disconnect();
    loadGallery(false, "viewport");
  }, { rootMargin: preloadMargin, threshold: 0.01 });
  observer.observe(galleryRoot);
}

function setupInteractions() {
  themeToggle?.addEventListener("click", () => {
    if (!forcedColorsQuery.matches) applyTheme(nextTheme(preferredTheme), true);
  });
  document.querySelectorAll("[data-copy-target]").forEach((button) => {
    button.addEventListener("click", () => copyCode(button));
  });
  document.querySelectorAll("[data-analytics-event]").forEach((link) => {
    link.addEventListener("click", () => {
      trackEvent(link.dataset.analyticsEvent, {
        asset: link.dataset.releaseAsset || (link.hasAttribute("data-primary-download") ? primaryAssetKey() : ""),
        release: releaseState.release?.tagName || fallbackVersion
      });
    });
  });
}

function setupSupport() {
  const root = document.querySelector("[data-support]");
  if (!root) return;
  const kofiOpen = root.querySelector("[data-kofi-open]");
  const kofiDialog = document.querySelector(".support-kofi-dialog");
  if (kofiOpen && typeof kofiDialog?.showModal === "function") {
    kofiOpen.addEventListener("click", (event) => {
      if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
      event.preventDefault();
      const template = kofiDialog.querySelector("template");
      if (template) template.replaceWith(template.content.cloneNode(true));
      kofiDialog.showModal();
    });
    kofiDialog.addEventListener("click", (event) => {
      if (event.target === kofiDialog && typeof kofiDialog.close === "function") kofiDialog.close();
    });
  }
  const controls = root.querySelector("[data-support-controls]");
  const instruction = root.querySelector("[data-support-instruction]");
  // An incomplete template keeps the static codes and leaves the controls hidden.
  if (!controls || !instruction
      || !controls.querySelector('[name="support-method"]:checked')
      || !controls.querySelector('[name="support-amount"]:checked')) return;
  const cnyFieldset = controls.querySelector("[data-support-cny]");
  const kofiActions = controls.querySelector("[data-support-kofi-actions]");
  const mobileHelp = root.querySelector("[data-support-mobile-help]");
  const kofiFallback = root.querySelector(".support-kofi-fallback");
  const payments = Array.from(root.querySelectorAll("[data-support-payment]"), (payment) => {
    const method = payment.dataset.supportPayment;
    const code = payment.querySelector("[data-support-code]");
    const viewport = payment.querySelector(".support-code-window");
    const loading = payment.querySelector("[data-support-loading]");
    const error = payment.querySelector("[data-support-error]");
    const genericImage = viewport.querySelector("img");
    const images = new Map([["any", genericImage]]);
    let activeImage = null;

    const syncImage = (image) => {
      // A late response for an earlier selection must not reveal or hide this code.
      if (image !== activeImage) return;
      const ready = image.complete && image.naturalWidth > 0;
      const failed = image.complete && image.naturalWidth === 0;
      image.hidden = !ready;
      code.hidden = failed;
      loading.hidden = image.complete;
      error.hidden = !failed;
    };
    const watchImage = (image) => {
      image.addEventListener("load", () => syncImage(image));
      image.addEventListener("error", () => syncImage(image));
    };
    watchImage(genericImage);

    const select = (amount) => {
      const key = method === "kofi" ? "any" : amount;
      let image = images.get(key);
      if (!image) {
        image = genericImage.cloneNode(false);
        image.removeAttribute("data-i18n-alt");
        image.dataset.supportFixed = "";
        image.alt = `${genericImage.alt} · ¥${key}`;
        // Resolve beside the localized generic asset, including project subpaths.
        image.src = new URL(`${method}-${key}.jpg`, genericImage.src).href;
        watchImage(image);
        images.set(key, image);
      }
      image.loading = "eager";
      activeImage = image;
      syncImage(image);
      // Separate image nodes prevent the previous amount's pixels lingering on load.
      viewport.replaceChildren(image);
    };
    return { payment, method, select };
  });
  // GitHub's Sponsor dialog shows funding links as bare URLs, so each link names its tier.
  const linkedAmount = new Map([
    ["iced-coffee", "19"], ["context-window", "49"], ["max-reasoning", "99"], ["freestyle", "any"]
  ]).get(new URLSearchParams(window.location.search).get("fuel"));
  const linkedInput = linkedAmount && controls.querySelector(`[name="support-amount"][value="${linkedAmount}"]`);
  if (linkedInput) {
    linkedInput.checked = true;
    // The English homepage defaults to Ko-fi; switch to WeChat Pay so CNY tier links show their QR code.
    if (controls.querySelector('[name="support-method"]:checked').value === "kofi") {
      const wechatInput = controls.querySelector('[name="support-method"][value="wechat"]');
      if (wechatInput) wechatInput.checked = true;
    }
  }
  // Greet the arrival until the visitor picks something else.
  let linked = Boolean(linkedInput);
  const render = () => {
    const method = controls.querySelector('[name="support-method"]:checked').value;
    const amount = controls.querySelector('[name="support-amount"]:checked').value;
    const isKofi = method === "kofi";
    if (cnyFieldset) cnyFieldset.hidden = isKofi;
    if (kofiActions) kofiActions.hidden = !isKofi;
    const values = dictionary();
    if (mobileHelp) {
      mobileHelp.textContent = values[isKofi ? "support.kofiMobileHelp" : "support.mobileHelp"];
    }
    if (isKofi) {
      instruction.textContent = values["support.kofiInstruction"];
    } else {
      const any = amount === "any";
      const message = linked
        ? values[any ? "support.linkedAnyInstruction" : "support.linkedInstruction"]
        : values[any ? "support.anyInstruction" : "support.instruction"];
      instruction.textContent = message
        .replace("{amount}", amount)
        .replace("{tier}", values[any ? "support.any" : `support.tier${amount}`]);
    }
    payments.forEach(({ payment, method: paymentMethod, select }) => {
      if (paymentMethod === method) select(amount);
      payment.hidden = paymentMethod !== method;
    });
  };

  controls.addEventListener("change", () => {
    linked = false;
    render();
  });
  // Reset the derived view when a browser restores radio selections from history.
  window.addEventListener("pageshow", render);
  render();
  if (kofiFallback) kofiFallback.hidden = true;
  controls.hidden = false;
}

function setupGalleryPreview() {
  const link = document.querySelector("[data-gallery-preview]");
  const dialog = document.querySelector(".gallery-preview");
  const source = document.querySelector(".hero-window img");
  if (!link || !source || typeof dialog?.showModal !== "function") return;

  link.addEventListener("click", (event) => {
    if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
    event.preventDefault();
    const image = dialog.querySelector("img");
    image.src = source.src;
    image.alt = source.alt;
    dialog.showModal();
    const scroll = dialog.querySelector(".gallery-preview-scroll");
    scroll.scrollTop = scroll.scrollLeft = 0;
  });
}

async function setupSpatialShowcase() {
  const root = document.querySelector('[data-spatial-showcase]');
  if (!root) return;
  try {
    const url = new URL(root.dataset.showcaseController, document.baseURI);
    const { createSpatialShowcase } = await import(url.href);
    spatialShowcase = createSpatialShowcase(root, { theme: activeTheme, strings: dictionary });
  } catch {
    // A failed controller must not leave the sample waiting with no way to retry.
    root.dataset.showcaseState = 'error';
    root.querySelector('[data-showcase-message]').textContent = dictionary()['gallery.errorTitle'];
    const detail = root.querySelector('[data-showcase-detail]');
    detail.textContent = dictionary()['gallery.errorDetail'];
    detail.hidden = false;
    const retry = root.querySelector('[data-showcase-start]');
    retry.textContent = dictionary()['gallery.retryAction'];
    retry.hidden = false;
    retry.disabled = false;
    retry.addEventListener('click', () => window.location.reload(), { once: true });
  }
}

document.documentElement.setAttribute("data-i18n-ready", "");
loadAnalytics();
setupNavigation();
setupInteractions();
setupGalleryPreview();
try {
  setupSupport();
} catch (error) {
  console.warn("Support controls could not initialize; continuing page setup.", error);
}
setupLiveGallery();
setupReveal();
releaseState.platform = detectPlatform();
initializeTheme();
setupSpatialShowcase();
import("./hero-particles.js?v=20260908-1")
  .then(({ createHeroParticles }) => createHeroParticles(document.querySelector(".hero")))
  .catch(() => { /* Keep the static hero if the optional decoration cannot load. */ });
hydrateLatestRelease();
detectArchitecture().then((architecture) => {
  releaseState.architecture = architecture;
  updateDownloads();
});
