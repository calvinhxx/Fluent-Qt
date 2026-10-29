"""Keep canonical artwork available at the existing preview and release URLs."""

import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import component_docs
import generate_localized_site as localized
import serve
from site_assets import COPIED_ASSETS, copy_assets


class CanonicalSiteAssetTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.site = self.root / "site"
        self.site.mkdir()
        for relative, canonical in COPIED_ASSETS.items():
            path = self.root / canonical
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(canonical.encode())

    def test_urls_hash_the_canonical_file_without_creating_a_source_copy(self):
        with patch.object(localized, "SITE_ROOT", self.site):
            for relative, canonical in COPIED_ASSETS.items():
                expected = hashlib.sha256((self.root / canonical).read_bytes()).hexdigest()[:12]
                self.assertEqual(localized.versioned_asset_url(relative), relative + "?v=" + expected)
                self.assertFalse((self.site / relative).exists())

    def test_preview_preserves_urls_and_wasm_routing(self):
        handler = object.__new__(serve.SiteHandler)
        handler.directory = str(self.site)
        handler.wasm_root = self.root / "wasm"
        with patch.object(serve, "SITE", self.site):
            for relative, canonical in COPIED_ASSETS.items():
                self.assertEqual(handler.translate_path("/" + relative + "?v=old"), str(self.root / canonical))
            self.assertEqual(handler.translate_path("/gallery/app.wasm"), str(self.root / "wasm/app.wasm"))
            self.assertEqual(handler.translate_path("/zh-CN/index.html"), str(self.site / "zh-CN/index.html"))

    def test_nested_documentation_uses_canonical_artwork_without_source_copies(self):
        with patch.object(component_docs, "SITE", self.site):
            for page, prefix in (("api/button/index.html", "../../"),
                                 ("zh-CN/api/button/index.html", "../../../")):
                for relative, canonical in COPIED_ASSETS.items():
                    digest = hashlib.sha256((self.root / canonical).read_bytes()).hexdigest()[:12]
                    self.assertEqual(component_docs.asset(relative, page), prefix + relative + "?v=" + digest)
                    self.assertFalse((self.site / relative).exists())

    def test_packaging_copies_exact_bytes_and_requires_canonical_sources(self):
        destination = self.root / "output"
        copy_assets(destination, self.root)
        for relative, canonical in COPIED_ASSETS.items():
            self.assertEqual((destination / relative).read_bytes(), (self.root / canonical).read_bytes())
        (self.root / "docs/assets/readme/hero.png").unlink()
        with self.assertRaises(FileNotFoundError):
            copy_assets(destination, self.root)


if __name__ == "__main__":
    unittest.main()
