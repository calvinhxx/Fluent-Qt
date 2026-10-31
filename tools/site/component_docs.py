"""Join canonical Gallery examples and native captures into static documentation."""

import html
import hashlib
import json
import os
import re
import struct
from string import Template

from generate_component_previews import ROOT, available_previews, config, preview_path
from public_api import inventory, resolve_type
from api_sections import (component_api, localized_description, primary_type, render_type,
                          reference_headers, reference_path, type_url, slug)
from documentation_guides import guides, guide_path
from documentation_navigation import navigation as site_navigation, breadcrumb, relative as relative_url
from site_assets import source_path
from generate_localized_site import BASE_URL
from release_history import page_path as release_page_path
from release_pages import document_notice

SITE = ROOT / "site"
WORDS = {
    "en": {"overview": "Example", "usage": "Usage", "api": "API",
           "source": "Declaration", "copy": "Copy code",
           "copied": "Code copied", "copyError": "Could not copy. Select the code to copy it manually.",
           "light": "Light", "dark": "Dark", "skip": "Skip to content",
           "theme": "Example theme",
           "loadingLive": "Loading example…", "failedLive": "The example could not load. A preview is shown instead.",
           "retryLive": "Retry", "noScript": "Enable JavaScript to use this example.",
           "onPage": "On this page"},
    "zh": {"overview": "示例", "usage": "用法", "api": "API",
           "source": "查看声明", "copy": "复制代码",
           "copied": "代码已复制", "copyError": "复制失败，请选择代码手动复制。",
           "light": "浅色", "dark": "深色", "skip": "跳转到正文",
           "theme": "示例主题",
           "loadingLive": "正在加载示例…", "failedLive": "示例加载失败，暂时显示预览图。",
           "retryLive": "重试", "noScript": "启用 JavaScript 后可操作示例。",
           "onPage": "本页内容"}
}


def escape(value):
    return html.escape(str(value), quote=True)


def relative(path, page):
    return os.path.relpath(SITE / path, (SITE / page).parent).replace(os.sep, "/")


def asset(path, page):
    return relative(path, page) + "?v=" + hashlib.sha256(source_path(path, SITE).read_bytes()).hexdigest()[:12]


def site_icon():
    """Reuse the existing 64px PNG from the application icon, without resampling."""
    source = (ROOT / "app/assets/Fluent-Qt-Gallery.ico").read_bytes()
    reserved, kind, count = struct.unpack_from("<3H", source)
    if reserved != 0 or kind != 1:
        raise ValueError("Expected the Gallery application ICO")
    for i in range(count):
        width, height, _, _, _, _, length, offset = struct.unpack_from("<4B2H2I", source, 6 + i * 16)
        if width == 64 and height == 64:
            data = source[offset:offset + length]
            if len(data) == length and data.startswith(b"\x89PNG\r\n\x1a\n"):
                return data
    raise ValueError("Gallery application icon is missing its 64px PNG")


SECTIONS = {"project": ("Project updates", "项目更新"), "foundation": ("Foundations", "基础能力"),
            "components": ("Components", "组件"), "modules": ("Modules", "扩展模块"),
            "reference": ("API reference", "API 参考")}
CATEGORY_NAMES_ZH = {"basic-input": "基础输入", "charts": "图表", "collections": "集合视图",
    "date-time": "日期与时间", "dialogs-flyouts": "对话框与浮层", "layout": "布局", "menus-toolbars": "菜单与工具栏",
    "navigation": "导航", "scrolling": "滚动", "status-info": "状态与信息", "text-fields": "文本输入", "windowing": "窗口"}


def section_label(key, language):
    return SECTIONS[key][language == "zh"]


def category_owners(catalog):
    """Map each catalog category that a guide introduces, such as Icons or Spatial, to that guide."""
    known = {c["id"] for c in catalog["categories"]}
    owners = {}
    for item in guides():
        category = item["component_category"]
        if category and (category in owners or category not in known):
            raise ValueError(f"Guide component category must exist and have a single owner: {category}")
        if category:
            owners[category] = item
    return owners


