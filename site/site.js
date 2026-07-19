const translations = {
  zh: {
    "meta.title": "Fluent-Qt — Qt Widgets 的 Fluent 控件库",
    "meta.description": "Fluent-Qt 是面向 Qt Widgets 的 C++17 控件库，支持 Qt 5.15+、Qt 6.2+ 与主流桌面平台。",
    "a11y.skip": "跳到主要内容",
    "a11y.primaryNav": "主导航",
    "a11y.home": "Fluent-Qt 首页",
    "a11y.openMenu": "打开导航",
    "a11y.closeMenu": "关闭导航",
    "a11y.language": "语言",
    "a11y.useDarkTheme": "切换到深色主题",
    "a11y.useLightTheme": "切换到浅色主题",
    "a11y.specs": "项目规格",
    "a11y.downloads": "平台下载",
    "nav.why": "概览",
    "nav.components": "控件",
    "nav.quickStart": "接入",
    "nav.gallery": "Gallery",
    "nav.downloads": "下载",
    "hero.tagline": "面向 Qt Widgets 的 Fluent 控件库",
    "hero.download": "下载 Gallery",
    "hero.quickStart": "查看接入方式",
    "hero.visualCaption": "Gallery 实际界面",
    "spec.runtime": "UI 运行时",
    "spec.qt": "Qt 支持",
    "spec.toolchain": "工具链",
    "spec.platforms": "桌面平台",
    "positioning.titleLine1": "独立控件",
    "positioning.titleLine2": "不止主题皮肤",
    "positioning.copy": "每个控件都有独立的 C++ API、状态与布局行为",
    "positioning.libraryTitle": "Qt Widgets 原生用法",
    "positioning.libraryCopy": "沿用 signals、slots、layout 与事件循环",
    "positioning.galleryTitle": "Qt 5 / Qt 6 共用接口",
    "positioning.galleryCopy": "应用侧无需为 Qt 主版本维护两套调用",
    "positioning.shipTitle": "清晰的公共边界",
    "positioning.shipCopy": "业务代码只依赖统一入口头文件与 FluentQt::FluentQt",
    "components.title": "控件按用途分类",
    "components.copy": "以下为 Gallery 中的部分控件",
    "components.buttonCopy": "包含默认、强调、禁用和图标状态",
    "components.familiesLabel": "10 个控件家族",
    "components.families": "基础输入 · 集合视图 · 日期与时间 · 对话框 · 菜单 · 导航 · 滚动 · 状态信息 · 文本输入 · 窗口系统",
    "quickStart.title": "CMake 接入",
    "quickStart.copy": "支持 FetchContent、add_subdirectory 与 find_package；应用启动时需在 QApplication 前调用 fluent::prepareHighDpiApplication()",
    "quickStart.example": "查看完整 hello_world",
    "copy.copy": "复制",
    "copy.copied": "已复制",
    "copy.announcement": "代码已复制到剪贴板",
    "copy.failed": "复制失败，请手动选择代码",
    "gallery.title": "Gallery 控件示例",
    "gallery.copy": "随仓库提供的桌面示例",
    "gallery.searchTitle": "搜索",
    "gallery.searchCopy": "按控件名定位页面",
    "gallery.stateTitle": "验证",
    "gallery.stateCopy": "切换 Light / Dark 与强调色",
    "gallery.codeTitle": "查看示例",
    "gallery.codeCopy": "展开对应的 C++ 代码",
    "downloads.title": "下载 Gallery",
    "downloads.copy": "提供 Windows、macOS 与 Linux 安装包",
    "downloads.recommended": "为当前设备推荐",
    "downloads.latest": "最新 Release",
    "downloads.all": "全部版本与校验文件",
    "footer.project": "项目",
    "footer.docs": "文档",
    "footer.readme": "README",
    "footer.license": "项目 MIT 许可证",
    "footer.notices": "第三方声明",
    "images.hero": "Fluent-Qt Gallery 首页，完整展示控件分类和示例",
    "images.button": "Fluent-Qt Gallery 中完整的 Button 控件页面",
    "images.toggle": "Fluent-Qt Gallery 中完整的 ToggleSwitch 控件页面",
    "images.slider": "Fluent-Qt Gallery 中完整的 Slider 控件页面",
    "images.settings": "Fluent-Qt Gallery 设置页面，完整展示主题、风格和窗口选项"
  },
  en: {
    "meta.title": "Fluent-Qt — Fluent controls for Qt Widgets",
    "meta.description": "Fluent-Qt is a C++17 control library for Qt Widgets, supporting Qt 5.15+, Qt 6.2+, and major desktop platforms.",
    "a11y.skip": "Skip to main content",
    "a11y.primaryNav": "Primary navigation",
    "a11y.home": "Fluent-Qt home",
    "a11y.openMenu": "Open navigation",
    "a11y.closeMenu": "Close navigation",
    "a11y.language": "Language",
    "a11y.useDarkTheme": "Switch to dark theme",
    "a11y.useLightTheme": "Switch to light theme",
    "a11y.specs": "Project specifications",
    "a11y.downloads": "Platform downloads",
    "nav.why": "Overview",
    "nav.components": "Controls",
    "nav.quickStart": "Quick start",
    "nav.gallery": "Gallery",
    "nav.downloads": "Download",
    "hero.tagline": "Fluent controls for Qt Widgets",
    "hero.download": "Download Gallery",
    "hero.quickStart": "Quick start",
    "hero.visualCaption": "Gallery interface",
    "spec.runtime": "UI runtime",
    "spec.qt": "Qt support",
    "spec.toolchain": "Toolchain",
    "spec.platforms": "Desktop platforms",
    "positioning.titleLine1": "Independent controls",
    "positioning.titleLine2": "beyond styling",
    "positioning.copy": "Each control has its own C++ API, states, and layout behavior",
    "positioning.libraryTitle": "Native Qt Widgets usage",
    "positioning.libraryCopy": "Uses signals, slots, layouts, and the event loop",
    "positioning.galleryTitle": "One API for Qt 5 and Qt 6",
    "positioning.galleryCopy": "Application code does not need separate call sites for each major Qt version",
    "positioning.shipTitle": "A clear public boundary",
    "positioning.shipCopy": "Application code depends on the public entry header and FluentQt::FluentQt",
    "components.title": "Controls by purpose",
    "components.copy": "A selection from Gallery",
    "components.buttonCopy": "Includes default, accent, disabled, and icon states",
    "components.familiesLabel": "10 control families",
    "components.families": "Basic input · Collections · Date and time · Dialogs · Menus · Navigation · Scrolling · Status · Text input · Windowing",
    "quickStart.title": "CMake setup",
    "quickStart.copy": "Supports FetchContent, add_subdirectory, and find_package; call fluent::prepareHighDpiApplication() before QApplication starts",
    "quickStart.example": "Open the complete hello_world",
    "copy.copy": "Copy",
    "copy.copied": "Copied",
    "copy.announcement": "Code copied to the clipboard",
    "copy.failed": "Copy failed; select the code manually",
    "gallery.title": "Gallery examples",
    "gallery.copy": "Desktop examples included in the repository",
    "gallery.searchTitle": "Search",
    "gallery.searchCopy": "Locate a page by control name",
    "gallery.stateTitle": "Verify",
    "gallery.stateCopy": "Switch Light, Dark, and accent color",
    "gallery.codeTitle": "Open an example",
    "gallery.codeCopy": "Expand the corresponding C++ code",
    "downloads.title": "Download Gallery",
    "downloads.copy": "Packages are available for Windows, macOS, and Linux",
    "downloads.recommended": "Recommended for this device",
    "downloads.latest": "Latest release",
    "downloads.all": "All versions and checksums",
    "footer.project": "Project",
    "footer.docs": "Docs",
    "footer.readme": "README",
    "footer.license": "Project MIT License",
    "footer.notices": "Third-party notices",
    "images.hero": "Fluent-Qt Gallery home showing the complete control catalog and samples",
    "images.button": "Complete Button control page in Fluent-Qt Gallery",
    "images.toggle": "Complete ToggleSwitch control page in Fluent-Qt Gallery",
    "images.slider": "Complete Slider control page in Fluent-Qt Gallery",
    "images.settings": "Complete Gallery settings page showing theme, style, and window options"
  }
};

