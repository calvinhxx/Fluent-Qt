"""Tests for the cross-version Shiboken stub-generator adapter."""

import ast
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
    def test_public_version_variables_have_string_stub_types(self):
        self.assertEqual(
            STUB_GENERATOR.MODULE_VARIABLE_TYPES[
                ("fluentqt", "__version__")
            ],
            "str",
        )
        self.assertEqual(
            STUB_GENERATOR.MODULE_VARIABLE_TYPES[
                ("fluentqt", "__api_version__")
            ],
            "str",
        )

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

    def test_legacy_stub_gains_signals_without_changing_existing_declarations(self):
        contents = '''import typing
import PySide6.QtCore

class fluent:
    class QmlSession:
        def activate(self) -> bool: ...
    class QmlTheme:
        changed: typing.ClassVar[PySide6.QtCore.Signal] = ...
'''
        signals = {"fluent.QmlSession": ["activeChanged"], "fluent.QmlTheme": ["changed"]}
        generated = STUB_GENERATOR.add_signal_declarations(contents, signals)
        tree = ast.parse(generated)
        session, theme = tree.body[2].body
        self.assertEqual([node.target.id for node in session.body if isinstance(node, ast.AnnAssign)],
                         ["activeChanged"])
        self.assertEqual([node.target.id for node in theme.body if isinstance(node, ast.AnnAssign)],
                         ["changed"])
        self.assertIn("def activate(self) -> bool: ...", generated)
        self.assertEqual(STUB_GENERATOR.add_signal_declarations(generated, signals), generated)

    def test_signal_declarations_keep_nested_and_same_named_class_owners(self):
        contents = '''import typing
import PySide6.QtCore

class first:
    class Session:
        class Nested:
            pass
class second:
    class Session:
        pass
'''
        generated = STUB_GENERATOR.add_signal_declarations(contents, {
            "first.Session": ["activeChanged"],
            "first.Session.Nested": ["changed"],
        })
        tree = ast.parse(generated)
        first, second = tree.body[2:]
        self.assertEqual(first.body[0].body[1].target.id, "activeChanged")
        self.assertEqual(first.body[0].body[0].body[1].target.id, "changed")
        self.assertIsInstance(second.body[0].body[0], ast.Pass)
        self.assertEqual(len(second.body[0].body), 1)


if __name__ == "__main__":
    unittest.main()
