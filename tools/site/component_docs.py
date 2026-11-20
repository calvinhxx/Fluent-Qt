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
from docs_i18n import (
    display_component_title,
    localized_component,
)
from documentation_guides import guides, guide_path, guide_ui
from documentation_navigation import navigation as site_navigation, breadcrumb, relative as relative_url
from site_assets import source_path
from site_locales import FULL_API_LOCALES, LOCALE_KEYS, LOCALES, api_language, locale_by_key, locale_url_prefix
from generate_localized_site import BASE_URL
from release_history import page_path as release_page_path
from release_pages import document_notice

SITE = ROOT / "site"


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


def shell_words(language):
    ui = guide_ui(language)
    base = {"overview": ui["exampleHeading"]}
    for key in (
        "usage", "api", "source", "copy", "copied", "copyError",
        "light", "dark", "skip", "theme", "loadingLive", "failedLive",
        "retryLive", "noScript", "onPage", "loadingPreview", "previewUnavailable",
    ):
        base[key] = ui[key]
    return base


def section_label(key, language):
    return guide_ui(language)["sections"][key]


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
    return guide_ui(language)["categories"].get(category["id"], category["title"])


def detail_path(component_id, language):
    return locale_url_prefix(language) + f"api/{component_id}/index.html"


def icon(name):
    paths = {"external": '<path d="M14 4h6v6M20 4 10 14M10 4H4v16h16v-6"/>',
             "copy": '<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V4H4v12h4"/>'}
    return f'<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.6">{paths[name]}</svg>'


def collect(catalog, capture=None):
    capture = capture or available_previews()
    components = {c["id"]: c for c in catalog["components"]}
    canonical = json.loads((ROOT / "docs/ai/generated/fluentqt-ai-catalog.json").read_text(encoding="utf-8"))
    samples = {(c["id"], s["id"]): s for c in canonical["components"] for s in c["samples"]}
    python = json.loads((ROOT / "app/data/gallery_python_snippets.json").read_text(encoding="utf-8"))
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
    ui = guide_ui(language)
    doc_lang = api_language(language)

    def group(key, label, entries, active=False, headers=False):
        attributes = (' open' if active else '') + (' data-header-group' if headers else '')
        return f'<details data-directory-group="{key}"{attributes}><summary>{escape(label)}</summary><ul>{"".join(entries)}</ul></details>'

    def guide_entry(item, overview=False):
        current = ' aria-current="page"' if item['id'] == doc['id'] else ''
        label = ui['overview'] if overview else item['name'][language]
        accessible = f' aria-label="{escape(item["name"][language] + ": " + label)}"' if overview else ''
        terms = ' '.join(dict.fromkeys([item['id'], label, *item['name'].values(), item['description'][language]]))
        return f'<li data-directory-entry data-kind="guide" data-search="{escape(terms.lower())}"><a href="{relative_url(guide_path(item["id"], language), page)}"{accessible}{current}>{escape(label)}</a></li>'

    category_groups = {}
    for category in doc["catalog"]["categories"]:
        introduction = category_guides.get(category['id'])
        entries = [guide_entry(introduction, overview=True)] if introduction else []
        components = [c for c in doc["catalog"]["components"] if c["category_id"] == category["id"]]
        for component in sorted(components, key=lambda c: (c["id"] not in available, c["title"].lower())):
            documented = available.get(component["id"])
            localized = localized_component(documented, doc_lang) if documented else None
            name = display_component_title(component["title"], localized["name"], doc_lang) if localized else component["title"]
            url = relative_url(detail_path(component["id"], doc_lang), page) if documented else component["cpp"]["declaration_url"]
            current = ' aria-current="page"' if component["id"] == doc["id"] else ''
            external = '' if documented else ' target="_blank" rel="noopener"'
            kind = '' if documented else '<span class="reference-kind">API</span>'
            search_names = [name, component['id'], component['cpp']['qualified_type'], component['python']['import_statement']]
            if documented:
                search_names.extend([documented['name']['en'], documented['name']['zh']])
            terms = ' '.join(dict.fromkeys(search_names))
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
    label = ui['releasesAndMigrations']
    project_terms = f'releases and migrations 版本记录与迁移 {label.lower()}'
    project_link = f'<li data-directory-entry data-search="{escape(project_terms)}"><a href="{relative_url(release_page_path("releases", doc_lang), page)}"{current}>{escape(label)}</a></li>'
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
        references.append(f'<li data-directory-entry data-kind="reference" data-search="{escape(terms.lower())}"><a href="{relative_url(reference_path(reference["source"], doc_lang), page)}"{current}><span>{escape(reference["title"])}</span></a></li>')
    groups.append(group('reference', section_label('reference', language), references, doc['component']['category_id'] == 'reference', headers=True))
    search = ui['searchDocumentation']
    title = ui['contentsAndSearch']
    return f'''<details class="documentation-directory" open><summary>{escape(title)}</summary>
      <div class="directory-content">
      <label class="directory-search"><span class="visually-hidden">{escape(search)}</span><input type="search" data-directory-search placeholder="{escape(search)}" autocomplete="off" aria-keyshortcuts="Control+K Meta+K"></label>
      <p class="directory-status" data-directory-status role="status" hidden data-results-template="{escape(ui["resultsTemplate"])}" data-empty-label="{escape(ui["noResults"])}"></p>
      <nav class="directory-groups" aria-label="{escape(title)}">{''.join(groups)}</nav></div></details>'''


