"""Check icon definition extraction and bundled semantic glyph mappings."""

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "generate_typography_assets", Path(__file__).with_name("generate_typography_assets.py")
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TypographyIconDefinitionTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="fluentqt-icon-definitions-")
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        self.header = directory / "Typography.h"
        self.source = directory / "Typography.cpp"
        for name, path in (("TYPOGRAPHY_HEADER", self.header), ("TYPOGRAPHY_SOURCE", self.source)):
            replacement = patch.object(MODULE, name, path, create=True)
            replacement.start()
            self.addCleanup(replacement.stop)

    def test_reads_definitions_after_the_public_header_exports_only_declarations(self):
        self.header.write_text(
            "extern const QString GlobalNav;\nextern const QString ChevronDown;\n", encoding="utf-8"
        )
        self.source.write_text(
            r'const QString GlobalNav = QString::fromUtf16(u"\uE700");' "\n"
            r'const QString ChevronDown = QString::fromUtf16(u"\uE70D");' "\n",
            encoding="utf-8",
        )
        self.assertEqual(MODULE.typography_icon_codepoints(), {"GlobalNav": 0xE700, "ChevronDown": 0xE70D})

    def test_stale_header_definitions_cannot_hide_missing_implementation_definitions(self):
        self.header.write_text(r'const QString Stale = QString::fromUtf16(u"\uE700");', encoding="utf-8")
        self.source.write_text("// No glyph definitions.\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "No icon codepoints found.*Typography.cpp"):
            MODULE.typography_icon_codepoints()


class BundledIconSemanticAliasTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        MODULE.validate_inputs()
        cls.aliases = MODULE.load_json_object(MODULE.ICON_ALIASES_SOURCE)
        cls.codepoints = MODULE.typography_icon_codepoints()
        cls.catalog = MODULE.load_json_object(MODULE.ICON_CATALOG_SOURCE)
        cls.runtime_aliases = MODULE.load_json_object(MODULE.ROOT / "res/icons/FluentQtIconAliases.json")
        cls.upstream = MODULE.TTFont(MODULE.ICON_SOURCE, recalcTimestamp=False)
        cls.addClassCleanup(cls.upstream.close)
        cls.bundled = MODULE.TTFont(MODULE.ROOT / "res/icons/FluentQtIcons.ttf", recalcTimestamp=False)
        cls.addClassCleanup(cls.bundled.close)

    def assert_semantic_alias(self, name, codepoint, source_name):
        self.assertEqual(self.aliases[name], source_name)
        self.assertEqual(self.codepoints[name], codepoint)
        self.assertEqual(self.runtime_aliases[f"{codepoint:X}"], source_name)
        source_codepoint = int(self.catalog[source_name])
        expected_glyph = MODULE.unicode_cmap(self.upstream)[source_codepoint]
        unicode_tables = [table for table in self.bundled["cmap"].tables if table.isUnicode()]
        self.assertTrue(unicode_tables)
        for table in unicode_tables:
            with self.subTest(platform=table.platformID, encoding=table.platEncID, format=table.format):
                self.assertEqual(table.cmap.get(codepoint), expected_glyph)

    def test_title_bar_back_uses_the_16_pixel_arrow(self):
        self.assert_semantic_alias("TitleBarBack", 0xE830, "ic_fluent_arrow_left_16_regular")

    def test_checkmark_badge_uses_the_12_pixel_inner_check(self):
        self.assert_semantic_alias("CheckmarkBadge12", 0xE65F, "ic_fluent_checkmark_12_regular")

    def test_error_badge_uses_the_12_pixel_inner_dismiss(self):
        self.assert_semantic_alias("ErrorBadge12", 0xEDAE, "ic_fluent_dismiss_12_regular")

    def test_important_badge_uses_the_12_pixel_inner_mark(self):
        self.assert_semantic_alias("ImportantBadge12", 0xEDB1, "ic_fluent_important_12_regular")


if __name__ == "__main__":
    unittest.main()
