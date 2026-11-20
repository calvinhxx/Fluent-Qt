#!/usr/bin/env python3
"""Keep cached homepage assets consistent with the generated page."""

import json
import re
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest.mock import patch

import generate_localized_site as site


class HomepageAssetTest(unittest.TestCase):
    def test_readme_documentation_links_stay_in_the_readers_language(self):
        destinations = re.compile(r'(?:href="|\]\()' + re.escape(site.BASE_URL) + r'([^"\s)]*)')
        english = (site.ROOT / 'README.md').read_text(encoding='utf-8')
        expected = {path for path in destinations.findall(english) if path.startswith(('api/', '#'))}
        home_paths = {locale.url_prefix for locale in site.LOCALES}
        self.assertIn('api/button/?playground=1', expected)
        for locale in site.LOCALES:
            with self.subTest(language=locale.key):
                readme = (site.ROOT / locale.readme_file).read_text(encoding='utf-8')
                links = set(destinations.findall(readme))
                for path in links:
                    if 'api' in path.split('/'):
                        self.assertTrue(path.startswith(locale.url_prefix + 'api/'), path)
                    elif path.partition('#')[0] in home_paths:
                        self.assertEqual(path.partition('#')[0], locale.url_prefix)
                self.assertTrue({locale.url_prefix + path for path in expected} <= links)

    def test_changed_controller_gets_a_new_url_in_all_locales(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            controller = root / 'spatial-showcase.js'
            template = '<div data-showcase-controller="spatial-showcase.js"></div>'
            with patch.object(site, 'SITE_ROOT', root):
                controller.write_text('old click-to-start controller', encoding='utf-8')
                old = [site.prefix_resources(template, locale.resource_prefix) for locale in site.LOCALES]
                controller.write_text('new visibility-start controller', encoding='utf-8')
                new = [site.prefix_resources(template, locale.resource_prefix) for locale in site.LOCALES]
                for index, locale in enumerate(site.LOCALES):
                    self.assertNotEqual(old[index], new[index])
                    expected_prefix = locale.resource_prefix
                    self.assertEqual(
                        new[index],
                        site.prefix_resources(template, '').replace('="spatial-', f'="{expected_prefix}spatial-'),
                    )

    def test_entry_script_and_styles_do_not_reuse_stale_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(site, 'SITE_ROOT', root):
                for asset, attribute in [('site.js', 'src'), ('styles.css', 'href')]:
                    with self.subTest(asset=asset):
                        path = root / asset
                        template = f'<link {attribute}="{asset}?v=old-manual-version">'
                        path.write_text('before', encoding='utf-8')
                        old = site.prefix_resources(template, '')
                        path.write_text('after', encoding='utf-8')
                        new = site.prefix_resources(template, '')
                        self.assertNotEqual(old, new)
                        self.assertNotIn('old-manual-version', new)

    def test_gallery_runtime_parameters_are_preserved(self):
        template = '<div data-showcase-src="gallery/?embed=site&amp;showcase=spatial&amp;render-scale=native"></div>'
        self.assertEqual(site.prefix_resources(template, ''), template)
        self.assertEqual(site.prefix_resources(template, '../'), template.replace('="gallery/', '="../gallery/'))

    def test_checkbox_showcase_uses_localized_artwork_and_accessible_text(self):
        source = site.TEMPLATE_PATH.read_text(encoding='utf-8')
        values = site.translations()
        for locale in site.LOCALES:
            with self.subTest(language=locale.key):
                page = site.render_page(source, locale, values[locale.key], site.project_version())
                image = re.search(r'<img class="playground-showcase-image"[^>]+>', page).group()
                for theme, attribute in [('light', 'src'), ('light', 'data-theme-src-light'),
                                         ('dark', 'data-theme-src-dark'), ('high-contrast', 'data-theme-src-contrast')]:
                    url = site.versioned_asset_url(f'assets/playground/checkbox-{locale.playground_art_lang}-{theme}.webp')
                    self.assertIn(f' {attribute}="{locale.resource_prefix}{url}"', image)
                alt = re.search(r' alt="([^"]+)"', image).group(1)
                self.assertIn('CheckBox', alt)
                self.assertEqual(alt, values[locale.key]['playground.imageAlt'])
                self.assertEqual(bool(re.search(r'[\u4e00-\u9fff]', alt)), locale.key in ('zh', 'zh-TW', 'ja'))
                self.assertIn(f'href="{locale.playground_url}"', page)

    def test_each_funding_link_has_a_tier_page_for_all_locales(self):
        funding = (site.ROOT / '.github/FUNDING.yml').read_text(encoding='utf-8')
        links = re.findall(r'^\s*- (\S+)$', funding, re.MULTILINE)
        self.assertEqual(links, [f'{site.BASE_URL}fuel/{tier}/' for tier in site.SUPPORT_TIERS])
        for tier in site.SUPPORT_TIERS:
            page = site.render_support_redirect(tier)
            self.assertIn(f'<meta http-equiv="refresh" content="0; url=../../?fuel={tier}#support">', page)
            self.assertIn(f'location.replace("../../zh-CN/?fuel={tier}#support")', page)
            self.assertIn(f'location.replace("../../zh-TW/?fuel={tier}#support")', page)
            self.assertIn(f'<a href="../../?fuel={tier}#support">', page)

    def test_rendered_pages_select_locale_default_support_channel(self):
        source = site.TEMPLATE_PATH.read_text(encoding='utf-8')
        values = site.translations()
        version = site.project_version()
        for locale in site.LOCALES:
            with self.subTest(language=locale.key):
                page = site.render_page(source, locale, values[locale.key], version)
                site.validate_page(page, locale, values[locale.key])
                expected = 'wechat' if locale.key == 'zh' else 'kofi'
                other = 'kofi' if expected == 'wechat' else 'wechat'
                self.assertRegex(page, rf'<input type="radio" name="support-method" value="{expected}"[^>]*\schecked>')
                self.assertNotRegex(page, rf'<input type="radio" name="support-method" value="{other}"[^>]*\schecked>')
                match = re.search(r'<script type="application/json" id="site-i18n">(.*?)</script>', page, re.DOTALL)
                self.assertIsNotNone(match)
                runtime = json.loads(match.group(1).replace('<\\/', '</'))
                self.assertEqual(set(runtime), set(site.RUNTIME_I18N_KEYS))

    def test_sitemap_covers_all_locales_for_homepages_and_guides(self):
        sitemap = site.render_sitemap()
        root = ET.fromstring(sitemap)
        ns = {'sm': 'http://www.sitemaps.org/schemas/sitemap/0.9'}
        locs = {node.text for node in root.findall('sm:url/sm:loc', ns)}
        for locale in site.LOCALES:
            self.assertIn(locale.canonical_url(site.BASE_URL), locs)
            for subpath in ('api/foundation/', 'api/button/', 'api/releases/', 'api/reference/motion-policy/'):
                self.assertIn(f'{site.BASE_URL}{locale.url_prefix}{subpath}', locs)


if __name__ == '__main__':
    unittest.main()