def picture(preview, page, language, eager=False, alt_text=None):
    light, dark = (preview["images"][theme] for theme in ("light", "dark"))
    def url(image):
        version = "?v=" + image["sha256"][:12] if image["sha256"] else ""
        return escape(relative(image["path"], page) + version)
    w, h = light["logical_size"]
    priority = ' fetchpriority="high"' if eager else ''
    alt = alt_text if alt_text is not None else preview["alt"][language]
    return (f'<picture><source data-dark-src="{url(dark)}" media="(prefers-color-scheme: dark)" srcset="{url(dark)}">'
            f'<img src="{url(light)}" width="{w}" height="{h}" alt="{escape(alt)}" '
            f'loading="{"eager" if eager else "lazy"}" decoding="async"{priority}></picture>')


def code_block(cpp, python, name, key, language):
    words = shell_words(language)
    copy_label = escape(words["copy"])
    tabs, panels = [], []
    for index, (kind, label, source) in enumerate((('cpp', 'C++', cpp), ('python', 'PySide6', python))):
        pid = key + '-' + kind
        tabs.append(f'<button type="button" role="tab" id="tab-{pid}" aria-controls="{pid}" aria-selected="{str(index == 0).lower()}" tabindex="{0 if index == 0 else -1}">{label}</button>')
        panels.append(f'<div role="tabpanel" id="{pid}" aria-labelledby="tab-{pid}"{" hidden" if index else ""}><pre tabindex="0"><code>{escape(source)}</code></pre></div>')
    return f'<div class="code-example" data-tabs><div class="code-toolbar"><div class="tab-list" role="tablist" aria-label="{escape(name)}">{"".join(tabs)}</div><button class="copy-code" type="button" data-copy aria-label="{copy_label}">{icon("copy")}<span>{copy_label}</span></button></div>{"".join(panels)}</div>'


def example_source(example, component, kind):
    source = example[kind].strip()
    if kind == 'python':
        return source if re.search(r'^import fluentqt\b', source, re.M) else 'import fluentqt\n\n' + source
    header = component['cpp']['public_header']
    namespace = component['cpp']['qualified_type'].rsplit('::', 1)[0]
    return f'#include {header}\nusing namespace {namespace};\n\n' + source


def api_search(language):
    ui = guide_ui(language)
    label = escape(ui['filterApi'])
    empty = escape(ui['noApiMatches'])
    return f'<label class="api-search"><span class="visually-hidden">{label}</span><input type="search" data-api-filter placeholder="{label}" autocomplete="off"></label><p data-api-empty hidden role="status">{empty}</p>'


