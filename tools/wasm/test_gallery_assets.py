#!/usr/bin/env python3
"""Verify the linked Gallery script and WASM are versioned as one runtime."""

from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class GalleryAssetVersionTest(unittest.TestCase):
    def test_incremental_binary_or_glue_change_updates_both_urls(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'fluent_qt_gallery.js'
            binary = root / 'fluent_qt_gallery.wasm'
            template = root / 'template.html'
            template.write_text((ROOT / 'app/platform/wasm/index.html.in').read_text())
            script.write_text('glue v1')
            binary.write_bytes(b'wasm v1')

            def generate():
                subprocess.run(['cmake', f'-DWEB_DIR={root}', f'-DTEMPLATE={template}', '-P',
                                str(ROOT / 'app/platform/wasm/VersionGalleryAssets.cmake')], check=True)
                page = (root / 'index.html').read_text()
                versions = re.findall(r'\?v=([0-9a-f]{16})', page)
                self.assertEqual(len(versions), 2)
                self.assertEqual(versions[0], versions[1])
                self.assertNotIn('__FLUENT_QT_GALLERY_RUNTIME_VERSION__', page)
                return versions[0]

            first = generate()
            self.assertEqual(generate(), first)
            binary.write_bytes(b'wasm v2')
            second = generate()
            self.assertNotEqual(second, first)
            script.write_text('glue v2')
            self.assertNotEqual(generate(), second)


if __name__ == '__main__':
    unittest.main()
