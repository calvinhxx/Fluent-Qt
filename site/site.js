const translations = {
  zh: {
    "meta.title": "Fluent-Qt — 无需重写 QML 的 Qt Widgets Fluent 控件库",
    "meta.description": "无需重写 QML，用原生 C++17 Fluent 控件升级现有 Qt Widgets 应用；支持 Qt 5/6、PySide6，并可在线体验 WebAssembly Gallery。",
    "a11y.skip": "跳到主要内容",
    "a11y.primaryNav": "主导航",
    "a11y.home": "Fluent-Qt 首页",
    "a11y.openGallery": "打开 C++ Web Gallery",
    "a11y.openMenu": "打开导航",
    "a11y.closeMenu": "关闭导航",
    "a11y.language": "语言",
    "a11y.useDarkTheme": "切换到深色主题",
    "a11y.useLightTheme": "切换到浅色主题",
    "a11y.specs": "项目规格",
    "a11y.downloads": "平台下载",
    "a11y.componentModules": "组件模块",
    "a11y.relatedControls": "相关控件",
    "a11y.aiBuildCapabilities": "Skill 能力",
    "a11y.heroTech": "C++17、Qt 5.15 或 Qt 6.2 及以上、MIT",
    "nav.why": "概览",
    "nav.components": "控件",
    "nav.quickStart": "接入",
    "nav.gallery": "Gallery",
    "nav.community": "Q&A",
    "nav.aiBuild": "GUI Skill",
    "nav.downloads": "下载",
    "hero.tagline": "无需重写 QML，升级现有 Qt Widgets 应用",
    "hero.detail": "原生 C++17 控件，可继续使用现有的 signals、slots、布局和工程结构。",
    "hero.tryGallery": "在线打开 Gallery",
    "hero.download": "下载 Gallery",
    "hero.quickStart": "查看接入说明",
    "hero.actionNote": "在线 Gallery 在浏览器中运行，无需在本机配置 Qt；接入项目仍需 Qt 5.15+ 或 6.2+ 开发环境。",
    "hero.pythonLabel": "Python / PySide6",
    "hero.pythonCopy": "Python 项目可通过 PySide6 使用同一套控件",
    "hero.pythonGallery": "Gallery",
    "hero.visualCaption": "Gallery 实际界面",
    "spec.runtime": "UI 运行时",
    "spec.qt": "Qt 支持",
    "spec.toolchain": "工具链",
    "spec.platforms": "运行平台",
    "path.kicker": "开始使用",
    "path.title": "开始使用 Fluent-Qt",
    "path.copy": "先打开在线 Gallery 查看控件效果，或直接查看 C++ / Python 接入说明。",
    "path.tryTitle": "在线 Gallery",
    "path.tryCopy": "搜索控件、切换主题，直接操作 C++ WebAssembly 示例。",
    "path.tryAction": "打开 Gallery",
    "path.cppTitle": "C++ / Qt Widgets",
    "path.cppCopy": "复制 CMake 配置，按最小示例接入现有 Qt Widgets 项目。",
    "path.cppAction": "查看 C++ 接入",
    "path.pythonTitle": "Python / PySide6",
    "path.pythonCopy": "从 PyPI 安装 FluentQt，通过 PySide6 使用同一套控件。",
    "path.pythonAction": "查看 PyPI 安装",
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
    "components.familiesLabel": "11 个控件家族",
    "components.families": "基础输入 · 集合视图 · 日期与时间 · 对话框 · 布局 · 菜单 · 导航 · 滚动 · 状态信息 · 文本输入 · 窗口系统",
    "quickStart.title": "快速接入",
    "quickStart.copy": "复制 CMake 片段即可接入现有工程；也支持 add_subdirectory 与 find_package。",
    "quickStart.keepProject": "保留现有 Widgets 工程与事件循环",
    "quickStart.libraryOnly": "库目标只依赖 Qt Widgets",
    "quickStart.sameApi": "Qt 5 / Qt 6 使用同一应用层接口",
    "quickStart.example": "查看完整 hello_world",
    "quickStart.python": "Python / PySide6 · pip install FluentQt",
    "quickStart.help": "提问与获取帮助",
    "copy.copy": "复制",
    "copy.copied": "已复制",
    "copy.announcement": "代码已复制到剪贴板",
    "copy.failed": "复制失败，请手动选择代码",
    "gallery.title": "Gallery 控件示例",
    "gallery.copy": "这个 Gallery 在浏览器中运行，无需本地配置 Qt；接入项目仍需 Qt 5.15+ 或 6.2+。",
    "gallery.searchTitle": "搜索",
    "gallery.searchCopy": "按控件名定位页面",
    "gallery.stateTitle": "验证",
    "gallery.stateCopy": "切换 Light / Dark 与强调色",
    "gallery.codeTitle": "查看示例",
    "gallery.codeCopy": "展开对应的 C++ 代码",
    "gallery.webAction": "启动实时 Gallery",
    "gallery.liveStateWaiting": "等待进入视口",
    "gallery.liveStateLoading": "正在启动",
    "gallery.liveStateReady": "可以操作",
    "gallery.liveStateSlow": "仍在加载",
    "gallery.liveStateError": "启动失败",
    "gallery.loadNow": "立即加载",
    "gallery.fullPage": "完整页面",
    "gallery.waitingTitle": "滚动到这里后自动启动",
    "gallery.waitingDetail": "首屏不会请求 WebAssembly 文件；也可以现在手动加载。",
    "gallery.loadingTitle": "正在加载 Qt 与 C++ Gallery",
    "gallery.loadingDetail": "首次下载取决于网络速度；完成后会直接显示可操作界面。",
    "gallery.slowTitle": "下载仍在继续",
    "gallery.slowDetail": "可以继续等待，或在新页面中打开完整 Gallery。",
    "gallery.readyTitle": "Gallery 已就绪",
    "gallery.readyDetail": "现在可以搜索组件、切换主题并操作示例。",
    "gallery.errorTitle": "实时体验没有成功启动",
    "gallery.errorDetail": "可以重试，或直接在完整页面中打开 Gallery。",
    "gallery.loadAction": "启动实时体验",
    "gallery.retryAction": "重新加载",
    "gallery.openAction": "在新页面打开",
    "gallery.frameTitle": "Fluent-Qt C++ Web Gallery 实时体验",
    "gallery.noscript": "打开 C++ Web Gallery",
    "aiBuild.title": "构建桌面 GUI",
    "aiBuild.structureTitle": "工程",
    "aiBuild.structureCopy": "C++ / PySide6 项目结构",
    "aiBuild.designTitle": "界面",
    "aiBuild.designCopy": "Fluent 组件、主题与配色",
    "aiBuild.reviewTitle": "检查",
    "aiBuild.reviewCopy": "Light / Dark、窄窗口与交互",
    "aiBuild.skillAction": "查看 Skill",
    "aiBuild.guideAction": "文档",
    "aiBuild.caseEyebrow": "参考实现",
    "aiBuild.caseCopy": "任务分组、运行时间线、输入与 Light / Dark 主题。",
    "aiBuild.disclaimer": "FluentQt 独立实现，非 DeepSeek 官方应用。",
    "aiBuild.upstreamAction": "上游项目",
    "downloads.title": "下载 Gallery",
    "downloads.copy": "桌面安装包用于离线体验；应用接入仍使用 CMake 或 PyPI。",
    "downloads.recommended": "为当前设备推荐",
    "downloads.latest": "最新 Release",
    "downloads.all": "全部版本与校验文件",
    "downloads.pythonEyebrow": "可选兼容",
    "downloads.pythonGallery": "Python Gallery",
    "footer.project": "项目",
    "footer.docs": "文档",
    "footer.readme": "README",
    "footer.license": "项目 MIT 许可证",
    "footer.notices": "第三方声明",
    "images.hero": "Fluent-Qt Gallery 首页，完整展示控件分类和示例",
    "images.button": "Fluent-Qt Gallery 中完整的 Button 控件页面",
    "images.toggle": "Fluent-Qt Gallery 中完整的 ToggleSwitch 控件页面",
    "images.slider": "Fluent-Qt Gallery 中完整的 Slider 控件页面",
    "images.collections": "Fluent-Qt Gallery 中的集合视图页面",
    "images.system": "Fluent-Qt Gallery 的导航与窗口界面",
    "images.webGallery": "C++ Web Gallery 加载前预览",
    "images.deepseekDesktop": "使用 build-fluentqt-gui 构建的 DeepSeek Desktop 原生桌面参考界面",
    "images.settings": "Fluent-Qt Gallery 设置页面，完整展示主题、风格和窗口选项"
  },
  en: {
    "meta.title": "Fluent-Qt — Modern Fluent controls for Qt Widgets",
    "meta.description": "Modernize existing Qt Widgets apps with native C++17 Fluent controls—without rewriting in QML. Optional PySide6 and a live WebAssembly Gallery.",
    "a11y.skip": "Skip to main content",
    "a11y.primaryNav": "Primary navigation",
    "a11y.home": "Fluent-Qt home",
    "a11y.openGallery": "Open the C++ Web Gallery",
    "a11y.openMenu": "Open navigation",
    "a11y.closeMenu": "Close navigation",
    "a11y.language": "Language",
    "a11y.useDarkTheme": "Switch to dark theme",
    "a11y.useLightTheme": "Switch to light theme",
    "a11y.specs": "Project specifications",
    "a11y.downloads": "Platform downloads",
    "a11y.componentModules": "Component modules",
    "a11y.relatedControls": "Related controls",
    "a11y.aiBuildCapabilities": "Skill capabilities",
    "a11y.heroTech": "C++17, Qt 5.15 or Qt 6.2 and above, MIT",
    "nav.why": "Overview",
    "nav.components": "Controls",
    "nav.quickStart": "Quick start",
    "nav.gallery": "Gallery",
    "nav.community": "Q&A",
    "nav.aiBuild": "GUI Skill",
    "nav.downloads": "Download",
    "hero.tagline": "Modernize Qt Widgets apps without rewriting in QML",
    "hero.detail": "Native C++17 controls that keep your existing signals, slots, layouts, and project structure.",
    "hero.tryGallery": "Open the online Gallery",
    "hero.download": "Download Gallery",
    "hero.quickStart": "Setup guide",
    "hero.actionNote": "The online Gallery runs in your browser and does not need a local Qt setup. Integrating Fluent-Qt still requires Qt 5.15+ or 6.2+.",
    "hero.pythonLabel": "Python / PySide6",
    "hero.pythonCopy": "Use the same controls from Python through PySide6",
    "hero.pythonGallery": "Gallery",
    "hero.visualCaption": "Gallery interface",
    "spec.runtime": "UI runtime",
    "spec.qt": "Qt support",
    "spec.toolchain": "Toolchain",
    "spec.platforms": "Platforms",
    "path.kicker": "GET STARTED",
    "path.title": "Get started with Fluent-Qt",
    "path.copy": "Open the online Gallery, or go straight to the C++ or Python setup guide.",
    "path.tryTitle": "Online Gallery",
    "path.tryCopy": "Search controls, switch themes, and interact with the C++ WebAssembly examples.",
    "path.tryAction": "Open Gallery",
    "path.cppTitle": "C++ / Qt Widgets",
    "path.cppCopy": "Copy the CMake setup and follow the minimal example for an existing Qt Widgets project.",
    "path.cppAction": "Read the C++ guide",
    "path.pythonTitle": "Python / PySide6",
    "path.pythonCopy": "Install FluentQt from PyPI and use the same controls through PySide6.",
    "path.pythonAction": "View installation",
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
    "components.familiesLabel": "11 control families",
    "components.families": "Basic input · Collections · Date and time · Dialogs · Layout · Menus · Navigation · Scrolling · Status · Text input · Windowing",
    "quickStart.title": "Quick start",
    "quickStart.copy": "Copy the CMake snippet into an existing project; add_subdirectory and find_package are also supported.",
    "quickStart.keepProject": "Keep the Widgets project and event loop you already have",
    "quickStart.libraryOnly": "The library target depends only on Qt Widgets",
    "quickStart.sameApi": "Use the same application-facing API with Qt 5 and Qt 6",
    "quickStart.example": "Open the complete hello_world",
    "quickStart.python": "Python / PySide6 · pip install FluentQt",
    "quickStart.help": "Ask a question",
    "copy.copy": "Copy",
    "copy.copied": "Copied",
    "copy.announcement": "Code copied to the clipboard",
    "copy.failed": "Copy failed; select the code manually",
    "gallery.title": "Gallery examples",
    "gallery.copy": "This Gallery runs in your browser and does not need a local Qt setup. Project integration still requires Qt 5.15+ or 6.2+.",
    "gallery.searchTitle": "Search",
    "gallery.searchCopy": "Locate a page by control name",
    "gallery.stateTitle": "Verify",
    "gallery.stateCopy": "Switch Light, Dark, and accent color",
    "gallery.codeTitle": "Open an example",
    "gallery.codeCopy": "Expand the corresponding C++ code",
    "gallery.webAction": "Start the live Gallery",
    "gallery.liveStateWaiting": "Waiting for viewport",
    "gallery.liveStateLoading": "Starting",
    "gallery.liveStateReady": "Interactive",
    "gallery.liveStateSlow": "Still loading",
    "gallery.liveStateError": "Could not start",
    "gallery.loadNow": "Load now",
    "gallery.fullPage": "Full page",
    "gallery.waitingTitle": "Starts automatically when you reach this section",
    "gallery.waitingDetail": "The first viewport does not request the WebAssembly file. You can also start it now.",
    "gallery.loadingTitle": "Loading Qt and the C++ Gallery",
    "gallery.loadingDetail": "The first download depends on your connection. The interactive app appears as soon as it is ready.",
    "gallery.slowTitle": "The download is still in progress",
    "gallery.slowDetail": "Keep waiting or open the complete Gallery in a separate page.",
    "gallery.readyTitle": "Gallery is ready",
    "gallery.readyDetail": "Search controls, switch themes, and use the examples.",
    "gallery.errorTitle": "The live Gallery did not start",
    "gallery.errorDetail": "Retry here or open the full Gallery in a separate page.",
    "gallery.loadAction": "Start live experience",
    "gallery.retryAction": "Try again",
    "gallery.openAction": "Open in a new page",
    "gallery.frameTitle": "Live Fluent-Qt C++ Web Gallery",
    "gallery.noscript": "Open the C++ Web Gallery",
    "aiBuild.title": "Build desktop GUIs",
    "aiBuild.structureTitle": "Project",
    "aiBuild.structureCopy": "C++ / PySide6 application structure",
    "aiBuild.designTitle": "UI",
    "aiBuild.designCopy": "Fluent components, themes, and color",
    "aiBuild.reviewTitle": "Checks",
    "aiBuild.reviewCopy": "Light / Dark, narrow layouts, and interaction",
    "aiBuild.skillAction": "Open the Skill",
    "aiBuild.guideAction": "Docs",
    "aiBuild.caseEyebrow": "REFERENCE BUILD",
    "aiBuild.caseCopy": "Task groups, run timeline, composer, and Light / Dark themes.",
    "aiBuild.disclaimer": "Independent FluentQt build; not an official DeepSeek app.",
    "aiBuild.upstreamAction": "Upstream project",
    "downloads.title": "Download Gallery",
    "downloads.copy": "Desktop packages are for offline evaluation; integrate applications through CMake or PyPI.",
    "downloads.recommended": "Recommended for this device",
    "downloads.latest": "Latest release",
    "downloads.all": "All versions and checksums",
    "downloads.pythonEyebrow": "Optional compatibility",
    "downloads.pythonGallery": "Python Gallery",
    "footer.project": "Project",
    "footer.docs": "Docs",
    "footer.readme": "README",
    "footer.license": "Project MIT License",
    "footer.notices": "Third-party notices",
    "images.hero": "Fluent-Qt Gallery home showing the complete control catalog and samples",
    "images.button": "Complete Button control page in Fluent-Qt Gallery",
    "images.toggle": "Complete ToggleSwitch control page in Fluent-Qt Gallery",
    "images.slider": "Complete Slider control page in Fluent-Qt Gallery",
    "images.collections": "Collections page in Fluent-Qt Gallery",
    "images.system": "Navigation and window UI in Fluent-Qt Gallery",
    "images.webGallery": "C++ Web Gallery preview before loading",
    "images.deepseekDesktop": "Native DeepSeek Desktop reference UI built with build-fluentqt-gui",
    "images.settings": "Complete Gallery settings page showing theme, style, and window options"
  }
};