def render_shell(doc, documents, page, language, title, description, body, toc, neighbors='', eyebrow=None, playground_toggle='', alternates=None):
    """Wrap a document body in the shared shell; non-component pages name their section above the title."""
    words = shell_words(language)
    if eyebrow is None:
        eyebrow = section_label(doc['component']['category_id'], language)
    if alternates is None:
        base_rel = page.removeprefix(locale_url_prefix(language))
        alternates = [
            (loc.html_lang, BASE_URL + (loc.url_prefix + base_rel).removesuffix("index.html"))
            for loc in LOCALES
        ] + [("x-default", BASE_URL + base_rel.removesuffix("index.html"))]
    alternate_links = '\n  '.join(
        f'<link rel="alternate" hreflang="{escape(hreflang)}" href="{escape(url)}">'
        for hreflang, url in alternates
    )
    values = {'lang': locale_by_key(language).html_lang, 'title': escape(title),
        'description': escape(description), 'canonical': BASE_URL + page.removesuffix('index.html'),
        'alternate_links': alternate_links,
        'directory': component_directory(doc, documents, page, language), 'neighbors': neighbors,
        'style': asset('api/component.css', page), 'navigation_style': asset('api/navigation.css', page),
        'script': asset('api/component.js', page), 'site_navigation': site_navigation(page, language),
        'playground_script': asset('api/playground.js', page),
        'playground_toggle': playground_toggle,
        'breadcrumb': breadcrumb(page, language, title), 'app_icon': relative('assets/app-icon-64.png', page),
        'eyebrow': f'<p class="document-eyebrow">{escape(eyebrow)}</p>' if eyebrow else '',
        'version_notice': document_notice(doc['id'], doc.get('source'), page, language),
        'body': body, 'page_navigation': ''.join(f'<a href="#{id}">{escape(label)}</a>' for id, label in toc)}
    values.update({key: escape(value) for key, value in words.items()})
    return Template((ROOT / 'tools/site/component.template.html').read_text(encoding="utf-8")).substitute(values)


def render_document(doc, documents, language):
    component = doc['component']
    page = detail_path(doc['id'], language)
    ui = guide_ui(language)
    words = shell_words(language)
    localized = localized_component(doc, language)
    title = display_component_title(component['title'], localized['name'], language)
    position = next(i for i, item in enumerate(documents) if item['id'] == doc['id'])
    neighbors = []
    for index, rel, label in ((position - 1, 'prev', ui['previous']), (position + 1, 'next', ui['next'])):
        if 0 <= index < len(documents):
            item = documents[index]
            neighbors.append(f'<a class="document-neighbor" rel="{rel}" href="{relative_url(detail_path(item["id"], language), page)}">'
                             f'<span>{escape(label)}</span><strong>{escape(item["component"]["title"])}</strong></a>')
    tabs, panels = [], []
    for i, preview in enumerate(doc['previews']):
        pid = 'state-' + preview['id']
        localized_preview = localized['previews'][preview['id']]
        tabs.append(f'<button type="button" role="tab" id="tab-{pid}" aria-controls="{pid}" data-sample="{escape(preview["sample"])}" data-state="{escape(preview["id"])}" aria-selected="{str(i == 0).lower()}" tabindex="{0 if i == 0 else -1}">{escape(localized_preview["name"])}</button>')
        panels.append(f'<div class="preview-panel" role="tabpanel" aria-labelledby="tab-{pid}" id="{pid}"{" hidden" if i else ""}>{picture(preview, page, language, i == 0, alt_text=localized_preview["alt"])}</div>')
    # The live example may grow to its sample's reported height, never below this.
    minimum = doc.get('minimum_preview_height', 196)
    playground = (ROOT / 'app/assets/playgrounds' / (doc['id'] + '.json')).is_file()
    customize = escape(ui['propertyPreview'])

    preview_values = {**{key: escape(value) for key, value in words.items()},
        'component_id': doc['id'], 'runtime_url': relative('gallery/', page) + '/',
        'playground_attribute': (' data-playground data-playground-layout="data"' if doc['component']['category_id'] in ('charts', 'collections') else ' data-playground') if playground else '',
        'playground_toggle': (f'<button type="button" class="playground-toggle" data-playground-toggle '
                              f'aria-label="{customize}" aria-pressed="false" aria-expanded="false" '
                              f'aria-controls="playground-popover" hidden><span class="playground-icon playground-icon-pick" '
                              f'aria-hidden="true"></span><span>{customize}</span></button>') if playground else '',
        'editor_error': escape(ui['editorError']),
        'title': escape(title), 'preview_light': doc['previews'][0]['images']['light']['background'],
        'preview_dark': doc['previews'][0]['images']['dark']['background'],
        'preview_min': minimum, 'preview_height': min(720, max(minimum,
            max(p['images']['light']['logical_size'][1] for p in doc['previews']) + 32)),
        'state_tabs_hidden': ' hidden' if len(doc['previews']) == 1 else '', 'tabs': ''.join(tabs), 'panels': ''.join(panels)}
    body = Template((ROOT / 'tools/site/component-preview.template.html').read_text(encoding="utf-8")).substitute(preview_values)
    examples = []
    for i, example in enumerate(doc['examples']):
        localized_example = localized['examples'][example['sample']]
        examples.append(f'<section class="usage-example" aria-labelledby="example-title-{i}"><h3 id="example-title-{i}">{escape(localized_example["name"])}</h3><p>{escape(localized_example["description"])}</p>'
            + code_block(example_source(example, component, 'cpp'), example_source(example, component, 'python'),
                         localized_example['name'], 'example-' + str(i), language) + '</section>')
    body += f'<section id="usage" aria-labelledby="usage-title"><h2 id="usage-title">{escape(words["usage"])}</h2>{"".join(examples)}</section>'
    body += f'<section id="api" aria-labelledby="api-title"><div class="section-heading"><h2 id="api-title">API</h2><a href="{escape(component["cpp"]["declaration_url"])}">{escape(words["source"])} {icon("external")}</a></div>'
    body += api_search(language) + component_api(component, doc['catalog'], page, language) + '</section>'
    category = next(c for c in doc['catalog']['categories'] if c['id'] == component['category_id'])
    return render_shell(doc, documents, page, language, title, localized['description'], body,
                        [('appearance', words['overview']), ('usage', words['usage']), ('api', 'API')], ''.join(neighbors),
                        eyebrow=category_label(category, category_owners(doc['catalog']), language),
                        playground_toggle=preview_values['playground_toggle'])