const fallbackVersion = "Latest";
const latestReleaseUrl = "https://github.com/calvinhxx/Fluent-Qt/releases/latest";
const latestReleaseApi = "https://api.github.com/repos/calvinhxx/Fluent-Qt/releases/latest";
const releaseState = {
  architecture: "",
  language: "zh",
  platform: "other",
  release: null
};

const languageButtons = Array.from(document.querySelectorAll("[data-lang]"));
const metaDescription = document.querySelector("meta[name='description']");
const metaThemeColor = document.querySelector("meta[name='theme-color']");
const siteHeader = document.querySelector(".site-header");
const menuButton = document.querySelector(".menu-button");
const themeToggle = document.querySelector("[data-theme-toggle]");
const copyAnnouncement = document.querySelector(".copy-announcement");
const systemThemeQuery = window.matchMedia("(prefers-color-scheme: dark)");
let followsSystemTheme = true;

function dictionary() {
  return translations[releaseState.language] || translations.zh;
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
  if (assetKey === "macChoice") {
    return releaseState.language === "zh" ? "选择 macOS 版本" : "Choose macOS build";
  }

  const platform = assetKey.startsWith("windows")
    ? "Windows"
    : assetKey.startsWith("mac")
      ? "macOS"
      : assetKey.startsWith("linux")
        ? "Linux"
        : "";

  if (!platform) {
    return releaseState.language === "zh" ? "打开最新 Release" : "Open latest release";
  }
  return releaseState.language === "zh" ? `下载 ${platform} Gallery` : `Download for ${platform}`;
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

function savedLanguage() {
  try {
    return localStorage.getItem("fluent-qt-language");
  } catch {
    return null;
  }
}

function storeLanguage(language) {
  try {
    localStorage.setItem("fluent-qt-language", language);
  } catch {
    // Switching still works when storage is disabled.
  }
}

function savedTheme() {
  try {
    const value = localStorage.getItem("fluent-qt-theme");
    return value === "dark" || value === "light" ? value : null;
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

function activeTheme() {
  return document.documentElement.dataset.theme === "dark" ? "dark" : "light";
}

function updateThemeToggle() {
  if (!themeToggle) return;
  const isDark = activeTheme() === "dark";
  const label = dictionary()[isDark ? "a11y.useLightTheme" : "a11y.useDarkTheme"];
  themeToggle.setAttribute("aria-pressed", String(isDark));
  themeToggle.setAttribute("aria-label", label);
  themeToggle.setAttribute("title", label);
}

function updateThemeMedia(theme) {
  const suffix = theme === "dark" ? "Dark" : "Light";

  document.querySelectorAll("[data-theme-src-light][data-theme-src-dark]").forEach((image) => {
    const source = image.dataset[`themeSrc${suffix}`];
    if (source && image.getAttribute("src") !== source) image.setAttribute("src", source);
  });

  document.querySelectorAll("[data-theme-label]").forEach((label) => {
    const value = label.dataset[`themeLabel${suffix}`];
    if (value) label.textContent = value;
  });
}

function applyTheme(theme, shouldStore = false) {
  const normalized = theme === "dark" ? "dark" : "light";
  document.documentElement.dataset.theme = normalized;
  document.documentElement.style.colorScheme = normalized;
  metaThemeColor?.setAttribute("content", normalized === "dark" ? "#151613" : "#f6f6f3");
  updateThemeMedia(normalized);
  updateThemeToggle();

  if (shouldStore) {
    followsSystemTheme = false;
    storeTheme(normalized);
  }
}

function initializeTheme() {
  const storedTheme = savedTheme();
  followsSystemTheme = storedTheme === null;
  applyTheme(storedTheme || (systemThemeQuery.matches ? "dark" : "light"));
  systemThemeQuery.addEventListener?.("change", (event) => {
    if (followsSystemTheme) applyTheme(event.matches ? "dark" : "light");
  });
}

function menuIsOpen() {
  return siteHeader?.dataset.menuOpen === "true";
}

function updateMenuLabel() {
  if (!menuButton) return;
  menuButton.setAttribute("aria-label", dictionary()[menuIsOpen() ? "a11y.closeMenu" : "a11y.openMenu"]);
}

function setLanguage(language, shouldStore = true) {
  const normalized = language === "en" ? "en" : "zh";
  const values = translations[normalized];
  releaseState.language = normalized;
  document.documentElement.lang = normalized === "zh" ? "zh-CN" : "en";
  document.title = values["meta.title"];
  metaDescription?.setAttribute("content", values["meta.description"]);

  document.querySelectorAll("[data-i18n]").forEach((node) => {
    const value = values[node.dataset.i18n];
    if (value) node.textContent = value;
  });
  document.querySelectorAll("[data-i18n-aria-label]").forEach((node) => {
    const value = values[node.dataset.i18nAriaLabel];
    if (value) node.setAttribute("aria-label", value);
  });
  document.querySelectorAll("[data-i18n-alt]").forEach((node) => {
    const value = values[node.dataset.i18nAlt];
    if (value) node.setAttribute("alt", value);
  });
  document.querySelectorAll("[data-readme-link]").forEach((link) => {
    link.href = normalized === "zh"
      ? "https://github.com/calvinhxx/Fluent-Qt/blob/main/README.zh-CN.md"
      : "https://github.com/calvinhxx/Fluent-Qt/blob/main/README.md";
  });
  languageButtons.forEach((button) => {
    button.setAttribute("aria-pressed", String(button.dataset.lang === normalized));
  });

  updateMenuLabel();
  updateThemeToggle();
  updateDownloads();
  if (shouldStore) storeLanguage(normalized);
}

function setMenuOpen(open) {
  if (!siteHeader || !menuButton) return;
  siteHeader.dataset.menuOpen = String(open);
  menuButton.setAttribute("aria-expanded", String(open));
  updateMenuLabel();
}

function setupNavigation() {
  menuButton?.addEventListener("click", () => setMenuOpen(!menuIsOpen()));
  document.querySelectorAll(".nav-links a, .brand").forEach((link) => {
    link.addEventListener("click", () => setMenuOpen(false));
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") setMenuOpen(false);
  });
  window.matchMedia("(min-width: 901px)").addEventListener?.("change", (event) => {
    if (event.matches) setMenuOpen(false);
  });
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
  window.setTimeout(() => {
    label.textContent = dictionary()["copy.copy"];
  }, 1500);
}

function setupInteractions() {
  languageButtons.forEach((button) => {
    button.addEventListener("click", () => setLanguage(button.dataset.lang));
  });
  themeToggle?.addEventListener("click", () => {
    applyTheme(activeTheme() === "dark" ? "light" : "dark", true);
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

loadAnalytics();
setupNavigation();
setupInteractions();
releaseState.platform = detectPlatform();
setLanguage(savedLanguage() === "en" ? "en" : "zh", false);
initializeTheme();
hydrateLatestRelease();
detectArchitecture().then((architecture) => {
  releaseState.architecture = architecture;
  updateDownloads();
});