const fallbackVersion = "Latest";
const latestReleaseUrl = "https://github.com/calvinhxx/Fluent-Qt/releases/latest";
const latestReleaseApi = "https://api.github.com/repos/calvinhxx/Fluent-Qt/releases/latest";
const releaseState = {
  architecture: "",
  language: document.documentElement.lang.toLowerCase().startsWith("zh") ? "zh" : "en",
  platform: "other",
  release: null
};

const metaThemeColor = document.querySelector("meta[name='theme-color']");
const siteHeader = document.querySelector(".site-header");
const menuButton = document.querySelector(".menu-button");
const themeToggle = document.querySelector("[data-theme-toggle]");
const copyAnnouncement = document.querySelector(".copy-announcement");
const systemThemeQuery = window.matchMedia("(prefers-color-scheme: dark)");
let followsSystemTheme = true;

const galleryRoot = document.querySelector("[data-live-gallery]");
const galleryFrame = document.querySelector("[data-gallery-frame]");
const galleryStatus = document.querySelector("[data-gallery-status]");
const galleryMessage = document.querySelector("[data-gallery-message]");
const galleryDetail = document.querySelector("[data-gallery-detail]");
const galleryLoadButtons = Array.from(document.querySelectorAll("[data-gallery-load]"));
let galleryState = "waiting";
let gallerySlowTimer = 0;
let galleryRetry = 0;

