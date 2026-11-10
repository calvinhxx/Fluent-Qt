"""Validated bilingual release summaries and migration records for the website.

English release notes remain canonical. Their normalized digests make outdated
translations fail the same source-only gate as the API documentation.
"""

import hashlib
import argparse
import json
from pathlib import Path
import re
from datetime import datetime
from functools import lru_cache

ROOT = Path(__file__).resolve().parents[2]
SOURCE = 'tools/site/release-history.json'
REPOSITORY = 'https://github.com/calvinhxx/Fluent-Qt'
LANGUAGES = ('en', 'zh')
MAX_CHANGES_PER_ENTRY = 6
FIELD_BUDGETS = {
    'summary': {'en': 250, 'zh': 125},
    'change': {'en': 185, 'zh': 95},
    'migration.title': {'en': 70, 'zh': 40},
    'migration.impact': {'en': 220, 'zh': 115},
    'migration.action': {'en': 220, 'zh': 115},
}
UNQUOTED_API_TYPES = (
    'Accordion', 'AnchorLayout', 'AreaChart', 'AutoSuggestBox', 'BarChart',
    'CalendarDatePicker', 'CalendarView', 'ChartView', 'CheckBox', 'CoachMark',
    'CodeBlock', 'ColorPicker', 'ComboBox', 'CommandBar', 'CommandBarFlyout',
    'CommandPalette', 'CompoundButton', 'ContentDialog', 'DataGrid', 'DataGridToolBar',
    'DatePicker', 'DateTimeRangePicker', 'DiagnosticSession', 'Divider', 'DonutChart',
    'DrawerView', 'DropDownButton', 'EditingCommandRouter', 'Expander', 'FileDropZone',
    'FileListView', 'FlipView', 'FlowView', 'FluentElement', 'FluentMenu', 'FontIcon',
    'FormController', 'GridView', 'HorizontalBarChart', 'HyperlinkButton', 'ImageViewer',
    'InfoBadge', 'InfoBar', 'LineChart', 'LineEdit', 'ListDetailView', 'ListView',
    'MotionPolicy', 'MultiSelectComboBox', 'NavigationView', 'NumberBox',
    'ParticleBackdrop', 'ParticlesSession', 'PasswordBox', 'PieChart', 'PipsPager',
    'ProgressBar', 'ProgressRing', 'PropertyPanel', 'QmlSession', 'QmlView',
    'RadioButton', 'RangeSlider', 'RatingControl', 'RepeatButton', 'ScatterChart',
    'ScrollBar', 'ScrollView', 'SegmentedControl', 'SelectorBar', 'Sparkline',
    'SpatialItem', 'SpatialSession', 'SpatialView', 'SplashScreen', 'SplitButton',
    'SplitView', 'StackView', 'Stepper', 'TabView', 'TagInput', 'TeachingTip',
    'TextEdit', 'ThemeRegistry', 'TimePicker', 'Timeline', 'TitleBar',
    'ToggleSplitButton', 'ToggleSwitch', 'Transfer', 'TreeSelect', 'TreeView',
    'UserTheme', 'WidgetOwnership',
)
UNQUOTED_API_RE = re.compile(
    # API names use ASCII boundaries even when adjacent prose is Chinese.
    r'\b(?:'
    + '|'.join(UNQUOTED_API_TYPES)
    + r'|FLUENT_QT_[A-Z0-9_]+|qt(?:base5|6-base)-private-dev|[A-Za-z_]\w*::[A-Za-z_]\w*)\b'
    r'|<FluentQt/[^>]+>',
    re.ASCII,
)
BANNED_EN_RE = re.compile(
    r'\b(?:'
    r'while preserving|without changing|existing (?:validation |check )?coverage|'
    r'grouped test executables|compiler parallelism|in order to|'
    r'seamless(?:ly)?|comprehensive(?:ly)?|streamlined?|robust(?:ness)?|'
    r'leverages?|leveraging|empowers?|empowering'
    r')\b',
    re.I,
)
BANNED_ZH_RE = re.compile(
    r'保留调用方的|保留现有检查|有容量上限的|支持保持|独立于编辑值的|成对性能测量|'
    r'全方位|无缝|赋能|旨在|致力于|进一步提升|显著提升|全面提升'
)


def validate_prose_field(value, kind, location):
    bilingual(value, location)
    budgets = FIELD_BUDGETS[kind]
    for lang in LANGUAGES:
        text = value[lang]
        if '\n' in text:
            raise ValueError(f'Keep {location} ({lang}) on a single line')
        if text.count('`') % 2 != 0 or '``' in text:
            raise ValueError(f'Unbalanced or empty inline code span in {location} ({lang})')
        prose_len = len(re.sub(r'`[^`]+`', 'code' if lang == 'en' else '名', text))
        if prose_len > budgets[lang]:
            raise ValueError(
                f'{location} ({lang}) prose length is {prose_len} (limit {budgets[lang]}); tighten the prose'
            )
        outside_code = re.sub(r'`[^`]+`', ' ', text)
        unquoted = UNQUOTED_API_RE.search(outside_code)
        if unquoted:
            raise ValueError(
                f'Wrap identifier `{unquoted.group(0)}` in backticks in {location} ({lang})'
            )
        if kind == 'change' and any(mark in outside_code for mark in (';', '；')):
            raise ValueError(
                f'Keep one topic per bullet instead of semicolon-splicing in {location} ({lang})'
            )
        banned = (BANNED_EN_RE if lang == 'en' else BANNED_ZH_RE).search(outside_code)
        if banned:
            raise ValueError(
                f'Avoid AI-slop or internal-plumbing phrasing "{banned.group(0)}" in {location} ({lang})'
            )


