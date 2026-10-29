#!/usr/bin/env python3
"""Check type discovery in the installed-header API index."""

import unittest
import copy
import html
import hashlib
import json
import os
import io
import re
import sys
from contextlib import redirect_stdout
import struct
import zlib
from html.parser import HTMLParser
import posixpath
from urllib.parse import urlsplit, unquote
from pathlib import Path
import tempfile
from unittest.mock import patch

from generate_api_reference import _declarations, _property_reference
from generate_api_reference import generate_reference, _encoded
import generate_api_reference
import component_docs
import component_inventory
import generate_component_previews as previews
import generate_localized_site
from api_sections import localized_description, reference_headers, reference_path
from public_api import resolve_type
from documentation_guides import guides, guide_path
from test_public_api import PublicApiTest


class PublicTypeDiscoveryTest(unittest.TestCase):
    def test_forward_declarations_are_independent_of_namespace_formatting(self):
        compact = "namespace fluent::basicinput { class Button; }\nclass LineEdit {};\n"
        expanded = "namespace fluent::basicinput {\nclass Button;\n}\nclass LineEdit {};\n"
        self.assertEqual(_declarations(compact), ["LineEdit"])
        self.assertEqual(_declarations(expanded), ["LineEdit"])

    def test_definitions_keep_exports_inheritance_and_multiline_bases(self):
        source = """
class FLUENTQT_EXPORT LineEdit final
    : public QLineEdit, public FluentElement
{
};
struct BackdropState {
};
class ScrollBar;
"""
        self.assertEqual(_declarations(source), ["LineEdit", "BackdropState"])

    def test_opaque_enums_are_not_reported_as_definitions(self):
        source = """
enum class Forward : int;
enum class BackdropEffect {
    Solid, Mica, Acrylic
};
enum Alignment { Left, Right };
"""
        self.assertEqual(_declarations(source), ["BackdropEffect", "Alignment"])


class PropertyReferenceTest(unittest.TestCase):
    def test_changed_public_header_refreshes_catalog_without_rendering_or_native_capture(self):
        root = generate_api_reference.ROOT
        (root / 'build').mkdir(exist_ok=True)
        header = root / 'src/components/basicinput/Button.h'
        read_text = Path.read_text
        marker = 'Automatically regenerated header summary.'

        def edited(path, *args, **kwargs):
            text = read_text(path, *args, **kwargs)
            return re.sub(r'(@brief[^\n]*)', r'\1 ' + marker, text, count=1) if path == header else text

        with tempfile.TemporaryDirectory(dir=root / 'build') as directory, redirect_stdout(io.StringIO()):
            output = Path(directory) / 'catalog.json'
            with patch.object(generate_api_reference, 'OUTPUT', output):
                with patch.object(sys, 'argv', ['generator', '--catalog-only']):
                    self.assertEqual(generate_api_reference.main(), 0)
                with patch.object(Path, 'read_text', edited), \
                     patch.object(component_docs, 'generated_outputs', side_effect=AssertionError('catalog-only renders no pages')), \
                     patch.object(previews, 'validated_manifest', side_effect=AssertionError('catalog generation needs no native captures')):
                    with patch.object(sys, 'argv', ['generator', '--check']):
                        self.assertEqual(generate_api_reference.main(), 1)
                    with patch.object(sys, 'argv', ['generator', '--catalog-only']):
                        self.assertEqual(generate_api_reference.main(), 0)
                    self.assertIn(marker, output.read_text())
                    with patch.object(sys, 'argv', ['generator', '--check']):
                        self.assertEqual(generate_api_reference.main(), 0)

    def test_uses_reader_comment_and_preserves_translations(self):
        source = '''
Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
Q_PROPERTY(Backend activeBackend READ activeBackend NOTIFY rendererChanged)
Q_PROPERTY(int undocumented READ undocumented)
/** @brief Scene zoom; default 1.
 * Applies to every surface.
 * zh_CN: 场景倍率，默认 1。
 * 作用于所有表面。 */
qreal zoom() const;
/** @brief Setter detail is not the property description. */
void setZoom(qreal value);
/** @brief Actual backend. zh_CN: 实际后端。 */
Backend activeBackend() const;
int undocumented() const;
'''
        properties = _property_reference(source)
        self.assertEqual([entry["name"] for entry in properties], ["zoom", "activeBackend"])
        self.assertEqual(properties[0]["description"], {
            "en": "Scene zoom; default 1. Applies to every surface.",
            "zh": "场景倍率，默认 1。 作用于所有表面。",
        })
        self.assertFalse(properties[0]["read_only"])
        self.assertTrue(properties[1]["read_only"])


class VisualDocumentationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = generate_reference()
        cls.documents = component_docs.collect(cls.catalog)

    def test_published_catalog_omits_generation_only_members(self):
        published = json.loads(_encoded(self.catalog))
        self.assertNotIn('types', published)
        self.assertTrue(self.catalog['types'])
        self.assertEqual(published['components'], self.catalog['components'])
        self.assertEqual(published['public_headers'], self.catalog['public_headers'])

    def test_committed_catalog_does_not_depend_on_captured_images(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(previews, 'MANIFEST', Path(directory) / 'missing.json'):
            placeholders = previews.available_previews()
        self.assertTrue(all(not image['sha256'] for image in placeholders['images']))
        catalog = generate_reference()
        component_docs.collect(catalog, placeholders)
        self.assertEqual(_encoded(catalog), _encoded(self.catalog))
        preview = {c['id']: c for c in catalog['components']}['button']['documentation']['preview']
        self.assertEqual(set(preview['light']), {'url'})

    def test_catalog_links_each_document_to_real_native_images(self):
        entries = {c['id']: c for c in self.catalog['components']}
        for name in ('button', 'checkbox'):
            doc = entries[name]['documentation']
            self.assertEqual(set(doc['urls']), {'en', 'zh'})
            self.assertTrue(doc['preview']['light']['url'].endswith('.png'))
            self.assertNotIn('control_images', doc['preview']['light']['url'])
            self.assertTrue(doc['description']['zh'])

    def test_navigation_icon_reuses_a_small_native_resolution_asset(self):
        data = component_docs.site_icon()
        self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack_from('>2I', data, 16), (64, 64))
        self.assertLess(len(data), 10_000)

    def test_every_catalog_component_has_bilingual_visual_documentation(self):
        documents = {doc['id']: doc for doc in self.documents}
        self.assertEqual(set(documents), {c['id'] for c in self.catalog['components']})
        outputs = component_docs.generated_outputs(self.documents)
        for component in self.catalog['components']:
            doc = documents[component['id']]
            self.assertTrue(doc['api'], component['id'])
            for preview in doc['previews']:
                self.assertEqual(set(preview['images']), {'light', 'dark'})
            for language in ('en', 'zh'):
                page = outputs[component_docs.detail_path(component['id'], language)]
                self.assertIn('data-component="' + component['id'] + '"', page)
                self.assertIn('data-kind="component"', page)
                self.assertNotIn('reference-kind', page)
            for example in doc['examples']:
                self.assertTrue(example['cpp'].strip())
                self.assertTrue(example['python'].strip())

    def test_document_pages_have_code_but_no_download_or_build_sections(self):
        outputs = component_docs.generated_outputs(self.documents)
        for doc in self.documents:
            page = outputs[component_docs.detail_path(doc['id'], 'en')]
            self.assertNotIn('class="run-instructions"', page)
            self.assertNotIn(' download', page)
            self.assertNotIn('class="imports"', page)
            for example in doc['examples']:
                folder = 'api/examples/' + example['sample'] + '/'
                self.assertIn(html.escape(example['cpp'].strip(), quote=True), page)
                self.assertIn(html.escape(example['python'].strip(), quote=True), page)
                self.assertNotIn(folder + 'example.cpp', outputs)
                self.assertNotIn(folder + 'example.py', outputs)
                self.assertNotIn(folder + 'CMakeLists.txt', outputs)

    def test_new_catalog_entries_require_editorial_coverage(self):
        catalog = component_inventory.catalog()
        catalog['components'].append({**catalog['components'][0], 'id': 'missing-guide'})
        with patch.object(component_inventory, 'catalog', return_value=catalog):
            with self.assertRaisesRegex(ValueError, 'translations differ'):
                component_inventory.config()

    def test_bilingual_static_pages_include_code_without_wasm_embedding(self):
        outputs = component_docs.generated_outputs(self.documents)
        for component in ('button', 'checkbox'):
            for language in ('en', 'zh'):
                page = outputs[component_docs.detail_path(component, language)]
                self.assertIn('role="tabpanel"', page)
                self.assertIn('PySide6', page)
                self.assertIn('<pre', page)
                self.assertNotIn('<iframe', page)
                self.assertNotIn('.wasm', page)
                self.assertIn('data-directory-search', page)
        self.assertIn('status = fluentqt.Label', outputs['api/checkbox/index.html'])
        self.assertIn('auto* status = new Label', outputs['api/checkbox/index.html'])
        self.assertFalse(any(path.startswith('api/examples/') for path in outputs))

    def test_prose_and_source_are_html_escaped(self):
        doc = copy.deepcopy(self.documents[0])
        doc['description']['en'] = '<script>untrusted()</script>'
        rendered = component_docs.render_document(doc, self.documents, 'en')
        self.assertNotIn('<script>untrusted()</script>', rendered)
        self.assertIn('&lt;script&gt;untrusted()', rendered)

    def test_document_navigation_keeps_catalog_coverage_without_an_index_page(self):
        outputs = component_docs.generated_outputs(self.documents)
        for path in ('zh-CN/api/button/index.html', 'zh-CN/api/checkbox/index.html'):
            self.assertIn('class="documentation-header"', outputs[path])
            self.assertIn('data-shell-link="home"', outputs[path])
            self.assertIn('data-shell-link="components"', outputs[path])
            self.assertNotIn('data-shell-link="version"', outputs[path])
            self.assertNotIn('data-shell-link="gallery"', outputs[path])
            self.assertNotIn('documentation-tools', outputs[path])
            self.assertNotIn('data-theme-toggle', outputs[path])
        chinese = outputs['zh-CN/api/button/index.html']
        self.assertIn('data-shell-link="home" href="../../"', chinese)
        self.assertIn('data-runtime-url="../../../gallery/"', chinese)
        self.assertIn('data-retry-live hidden', chinese)
        self.assertNotIn('data-start-live', chinese)
        self.assertNotIn('capture-label', chinese)
        self.assertNotIn('data-stop-live', chinese)
        self.assertNotIn('<iframe', chinese)
        self.assertEqual(chinese.count('data-kind="component"'), len(self.catalog['components']))
        self.assertEqual(chinese.count('data-kind="reference"'), len(reference_headers(self.catalog)))
        self.assertEqual(chinese.count('data-kind="guide"'), len(guides()))
        self.assertIn('fluent-qt-theme', chinese)
        self.assertNotIn('class="gallery-link"', chinese)
        self.assertIn('location.replace(target.href)', outputs['api/index.html'])
        self.assertIn('location.replace(target.href)', outputs['zh-CN/api/index.html'])
        self.assertNotIn('api-card', outputs['api/index.html'])
        self.assertNotIn('catalog.json', outputs['api/index.html'])

    def test_foundations_and_nonvisual_api_are_real_local_documents(self):
        outputs = component_docs.generated_outputs(self.documents)
        for language in ('en', 'zh'):
            for guide in guides():
                page = outputs[guide_path(guide['id'], language)]
                self.assertIn(html.escape(guide['name'][language]), page)
                self.assertIn('class="guide-section"', page)
                self.assertNotIn('data-runtime-url', page)
                self.assertNotIn('class="preview"', page)
            for reference in reference_headers(self.catalog):
                page = outputs[reference_path(reference['source'], language)]
                self.assertIn('data-api-member', page)
                self.assertIn('data-api-filter', page)
        button = outputs['zh-CN/api/button/index.html']
        for member in ('setIconGlyph', 'fluentStyleChanged', 'ButtonStyle', 'anchors()', 'setTheme('):
            self.assertIn(member, button)
        self.assertIn('qpushbutton.html', button)
        self.assertIn('setBorrowedEditor', outputs['zh-CN/api/field/index.html'])

    def test_reference_pages_lead_with_their_namesake_type(self):
        outputs = component_docs.generated_outputs(self.documents)
        for name, helper in (('fluent::charts::ChartModel', 'fluent::charts::ChartData'),
                             ('fluent::diagnostics::Inspector', 'fluent::diagnostics::InspectorOptions')):
            record, other = resolve_type(name), resolve_type(helper)
            for language in ('en', 'zh'):
                page = outputs[reference_path(record['source'], language)]
                heading = page.split('<header class="document-heading">', 1)[1].split('</header>', 1)[0]
                self.assertIn(html.escape(localized_description(record, language)), heading)
                self.assertNotIn(html.escape(localized_description(other, language)), heading)
                self.assertIn(html.escape(localized_description(other, language)), page)

    def test_generated_document_links_and_member_anchors_resolve(self):
        class Links(HTMLParser):
            def __init__(self, text):
                super().__init__(); self.ids = []; self.links = []; self.feed(text)

            def handle_starttag(self, tag, attrs):
                values = dict(attrs)
                if values.get('id'):
                    self.ids.append(values['id'])
                if tag == 'a' and values.get('href'):
                    self.links.append(values['href'])

        homepage = {locale.output.relative_to(generate_localized_site.SITE_ROOT).as_posix()
                    for locale in generate_localized_site.LOCALES}
        outputs = component_docs.generated_outputs(self.documents)
        parsed = {path: Links(text) for path, text in outputs.items() if path.endswith('.html')}
        for path, page in parsed.items():
            self.assertEqual(len(page.ids), len(set(page.ids)), 'Duplicate member anchor in ' + path)
            for href in page.links:
                url = urlsplit(href)
                if url.scheme or url.netloc:
                    continue
                target = posixpath.normpath(posixpath.join(posixpath.dirname(path), unquote(url.path))) if url.path else path
                if url.path.endswith('/'):
                    target = posixpath.normpath(target + '/index.html')
                if target in parsed and url.fragment:
                    self.assertIn(unquote(url.fragment), parsed[target].ids, f'{path}: {href}')
                elif target not in parsed and target not in homepage:
                    self.assertTrue((component_docs.SITE / target).is_file() or (component_docs.SITE / target).is_dir(), f'{path}: {href}')


