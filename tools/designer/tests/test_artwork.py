"""Check that Designer detects stale SVG, PNG and exporter inputs on every host."""
from pathlib import Path
from tempfile import TemporaryDirectory
import sys
import unittest
from unittest.mock import patch

DESIGNER = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(DESIGNER), str(DESIGNER.parent / "gallery")]
from prepare_catalog import prepare_icons
from export_control_images import render_source


class DesignerArtworkTest(unittest.TestCase):
    def test_exact_inputs_and_incremental_regeneration(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "Test.svg"
            source.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="72" height="72" '
                              'viewBox="0 0 72 72"><rect width="72" height="72" fill="red"/></svg>')
            other = root / "Other.svg"
            other.write_bytes(source.read_bytes())
            recipe = root / "export.py"
            recipe.write_text("initial recipe")
            records = [({"name": "Test"}, source), ({"name": "Other"}, other)]
            prepare_icons(records, root, recipe, False)
            png = root / "icons/Test.png"
            original = png.read_bytes()
            with patch("export_control_images.render_source", wraps=render_source) as render:
                prepare_icons(records, root, recipe, True)
                render.assert_not_called()
                png.write_bytes(original + b"x")
                with self.assertRaisesRegex(SystemExit, "Stale Designer icon"):
                    prepare_icons(records, root, recipe, True)
                prepare_icons(records, root, recipe, False)
                render.assert_called_once_with(source)
                self.assertEqual(original, png.read_bytes())
            source.write_text(source.read_text().replace("red", "blue"))
            with self.assertRaisesRegex(SystemExit, "Stale Designer icon"):
                prepare_icons(records, root, recipe, True)
            prepare_icons(records, root, recipe, False)
            self.assertNotEqual(original, png.read_bytes())
            prepare_icons(records, root, recipe, True)
            recipe.write_text("updated recipe")
            with self.assertRaisesRegex(SystemExit, "Stale Designer icon"):
                prepare_icons(records, root, recipe, True)
