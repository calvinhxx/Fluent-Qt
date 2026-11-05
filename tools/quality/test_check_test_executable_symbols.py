#!/usr/bin/env python3
"""Qt-free regressions for the shared test-executable symbol check."""

from __future__ import annotations

from contextlib import redirect_stdout
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "check_test_executable_symbols", ROOT / "tools/quality/check_test_executable_symbols.py"
)
assert SPEC is not None and SPEC.loader is not None
CHECK = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CHECK
SPEC.loader.exec_module(CHECK)


class ExternalDefinitionsTest(unittest.TestCase):
    def names(self, text: str) -> list[tuple[str, str]]:
        return CHECK.external_definitions(text)

    def test_reports_namespace_scope_types_and_inline_helpers(self):
        self.assertEqual(
            self.names(
                "class FluentTestWindow : public QWidget, public fluent::FluentElement {\n"
                "    void onThemeUpdated() override { setVisible(true); }\n"
                "};\n"
                "namespace fluent::testing { struct Probe final { int value = 0; }; }\n"
                "inline QString label() { return {}; }\n"
                "template <typename T> T* findChildOf(QWidget* root) { return nullptr; }\n"
                "enum class Mode : int { Light, Dark };\n"
            ),
            [
                ("type", "FluentTestWindow"),
                ("type", "fluent::testing::Probe"),
                ("inline function", "label"),
                ("inline function", "findChildOf"),
                ("type", "Mode"),
            ],
        )

    def test_ignores_internal_linkage_members_and_text(self):
        self.assertEqual(
            self.names(
                "namespace {\nclass Hidden { class Nested {}; };\n}\n"
                "namespace outer { namespace { struct AlsoHidden {}; } }\n"
                "static QString helper() { return {}; }\n"
                "QString strongHelper() { return {}; }\n"
                "void Owner::member() { struct Local {}; }\n"
                "const auto paint = [](QPainter& painter) { struct InLambda {}; };\n"
                "// class Commented {};\n"
                "const char* text = \"class Quoted {\";\n"
                "#define DECLARE class Macro {};\n"
                "TEST_F(ButtonTest, Clicks) { class InTest {}; }\n"
            ),
            [],
        )

    def test_template_parameters_and_specializations_are_not_owned_types(self):
        self.assertEqual(
            self.names(
                "template <class T, typename U> struct Holder { T value; };\n"
                "template <> struct std::hash<Owned> { size_t operator()(const Owned&) const; };\n"
                "template <> struct QMetaTypeId<Owned> { static int qt_metatype_id(); };\n"
            ),
            [("type", "Holder")],
        )

    def test_macro_invocations_without_semicolons_do_not_hide_the_next_type(self):
        self.assertEqual(
            self.names("Q_DECLARE_METATYPE(Item)\n\nclass Next { };\n[[nodiscard]] struct Attributed {};\n"),
            [("type", "Next"), ("type", "Attributed")],
        )

    def test_extern_c_blocks_stay_visible(self):
        self.assertEqual(self.names('extern "C" { struct Exported {}; }\n'), [("type", "Exported")])


class FindConflictsTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def source(self, name: str, text: str) -> Path:
        path = self.root / name
        path.write_text(text, encoding="utf-8")
        return path

    def test_reports_types_shared_by_modules_of_one_executable(self):
        first = self.source("TestA.cpp", "class FluentTestWindow {};\n")
        second = self.source("TestB.cpp", "class FluentTestWindow { int other; };\n")
        errors = CHECK.find_conflicts([
            ("test_group_dialogs_flyouts_local", "test_dialog", first),
            ("test_group_dialogs_flyouts_local", "test_flyout", second),
        ])
        self.assertEqual(len(errors), 1)
        self.assertIn("type 'FluentTestWindow'", errors[0])
        self.assertIn("test_dialog", errors[0])
        self.assertIn("test_flyout", errors[0])

    def test_separate_executables_and_anonymous_helpers_do_not_conflict(self):
        first = self.source("TestA.cpp", "class FluentTestWindow {};\n")
        second = self.source("TestB.cpp", "class FluentTestWindow {};\n")
        hidden = self.source("TestC.cpp", "namespace { class FluentTestWindow {}; }\n")
        self.assertEqual(CHECK.find_conflicts([
            ("test_group_textfields_ci_full", "test_line_edit", first),
            ("test_group_textfields_local", "test_number_box", second),
            ("test_group_textfields_ci_full", "test_text_edit", hidden),
        ]), [])

    def test_a_header_listed_by_two_modules_is_one_definition(self):
        header = self.source("Model.h", "class SharedModel {};\n")
        errors = CHECK.find_conflicts([
            ("test_group_qmlbridge_ci_full", "test_qml_bridge", header),
            ("test_group_qmlbridge_ci_full", "test_qml_bridge_texture", header),
        ])
        self.assertEqual(errors, [])

    def test_a_compiled_source_shared_by_two_modules_is_reported(self):
        helper = self.source("Utils.cpp", "namespace { int value; }\n")
        errors = CHECK.find_conflicts([
            ("test_group_gallery_ci_full", "test_gallery_content_pages", helper),
            ("test_group_gallery_ci_full", "test_gallery_shell_framework", helper),
        ])
        self.assertEqual(len(errors), 1)
        self.assertIn("support library", errors[0])

    def test_manifest_round_trip_and_malformed_rows(self):
        source = self.source("TestA.cpp", "class A {};\n")
        manifest = self.root / "manifest.tsv"
        manifest.write_text(f"# executable\tmodule\tsource\ntest_group_x_local\ttest_a\t{source}\n",
                            encoding="utf-8")
        self.assertEqual(CHECK.read_manifest(manifest), [("test_group_x_local", "test_a", source)])
        with redirect_stdout(io.StringIO()) as output:
            self.assertEqual(CHECK.main(["--manifest", str(manifest)]), 0)
        self.assertIn("1 module sources in 1 test executables", output.getvalue())
        manifest.write_text("test_group_x_local\ttest_a\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            CHECK.read_manifest(manifest)


if __name__ == "__main__":
    unittest.main()