def png(width=4, height=2):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + b'\xf9\xf9\xf9' * width for _ in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


class PreviewManifestTest(unittest.TestCase):
    """Publication needs current captures; local drafts reuse or replace them."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        assets = root / 'site/assets/component-previews'
        self.images = []
        for theme in ('light', 'dark'):
            path = previews.preview_path('button', 'styles', theme)
            (root / 'site' / path).parent.mkdir(parents=True, exist_ok=True)
            (root / 'site' / path).write_bytes(png())
            self.images.append({'component': 'button', 'state': 'styles', 'theme': theme, 'path': path,
                                'sha256': hashlib.sha256(png()).hexdigest(), 'pixel_size': [4, 2],
                                'logical_size': [2, 1], 'background': '#f9f9f9'})
        self.manifest = assets / 'manifest.json'
        self.write_manifest()
        recipe = [{'id': 'button', 'previews': [{'id': 'styles', 'sample': 'styles'}]}]
        for name, value in (('ROOT', root), ('ASSETS', assets), ('MANIFEST', self.manifest)):
            self.start(patch.object(previews, name, value))
        self.start(patch.object(previews, 'config', return_value=recipe))
        self.digest = self.start(patch.object(previews, 'capture_digest', return_value='current'))

    def start(self, patcher):
        self.addCleanup(patcher.stop)
        return patcher.start()

    def write_manifest(self):
        self.manifest.write_text(json.dumps({'input_digest': 'current', 'images': self.images,
                                            'capture_settings': previews.capture_settings()}))

    def test_different_capture_environment_is_not_reused(self):
        previews.validated_manifest()
        with patch.dict(os.environ, {'QT_SCALE_FACTOR': '17'}):
            with self.assertRaisesRegex(ValueError, 'stale'):
                previews.validated_manifest()

    def test_publication_rejects_changed_bytes_and_stale_inputs(self):
        previews.validated_manifest()
        self.digest.return_value = 'changed'
        with self.assertRaisesRegex(ValueError, 'stale'):
            previews.validated_manifest()
        self.digest.return_value = 'current'
        self.images[0]['sha256'] = '0' * 64
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, 'does not match'):
            previews.validated_manifest()

    def test_drafts_reuse_stale_images_and_replace_unusable_ones(self):
        self.digest.return_value = 'changed'
        draft = previews.available_previews()
        self.assertEqual([image['sha256'] for image in draft['images']], [i['sha256'] for i in self.images])
        self.assertIn('stale', draft['notice'])
        (previews.ROOT / 'site' / self.images[1]['path']).write_bytes(b'not the captured image')
        dark = previews.available_previews()['images'][1]
        self.assertEqual((dark['sha256'], dark['path'], dark['background']), ('', self.images[1]['path'], '#3d3d3d'))
        self.manifest.unlink()
        draft = previews.available_previews()
        self.assertTrue(all(not image['sha256'] for image in draft['images']))
        self.assertIn('2 placeholders', draft['notice'])


if __name__ == "__main__":
    unittest.main()
