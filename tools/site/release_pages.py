"""Render version history inside the existing documentation shell."""

import html
import re
from urllib.parse import urlencode
from documentation_navigation import relative
from generate_localized_site import BASE_URL
from release_history import load_history, page_path, REPOSITORY


def escape(value):
    return html.escape(str(value), quote=True)


def prose(value):
    """Escape reviewed copy and render its `identifier` spans as inline code."""
    return re.sub(r'`([^`]+)`', r'<code>\1</code>', escape(value))


def plain(value):
    return value.replace('`', '')


def document_notice(document_id, source, page, language):
    """Identify pending changes only on the documents they affect."""
    data = load_history()
    pending = [data['unreleased'], *[item for item in data['versions'] if item['status'] == 'candidate']]
    for item in pending:
        by_id = document_id in item.get('related', [])
        if not by_id and (not source or source not in item.get('headers', [])):
            continue
        anchor = 'v' + item['version'] if 'version' in item else 'unreleased'
        target = page_path('releases', language) + '?' + urlencode({'change': document_id if by_id else source}) + '#' + anchor
        if language == 'zh':
            text = f'本页含未发布变更，尚未包含在稳定版 {data["latest"]} 中。'
            label = '查看相关版本记录'
        else:
            text = f'This page includes unreleased changes not available in stable {data["latest"]}.'
            label = 'View related release notes'
        return f'<p class="document-version-note">{escape(text)} <a href="{escape(relative(target, page))}">{label}</a></p>'
    return ''


def migration_html(migration, language):
    zh = language == 'zh'
    commit = migration.get('commit')
    evidence = f'<p class="migration-commit">{"引入提交" if zh else "Introduced in"} <a href="{REPOSITORY}/commit/{commit}"><code>{commit[:7]}</code></a></p>' if commit else ''
    return f'''<section class="migration-note" id="{migration['id']}">
      <h3><a href="#{migration['id']}">{prose(migration['title'][language])}</a></h3>
      <p>{prose(migration['impact'][language])}</p>
      <p>{prose(migration['action'][language])}</p>
      <div class="migration-comparison">
        <div><h4>{'旧写法' if zh else 'Before'}</h4><pre tabindex="0"><code>{escape(migration['before'])}</code></pre></div>
        <div><h4>{'新写法' if zh else 'After'}</h4><pre tabindex="0"><code>{escape(migration['after'])}</code></pre></div>
      </div>{evidence}</section>'''


def release_entry(item, language, page, latest):
    zh = language == 'zh'
    version = item.get('version')
    anchor = 'v' + version if version else 'unreleased'
    migrations = item.get('migrations', [])
    status = item.get('status', 'unreleased')
    # Ordinary published releases need no label; the date already says so.
    labels = {'candidate': ('Release candidate', '候选版本')}
    label = ('最新版本' if zh else 'Latest') if version == latest else (labels[status][zh] if status in labels else '')
    timestamp = item.get('published_at')
    date = f'<time datetime="{timestamp}">{timestamp[:10]}</time>' if timestamp else ''
    if status == 'archive':
        date = '<span class="release-date">' + ('未记录发布日期' if zh else 'Date not recorded') + '</span>'
    heading = version or ('开发中' if zh else 'Unreleased')
    if version:
        summary = item['summary'][language]
    else:
        summary = (f'当前源码相对 {latest} 的改动，尚未确定发布版本。' if zh else
                   f'Changes in current source since {latest}. Not yet assigned to a release.')
        if migrations:
            summary += '从源码构建前，请先阅读下面的迁移说明。' if zh else ' Read the migration notes below before building from source.'
    terms = ' '.join([version or 'unreleased 开发中', *item.get('related', []), *item.get('headers', []), plain(summary),
                      *[plain(change[language]) for change in item.get('changes', [])],
                      *[plain(m[field][language]) for m in migrations for field in ('title', 'impact', 'action')],
                      *[m['before'] + ' ' + m['after'] for m in migrations]])
    links = []
    if version:
        if timestamp:
            links.append(f'<a href="{REPOSITORY}/releases/tag/v{version}">{"下载" if zh else "Downloads"}</a>')
        links.append(f'<a href="{REPOSITORY}/blob/main/docs/releases/v{version}.md"' + (' hreflang="en">发布说明（英文）' if zh else '>Release notes') + '</a>')
        previous = item.get('previous')
        if previous:
            links.append(f'<a href="{REPOSITORY}/compare/v{previous}...v{version}">' + (f'与 {previous} 对比' if zh else f'Compare with {previous}') + '</a>')
    else:
        links.append(f'<a href="{REPOSITORY}/compare/v{latest}...main">' + (f'{latest} 之后的提交' if zh else f'Commits since {latest}') + '</a>')
    changes = ''.join(f'<li>{prose(change[language])}</li>' for change in item.get('changes', []))
    change_list = f'<ul class="release-changes">{changes}</ul>' if changes else ''
    state = f'<span class="release-state">{label}</span>' if label else ''
    badge = '<span class="release-breaking">' + ('需要迁移' if zh else 'Migration required') + '</span>' if migrations else ''
    return f'''<article class="release-entry" id="{anchor}" data-release-entry data-version="{version or 'unreleased'}" data-breaking="{str(bool(migrations)).lower()}" data-release-search="{escape(terms.lower())}">
      <header class="release-heading"><h2><a href="#{anchor}">{heading}</a></h2>{state}{badge}{date}</header>
      <p>{prose(summary)}</p>{change_list}{''.join(migration_html(m, language) for m in migrations)}
      <nav class="release-links" aria-label="{heading} {'相关链接' if zh else 'links'}">{' '.join(links)}</nav>
    </article>'''


