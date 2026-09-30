"""Qt-free regression coverage for the shared Gallery artwork CI gate."""

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from validate_control_artwork import validate


SVG = '<svg xmlns="http://www.w3.org/2000/svg" width="72" height="72" viewBox="0 0 72 72"><path d="M4 4H68V68H4Z"/></svg>'
ENTRY = '<file alias="assets/control_images/navigation/Stepper.svg">../tools/gallery/artwork/navigation/Stepper.svg</file>'


class ArtworkValidationTest(unittest.TestCase):
    def setUp(self):
        directory = TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source = self.root / "tools/gallery/artwork/navigation/Stepper.svg"
        self.output = self.root / "app/assets/control_images/navigation/Stepper.png"
        self.qrc = self.root / "app/gallery_resources.qrc"
        self.source.parent.mkdir(parents=True)
        self.output.parent.mkdir(parents=True)
        self.source.write_text(SVG)
        self.output.touch()
        self.resources(ENTRY)

    def resources(self, entries, prefix="/app"):
        self.qrc.write_text(f'<RCC><qresource prefix="{prefix}">{entries}</qresource></RCC>')

    def test_mirrored_sources_and_correct_runtime_alias_pass(self):
        self.assertEqual(validate(self.root), [])

    def test_missing_export_and_orphaned_png_fail(self):
        self.output.unlink()
        self.assertTrue(any("missing PNG export" in error for error in validate(self.root)))
        self.output.with_name("Unknown.png").touch()
        self.assertTrue(any("without SVG masters" in error for error in validate(self.root)))

    def test_missing_registration_and_duplicate_registration_fail(self):
        self.resources("")
        self.assertTrue(any("missing from" in error for error in validate(self.root)))
        self.resources(ENTRY + ENTRY)
        self.assertTrue(any("duplicate" in error for error in validate(self.root)))

    def test_wrong_resource_alias_and_prefix_fail(self):
        self.resources(ENTRY.replace("Stepper.svg\"", "Wrong.svg\""))
        self.assertTrue(any("resource alias" in error for error in validate(self.root)))
        self.resources(ENTRY, prefix="/wrong")
        self.assertTrue(any("resource alias" in error for error in validate(self.root)))

    def test_legacy_runtime_png_and_missing_source_fail(self):
        self.resources('<file>assets/control_images/navigation/Stepper.png</file>')
        self.assertTrue(any("no matching SVG master" in error for error in validate(self.root)))
        self.resources(ENTRY.replace("../tools/gallery/artwork/navigation/Stepper.svg", "../tools/gallery/artwork/navigation/Missing.svg"))
        self.assertTrue(any("no matching SVG master" in error for error in validate(self.root)))

    def test_unsupported_svg_and_malformed_resource_xml_fail(self):
        self.source.write_text(SVG.replace("</svg>", "<clipPath/></svg>"))
        self.assertTrue(any("Qt 5.15-compatible" in error for error in validate(self.root)))
        self.qrc.write_text("<RCC>")
        self.assertTrue(validate(self.root))


if __name__ == "__main__":
    unittest.main()
