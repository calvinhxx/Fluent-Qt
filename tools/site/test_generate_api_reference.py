#!/usr/bin/env python3
"""Check type discovery in the installed-header API index."""

import unittest

from generate_api_reference import _declarations, _property_reference


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


class PropertyReferenceTest(unittest.TestCase):
    def test_uses_reader_comment_and_preserves_translations(self):
        source = '''
Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
Q_PROPERTY(Backend activeBackend READ activeBackend NOTIFY rendererChanged)
Q_PROPERTY(int undocumented READ undocumented)
/** @brief Scene zoom; default 1.
 * Applies to every surface.
 * zh_CN: 场景倍率，默认 1。
 * 作用于所有表面。 */
qreal zoom() const;
/** @brief Setter detail is not the property description. */
void setZoom(qreal value);
/** @brief Actual backend. zh_CN: 实际后端。 */
Backend activeBackend() const;
int undocumented() const;
'''
        properties = _property_reference(source)
        self.assertEqual([entry["name"] for entry in properties], ["zoom", "activeBackend"])
        self.assertEqual(properties[0]["description"], {
            "en": "Scene zoom; default 1. Applies to every surface.",
            "zh": "场景倍率，默认 1。 作用于所有表面。",
        })
        self.assertFalse(properties[0]["read_only"])
        self.assertTrue(properties[1]["read_only"])


if __name__ == "__main__":
    unittest.main()
