"""Render public declarations as readable, linked reference sections."""
from collections import defaultdict
import hashlib
import html
from pathlib import Path
import re

from public_api import inventory, resolve_type, inherited_types, python_extensions
from documentation_navigation import relative

GROUPS = {
    'constructors': ('Constructors', '构造函数'), 'properties': ('Properties', '属性'),
    'methods': ('Methods', '方法'), 'signals': ('Signals', '信号'), 'enums': ('Enumerations', '枚举'),
    'types': ('Types and aliases', '类型与别名'), 'fields': ('Fields', '字段'),
    'constants': ('Constants', '常量'), 'destructors': ('Destructors', '析构函数'),
    'python': ('PySide6 additions', 'PySide6 扩展'),
}


def slug(value):
    return re.sub(r'[^a-z0-9]+', '-', re.sub(r'([a-z0-9])([A-Z])', r'\1-\2', value).lower()).strip('-')


def reference_path(source, language='en'):
    return ('zh-CN/' if language == 'zh' else '') + 'api/reference/' + slug(Path(source).stem) + '/index.html'


def localized_description(record, language):
    description = record.get('description', {})
    return description.get(language) or description.get('en') or ''


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
    path = (('zh-CN/' if language == 'zh' else '') + 'api/' + component['id'] + '/index.html') if component else reference_path(record['source'], language)
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
    label = '继承自' if language == 'zh' else 'Inherits'
    return f'<p class="api-inheritance">{label} {" · ".join(links)}</p>'


def member_markup(member, language, prefix):
    digest = hashlib.sha1(member['signature'].encode()).hexdigest()[:8]
    anchor = prefix + '-' + slug(member['name']) + '-' + digest
    description = member['description'].get(language) or member['description'].get('en') or member['description'].get('zh')
    details = f'<p>{html.escape(description)}</p>' if description else ''
    if member.get('deprecated'):
        label = '已弃用' if language == 'zh' else 'Deprecated'
        details = f'<p>{label} · {html.escape(member["deprecated"])}</p>' + details
    if member['kind'] == 'properties':
        labels = {'read': ('Read', '读取'), 'write': ('Write', '设置'), 'reset': ('Reset', '重置'), 'notify': ('Notify', '通知')}
        accessors = []
        for key, values in labels.items():
            value = member.get(key)
            if isinstance(value, str):
                accessors.append(f'<span>{values[language == "zh"]} <code>{html.escape(value)}()</code></span>')
        if accessors:
            details += '<div class="api-accessors">' + ''.join(accessors) + '</div>'
    if member.get('values'):
        details += '<ul class="api-enum-values">' + ''.join(
            f'<li><code>{html.escape(v["signature"])}</code>'
            + (f' <span>{html.escape(v["description"].get(language) or v["description"].get("en") or "")}</span>'
               if any(v['description'].values()) else '') + '</li>' for v in member['values']) + '</ul>'
    return f'<div class="api-member" id="{anchor}" data-api-member><dt><a href="#{anchor}"><code>{html.escape(member["signature"])}</code></a></dt><dd>{details}</dd></div>'


def render_members(members, language, prefix):
    sections = []
    for kind, labels in GROUPS.items():
        group = [m for m in members if m['kind'] == kind]
        if not group:
            continue
        label = labels[language == 'zh']
        sections.append(f'<section class="api-group" data-api-group id="{prefix}-{kind}"><h4>{label}</h4><dl class="api-list">'
                        + ''.join(member_markup(m, language, prefix) for m in group) + '</dl></section>')
    return ''.join(sections)


def render_type(record, catalog, page, language, include_inherited=False, python=None, show_description=True):
    prefix = 'type-' + slug(record['qualified_name'])
    public = [m for m in record['members'] if m['access'] == 'public']
    protected = [m for m in record['members'] if m['access'] == 'protected']
    summary = localized_description(record, language) if show_description else ''
    summary_html = f'<p class="api-type-description">{html.escape(summary)}</p>' if summary else ''
    parts = [f'<section class="api-type" id="{prefix}"><h3><code>{html.escape(record["qualified_name"])}</code></h3>',
             base_links(record, catalog, page, language), summary_html, render_members(public, language, prefix)]
    if python:
        known = {m['name'] for b in [record, *inherited_types(record)] for m in b['members']}
        additions = [m for m in python_extensions(python['module'], python['type']) if m['name'] not in known]
        parts.append(render_members(additions, language, prefix))
    if protected:
        label = '子类扩展接口' if language == 'zh' else 'Subclass interfaces'
        parts.append(f'<details class="api-inherited"><summary>{label}</summary>{render_members(protected, language, prefix + "-protected")}</details>')
    if include_inherited:
        for base in inherited_types(record):
            label = ('继承自 ' if language == 'zh' else 'Inherited from ') + base['qualified_name']
            parts.append(f'<details class="api-inherited"><summary>{html.escape(label)}</summary>'
                         + render_members([m for m in base['members'] if m['access'] == 'public' and m['kind'] not in ('constructors', 'destructors')], language,
                                          prefix + '-' + slug(base['qualified_name'])) + '</details>')
        qt_bases = list(dict.fromkeys(name for base in [record, *inherited_types(record)] for name in base['bases']
                                    if name.startswith('Q') and resolve_type(name) is None and re.fullmatch(r'Q\w+', name)))
        if qt_bases:
            label = 'Qt 基类完整成员' if language == 'zh' else 'All inherited Qt members'
            links = ' · '.join(f'<a href="https://doc.qt.io/qt-6/{name.lower()}-members.html">{name}</a>' for name in qt_bases)
            parts.append(f'<p class="api-inheritance">{label}：{links}</p>')
    parts.append('</section>')
    return ''.join(parts)


def component_api(component, catalog, page, language):
    primary = resolve_type(component['cpp']['qualified_type'])
    siblings = [r for r in inventory() if r['source'] == primary['source'] and r is not primary and r['members']]
    return render_type(primary, catalog, page, language, include_inherited=True, python=component['python']) + ''.join(
        render_type(record, catalog, page, language) for record in siblings)