def render_guide(guide, documents, catalog, language):
    page = guide_path(guide['id'], language)
    context = {'id': guide['id'], 'catalog': catalog, 'component': {'category_id': guide['group']}}
    available = {item['id']: item for item in documents}
    ui = guide_ui(language)
    doc_lang = api_language(language)
    body, toc = [], []
    if guide['id'] == 'foundation':
        heading = ui['playgroundHeading']
        copy = ui['playgroundCopy']
        primary = ui['playgroundPrimary']
        primary_url = relative_url(detail_path('checkbox', doc_lang) + '?playground=1', page)
        other_previews = ('button', 'combobox', 'data-grid', 'line-chart', 'spatial-view', 'particle-backdrop', 'qml-view')
        pills = ''.join(
            f'<a class="guide-playground-pill" href="{escape(relative_url(detail_path(cid, doc_lang) + "?playground=1", page))}">{escape(available[cid]["component"]["title"])}</a>'
            for cid in other_previews if cid in available
        )
        body.append(
            f'<aside class="guide-playground-banner" aria-labelledby="guide-playground-title">'
            f'<h2 id="guide-playground-title">{escape(heading)}</h2>'
            f'<p>{escape(copy)}</p>'
            f'<div class="guide-playground-actions">'
            f'<a class="guide-playground-primary" href="{escape(primary_url)}">{escape(primary)}</a>'
            f'<div class="guide-playground-pills">{pills}</div>'
            f'</div></aside>'
        )
    elif guide['component_category']:
        category_components = [c for c in catalog['components']
                               if c['category_id'] == guide['component_category'] and c['id'] in available]
        if category_components:
            heading = ui['moduleHeading']
            copy = ui['moduleCopy']
            cta_label = ui['propertyPreview']
            rows = []
            for comp in category_components:
                doc = available[comp['id']]
                name = display_component_title(comp['title'], localized_component(doc, doc_lang)['name'], doc_lang)
                doc_url = relative_url(detail_path(comp['id'], doc_lang), page)
                pg_url = relative_url(detail_path(comp['id'], doc_lang) + '?playground=1', page)
                rows.append(
                    f'<div class="guide-module-item">'
                    f'<a class="guide-module-doc" href="{escape(doc_url)}">{escape(name)}</a>'
                    f'<a class="guide-playground-pill" href="{escape(pg_url)}">{escape(cta_label)}</a>'
                    f'</div>'
                )
            body.append(
                f'<aside class="guide-playground-banner guide-module-banner" aria-labelledby="guide-playground-title">'
                f'<h2 id="guide-playground-title">{escape(heading)}</h2>'
                f'<p>{escape(copy)}</p>'
                f'<div class="guide-module-components">{"".join(rows)}</div>'
                f'</aside>'
            )
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
            references.append(f'<li><a href="{escape(type_url(record, catalog, page, doc_lang))}"><code>{escape(label)}</code></a></li>')
    title = ui['apiReference']
    body.append(f'<section class="guide-section" id="api"><h2>{escape(title)}</h2><ul class="reference-links">{"".join(references)}</ul></section>')
    if guide['id'] == 'foundation-qt':
        body.append('<p><a href="https://doc.qt.io/qt-6/qtcore-index.html">Qt Core</a> · <a href="https://doc.qt.io/qt-6/qwidget.html">QWidget</a></p>')
    toc.append(('api', title))
    alternates = [
        (loc.html_lang, BASE_URL + guide_path(guide['id'], loc.key).removesuffix('index.html'))
        for loc in LOCALES
    ] + [('x-default', BASE_URL + guide_path(guide['id'], 'en').removesuffix('index.html'))]
    return render_shell(context, documents, page, language, guide['name'][language], guide['description'][language], ''.join(body), toc, alternates=alternates)