def category_label(category, owners, language):
    """An introduced category takes its guide's name, so the branch and its overview agree."""
    if category["id"] in owners:
        return owners[category["id"]]["name"][language]
    return CATEGORY_NAMES_ZH.get(category["id"], category["title"]) if language == "zh" else category["title"]


def detail_path(component_id, language):
    return ("zh-CN/" if language == "zh" else "") + f"api/{component_id}/index.html"


def icon(name):
    paths = {"external": '<path d="M14 4h6v6M20 4 10 14M10 4H4v16h16v-6"/>',
             "copy": '<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V4H4v12h4"/>'}
    return f'<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.6">{paths[name]}</svg>'


def collect(catalog, capture=None):
    capture = capture or available_previews()
    components = {c["id"]: c for c in catalog["components"]}
    canonical = json.loads((ROOT / "docs/ai/generated/fluentqt-ai-catalog.json").read_text())
    samples = {(c["id"], s["id"]): s for c in canonical["components"] for s in c["samples"]}
    python = json.loads((ROOT / "app/data/gallery_python_snippets.json").read_text())
    python = {(s["route_id"], s["sample_id"]): s["source"] for s in python["samples"]}
    images = {(s["component"], s["state"], s["theme"]): s for s in capture["images"]}
    documents = []
    for doc in config():
        component_id = doc["id"]
        if not re.fullmatch(r"[a-z][a-z0-9-]*", component_id) or component_id not in components:
            raise ValueError(f"Unknown documentation component: {component_id}")
        for field in ("name", "description"):
            if set(doc[field]) != {"en", "zh"} or not all(doc[field].values()):
                raise ValueError(f"Missing translation: {component_id}/{field}")
        for preview in doc["previews"]:
            if (component_id, preview["sample"]) not in samples:
                raise ValueError(f"Unknown Gallery sample: {preview['sample']}")
            preview["images"] = {theme: images[(component_id, preview["id"], theme)] for theme in ("light", "dark")}
        for example in doc["examples"]:
            key = (component_id, example["sample"])
            example["cpp"] = samples[key]["cpp_snippet"]
            example["python"] = python[key]
        doc["api"] = resolve_type(components[component_id]["cpp"]["qualified_type"])["members"]
        doc["component"] = components[component_id]
        doc["catalog"] = catalog
        hero = doc["previews"][0]
        # The committed catalog must not depend on which machine captured the images.
        components[component_id]["documentation"] = {
            "name": doc["name"], "description": doc["description"],
            "urls": {lang: ("../zh-CN/api/" if lang == "zh" else "") + component_id + "/" for lang in ("en", "zh")},
            "preview": {"alt": hero["alt"], **{theme: {"url": "../" + preview_path(component_id, hero["id"], theme)}
                                              for theme in ("light", "dark")}}}
        documents.append(doc)
    return documents


