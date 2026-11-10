"""Contract checks for bilingual history, migration discoverability and release status."""

import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

import release_history as history
import release_pages


class ReleaseHistoryTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in ('docs/releases', 'tools/site'):
            (self.root / directory).mkdir(parents=True)
        for source in (history.ROOT / 'docs/releases').glob('v*.md'):
            shutil.copyfile(source, self.root / 'docs/releases' / source.name)
        shutil.copyfile(history.ROOT / history.SOURCE, self.root / history.SOURCE)
        self.data = json.loads((self.root / history.SOURCE).read_text(encoding='utf-8'))

    def write(self):
        (self.root / history.SOURCE).write_text(json.dumps(self.data), encoding='utf-8')
        history.load_history.cache_clear()

    def test_each_release_has_a_reviewed_translation_and_real_publication_state(self):
        data = history.load_history(self.root)
        self.assertEqual(next(v for v in data['versions'] if v['version'] == data['latest'])['status'], 'published')
        self.assertEqual(data['unreleased']['after'], data['latest'])
        self.assertTrue(all(item['summary']['zh'] for item in data['versions']))
        self.assertTrue(all(not item['published_at'] for item in data['versions'] if item['status'] == 'archive'))

    def test_a_new_note_or_stale_translation_blocks_generation(self):
        note = self.root / 'docs/releases/v9.0.0.md'
        note.write_text('# A new release\n\nA new feature.\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'each release note'):
            history.load_history(self.root)
        note.unlink()
        note = self.root / 'docs/releases/v1.10.3.md'
        note.write_text(note.read_text(encoding='utf-8') + '\nChanged behavior.\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Review the bilingual summary'):
            history.load_history(self.root)

    def test_generated_navigation_does_not_invalidate_a_translation(self):
        source = '# Release\n\nBody.\n'
        generated = source + '\n<!-- docs-nav:bottom:start -->\nChanged links\n<!-- docs-nav:bottom:end -->\n'
        self.assertEqual(history.note_digest(source), history.note_digest(generated))

    def test_missing_translation_and_migration_are_rejected(self):
        self.data['versions'][0]['summary']['zh'] = ''
        self.write()
        with self.assertRaisesRegex(ValueError, 'bilingual'):
            history.load_history(self.root)
        self.data = json.loads((history.ROOT / history.SOURCE).read_text(encoding='utf-8'))
        next(v for v in self.data['versions'] if v['version'] == '1.7.0')['migrations'] = []
        self.write()
        with self.assertRaisesRegex(ValueError, 'migration record'):
            history.load_history(self.root)

    def test_unreleased_baseline_cannot_silently_drift(self):
        self.data['unreleased']['after'] = '1.9.0'
        self.write()
        with self.assertRaisesRegex(ValueError, 'baseline'):
            history.load_history(self.root)

    def test_only_explicit_breaking_headings_require_migrations(self):
        note = self.root / 'docs/releases/v1.10.2.md'
        original = note.read_text(encoding='utf-8')
        record = next(v for v in self.data['versions'] if v['version'] == '1.10.2')
        for heading, breaking in (
            ('Non-breaking changes', False), ('Nonbreaking updates', False),
            ('Changes without breaking APIs', False), ('Breaking changes', True),
            ('Breaking migration from 1.10', True), ('BREAKING CHANGES', True),
        ):
            with self.subTest(heading=heading):
                note.write_text(original + f'\n## {heading}\n\nA change.\n', encoding='utf-8')
                record['source_sha256'] = history.note_digest(note.read_text(encoding='utf-8'))
                self.write()
                if breaking:
                    with self.assertRaisesRegex(ValueError, 'needs a bilingual migration record'):
                        history.load_history(self.root)
                else:
                    history.load_history(self.root)

    def test_pending_api_notice_tracks_publication_and_header_associations(self):
        data = copy.deepcopy(history.load_history())
        data['unreleased']['related'] = ['font-icon']
        data['unreleased']['headers'] = []
        data['versions'] = [item for item in data['versions'] if item['status'] != 'candidate']
        page = 'api/font-icon/index.html'
        source = 'src/components/foundation/FontIcon.h'
        candidate = {'version': '9.0.0', 'status': 'candidate', 'headers': [source]}
        with patch.object(release_pages, 'load_history', return_value=data):
            notice = release_pages.document_notice('font-icon', None, page, 'en')
            self.assertIn('unreleased changes', notice)
            self.assertIn('releases/?change=font-icon#unreleased', notice)
            self.assertEqual(release_pages.document_notice('checkbox', None, 'api/checkbox/index.html', 'en'), '')
            data['unreleased']['related'] = []
            data['versions'].insert(0, candidate)
            self.assertIn('#v9.0.0', release_pages.document_notice('font-icon', source, page, 'en'))
            candidate['status'] = 'published'
            self.assertEqual(release_pages.document_notice('font-icon', source, page, 'en'), '')

    def test_migrations_are_static_safe_and_searchable(self):
        data = history.load_history()
        body = release_pages.history_body(data, 'zh', 'zh-CN/api/releases/index.html')
        self.assertIn('id="v1.7.0"', body)
        self.assertIn('id="particles-module"', body)
        self.assertIn(f'自 {data["latest"]} 发布以来已合入源码的改动：', body)
        empty_unreleased = copy.deepcopy(data)
        empty_unreleased['unreleased']['changes'] = []
        empty_unreleased['unreleased']['migrations'] = []
        empty_body = release_pages.history_body(empty_unreleased, 'en', 'api/releases/index.html')
        self.assertIn(f'No unreleased public changes since {data["latest"]}.', empty_body)
        self.assertIn('&lt;FluentQt/Particles.h&gt;', body)
        self.assertIn('2026-10-27', body)
        self.assertIn('setFontFamily', body.split('id="unreleased"', 1)[1].split('id="v1.10.3"', 1)[0])
        particle_entry = next(item for item in [data['unreleased'], *data['versions']]
                              if any(m['id'] == 'particles-module' for m in item.get('migrations', [])))
        anchor = 'v' + particle_entry['version'] if 'version' in particle_entry else 'unreleased'
        self.assertIn('particle-backdrop', body.split(f'id="{anchor}"', 1)[1].split('data-release-search="', 1)[1].split('"', 1)[0])
        malicious = copy.deepcopy(next(m for m in particle_entry['migrations'] if m['id'] == 'particles-module'))
        malicious['impact']['en'] = '<script>alert(1)</script> `<img src=x>`'
        rendered = release_pages.migration_html(malicious, 'en')
        self.assertNotIn('<script>', rendered)
        self.assertIn('<code>&lt;img src=x&gt;</code>', rendered)
        self.assertNotIn('`', body.split('data-release-search="', 2)[1].split('"', 1)[0])

    def test_unknown_component_links_are_not_silently_lost(self):
        with self.assertRaisesRegex(ValueError, 'unknown documentation'):
            history.validate_related_documents({'unreleased': {'related': ['retired-widget']}, 'versions': []}, ['button'])
        history.validate_related_documents({'unreleased': {}, 'versions': [{'related': ['retired-widget']}],
                                            'retired_document_ids': ['retired-widget']}, ['button'])


    def test_api_identifiers_use_ascii_boundaries_next_to_chinese(self):
        for identifier in ('LineEdit', 'FLUENT_QT_BUILD_GALLERY', 'FontIcon::setFontFamily',
                           'qt6-base-private-dev', '<FluentQt/Particles.h>'):
            for surrounding in ('新增{}。', '{}支持此功能。', '新增{}支持此功能。'):
                for quoted in (False, True):
                    with self.subTest(identifier=identifier, surrounding=surrounding, quoted=quoted):
                        token = f'`{identifier}`' if quoted else identifier
                        self.data['unreleased']['changes'] = [{'en': 'Adds a feature.', 'zh': surrounding.format(token)}]
                        self.write()
                        if quoted:
                            history.load_history(self.root)
                        else:
                            with self.assertRaisesRegex(ValueError, 'Wrap identifier'):
                                history.load_history(self.root)
        # A known name embedded in a larger ASCII identifier is not that API.
        for identifier in ('MyLineEdit', 'LineEditor', 'LineEdit2', '_LineEdit'):
            with self.subTest(larger_identifier=identifier):
                history.validate_prose_field({'en': f'Updates {identifier}.', 'zh': '更新示例。'}, 'change', 'example')

    def test_both_semicolon_forms_are_checked_in_both_languages(self):
        for lang in history.LANGUAGES:
            for mark in (';', '；'):
                with self.subTest(lang=lang, mark=mark):
                    value = {'en': 'Fixes `ComboBox` and `LineEdit`.', 'zh': '修复 `ComboBox` 与 `LineEdit`。'}
                    value[lang] = (f'Fixes `ComboBox`{mark} fixes `LineEdit`.' if lang == 'en'
                                   else f'修复 `ComboBox`{mark}修复 `LineEdit`。')
                    self.data['unreleased']['changes'] = [value]
                    self.write()
                    with self.assertRaisesRegex(ValueError, 'semicolon-splicing'):
                        history.load_history(self.root)
                    history.validate_prose_field(value, 'migration.action', 'migration')
                    value[lang] = (f'Handles the literal `{mark}` in text.' if lang == 'en'
                                   else f'处理文本中的 `{mark}` 字符。')
                    self.write()
                    history.load_history(self.root)

    def test_event_delivery_verbs_are_allowed_but_promotional_copy_is_rejected(self):
        for english in ('`TextEdit` now delivers key events to its completion popup.',
                        'Deliver queued signals on the UI thread.',
                        'Fixes a crash while delivering queued events.'):
            with self.subTest(english=english):
                self.data['unreleased']['changes'] = [{'en': english, 'zh': '修复事件传递。'}]
                self.write()
                history.load_history(self.root)
        for english in ('`TextEdit` delivers a seamless experience.',
                        'Delivers comprehensive improvements.',
                        'Improves editing while preserving caller ownership.'):
            with self.subTest(english=english):
                self.data['unreleased']['changes'] = [{'en': english, 'zh': '更新编辑功能。'}]
                self.write()
                with self.assertRaisesRegex(ValueError, 'AI-slop or internal-plumbing phrasing'):
                    history.load_history(self.root)

    def test_feedback_release_note_requires_the_explicit_issue_action(self):
        data = history.load_history(self.root)
        entry = next(item for item in [data['unreleased'], *data['versions']]
                     if 'src/utils/DiagnosticSession.h' in item.get('headers', []))
        for lang, action, old_claim in (
            ('en', 'Choose Open GitHub issue in the notification', 'and opens GitHub Issues'),
            ('zh', '在提示中点击“Open GitHub issue”后才打开 GitHub', '复制到剪贴板并打开 GitHub'),
        ):
            with self.subTest(lang=lang):
                rendered = release_pages.release_entry(entry, lang, history.page_path('releases', lang), data['latest'])
                self.assertIn(action, rendered)
                self.assertNotIn(old_claim, rendered)

    def test_copy_rules_reject_ai_slop_unquoted_apis_and_bloated_entries(self):
        baseline = json.loads((history.ROOT / history.SOURCE).read_text(encoding='utf-8'))

        def assert_rejected(mutate, pattern):
            self.data = copy.deepcopy(baseline)
            mutate(self.data)
            self.write()
            with self.assertRaisesRegex(ValueError, pattern):
                history.load_history(self.root)

        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({'en': 'LineEdit adds suffixes.', 'zh': '`LineEdit` 新增后缀。'}),
            'Wrap identifier `LineEdit` in backticks',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({'en': 'Use FLUENT_QT_BUILD_GALLERY.', 'zh': '使用 `FLUENT_QT_BUILD_GALLERY`。'}),
            'Wrap identifier `FLUENT_QT_BUILD_GALLERY` in backticks',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({'en': 'Fixes `ComboBox`; fixes `LineEdit`.', 'zh': '修复 `ComboBox` 与 `LineEdit`。'}),
            'semicolon-splicing',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({'en': 'Fixes `ComboBox` and `LineEdit`.', 'zh': '修复 `ComboBox`；修复 `LineEdit`。'}),
            'semicolon-splicing',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({
                'en': '`LineEdit` adds suffixes without changing caller ownership.',
                'zh': '`LineEdit` 新增后缀。',
            }),
            'AI-slop or internal-plumbing phrasing',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({
                'en': '`LineEdit` adds suffixes.',
                'zh': '`LineEdit` 新增后缀，并保留调用方的所有权约定。',
            }),
            'AI-slop or internal-plumbing phrasing',
        )
        assert_rejected(
            lambda d: d['versions'][0].__setitem__(
                'changes',
                [{'en': f'Item {index}.', 'zh': f'第 {index} 项。'} for index in range(history.MAX_CHANGES_PER_ENTRY + 1)],
            ),
            'change bullets',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({'en': 'Unclosed `LineEdit span.', 'zh': '`LineEdit` 正常。'}),
            'Unbalanced or empty inline code span',
        )
        assert_rejected(
            lambda d: d['versions'][0]['changes'].append({'en': 'word ' * 50, 'zh': '简短说明。'}),
            'tighten the prose',
        )


if __name__ == '__main__':
    unittest.main()
