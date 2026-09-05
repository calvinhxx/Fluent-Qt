#!/usr/bin/env python3
"""Regression checks for the include dependency boundaries."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "gallery_boundary", Path(__file__).with_name("validate-gallery-boundary.py")
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class IncludeBoundaryTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.write("cmake/FluentQtInstallHeaders.cmake", "    src/components/Window.h\n")

    def write(self, path, content):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")

    def test_public_gallery_and_lower_layer_dependencies_are_allowed(self):
        self.write("app/Main.cpp", '#include <FluentQt/components/Window.h>\n')
        self.write("src/components/Window.cpp", '#include "compatibility/Types.h"\n')
        self.write("src/compatibility/Native.cpp", '#include "private/Events_p.h"\n')
        self.assertEqual(MODULE.validate(self.root), [])

    def test_gallery_private_headers_are_rejected_in_each_spelling(self):
        for include in (
            "components/private/Window_p.h",
            "FluentQt/components/private/Window_p.h",
            "../src/components/private/Window_p.h",
            "src/components/Window.h",
            "components/Uninstalled.h",
        ):
            with self.subTest(include=include):
                self.write("app/Main.cpp", f'#include "{include}"\n')
                self.assertTrue(MODULE.validate(self.root))

    def test_compatibility_cannot_include_components_or_umbrella(self):
        for include in (
            "components/Window.h",
            "../components/Window.h",
            "src/components/Window.h",
            "FluentQt/components/Window.h",
            "FluentQt/FluentQt.h",
        ):
            with self.subTest(include=include):
                self.write("src/compatibility/Native.cpp", f'#include "{include}"\n')
                violations = MODULE.validate(self.root)
                self.assertEqual(len(violations), 1)
                self.assertIn("platform compatibility", violations[0])

    def test_library_cannot_include_application_logging(self):
        for include in (
            "app/Controller.h",
            "../../app/Controller.h",
            "support/logging/Log.h",
            "../../support/logging/Log.h",
            "spdlog/spdlog.h",
            "fmt/format.h",
        ):
            with self.subTest(include=include):
                self.write("src/components/Window.cpp", f'#include "{include}"\n')
                violations = MODULE.validate(self.root)
                self.assertEqual(len(violations), 1)
                self.assertIn("application logging", violations[0])

    def test_empty_install_allowlist_fails_closed(self):
        self.write("cmake/FluentQtInstallHeaders.cmake", "")
        self.assertEqual(len(MODULE.validate(self.root)), 1)


if __name__ == "__main__":
    unittest.main()
