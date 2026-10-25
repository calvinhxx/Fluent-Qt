from pathlib import Path
import tempfile
import unittest

from check_gallery_language import CJK, cpp_literals, python_literals, validate


class GalleryLanguageTests(unittest.TestCase):
    def test_cpp_comments_are_allowed_but_raw_and_escaped_literals_are_checked(self):
        source = '// 中文注释\n/* 说明 */ auto a = "English";\nauto b = u8R"tag(团队)tag";\nauto c = "\\u4e2d";'
        self.assertEqual([line for line, value in cpp_literals(source) if CJK.search(value)], [3, 4])

    def test_python_source_comments_are_allowed_but_displayed_snippets_are_checked(self):
        source = '''"""中文模块说明"""
# 中文注释
snippet = """
    # 示例注释
    label.setText("团队")
"""
label = f"中文 {count}"
'''
        values = [value for _, value in python_literals(source) if CJK.search(value)]
        self.assertEqual(len(values), 2)
        self.assertIn('label.setText("团队")', values[0])
        self.assertEqual(values[1], "中文 ")

    def test_scan_includes_generated_and_python_copy_but_not_library_or_tests(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = {
                "app/sample.cpp": 'auto text = "团队";',
                "app/data/snippets.json": '{"code": "label.setText(\\\"\\u4e2d\\\")"}',
                "bindings/pyside6/gallery/src/sample.py": 'snippet = r\'label.setText("\\u6210\\u5458")\'',
                "src/components/Test.h": 'auto text = "中文";',
                "tests/components/test.py": 'query = "中文"',
            }
            for name, content in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            self.assertEqual(len(validate(root)), 3)

    def test_latin_names_symbols_and_user_input_handlers_remain_valid(self):
        source = 'label = "María García · Design"\ntext = editor.text()\n'
        self.assertFalse(any(CJK.search(value) for _, value in python_literals(source)))

    def test_plain_multiline_copy_is_not_mistaken_for_a_code_identifier(self):
        self.assertTrue(any(CJK.search(value) for _, value in python_literals('label = "中文\\n"')))


if __name__ == "__main__":
    unittest.main()