def history_body(data, language, page):
    zh = language == 'zh'
    options = ''.join(f'<option value="{v["version"]}">{v["version"]}</option>' for v in data['versions'])
    filters = f'''<form class="release-filters" data-release-filters hidden role="search" aria-label="{'筛选版本记录' if zh else 'Filter releases'}">
      <label>{'搜索' if zh else 'Search'}<input type="search" name="change" data-release-query placeholder="{'组件名或关键词，如 ParticleBackdrop' if zh else 'Component or keyword, e.g. ParticleBackdrop'}"></label>
      <label>{'从哪个版本升级' if zh else 'Upgrading from'}<select name="from" data-release-from><option value="">{'不限' if zh else 'Any version'}</option>{options}</select></label>
      <label class="release-checkbox"><input type="checkbox" name="breaking" data-release-breaking>{'只看需要迁移的版本' if zh else 'Only releases that need migration'}</label>
      <button type="reset">{'清除筛选' if zh else 'Clear filters'}</button>
      <p class="release-result" data-release-result role="status" aria-live="polite"></p>
    </form>
    <p class="release-empty" data-release-empty hidden>{'没有符合条件的记录。换个关键词，或清除筛选。' if zh else 'Nothing matches. Try another keyword or clear the filters.'}</p>'''
    entries = [release_entry(data['unreleased'], language, page, data['latest'])]
    for index, item in enumerate(data['versions']):
        version = dict(item)
        if index + 1 < len(data['versions']):
            version['previous'] = data['versions'][index + 1]['version']
        entries.append(release_entry(version, language, page, data['latest']))
    return filters + ''.join(entries)


def roadmap_redirect(language):
    """Keep published roadmap URLs usable without a second project-updates page."""
    target = page_path('releases', language)
    link = relative(target, page_path('roadmap', language))
    title = '版本记录与迁移' if language == 'zh' else 'Releases and migrations'
    return f'''<!doctype html>
<html lang="{'zh-CN' if language == 'zh' else 'en'}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="0; url={link}">
  <link rel="canonical" href="{BASE_URL}{target.removesuffix('index.html')}">
  <title>{title} · Fluent-Qt</title>
</head>
<body><p><a href="{link}">{title}</a></p></body>
</html>
'''


def generated_outputs(documents, catalog, render_shell):
    data = load_history()
    outputs = {}
    for language in ('en', 'zh'):
        zh = language == 'zh'
        page = page_path('releases', language)
        context = {'id': 'releases', 'catalog': catalog, 'component': {'category_id': 'project'}}
        title = '版本记录与迁移' if zh else 'Releases and migrations'
        description = '每个版本改了什么，升级时需要改哪些代码。' if zh else 'What changed in each release, and what to change in your code when you upgrade.'
        body = history_body(data, language, page)
        toc = [('unreleased', '开发中' if zh else 'Unreleased')]
        toc += [('v' + v['version'], v['version']) for v in data['versions']]
        outputs[page] = render_shell(context, documents, page, language, title, description, body, toc)
        outputs[page_path('roadmap', language)] = roadmap_redirect(language)
    return outputs
