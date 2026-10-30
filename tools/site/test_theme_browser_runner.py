#!/usr/bin/env python3
"""Qt-free regressions for the standalone website browser runner."""

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import test_theme_browser as MODULE


class SourceWebsitePreparationTest(unittest.TestCase):
    def fixture(self, root: Path, source: str) -> None:
        generator = root / "tools/site/generate_localized_site.py"
        generator.parent.mkdir(parents=True)
        generator.write_text(source, encoding="utf-8")

    def test_clean_and_stale_homepages_are_generated_for_both_locales(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, "from pathlib import Path\n"
                         "for name in ('site/index.html', 'site/zh-CN/index.html'):\n"
                         "    page = Path(name)\n"
                         "    page.parent.mkdir(parents=True, exist_ok=True)\n"
                         "    page.write_text('current template', encoding='utf-8')\n")
            for phase in ("clean", "stale"):
                with self.subTest(phase=phase):
                    MODULE.prepare_site_theme_contract(root)
                    for relative in ("site/index.html", "site/zh-CN/index.html"):
                        page = root / relative
                        self.assertEqual(page.read_text(encoding="utf-8"), "current template")
                        page.write_text("old template", encoding="utf-8")

    def test_generation_failure_preserves_old_homepages_without_serving_them(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root, "raise SystemExit(19)\n")
            old_pages = {"site/index.html": b"old English template\n",
                         "site/zh-CN/index.html": b"old Chinese template\n"}
            for relative, content in old_pages.items():
                page = root / relative
                page.parent.mkdir(parents=True, exist_ok=True)
                page.write_bytes(content)
            browser = Mock()
            with patch.object(MODULE, "PROJECT_ROOT", root), \
                    patch.object(MODULE, "ThreadingHTTPServer") as server:
                with self.assertRaises(subprocess.CalledProcessError) as error:
                    MODULE.run_site_theme_contract(browser, root / "build")
                server.assert_not_called()
                browser.new_page.assert_not_called()
            self.assertEqual(error.exception.returncode, 19)
            for relative, content in old_pages.items():
                self.assertEqual((root / relative).read_bytes(), content)

    def test_source_handler_uses_canonical_artwork(self):
        handler = object.__new__(MODULE.QuietSiteHandler)
        handler.directory = str(MODULE.PROJECT_ROOT / "site")
        handler.wasm_root = Path("no-wasm-runtime")
        self.assertEqual(Path(handler.translate_path("/assets/app-icon.png?v=old")),
                         MODULE.PROJECT_ROOT / "app/assets/app-icon.png")

    def test_packaged_site_must_contain_both_homepages_before_serving(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "index.html").write_text("English only", encoding="utf-8")
            browser = Mock()
            with patch.object(MODULE, "prepare_site_theme_contract") as prepare, \
                    patch.object(MODULE, "ThreadingHTTPServer") as server:
                with self.assertRaisesRegex(RuntimeError, "zh-CN/index.html"):
                    MODULE.run_site_theme_contract(browser, root / "output", site_root=root)
                prepare.assert_not_called()
                server.assert_not_called()
                browser.new_page.assert_not_called()


if __name__ == "__main__":
    unittest.main()
