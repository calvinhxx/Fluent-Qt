"""Render version history inside the existing documentation shell."""

import html
import re
from urllib.parse import urlencode
from docs_i18n import translate_release_text
from documentation_navigation import relative
from documentation_guides import guide_ui
from generate_localized_site import BASE_URL
from release_history import load_history, page_path, REPOSITORY, SCOPES
from site_locales import LOCALE_KEYS, locale_by_key


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
            ui = guide_ui(language)
            text = ui['unreleasedNotice'].format(latest=data['latest'])
            label = ui['viewReleaseNotes']
            return f'<p class="document-version-note">{escape(text)} <a href="{escape(relative(target, page))}">{escape(label)}</a></p>'
    return ''


def migration_html(migration, language, ui=None):
    ui = ui or guide_ui(language)
    commit = migration.get('commit')
    evidence = f'<p class="migration-commit">{escape(ui["introducedIn"])} <a href="{REPOSITORY}/commit/{commit}"><code>{commit[:7]}</code></a></p>' if commit else ''
    return f'''<section class="migration-note" id="{migration['id']}">
      <h4><a href="#{migration['id']}">{prose(translate_release_text(migration['title'], language))}</a></h4>
      <p>{prose(translate_release_text(migration['impact'], language))}</p>
      <p>{prose(translate_release_text(migration['action'], language))}</p>
      <div class="migration-comparison">
        <div><h5>{escape(ui['before'])}</h5><pre tabindex="0"><code>{escape(migration['before'])}</code></pre></div>
        <div><h5>{escape(ui['after'])}</h5><pre tabindex="0"><code>{escape(migration['after'])}</code></pre></div>
      </div>{evidence}</section>'''


def scope_group(group, language, version, ui=None):
    ui = ui or guide_ui(language)
    migrations = group.get('migrations', [])
    summary_dict = group.get('summary', {})
    summary = translate_release_text(summary_dict, language) if summary_dict else ''
    changes = group.get('changes', [])
    terms = [version or f'unreleased 开发中 {ui["unreleased"]}', *group.get('related', []), *group.get('headers', [])]
    if summary_dict:
        terms.extend([plain(summary), plain(summary_dict.get('en', '')), plain(summary_dict.get('zh', ''))])
    for change in changes:
        terms.extend([plain(translate_release_text(change, language)), plain(change.get('en', '')), plain(change.get('zh', ''))])
    for migration in migrations:
        for field in ('title', 'impact', 'action'):
            terms.extend([plain(translate_release_text(migration[field], language)), plain(migration[field].get('en', '')), plain(migration[field].get('zh', ''))])
        terms.append(migration['before'] + ' ' + migration['after'])
    badge = f'<span class="release-breaking">{escape(ui["migrationRequired"])}</span>' if migrations else ''
    change_list = '<ul class="release-changes">' + ''.join(f'<li>{prose(translate_release_text(c, language))}</li>' for c in changes) + '</ul>' if changes else ''
    lead = f'<p>{prose(summary)}</p>' if summary else ''
    return f'''<section class="release-scope" data-release-scope="{group['scope']}" data-breaking="{str(bool(migrations)).lower()}" data-release-search="{escape(' '.join(dict.fromkeys(terms)).lower())}">
      <header class="release-scope-heading"><h3>{escape(ui['scopes'][group['scope']])}</h3>{badge}</header>
      {lead}{change_list}{''.join(migration_html(m, language, ui=ui) for m in migrations)}
    </section>'''


def release_entry(item, language, page, latest, ui=None):
    ui = ui or guide_ui(language)
    version = item.get('version')
    anchor = 'v' + version if version else 'unreleased'
    status = item.get('status', 'unreleased')
    # Ordinary published releases need no label; the date already says so.
    label = ui['latest'] if version == latest else (ui['releaseCandidate'] if status == 'candidate' else '')
    timestamp = item.get('published_at')
    date = f'<time datetime="{timestamp}">{timestamp[:10]}</time>' if timestamp else ''
    if status == 'archive':
        date = f'<span class="release-date">{escape(ui["dateNotRecorded"])}</span>'
    heading = version or ui['unreleased']
    groups = item.get('groups', [])
    intro = ''
    if not version:
        summary = ui['unreleasedMerged'].format(latest=latest) if groups else ui['unreleasedNone'].format(latest=latest)
        intro = f'<p>{prose(summary)}</p>'
    links = []
    if version:
        if timestamp:
            links.append(f'<a href="{REPOSITORY}/releases/tag/v{version}">{escape(ui["downloads"])}</a>')
        links.append(f'<a href="{REPOSITORY}/blob/main/docs/releases/v{version}.md"'
                     + (' hreflang="en">' if language != 'en' else '>') + escape(ui['releaseNotesEn']) + '</a>')
        previous = item.get('previous')
        if previous:
            links.append(f'<a href="{REPOSITORY}/compare/v{previous}...v{version}">{escape(ui["compareWith"].format(previous=previous))}</a>')
    else:
        links.append(f'<a href="{REPOSITORY}/compare/v{latest}...main">{escape(ui["commitsSince"].format(latest=latest))}</a>')
    state = f'<span class="release-state">{escape(label)}</span>' if label else ''
    content = ''.join(scope_group(group, language, version, ui=ui) for scope in SCOPES for group in groups if group['scope'] == scope)
    return f'''<article class="release-entry" id="{anchor}" data-release-entry data-version="{version or 'unreleased'}">
      <header class="release-heading"><h2><a href="#{anchor}">{escape(heading)}</a></h2>{state}{date}</header>
      {intro}{content}
      <nav class="release-links" aria-label="{escape(ui['linksAria'].format(heading=heading))}">{' '.join(links)}</nav>
    </article>'''