def component_directory(doc, documents, page, language):
    available = {item["id"]: item for item in documents}
    introductions = guides()
    category_guides = category_owners(doc['catalog'])
    active_category = available[doc['id']]['component']['category_id'] if doc['id'] in available else None

    def group(key, label, entries, active=False, headers=False):
        attributes = (' open' if active else '') + (' data-header-group' if headers else '')
        return f'<details data-directory-group="{key}"{attributes}><summary>{escape(label)}</summary><ul>{"".join(entries)}</ul></details>'

    def guide_entry(item, overview=False):
        current = ' aria-current="page"' if item['id'] == doc['id'] else ''
        label = ('概述与用法' if language == 'zh' else 'Overview and usage') if overview else item['name'][language]
        accessible = f' aria-label="{escape(item["name"][language] + ": " + label)}"' if overview else ''
        terms = ' '.join([item['id'], label, *item['name'].values(), *item['description'].values()])
        return f'<li data-directory-entry data-kind="guide" data-search="{escape(terms.lower())}"><a href="{relative_url(guide_path(item["id"], language), page)}"{accessible}{current}>{escape(label)}</a></li>'

    category_groups = {}
    for category in doc["catalog"]["categories"]:
        introduction = category_guides.get(category['id'])
        entries = [guide_entry(introduction, overview=True)] if introduction else []
        components = [c for c in doc["catalog"]["components"] if c["category_id"] == category["id"]]
        for component in sorted(components, key=lambda c: (c["id"] not in available, c["title"].lower())):
            documented = available.get(component["id"])
            name = component["title"] + (" " + documented["name"]["zh"] if documented and language == "zh" else "")
            url = relative_url(detail_path(component["id"], language), page) if documented else component["cpp"]["declaration_url"]
            current = ' aria-current="page"' if component["id"] == doc["id"] else ''
            external = '' if documented else ' target="_blank" rel="noopener"'
            kind = '' if documented else '<span class="reference-kind">API</span>'
            terms = ' '.join([name, component['id'], component['cpp']['qualified_type'], component['python']['import_statement']])
            entries.append(f'<li data-directory-entry data-kind="component" data-search="{escape(terms.lower())}">'
                           f'<a data-component-id="{component["id"]}" href="{escape(url)}"{current}{external}><span>{escape(name)}</span>{kind}</a></li>')
        name = category_label(category, category_guides, language)
        active = active_category == category['id'] or bool(introduction and introduction['id'] == doc['id'])
        category_groups[category['id']] = (group('category-' + category['id'], name, entries, active), active)

    guide_groups = {}
    for family in ('foundation', 'modules'):
        entries, active = [], False
        for item in introductions:
            if item['group'] != family:
                continue
            if item['component_category']:
                branch, selected = category_groups[item['component_category']]
                entries.append(f'<li class="directory-branch">{branch}</li>')
            else:
                entries.append(guide_entry(item))
                selected = item['id'] == doc['id']
            active = active or selected
        guide_groups[family] = group(family, section_label(family, language), entries, active)

    current = ' aria-current="page"' if doc['id'] == 'releases' else ''
    label = '版本记录与迁移' if language == 'zh' else 'Releases and migrations'
    project_link = f'<li data-directory-entry data-search="releases and migrations 版本记录与迁移"><a href="{relative_url(release_page_path("releases", language), page)}"{current}>{label}</a></li>'
    core = [value for key, value in category_groups.items() if key not in category_guides]
    groups = [group('project', section_label('project', language), [project_link], doc['id'] == 'releases'),
              guide_groups['foundation'],
              group('components', section_label('components', language),
                    [f'<li class="directory-branch">{branch}</li>' for branch, _ in core], any(active for _, active in core)),
              guide_groups['modules']]
    references = []
    for reference in sorted(reference_headers(doc['catalog']), key=lambda item: item['title'].lower()):
        terms = ' '.join([reference['title'], *[r['qualified_name'] for r in reference['types']]])
        current = ' aria-current="page"' if reference['id'] == doc['id'] else ''
        references.append(f'<li data-directory-entry data-kind="reference" data-search="{escape(terms.lower())}"><a href="{relative_url(reference_path(reference["source"], language), page)}"{current}><span>{escape(reference["title"])}</span></a></li>')
    groups.append(group('reference', section_label('reference', language), references, doc['component']['category_id'] == 'reference', headers=True))
    search = '搜索文档' if language == 'zh' else 'Search documentation'
    title = '目录与搜索' if language == 'zh' else 'Contents and search'
    return f'''<details class="documentation-directory" open><summary>{title}</summary>
      <div class="directory-content">
      <label class="directory-search"><span class="visually-hidden">{search}</span><input type="search" data-directory-search placeholder="{search}" autocomplete="off" aria-keyshortcuts="Control+K Meta+K"></label>
      <p class="directory-status" data-directory-status role="status" hidden></p>
      <nav class="directory-groups" aria-label="{title}">{''.join(groups)}</nav></div></details>'''


