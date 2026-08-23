const labels = {
  en: {
    gallery: "Gallery", source: "Source", intro: "Find a control, its include, Gallery example, and focused test.",
    controls: "Controls", headers: "Public headers", version: "Version", search: "Search",
    placeholder: "Search controls, types, or headers…", all: "All", results: "results",
    declaration: "Declaration", example: "Gallery", test: "Test", headerSource: "Header source",
    noResults: "No matches", tryAnother: "Try another name or clear the category filter.",
    generated: "Generated from the installed-header allowlist and Gallery catalog."
  },
  zh: {
    gallery: "Gallery", source: "源码", intro: "查找控件、头文件、Gallery 示例和聚焦测试。",
    controls: "控件", headers: "公开头文件", version: "版本", search: "搜索",
    placeholder: "搜索控件、类型或头文件…", all: "全部", results: "项结果",
    declaration: "声明", example: "Gallery", test: "测试", headerSource: "查看头文件",
    noResults: "没有匹配项", tryAnother: "换个关键词，或清除分类筛选。",
    generated: "由安装头文件白名单和 Gallery 目录生成。"
  }
};

const state = { catalog: null, language: "en", view: "components", category: "all", query: "" };
const searchInput = document.querySelector("[data-search]");
const results = document.querySelector("[data-results]");
const empty = document.querySelector("[data-empty]");
const resultMeta = document.querySelector("[data-result-meta]");
const filters = document.querySelector("[data-category-filters]");

function text(key) { return labels[state.language][key]; }
function node(tag, className, content) {
  const value = document.createElement(tag);
  if (className) value.className = className;
  if (content !== undefined) value.textContent = content;
  return value;
}
function link(label, href) {
  const value = node("a", "", label);
  value.href = href;
  return value;
}
function normalized(value) {
  return String(value || "").normalize("NFKD").toLocaleLowerCase();
}
function matches(values) {
  const terms = normalized(state.query).split(/\s+/).filter(Boolean);
  const haystack = normalized(values.join(" "));
  return terms.every(term => haystack.includes(term));
}

function updateLabels() {
  document.documentElement.lang = state.language === "zh" ? "zh-CN" : "en";
  document.querySelectorAll("[data-label]").forEach(value => {
    value.textContent = text(value.dataset.label);
  });
  searchInput.placeholder = text("placeholder");
  document.querySelector("[data-language]").textContent = state.language === "zh" ? "EN" : "中";
}

function componentCard(component) {
  const card = node("article", "api-card");
  const heading = node("div", "card-heading");
  heading.append(node("h2", "", component.title));
  heading.append(node("span", "category", component.category_id.replaceAll("-", " ")));
  card.append(heading, node("p", "", component.description));
  card.append(node("code", "signature", `#include ${component.cpp.public_header}`));
  card.append(node("div", "subline", component.cpp.qualified_type));
  const links = node("div", "card-links");
  links.append(
    link(text("declaration"), component.cpp.declaration_url),
    link(text("example"), component.gallery.url),
    link(text("test"), component.tests[0].source_url)
  );
  card.append(links);
  return card;
}

function headerCard(header) {
  const card = node("article", "api-card header-card");
  const heading = node("div", "card-heading");
  heading.append(node("h2", "", header.include));
  card.append(heading, node("p", "", header.summary));
  card.append(node("code", "signature", `#include ${header.include}`));
  if (header.declarations.length) {
    card.append(node("div", "declarations", header.declarations.join(" · ")));
  }
  const links = node("div", "card-links");
  links.append(link(text("headerSource"), header.source_url));
  card.append(links);
  return card;
}

function visibleRecords() {
  if (state.view === "headers") {
    return state.catalog.public_headers.filter(header => matches([
      header.include, header.source, header.summary, ...header.declarations
    ]));
  }
  return state.catalog.components.filter(component => {
    const categoryMatches = state.category === "all" || component.category_id === state.category;
    return categoryMatches && matches([
      component.id, component.title, component.description, component.category_id,
      component.cpp.public_header, component.cpp.installed_declaration_header,
      component.cpp.qualified_type, component.python.import_statement,
      ...component.capabilities
    ]);
  });
}

function renderFilters() {
  filters.replaceChildren();
  filters.hidden = state.view !== "components";
  if (filters.hidden) return;
  const values = [{ id: "all", title: text("all") }, ...state.catalog.categories];
  values.forEach(category => {
    const button = node("button", "", category.title);
    button.type = "button";
    button.ariaPressed = String(state.category === category.id);
    button.addEventListener("click", () => {
      state.category = category.id;
      render();
    });
    filters.append(button);
  });
}

function updateUrl() {
  const url = new URL(location.href);
  state.query ? url.searchParams.set("q", state.query) : url.searchParams.delete("q");
  state.view === "headers" ? url.searchParams.set("view", "headers") : url.searchParams.delete("view");
  history.replaceState(null, "", url);
}

function render() {
  updateLabels();
  renderFilters();
  document.querySelectorAll("[data-view]").forEach(button => {
    button.ariaPressed = String(button.dataset.view === state.view);
  });
  const records = visibleRecords();
  results.replaceChildren(...records.map(state.view === "components" ? componentCard : headerCard));
  empty.hidden = records.length !== 0;
  resultMeta.textContent = `${records.length} ${text("results")}`;
  updateUrl();
}

function installEvents() {
  searchInput.addEventListener("input", event => {
    state.query = event.target.value;
    render();
  });
  document.querySelectorAll("[data-view]").forEach(button => {
    button.addEventListener("click", () => {
      state.view = button.dataset.view;
      render();
    });
  });
  document.querySelector("[data-language]").addEventListener("click", () => {
    state.language = state.language === "en" ? "zh" : "en";
    localStorage.setItem("fluent-qt-api-language", state.language);
    render();
  });
  document.querySelector("[data-theme-toggle]").addEventListener("click", () => {
    const theme = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
    document.documentElement.dataset.theme = theme;
    localStorage.setItem("fluent-qt-theme", theme);
  });
  document.addEventListener("keydown", event => {
    if ((event.metaKey || event.ctrlKey) && event.key.toLocaleLowerCase() === "k") {
      event.preventDefault();
      searchInput.focus();
    }
  });
}

async function start() {
  const parameters = new URLSearchParams(location.search);
  state.query = parameters.get("q") || "";
  state.view = parameters.get("view") === "headers" ? "headers" : "components";
  state.language = localStorage.getItem("fluent-qt-api-language") === "zh" ? "zh" : "en";
  searchInput.value = state.query;
  const response = await fetch("catalog.json");
  if (!response.ok) throw new Error(`Could not load API catalog: ${response.status}`);
  state.catalog = await response.json();
  document.querySelector("[data-components-count]").textContent = state.catalog.summary.components;
  document.querySelector("[data-headers-count]").textContent = state.catalog.summary.public_headers;
  document.querySelector("[data-version]").textContent = state.catalog.project.api_version;
  installEvents();
  render();
}

start().catch(error => {
  resultMeta.textContent = error.message;
  empty.hidden = false;
});