def history_body(data, language, page):
    ui = guide_ui(language)
    options = ''.join(f'<option value="{v["version"]}">{v["version"]}</option>' for v in data['versions'])
    filters = f'''<form class="release-filters" data-release-filters hidden role="search" aria-label="{escape(ui['filterReleases'])}">
      <fieldset class="release-audience"><legend>{escape(ui['showUpdatesFor'])}</legend>
        <div class="release-scope-options">
          <label><input type="radio" name="scope" value="all" data-release-scope-filter checked><span>{escape(ui['scopeAll'])}</span></label>
          <label><input type="radio" name="scope" value="uilib" data-release-scope-filter><span>{escape(ui['scopeUilib'])}</span></label>
          <label><input type="radio" name="scope" value="gallery" data-release-scope-filter><span>{escape(ui['scopeGallery'])}</span></label>
        </div>
      </fieldset>
      <label>{escape(ui['searchLabel'])}<input type="search" name="change" data-release-query placeholder="{escape(ui['searchPlaceholder'])}"></label>
      <label>{escape(ui['upgradingFrom'])}<select name="from" data-release-from><option value="">{escape(ui['anyVersion'])}</option>{options}</select></label>
      <label class="release-checkbox"><input type="checkbox" name="breaking" data-release-breaking>{escape(ui['onlyMigration'])}</label>
      <button type="reset">{escape(ui['clearFilters'])}</button>
      <p class="release-result" data-release-result role="status" aria-live="polite" data-count-template="{escape(ui['releaseCountTemplate'])}" data-count-single-template="{escape(ui['releaseCountSingleTemplate'])}" data-count-after-template="{escape(ui['releaseCountAfterTemplate'])}" data-count-single-after-template="{escape(ui['releaseCountSingleAfterTemplate'])}"></p>
    </form>
    <p class="release-empty" data-release-empty hidden>{escape(ui['releaseEmpty'])}</p>'''
    entries = [release_entry(data['unreleased'], language, page, data['latest'], ui=ui)]
    for index, item in enumerate(data['versions']):
        version = dict(item)
        if index + 1 < len(data['versions']):
            version['previous'] = data['versions'][index + 1]['version']
        entries.append(release_entry(version, language, page, data['latest'], ui=ui))
    return filters + ''.join(entries)


def roadmap_redirect(language):
    """Keep published roadmap URLs usable without a second project-updates page."""
    ui = guide_ui(language)
    target = page_path('releases', language)
    link = relative(target, page_path('roadmap', language))
    title = ui['releasesAndMigrations']
    return f'''<!doctype html>
<html lang="{locale_by_key(language).html_lang}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="0; url={link}">
  <link rel="canonical" href="{BASE_URL}{target.removesuffix('index.html')}">
  <title>{escape(title)} · Fluent-Qt</title>
</head>
<body><p><a href="{link}">{escape(title)}</a></p></body>
</html>
'''


def generated_outputs(documents, catalog, render_shell):
    data = load_history()
    outputs = {}
    for language in LOCALE_KEYS:
        ui = guide_ui(language)
        page = page_path('releases', language)
        context = {'id': 'releases', 'catalog': catalog, 'component': {'category_id': 'project'}}
        title = ui['releasesAndMigrations']
        description = ui['releasesDescription']
        body = history_body(data, language, page)
        toc = [('unreleased', ui['unreleased'])]
        toc += [('v' + v['version'], v['version']) for v in data['versions']]
        outputs[page] = render_shell(context, documents, page, language, title, description, body, toc)
        outputs[page_path('roadmap', language)] = roadmap_redirect(language)
    return outputs
