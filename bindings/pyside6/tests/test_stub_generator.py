"""Tests for the cross-version Shiboken stub-generator adapter."""

import importlib.util
from pathlib import Path
import unittest


STUB_GENERATOR_PATH = (
    Path(__file__).resolve().parents[1]
    / "tools"
    / "generate_stubs.py"
)
STUB_GENERATOR_SPEC = importlib.util.spec_from_file_location(
    "fluentqt_generate_stubs",
    STUB_GENERATOR_PATH,
)
STUB_GENERATOR = importlib.util.module_from_spec(STUB_GENERATOR_SPEC)
STUB_GENERATOR_SPEC.loader.exec_module(STUB_GENERATOR)


class StubGeneratorTest(unittest.TestCase):
    def test_options_cover_shiboken_62_generate_pyi_contract(self):
        options = STUB_GENERATOR.shiboken_generator_options()

        for name in (
            "_pyside_call",
            "check",
            "is_ci",
            "logger",
            "quiet",
        ):
            with self.subTest(name=name):
                self.assertTrue(hasattr(options, name))

        self.assertFalse(options._pyside_call)
        self.assertFalse(options.check)
        self.assertFalse(options.is_ci)
        self.assertTrue(options.quiet)


if __name__ == "__main__":
    unittest.main()