def render_reference(reference, documents, catalog, language):
    page = reference_path(reference['source'], language)
    context = {'id': reference['id'], 'source': reference['source'], 'catalog': catalog, 'component': {'category_id': 'reference'}}
    # Lead with the header's namesake type; a helper struct's brief would misdescribe the page.
    primary = primary_type(reference['types'], reference['source'])
    lead = localized_description(primary, language) if primary else ''
    fallback = guide_ui(language)['headerFallback']
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
    api_template = Template((ROOT / 'tools/site/api-entry.template.html').read_text(encoding="utf-8"))
    supported_languages = json.dumps(list(LOCALE_KEYS))
    release_names = list(dict.fromkeys([
        'releases', 'Changelog', 'roadmap', '版本记录', '更新日志', '路线图',
        *(guide_ui(lang)['releasesAndMigrations'] for lang in LOCALE_KEYS),
    ]))
    for loc in LOCALES:
        language = loc.key
        entry = f'{loc.url_prefix}api/index.html'
        redirects = [{'names': list(dict.fromkeys([
                          d['id'], d['component']['title'], d['name']['en'], d['name']['zh'],
                          *(localized_component(d, lang)['name'] for lang in LOCALE_KEYS),
                      ])),
                      'urls': {lang: relative_url(detail_path(d['id'], api_language(lang)), entry) for lang in LOCALE_KEYS}} for d in documents]
        introductions = [{'names': [g['id'], *g['name'].values()],
            'urls': {lang: relative_url(guide_path(g['id'], lang), entry) for lang in LOCALE_KEYS}} for g in guides()]
        redirects = introductions + redirects
        redirects.append({'names': release_names,
                          'urls': {lang: relative_url(release_page_path('releases', api_language(lang)), entry) for lang in LOCALE_KEYS},
                          'filterDirectory': False})
        fallback_links = ' · '.join(
            f'<a href="{escape(redirects[0]["urls"][target.key])}">{escape(target.native_name)}</a>'
            for target in LOCALES
        )
        outputs[entry] = api_template.substitute(
            lang=loc.html_lang, default_language=language,
            supported_languages=supported_languages,
            canonical=BASE_URL + guide_path('foundation', language).removesuffix('index.html'),
            documents=json.dumps(redirects, ensure_ascii=False).replace('<', '\\u003c'),
            fallback_links=fallback_links)
    for language in LOCALE_KEYS:
        for guide in guides():
            outputs[guide_path(guide['id'], language)] = render_guide(guide, documents, catalog, language)
    for language in FULL_API_LOCALES:
        for doc in documents:
            outputs[detail_path(doc['id'], language)] = render_document(doc, documents, language)
        for reference in reference_headers(catalog):
            outputs[reference_path(reference['source'], language)] = render_reference(reference, documents, catalog, language)
    from release_pages import generated_outputs as release_outputs
    outputs.update(release_outputs(documents, catalog, render_shell))
    return outputs