def normalized_note(text):
    text = re.sub(r'<!-- docs-nav:(?:top|bottom):start -->.*?<!-- docs-nav:(?:top|bottom):end -->', '', text, flags=re.S)
    return '\n'.join(line.rstrip() for line in text.strip().splitlines()) + '\n'


def note_digest(text):
    return hashlib.sha256(normalized_note(text).encode()).hexdigest()


def version_key(version):
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError(f'Invalid release version: {version}')
    return tuple(map(int, version.split('.')))


def bilingual(value, location):
    if not isinstance(value, dict) or any(not isinstance(value.get(lang), str) or not value[lang].strip() for lang in LANGUAGES):
        raise ValueError(f'Missing bilingual text: {location}')


@lru_cache(maxsize=4)
def load_history(root=ROOT):
    data = json.loads((root / SOURCE).read_text(encoding='utf-8'))
    if data.get('schema_version') != 1:
        raise ValueError('Unsupported release history schema')
    versions = data['versions']
    names = [item['version'] for item in versions]
    expected = {p.stem[1:] for p in (root / 'docs/releases').glob('v[0-9]*.md') if re.fullmatch(r'v\d+\.\d+\.\d+', p.stem)}
    if len(names) != len(set(names)) or set(names) != expected:
        raise ValueError('Release history must cover each release note exactly once; add its Chinese summary and metadata')
    for item in versions:
        version_key(item['version'])
        path = root / f'docs/releases/v{item["version"]}.md'
        note = path.read_text(encoding='utf-8')
        if item.get('source_sha256') != note_digest(note):
            raise ValueError(f'Review the bilingual summary after changing {path.relative_to(root)}')
        if re.search(r'^##[ \t]+breaking\b', note, re.M | re.I) and not item.get('migrations'):
            raise ValueError(f'Breaking release {item["version"]} needs a bilingual migration record')
        validate_prose_field(item['summary'], 'summary', f'{item["version"]} summary')
        if item['status'] not in ('published', 'archive', 'candidate'):
            raise ValueError(f'Invalid release status: {item["version"]}')
        if item['status'] == 'published':
            datetime.fromisoformat(item['published_at'].replace('Z', '+00:00'))
        elif item.get('published_at'):
            raise ValueError('Only published releases have publication timestamps')
    versions.sort(key=lambda item: version_key(item['version']), reverse=True)
    data['latest'] = next(item['version'] for item in versions if item['status'] == 'published')
    if data['unreleased']['after'] != data['latest']:
        raise ValueError('Move shipped changes out of Unreleased and update its baseline')
    for item in [data['unreleased'], *versions]:
        label = item.get('version', 'unreleased')
        changes = item.get('changes', [])
        if len(changes) > MAX_CHANGES_PER_ENTRY:
            raise ValueError(f'{label} has {len(changes)} change bullets (limit {MAX_CHANGES_PER_ENTRY})')
        for index, change in enumerate(changes):
            validate_prose_field(change, 'change', f'{label} change {index}')
        for migration in item.get('migrations', []):
            for field in ('title', 'impact', 'action'):
                validate_prose_field(migration[field], f'migration.{field}', f'migration {migration["id"]} {field}')
            if not re.fullmatch(r'[a-z][a-z0-9-]*', migration['id']):
                raise ValueError('Migration anchors must be stable slugs')
            if not migration.get('before') or not migration.get('after'):
                raise ValueError('Migration records need before and after examples')
            if migration.get('commit') and not re.fullmatch(r'[0-9a-f]{40}', migration['commit']):
                raise ValueError('Migration commit references must use a full Git revision')
    ids = [m['id'] for item in [data['unreleased'], *versions] for m in item.get('migrations', [])]
    if len(ids) != len(set(ids)):
        raise ValueError('Duplicate migration anchor')
    return data


def page_path(kind, language='en'):
    return ('zh-CN/' if language == 'zh' else '') + f'api/{kind}/index.html'


def validate_related_documents(data, document_ids):
    allowed = set(document_ids) | set(data.get('retired_document_ids', []))
    unknown = {name for item in [data['unreleased'], *data['versions']] for name in item.get('related', [])} - allowed
    if unknown:
        raise ValueError(f'Release history links to unknown documentation: {sorted(unknown)}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh-digest', metavar='VERSION', help='Acknowledge one reviewed translation after editing its canonical note')
    args = parser.parse_args()
    if args.refresh_digest:
        path = ROOT / SOURCE
        data = json.loads(path.read_text(encoding='utf-8'))
        version_key(args.refresh_digest)
        entry = next((item for item in data['versions'] if item['version'] == args.refresh_digest), None)
        if entry is None:
            parser.error('Add the bilingual version record before acknowledging its source')
        bilingual(entry['summary'], args.refresh_digest)
        entry['source_sha256'] = note_digest((ROOT / f'docs/releases/v{args.refresh_digest}.md').read_text(encoding='utf-8'))
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    data = load_history()
    print(f'Verified bilingual history: {len(data["versions"])} versions; latest stable {data["latest"]}')


if __name__ == '__main__':
    main()
