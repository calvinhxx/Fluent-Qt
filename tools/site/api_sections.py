"""Render public declarations as readable, linked reference sections."""
from collections import defaultdict
import hashlib
import html
from pathlib import Path
import re

from public_api import inventory, resolve_type, inherited_types, python_only_members
from documentation_guides import guide_ui
from documentation_navigation import relative
from docs_i18n import translate_api_description
from site_locales import locale_url_prefix


def slug(value):
    return re.sub(r'[^a-z0-9]+', '-', re.sub(r'([a-z0-9])([A-Z])', r'\1-\2', value).lower()).strip('-')


def reference_path(source, language='en'):
    return locale_url_prefix(language) + 'api/reference/' + slug(Path(source).stem) + '/index.html'


def localized_description(record, language):
    return translate_api_description(record.get('description', {}), language)


def primary_type(records, source):
    """Return the record named after its header: the type a header page is about."""
    return next((r for r in records if r['name'] == Path(source).stem), None)


def reference_headers(catalog):
    component_sources = {resolve_type(c['cpp']['qualified_type'])['source'] for c in catalog['components']}
    by_source = defaultdict(list)
    for record in inventory():
        if record['members'] and record['source'] not in component_sources:
            by_source[record['source']].append(record)
    return [{'id': 'reference-' + slug(Path(source).stem), 'title': Path(source).stem,
             'source': source, 'types': records} for source, records in by_source.items()]


def type_url(record, catalog, page, language):
    component = next((c for c in catalog['components'] if resolve_type(c['cpp']['qualified_type'])['source'] == record['source']), None)
    path = (locale_url_prefix(language) + 'api/' + component['id'] + '/index.html') if component else reference_path(record['source'], language)
    return relative(path, page) + '#type-' + slug(record['qualified_name'])


def base_links(record, catalog, page, language):
    links = []
    for name in record['bases']:
        base = resolve_type(name)
        if base:
            url = type_url(base, catalog, page, language)
        elif name.startswith('Q') and re.fullmatch(r'Q\w+', name):
            url = 'https://doc.qt.io/qt-6/' + name.lower() + '.html'
        else:
            links.append('<code>' + html.escape(name) + '</code>'); continue
        links.append(f'<a href="{html.escape(url)}"><code>{html.escape(name)}</code></a>')
    if not links:
        return ''
    label = html.escape(guide_ui(language)['inherits'])
    return f'<p class="api-inheritance">{label} {" · ".join(links)}</p>'


def member_markup(member, language, prefix, ui=None):
    ui = ui or guide_ui(language)
    digest = hashlib.sha1(member['signature'].encode()).hexdigest()[:8]
    anchor = prefix + '-' + slug(member['name']) + '-' + digest
    description = translate_api_description(member.get('description', {}), language)
    details = f'<p>{html.escape(description)}</p>' if description else ''
    if member.get('deprecated'):
        details = f'<p>{html.escape(ui["deprecated"])} · {html.escape(member["deprecated"])}</p>' + details
    if member['kind'] == 'properties':
        accessor_labels = ui['accessors']
        accessors = []
        for key in ('read', 'write', 'reset', 'notify'):
            value = member.get(key)
            if isinstance(value, str):
                accessors.append(f'<span>{html.escape(accessor_labels[key])} <code>{html.escape(value)}()</code></span>')
        if accessors:
            details += '<div class="api-accessors">' + ''.join(accessors) + '</div>'
    if member.get('values'):
        details += '<ul class="api-enum-values">' + ''.join(
            f'<li><code>{html.escape(v["signature"])}</code>'
            + (f' <span>{html.escape(translate_api_description(v["description"], language))}</span>'
               if any(v['description'].values()) else '') + '</li>' for v in member['values']) + '</ul>'
    return f'<div class="api-member" id="{anchor}" data-api-member><dt><a href="#{anchor}"><code>{html.escape(member["signature"])}</code></a></dt><dd>{details}</dd></div>'


def render_members(members, language, prefix, ui=None):
    ui = ui or guide_ui(language)
    sections = []
    for kind, label in ui['apiGroups'].items():
        group = [m for m in members if m['kind'] == kind]
        if not group:
            continue
        sections.append(f'<section class="api-group" data-api-group id="{prefix}-{kind}"><h4>{html.escape(label)}</h4><dl class="api-list">'
                        + ''.join(member_markup(m, language, prefix, ui=ui) for m in group) + '</dl></section>')
    return ''.join(sections)


def render_type(record, catalog, page, language, include_inherited=False, python=None, show_description=True):
    ui = guide_ui(language)
    prefix = 'type-' + slug(record['qualified_name'])
    public = [m for m in record['members'] if m['access'] == 'public']
    protected = [m for m in record['members'] if m['access'] == 'protected']
    summary = localized_description(record, language) if show_description else ''
    summary_html = f'<p class="api-type-description">{html.escape(summary)}</p>' if summary else ''
    parts = [f'<section class="api-type" id="{prefix}"><h3><code>{html.escape(record["qualified_name"])}</code></h3>',
             base_links(record, catalog, page, language), summary_html, render_members(public, language, prefix, ui=ui)]
    if python:
        parts.append(render_members(python_only_members(record, python), language, prefix, ui=ui))
    if protected:
        parts.append(f'<details class="api-inherited"><summary>{html.escape(ui["protectedMembers"])}</summary>{render_members(protected, language, prefix + "-protected", ui=ui)}</details>')
    if include_inherited:
        for base in inherited_types(record):
            label = ui['inheritedFrom'].format(name=base['qualified_name'])
            parts.append(f'<details class="api-inherited"><summary>{html.escape(label)}</summary>'
                         + render_members([m for m in base['members'] if m['access'] == 'public' and m['kind'] not in ('constructors', 'destructors')], language,
                                          prefix + '-' + slug(base['qualified_name']), ui=ui) + '</details>')
        qt_bases = list(dict.fromkeys(name for base in [record, *inherited_types(record)] for name in base['bases']
                                    if name.startswith('Q') and resolve_type(name) is None and re.fullmatch(r'Q\w+', name)))
        if qt_bases:
            links = ' · '.join(f'<a href="https://doc.qt.io/qt-6/{name.lower()}-members.html">{name}</a>' for name in qt_bases)
            parts.append(f'<p class="api-inheritance">{html.escape(ui["qtBaseDocs"])}{links}</p>')
    parts.append('</section>')
    return ''.join(parts)


def component_api(component, catalog, page, language):
    primary = resolve_type(component['cpp']['qualified_type'])
    siblings = [r for r in inventory() if r['source'] == primary['source'] and r is not primary and r['members']]
    return render_type(primary, catalog, page, language, include_inherited=True, python=component['python']) + ''.join(
        render_type(record, catalog, page, language) for record in siblings)