def picture(preview, page, language, eager=False):
    light, dark = (preview["images"][theme] for theme in ("light", "dark"))
    def url(image):
        version = "?v=" + image["sha256"][:12] if image["sha256"] else ""
        return escape(relative(image["path"], page) + version)
    w, h = light["logical_size"]
    priority = ' fetchpriority="high"' if eager else ''
    return (f'<picture><source data-dark-src="{url(dark)}" media="(prefers-color-scheme: dark)" srcset="{url(dark)}">'
            f'<img src="{url(light)}" width="{w}" height="{h}" alt="{escape(preview["alt"][language])}" '
            f'loading="{"eager" if eager else "lazy"}" decoding="async"{priority}></picture>')


def code_block(cpp, python, name, key, language):
    words = WORDS[language]
    tabs, panels = [], []
    for index, (kind, label, source) in enumerate((('cpp', 'C++', cpp), ('python', 'PySide6', python))):
        pid = key + '-' + kind
        tabs.append(f'<button type="button" role="tab" id="tab-{pid}" aria-controls="{pid}" aria-selected="{str(index == 0).lower()}" tabindex="{0 if index == 0 else -1}">{label}</button>')
        panels.append(f'<div role="tabpanel" id="{pid}" aria-labelledby="tab-{pid}"{" hidden" if index else ""}><pre tabindex="0"><code>{escape(source)}</code></pre></div>')
    return f'<div class="code-example" data-tabs><div class="code-toolbar"><div class="tab-list" role="tablist" aria-label="{escape(name)}">{"".join(tabs)}</div><button class="copy-code" type="button" data-copy aria-label="{words["copy"]}">{icon("copy")}<span>{words["copy"]}</span></button></div>{"".join(panels)}</div>'


def example_source(example, component, kind):
    source = example[kind].strip()
    if kind == 'python':
        return source if re.search(r'^import fluentqt\b', source, re.M) else 'import fluentqt\n\n' + source
    header = component['cpp']['public_header']
    namespace = component['cpp']['qualified_type'].rsplit('::', 1)[0]
    return f'#include {header}\nusing namespace {namespace};\n\n' + source


def api_search(language):
    label = '筛选 API 成员' if language == 'zh' else 'Filter API members'
    empty = '没有匹配的 API 成员。' if language == 'zh' else 'No matching API members.'
    return f'<label class="api-search"><span class="visually-hidden">{label}</span><input type="search" data-api-filter placeholder="{label}" autocomplete="off"></label><p data-api-empty hidden role="status">{empty}</p>'


def render_shell(doc, documents, page, language, title, description, body, toc, neighbors='', eyebrow=None):
    """Wrap a document body in the shared shell; non-component pages name their section above the title."""
    words = WORDS[language]
    if eyebrow is None:
        eyebrow = section_label(doc['component']['category_id'], language)
    alternate = page.removeprefix('zh-CN/') if language == 'zh' else 'zh-CN/' + page
    values = {'lang': 'zh-CN' if language == 'zh' else 'en', 'title': escape(title),
        'description': escape(description), 'canonical': BASE_URL + page.removesuffix('index.html'),
        'alternate': BASE_URL + alternate.removesuffix('index.html'),
        'alternate_lang': 'en' if language == 'zh' else 'zh-CN',
        'directory': component_directory(doc, documents, page, language), 'neighbors': neighbors,
        'style': asset('api/component.css', page), 'navigation_style': asset('api/navigation.css', page),
        'script': asset('api/component.js', page), 'site_navigation': site_navigation(page, language),
        'breadcrumb': breadcrumb(page, language, title), 'app_icon': relative('assets/app-icon-64.png', page),
        'eyebrow': f'<p class="document-eyebrow">{escape(eyebrow)}</p>' if eyebrow else '',
        'version_notice': document_notice(doc['id'], doc.get('source'), page, language),
        'body': body, 'page_navigation': ''.join(f'<a href="#{id}">{escape(label)}</a>' for id, label in toc)}
    values.update(words)
    return Template((ROOT / 'tools/site/component.template.html').read_text()).substitute(values)


