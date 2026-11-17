"""Reject invented API names and API references on the wrong component."""
import unittest

from validate_documentation_claims import check_text, public_methods, validate


class DocumentationClaimsTest(unittest.TestCase):
    def test_button_cannot_borrow_an_unrelated_animation_switch(self):
        errors, _ = check_text('Call setAnimationEnabled(false).', public_methods('Button'), 'button', set())
        self.assertEqual(len(errors), 1)
        errors, _ = check_text('Call Dialog::setAnimationEnabled(false).', public_methods('Button'), 'button', set())
        self.assertEqual(errors, [])

    def test_qualified_owner_must_exist_and_have_the_method(self):
        for text in ('ImaginaryControl::open()', 'Button::setAnimationEnabled(false)',
                     'ChartView::setPoints()', 'imaginary::ChartModel::setPoints()'):
            with self.subTest(text=text):
                self.assertTrue(check_text(text, public_methods('ChartView'), 'example', set())[0])
        self.assertFalse(check_text('ChartModel::setPoints()', set(), 'example', set())[0])

    def test_inherited_qt_members_are_type_scoped(self):
        self.assertFalse(check_text('setTristate(true)', public_methods('CheckBox'), 'checkbox', set())[0])
        self.assertTrue(check_text('setTristate(true)', public_methods('Button'), 'button', set())[0])
        self.assertFalse(check_text('setFluentStyle(Button::Accent)', public_methods('CompoundButton'), 'compound', set())[0])
        self.assertFalse(check_text('setEnabled(false)', public_methods('Button'), 'button', set())[0])
        for name in ('setText', 'setFocus', 'setGeometry', 'setShortcut'):
            with self.subTest(method=name):
                self.assertFalse(check_text(f'Button::{name}()', public_methods('Button'), 'button', set())[0])

    def test_menu_inherits_recursive_submenu_api(self):
        errors, refs = check_text('QMenu::addMenu()', public_methods('FluentMenu'), 'menu', set())
        self.assertEqual(errors, [])
        self.assertFalse(any('warning' in ref for ref in refs))

    def test_partial_qt_index_reports_uncertainty_without_rejecting_real_members(self):
        errors, references = check_text('setContentsMargins(4, 4, 4, 4)', public_methods('Button'),
                                       'button', set(), 'Button')
        self.assertEqual(errors, [])
        self.assertIn('warning', references[0])
        self.assertTrue(check_text('ImaginaryControl::setContentsMargins()', set(), 'button', set())[0])
        self.assertTrue(check_text('fluentqt.nonexistent_api()', set(), 'guide', set())[0])

    def test_guides_and_components_have_valid_references(self):
        errors, coverage = validate()
        self.assertEqual(errors, [])
        self.assertTrue(coverage)


if __name__ == '__main__':
    unittest.main()
