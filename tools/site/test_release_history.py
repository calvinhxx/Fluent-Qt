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
        self.data = json.loads((self.root / history.SOURCE).read_text())

    def write(self):
        (self.root / history.SOURCE).write_text(json.dumps(self.data))
        history.load_history.cache_clear()

    def test_each_release_has_a_reviewed_translation_and_real_publication_state(self):
        data = history.load_history(self.root)
        self.assertEqual(next(v for v in data['versions'] if v['version'] == data['latest'])['status'], 'published')
        self.assertEqual(data['unreleased']['after'], data['latest'])
        self.assertTrue(all(item['summary']['zh'] for item in data['versions']))
        self.assertTrue(all(not item['published_at'] for item in data['versions'] if item['status'] == 'archive'))

    def test_a_new_note_or_stale_translation_blocks_generation(self):
        note = self.root / 'docs/releases/v9.0.0.md'
        note.write_text('# A new release\n\nA new feature.\n')
        with self.assertRaisesRegex(ValueError, 'each release note'):
            history.load_history(self.root)
        note.unlink()
        note = self.root / 'docs/releases/v1.10.3.md'
        note.write_text(note.read_text() + '\nChanged behavior.\n')
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
        self.data = json.loads((history.ROOT / history.SOURCE).read_text())
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
        original = note.read_text()
        record = next(v for v in self.data['versions'] if v['version'] == '1.10.2')
        for heading, breaking in (
            ('Non-breaking changes', False), ('Nonbreaking updates', False),
            ('Changes without breaking APIs', False), ('Breaking changes', True),
            ('Breaking migration from 1.10', True), ('BREAKING CHANGES', True),
        ):
            with self.subTest(heading=heading):
                note.write_text(original + f'\n## {heading}\n\nA change.\n')
                record['source_sha256'] = history.note_digest(note.read_text())
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
        self.assertIn('尚未确定发布版本', body)
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


if __name__ == '__main__':
    unittest.main()