def render_document(doc, documents, language):
    component = doc['component']
    page = detail_path(doc['id'], language)
    words = WORDS[language]
    title = component['title'] + (' ' + doc['name'][language] if language == 'zh' else '')
    position = next(i for i, item in enumerate(documents) if item['id'] == doc['id'])
    neighbors = []
    for index, rel, labels in ((position - 1, 'prev', ('Previous', '上一项')), (position + 1, 'next', ('Next', '下一项'))):
        if 0 <= index < len(documents):
            item = documents[index]
            neighbors.append(f'<a class="document-neighbor" rel="{rel}" href="{relative_url(detail_path(item["id"], language), page)}">'
                             f'<span>{labels[language == "zh"]}</span><strong>{escape(item["component"]["title"])}</strong></a>')
    tabs, panels = [], []
    for i, preview in enumerate(doc['previews']):
        pid = 'state-' + preview['id']
        tabs.append(f'<button type="button" role="tab" id="tab-{pid}" aria-controls="{pid}" data-sample="{escape(preview["sample"])}" data-state="{escape(preview["id"])}" aria-selected="{str(i == 0).lower()}" tabindex="{0 if i == 0 else -1}">{escape(preview["name"][language])}</button>')
        panels.append(f'<div class="preview-panel" role="tabpanel" aria-labelledby="tab-{pid}" id="{pid}"{" hidden" if i else ""}>{picture(preview, page, language, i == 0)}</div>')
    preview_values = {**words, 'component_id': doc['id'], 'runtime_url': relative('gallery/', page) + '/',
        'title': escape(title), 'preview_light': doc['previews'][0]['images']['light']['background'],
        'preview_dark': doc['previews'][0]['images']['dark']['background'],
        'preview_height': min(720, max(doc.get('minimum_preview_height', 196),
            max(p['images']['light']['logical_size'][1] for p in doc['previews']) + 32)),
        'state_tabs_hidden': ' hidden' if len(doc['previews']) == 1 else '', 'tabs': ''.join(tabs), 'panels': ''.join(panels)}
    body = Template((ROOT / 'tools/site/component-preview.template.html').read_text()).substitute(preview_values)
    examples = []
    for i, example in enumerate(doc['examples']):
        examples.append(f'<section class="usage-example" aria-labelledby="example-title-{i}"><h3 id="example-title-{i}">{escape(example["name"][language])}</h3><p>{escape(example["description"][language])}</p>'
            + code_block(example_source(example, component, 'cpp'), example_source(example, component, 'python'),
                         example['name'][language], 'example-' + str(i), language) + '</section>')
    body += f'<section id="usage" aria-labelledby="usage-title"><h2 id="usage-title">{words["usage"]}</h2>{"".join(examples)}</section>'
    body += f'<section id="api" aria-labelledby="api-title"><div class="section-heading"><h2 id="api-title">API</h2><a href="{escape(component["cpp"]["declaration_url"])}">{words["source"]} {icon("external")}</a></div>'
    body += api_search(language) + component_api(component, doc['catalog'], page, language) + '</section>'
    category = next(c for c in doc['catalog']['categories'] if c['id'] == component['category_id'])
    return render_shell(doc, documents, page, language, title, doc['description'][language], body,
                        [('appearance', words['overview']), ('usage', words['usage']), ('api', 'API')], ''.join(neighbors),
                        eyebrow=category_label(category, category_owners(doc['catalog']), language))


