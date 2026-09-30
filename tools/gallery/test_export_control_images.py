#!/usr/bin/env python3

"""Contracts for vector masters, mirrored exports, and read-only freshness checks."""

from pathlib import Path
import os
from tempfile import TemporaryDirectory
import unittest

from export_control_images import export_images, source_pairs, validate_source


SVG = '''<svg xmlns="http://www.w3.org/2000/svg" width="72" height="72" viewBox="0 0 72 72">
<rect x="3" y="3" width="66" height="66" rx="16" fill="{color}"/>
</svg>'''


class ArtworkFixture(unittest.TestCase):
    def setUp(self):
        self.directory = TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.sources = self.root / "artwork"
        self.images = self.root / "images"
        self.sources.mkdir()
        self.images.mkdir()

    def write_source(self, name="navigation/Stepper", data=None):
        file = self.sources / (name + ".svg")
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(data or SVG.format(color="#005fb8"))
        return file


class SourceContract(ArtworkFixture):
    def test_sources_and_exports_share_category_and_title(self):
        source = self.write_source()
        self.assertEqual(source_pairs(self.sources, self.images),
                         [(source, self.images / "navigation/Stepper.png")])

    def test_orphaned_png_is_reported(self):
        self.write_source()
        (self.images / "Unknown.png").touch()
        with self.assertRaisesRegex(ValueError, "Unknown.png"):
            source_pairs(self.sources, self.images)

    def test_empty_source_inventory_is_reported(self):
        with self.assertRaisesRegex(ValueError, "no SVG masters"):
            source_pairs(self.sources, self.images)

    def test_canvas_dimensions_and_view_box_must_match(self):
        for data in (SVG.format(color="blue").replace('width="72"', 'width="96"'),
                     SVG.format(color="blue").replace('viewBox="0 0 72 72"', 'viewBox="0 0 24 24"')):
            with self.subTest(data=data):
                with self.assertRaisesRegex(ValueError, "viewBox"):
                    validate_source(self.write_source(data=data))

    def test_bitmap_wrappers_and_unoutlined_text_are_rejected(self):
        for element in ('<image href="data:image/png;base64,AA=="/>',
                        '<text x="10" y="20">A</text>'):
            with self.subTest(element=element):
                data = SVG.format(color="blue").replace('</svg>', element + '</svg>')
                with self.assertRaisesRegex(ValueError, "outlined vector artwork"):
                    validate_source(self.write_source(data=data))

    def test_external_resources_are_rejected(self):
        data = SVG.format(color="blue").replace('</svg>', '<use href="other.svg#shape"/></svg>')
        with self.assertRaisesRegex(ValueError, "external artwork"):
            validate_source(self.write_source(data=data))

    def test_features_outside_the_shared_qt_svg_subset_are_rejected(self):
        for element in ('<clipPath id="bounds"/>', '<mask id="mask"/>', '<filter id="blur"/>',
                        '<pattern id="dots"/>', '<symbol id="shape"/>', '<marker id="arrow"/>'):
            with self.subTest(element=element):
                data = SVG.format(color="blue").replace('</svg>', element + '</svg>')
                with self.assertRaisesRegex(ValueError, "Qt 5.15-compatible"):
                    validate_source(self.write_source(data=data))
        for attribute in ('clip-path', 'mask', 'filter', 'marker-start', 'marker-mid', 'marker-end'):
            with self.subTest(attribute=attribute):
                data = SVG.format(color="blue").replace('<rect ', f'<rect {attribute}="url(#effect)" ')
                with self.assertRaisesRegex(ValueError, "Qt 5.15-compatible"):
                    validate_source(self.write_source(data=data))


class ExportContract(ArtworkFixture):
    @classmethod
    def setUpClass(cls):
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        from PySide6.QtGui import QGuiApplication
        cls.app = QGuiApplication.instance() or QGuiApplication([])

    def test_export_creates_rgba_canvas_and_partial_alpha_edges(self):
        from PySide6.QtGui import QImage
        self.write_source()
        self.assertEqual(export_images(self.sources, self.images), 0)
        image = QImage(str(self.images / "navigation/Stepper.png"))
        self.assertEqual((image.width(), image.height()), (72, 72))
        self.assertTrue(image.hasAlphaChannel())
        self.assertEqual(image.pixelColor(0, 0).alpha(), 0)
        self.assertTrue(any(0 < image.pixelColor(x, y).alpha() < 255
                            for y in range(72) for x in range(72)))
        self.assertEqual(export_images(self.sources, self.images, check=True), 0)

    def test_check_reports_stale_pixels_without_overwriting_them(self):
        source = self.write_source()
        export_images(self.sources, self.images)
        output = self.images / "navigation/Stepper.png"
        original = output.read_bytes()
        source.write_text(SVG.format(color="#f25c3d"))
        self.assertEqual(export_images(self.sources, self.images, check=True), 1)
        self.assertEqual(output.read_bytes(), original)
        self.assertEqual(export_images(self.sources, self.images), 0)
        self.assertNotEqual(output.read_bytes(), original)

    def test_selection_updates_only_the_requested_component(self):
        first = self.write_source()
        second = self.write_source("collections/Timeline")
        export_images(self.sources, self.images)
        timeline = self.images / "collections/Timeline.png"
        original = timeline.read_bytes()
        first.write_text(SVG.format(color="#f25c3d"))
        second.write_text(SVG.format(color="#a38be2"))
        self.assertEqual(export_images(self.sources, self.images, assets=("navigation/Stepper",)), 0)
        self.assertEqual(timeline.read_bytes(), original)
        self.assertEqual(export_images(self.sources, self.images, check=True,
                                      assets=("navigation/Stepper",)), 0)
        self.assertEqual(export_images(self.sources, self.images, check=True,
                                      assets=("collections/Timeline",)), 1)

    def test_missing_export_is_reported_without_creating_a_file(self):
        self.write_source()
        self.assertEqual(export_images(self.sources, self.images, check=True), 1)
        self.assertFalse((self.images / "navigation/Stepper.png").exists())


if __name__ == "__main__":
    unittest.main()
