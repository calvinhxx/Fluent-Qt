#!/usr/bin/env python3
"""Keep cached homepage assets consistent with the generated page."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import generate_localized_site as site


class HomepageAssetTest(unittest.TestCase):
    def test_changed_controller_gets_a_new_url_in_both_locales(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            controller = root / 'spatial-showcase.js'
            template = '<div data-showcase-controller="spatial-showcase.js"></div>'
            with patch.object(site, 'SITE_ROOT', root):
                controller.write_text('old click-to-start controller', encoding='utf-8')
                old = [site.prefix_resources(template, locale.resource_prefix) for locale in site.LOCALES]
                controller.write_text('new visibility-start controller', encoding='utf-8')
                new = [site.prefix_resources(template, locale.resource_prefix) for locale in site.LOCALES]
                self.assertNotEqual(old[0], new[0])
                self.assertNotEqual(old[1], new[1])
                self.assertEqual(new[1], new[0].replace('="spatial-', '="../spatial-'))
                self.assertEqual(new[0], site.prefix_resources(template, ''))

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


if __name__ == '__main__':
    unittest.main()