def render_guide(guide, documents, catalog, language):
    page = guide_path(guide['id'], language)
    context = {'id': guide['id'], 'catalog': catalog, 'component': {'category_id': guide['group']}}
    body, toc = [], []
    for index, section in enumerate(guide['sections']):
        sid = 'section-' + str(index)
        title = section['title'][language]
        toc.append((sid, title))
        code = code_block(section['cpp'], section['python'], title, sid, language) if section['cpp'] or section['python'] else ''
        body.append(f'<section class="guide-section" id="{sid}"><h2>{escape(title)}</h2><p>{escape(section["body"][language])}</p>{code}</section>')
    references = []
    for source in guide['headers']:
        records = [r for r in inventory() if r['source'] == source and r['members']]
        if records:
            record = primary_type(records, source) or records[0]
            label = source.rsplit('/', 1)[-1].removesuffix('.h')
            references.append(f'<li><a href="{escape(type_url(record, catalog, page, language))}"><code>{escape(label)}</code></a></li>')
    title = 'API 参考' if language == 'zh' else 'API reference'
    body.append(f'<section class="guide-section" id="api"><h2>{title}</h2><ul class="reference-links">{"".join(references)}</ul></section>')
    if guide['id'] == 'foundation-qt':
        body.append('<p><a href="https://doc.qt.io/qt-6/qtcore-index.html">Qt Core</a> · <a href="https://doc.qt.io/qt-6/qwidget.html">QWidget</a></p>')
    toc.append(('api', title))
    return render_shell(context, documents, page, language, guide['name'][language], guide['description'][language], ''.join(body), toc)


def render_reference(reference, documents, catalog, language):
    page = reference_path(reference['source'], language)
    context = {'id': reference['id'], 'source': reference['source'], 'catalog': catalog, 'component': {'category_id': 'reference'}}
    # Lead with the header's namesake type; a helper struct's brief would misdescribe the page.
    primary = primary_type(reference['types'], reference['source'])
    lead = localized_description(primary, language) if primary else ''
    fallback = '该头文件公开的类型、函数与常量参考。' if language == 'zh' else 'Public types, functions, and constants declared in this header.'
    description = lead or fallback
    body = '<section id="api" class="reference-body">' + api_search(language)
    body += ''.join(render_type(record, catalog, page, language, include_inherited=True,
                                show_description=not (lead and record is primary))
                    for record in reference['types']) + '</section>'
    toc = [('type-' + slug(record['qualified_name']), record['name']) for record in reference['types']]
    return render_shell(context, documents, page, language, reference['title'], description, body, toc)


def generated_outputs(documents):
    catalog = documents[0]['catalog']
    from release_history import load_history, validate_related_documents
    validate_related_documents(load_history(), [d['id'] for d in documents] + [g['id'] for g in guides()])
    outputs = {'assets/app-icon-64.png': site_icon()}
    for language, entry in (('en', 'api/index.html'), ('zh', 'zh-CN/api/index.html')):
        redirects = [{'names': [d['id'], d['name']['en'], d['name']['zh']],
                      'urls': {lang: relative_url(detail_path(d['id'], lang), entry) for lang in ('en', 'zh')}} for d in documents]
        introductions = [{'names': [g['id'], *g['name'].values()],
            'urls': {lang: relative_url(guide_path(g['id'], lang), entry) for lang in ('en', 'zh')}} for g in guides()]
        redirects = introductions + redirects
        redirects.append({'names': ['releases', '版本记录', '更新日志', 'Changelog', 'roadmap', '路线图'],
                          'urls': {lang: relative_url(release_page_path('releases', lang), entry) for lang in ('en', 'zh')},
                          'filterDirectory': False})
        outputs[entry] = Template((ROOT / 'tools/site/api-entry.template.html').read_text()).substitute(
            lang='zh-CN' if language == 'zh' else 'en', default_language=language,
            canonical=BASE_URL + guide_path('foundation', language).removesuffix('index.html'),
            documents=json.dumps(redirects, ensure_ascii=False).replace('<', '\\u003c'),
            english=redirects[0]['urls']['en'], chinese=redirects[0]['urls']['zh'])
    for language in ('en', 'zh'):
        for doc in documents:
            outputs[detail_path(doc['id'], language)] = render_document(doc, documents, language)
        for guide in guides():
            outputs[guide_path(guide['id'], language)] = render_guide(guide, documents, catalog, language)
        for reference in reference_headers(catalog):
            outputs[reference_path(reference['source'], language)] = render_reference(reference, documents, catalog, language)
    from release_pages import generated_outputs as release_outputs
    outputs.update(release_outputs(documents, catalog, render_shell))
    return outputs
