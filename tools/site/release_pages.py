"""Render version history inside the existing documentation shell."""

import html
import re
from urllib.parse import urlencode
from documentation_navigation import relative
from generate_localized_site import BASE_URL
from release_history import load_history, page_path, REPOSITORY, SCOPES

SCOPE_LABELS = {'uilib': {'en': 'UILib', 'zh': '组件库 UILib'},
                'gallery': {'en': 'Gallery', 'zh': 'Gallery 示例应用'},
                'project': {'en': 'Project notes', 'zh': '项目说明'}}


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
        for group in item.get('groups', []):
            by_id = document_id in group.get('related', [])
            if not by_id and (not source or source not in group.get('headers', [])):
                continue
            anchor = 'v' + item['version'] if 'version' in item else 'unreleased'
            params = {'change': document_id if by_id else source, 'scope': group['scope']}
            target = page_path('releases', language) + '?' + urlencode(params) + '#' + anchor
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
      <h4><a href="#{migration['id']}">{prose(migration['title'][language])}</a></h4>
      <p>{prose(migration['impact'][language])}</p>
      <p>{prose(migration['action'][language])}</p>
      <div class="migration-comparison">
        <div><h5>{'旧写法' if zh else 'Before'}</h5><pre tabindex="0"><code>{escape(migration['before'])}</code></pre></div>
        <div><h5>{'新写法' if zh else 'After'}</h5><pre tabindex="0"><code>{escape(migration['after'])}</code></pre></div>
      </div>{evidence}</section>'''


def scope_group(group, language, version):
    migrations = group.get('migrations', [])
    summary = group.get('summary', {}).get(language, '')
    changes = group.get('changes', [])
    terms = ' '.join([version or 'unreleased 开发中', *group.get('related', []), *group.get('headers', []), plain(summary),
                      *[plain(change[language]) for change in changes],
                      *[plain(m[field][language]) for m in migrations for field in ('title', 'impact', 'action')],
                      *[m['before'] + ' ' + m['after'] for m in migrations]])
    badge = ('<span class="release-breaking">' + ('需要迁移' if language == 'zh' else 'Migration required')
             + '</span>') if migrations else ''
    change_list = '<ul class="release-changes">' + ''.join(f'<li>{prose(c[language])}</li>' for c in changes) + '</ul>' if changes else ''
    lead = f'<p>{prose(summary)}</p>' if summary else ''
    return f'''<section class="release-scope" data-release-scope="{group['scope']}" data-breaking="{str(bool(migrations)).lower()}" data-release-search="{escape(terms.lower())}">
      <header class="release-scope-heading"><h3>{SCOPE_LABELS[group['scope']][language]}</h3>{badge}</header>
      {lead}{change_list}{''.join(migration_html(m, language) for m in migrations)}
    </section>'''


def release_entry(item, language, page, latest):
    zh = language == 'zh'
    version = item.get('version')
    anchor = 'v' + version if version else 'unreleased'
    status = item.get('status', 'unreleased')
    # Ordinary published releases need no label; the date already says so.
    labels = {'candidate': ('Release candidate', '候选版本')}
    label = ('最新版本' if zh else 'Latest') if version == latest else (labels[status][zh] if status in labels else '')
    timestamp = item.get('published_at')
    date = f'<time datetime="{timestamp}">{timestamp[:10]}</time>' if timestamp else ''
    if status == 'archive':
        date = '<span class="release-date">' + ('未记录发布日期' if zh else 'Date not recorded') + '</span>'
    heading = version or ('开发中' if zh else 'Unreleased')
    groups = item.get('groups', [])
    intro = ''
    if not version:
        summary = (f'自 {latest} 发布以来已合入源码的改动：' if zh else f'Merged into source since {latest}:') if groups else (
            f'{latest} 发布后暂无未发布的公开改动。' if zh else f'No unreleased public changes since {latest}.')
        intro = f'<p>{prose(summary)}</p>'
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
    state = f'<span class="release-state">{label}</span>' if label else ''
    content = ''.join(scope_group(group, language, version) for scope in SCOPES for group in groups if group['scope'] == scope)
    return f'''<article class="release-entry" id="{anchor}" data-release-entry data-version="{version or 'unreleased'}">
      <header class="release-heading"><h2><a href="#{anchor}">{heading}</a></h2>{state}{date}</header>
      {intro}{content}
      <nav class="release-links" aria-label="{heading} {'相关链接' if zh else 'links'}">{' '.join(links)}</nav>
    </article>'''


def history_body(data, language, page):
    zh = language == 'zh'
    options = ''.join(f'<option value="{v["version"]}">{v["version"]}</option>' for v in data['versions'])
    filters = f'''<form class="release-filters" data-release-filters hidden role="search" aria-label="{'筛选版本记录' if zh else 'Filter releases'}">
      <fieldset class="release-audience"><legend>{'查看范围' if zh else 'Show updates for'}</legend>
        <div class="release-scope-options">
          <label><input type="radio" name="scope" value="all" data-release-scope-filter checked><span>{'全部' if zh else 'All'}</span></label>
          <label><input type="radio" name="scope" value="uilib" data-release-scope-filter><span>{'组件库' if zh else 'UILib'}</span></label>
          <label><input type="radio" name="scope" value="gallery" data-release-scope-filter><span>Gallery</span></label>
        </div>
      </fieldset>
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
        description = '按组件库与 Gallery 分别查看更新和迁移要求。' if zh else 'Updates and migration requirements for UILib and Gallery, grouped by version.'
        body = history_body(data, language, page)
        toc = [('unreleased', '开发中' if zh else 'Unreleased')]
        toc += [('v' + v['version'], v['version']) for v in data['versions']]
        outputs[page] = render_shell(context, documents, page, language, title, description, body, toc)
        outputs[page_path('roadmap', language)] = roadmap_redirect(language)
    return outputs
