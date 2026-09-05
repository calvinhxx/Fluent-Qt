#!/usr/bin/env python3
"""Check type discovery in the installed-header API index."""

import unittest

from generate_api_reference import _declarations


class PublicTypeDiscoveryTest(unittest.TestCase):
    def test_forward_declarations_are_independent_of_namespace_formatting(self):
        compact = "namespace fluent::basicinput { class Button; }\nclass LineEdit {};\n"
        expanded = "namespace fluent::basicinput {\nclass Button;\n}\nclass LineEdit {};\n"
        self.assertEqual(_declarations(compact), ["LineEdit"])
        self.assertEqual(_declarations(expanded), ["LineEdit"])

    def test_definitions_keep_exports_inheritance_and_multiline_bases(self):
        source = """
class FLUENTQT_EXPORT LineEdit final
    : public QLineEdit, public FluentElement
{
};
struct BackdropState {
};
class ScrollBar;
"""
        self.assertEqual(_declarations(source), ["LineEdit", "BackdropState"])

    def test_opaque_enums_are_not_reported_as_definitions(self):
        source = """
enum class Forward : int;
enum class BackdropEffect {
    Solid, Mica, Acrylic
};
enum Alignment { Left, Right };
"""
        self.assertEqual(_declarations(source), ["BackdropEffect", "Alignment"])


if __name__ == "__main__":
    unittest.main()