function dictionary() {
  return translations[releaseState.language] || translations.en;
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

function syncGalleryTheme() {
  if (!galleryFrame?.contentWindow || !galleryFrame.getAttribute("src")) return;
  galleryFrame.contentWindow.postMessage({
    source: "fluent-qt-site",
    type: "theme",
    theme: activeTheme()
  }, window.location.origin);
}

function applyTheme(theme, shouldStore = false) {
  const normalized = theme === "dark" ? "dark" : "light";
  document.documentElement.dataset.theme = normalized;
  document.documentElement.style.colorScheme = normalized;
  metaThemeColor?.setAttribute("content", normalized === "dark" ? "#070c12" : "#eef3f7");
  updateThemeMedia(normalized);
  updateThemeToggle();
  syncGalleryTheme();

  if (shouldStore) {
    followsSystemTheme = false;
    storeTheme(normalized);
  }
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

function renderGalleryState() {
  if (!galleryRoot) return;
  const values = dictionary();
  const stateKeys = {
    waiting: ["gallery.liveStateWaiting", "gallery.waitingTitle", "gallery.waitingDetail"],
    loading: ["gallery.liveStateLoading", "gallery.loadingTitle", "gallery.loadingDetail"],
    slow: ["gallery.liveStateSlow", "gallery.slowTitle", "gallery.slowDetail"],
    ready: ["gallery.liveStateReady", "gallery.readyTitle", "gallery.readyDetail"],
    error: ["gallery.liveStateError", "gallery.errorTitle", "gallery.errorDetail"]
  };
  const [statusKey, messageKey, detailKey] = stateKeys[galleryState] || stateKeys.waiting;
  galleryRoot.dataset.galleryState = galleryState;
  document.documentElement.dataset.galleryLoadState = galleryState;
  if (galleryStatus) galleryStatus.textContent = values[statusKey];
  if (galleryMessage) galleryMessage.textContent = values[messageKey];
  if (galleryDetail) galleryDetail.textContent = values[detailKey];

  const actionKey = galleryState === "error" ? "gallery.retryAction" : "gallery.loadAction";
  galleryLoadButtons.forEach((button) => {
    const label = button.querySelector("span") || button;
    if (button.closest(".live-gallery-toolbar") && galleryState !== "error") {
      label.textContent = values["gallery.loadNow"];
    } else {
      label.textContent = values[actionKey];
    }
    button.disabled = galleryState === "loading"
      || galleryState === "slow"
      || galleryState === "ready";
  });
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

  if (!("IntersectionObserver" in window)) return;
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

document.documentElement.setAttribute("data-i18n-ready", "");
loadAnalytics();
setupNavigation();
setupInteractions();
setupLiveGallery();
setupReveal();
releaseState.platform = detectPlatform();
initializeTheme();
hydrateLatestRelease();
detectArchitecture().then((architecture) => {
  releaseState.architecture = architecture;
  updateDownloads();
});
